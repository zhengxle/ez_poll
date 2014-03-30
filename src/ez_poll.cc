#include "ez_poll.h"
#include <sys/epoll.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>

ez_poll::ez_poll()
{
    epfd_ = -1;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);
    pthread_mutex_init(&lock_, NULL);
}

ez_poll::~ez_poll()
{
    pthread_mutex_destroy(&lock_);
}

int ez_poll::run()
{
    run_ = true;
    while (run_)
    {
        this->poll(1);
    }
    this->poll(0); // clear unproc tasks
    return 0;
}

int ez_poll::stop()
{
    assert(run_);
    run_ = false;
    return 0;
}

int ez_poll::init()
{
    assert(epfd_ == -1);
    timer_id_ = 0;
    wake_ = NULL;
    wake_fd_[0] = wake_fd_[1] = -1;
    fd2data_ = NULL;
    maxfd_ = -1;
    inloop_ = false;
    closed_ = NULL;
    closed_size_ = closed_count_ = 0;
    epfd_ = epoll_create(10240);
    assert(init_wakeup() == 0);
    tid_ = pthread_self();
    return epfd_;
}

int ez_poll::shutdown()
{
    assert(epfd_ != -1);
    this->poll(0); // only for handle the left timeout timers/events once
    if (wake_)
    {
        close(wake_fd_[0]);
        close(wake_fd_[1]);
        delete wake_;
        wake_fd_[0] = wake_fd_[1] = -1;
        wake_ = NULL;
    }
    for (int fd = 0; fd <= maxfd_; ++fd)
    {
        delete fd2data_[fd];
    }
    free(fd2data_);
    fd2data_ = NULL;
    for (int i = 0; i < closed_count_; ++i)
    {
        delete closed_[i];
    }
    free(closed_);
    closed_ = NULL;
    closed_count_ = closed_size_ = 0;
    maxfd_ = -1;
    inloop_ = false;
    close(epfd_);
    epfd_ = -1;
    tasks_.clear();
    timer_map_.clear();
    id_map_.clear();
    return 0;
}

int ez_poll::add(int fd, ez_fd *ezfd)
{
    assert(fd >= 0);
    assert(ezfd);
    assert(epfd_ != -1);
    assert(fd > maxfd_ || (fd <= maxfd_ && !fd2data_[fd]));
    
    if (setnonblock(fd) == -1)
        return -1;

    ez_data *data = new ez_data();
    data->fd_ = fd;
    data->ezfd_ = ezfd;
    data->event_ = ez_none;
    data->closed_ = false;
    if (fd > maxfd_)
    {
        ez_data **tmp = (ez_data **)realloc(fd2data_, (fd + 1) * sizeof(*tmp));
        if (!tmp)
        {
            delete data;
            return -1;
        }
        fd2data_ = tmp;
        memset(fd2data_ + maxfd_ + 1, 0, sizeof(*fd2data_) * (fd - maxfd_));
        maxfd_ = fd;
    }
    fd2data_[fd] = data;
    
    struct epoll_event event = {0};
    event.data.ptr = (void *)(data);
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        delete data;
        fd2data_[fd] = NULL;
        return -1;
    }
    return 0;
}

int ez_poll::del(int fd)
{
    assert(fd <= maxfd_);
    assert(fd >= 0);
    assert(fd2data_[fd]);

    if (inloop_)
    {
        if (closed_count_ >= closed_size_)
        {
            ez_data **tmp = (ez_data **)realloc(closed_, (closed_size_ + 1) * sizeof(*tmp));
            if (!tmp)
                return -1;
            closed_ = tmp;
            closed_size_++;
        }
        closed_[closed_count_++] = fd2data_[fd];
        fd2data_[fd]->closed_ = true;
    } 
    else {
        delete fd2data_[fd];
    }
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL); 
    fd2data_[fd] = NULL;
    return 0;
}

int ez_poll::modr(int fd, bool set)
{
    assert(fd >= 0);
    assert(fd <= maxfd_);
    assert(fd2data_[fd]);

    if (set)
        fd2data_[fd]->event_ |= ez_read;
    else
        fd2data_[fd]->event_ &= ~ez_read;

    return ez_poll::mod(fd);
}

int ez_poll::modw(int fd, bool set)
{
    assert(fd >= 0);
    assert(fd <= maxfd_);
    assert(fd2data_[fd]);

    if (set)
        fd2data_[fd]->event_ |= ez_write;
    else
        fd2data_[fd]->event_ &= ~ez_write;

    return ez_poll::mod(fd);
}

int ez_poll::run_task()
{
    std::deque<ez_task *> temp;
    pthread_mutex_lock(&lock_);
    temp.swap(tasks_);
    pthread_mutex_unlock(&lock_);
    while (!temp.empty())
    {
        ez_task *task = temp.front();
        temp.pop_front();
        task->on_action(this);
    }
    return 0;
}

int ez_poll::poll(int timeout)
{
    assert(epfd_ != -1);
    run_task();
    run_timer();
    inloop_ = true;
    struct epoll_event events[32];
    int numfd = epoll_wait(epfd_, events, 32, timeout);
    if (numfd <= 0)
        return 0;
    for (int i = 0; i < numfd; ++i)
    {
        ez_data *data = (ez_data *)events[i].data.ptr;
        ez_fd *ezfd = data->ezfd_;
        if (data->closed_) // bugfix, i have missed this check before
            continue;
        short event = ez_none; // bugfix, double check, i have missed too!
        if ((data->event_ & ez_read) && (events[i].events & EPOLLIN))
            event |= ez_read;
        if ((data->event_ & ez_write) && (events[i].events & EPOLLOUT))
            event |= ez_write;
        if (events[i].events & (EPOLLERR | EPOLLHUP))
            event |= ez_error;
        ezfd->on_event(this, data->fd_, event);
    }
    for (int i = 0; i < closed_count_; ++i)
    {
        assert(closed_[i]->closed_);
        delete closed_[i];
    }
    closed_count_ = 0;
    inloop_ = false;
    return 0;
}

int ez_poll::setnonblock(int fd)
{
    int flag = fcntl(fd, F_GETFL);
    if (flag == -1)
        return -1;
    if (fcntl(fd, F_SETFL, flag | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

int ez_poll::init_wakeup()
{
    assert(epfd_ != -1);
    assert(wake_fd_[0] == -1);

    if (pipe(wake_fd_) == -1)
        return -1;
   
    if (setnonblock(wake_fd_[0]) == -1 || setnonblock(wake_fd_[1]) == -1)
    {
        close(wake_fd_[0]);
        close(wake_fd_[1]);
        return -1;
    }

    ez_wakefd *wfd = new ez_wakefd();
    if (add(wake_fd_[0], wfd) == -1)
    {
        delete wfd;
        close(wake_fd_[0]);
        close(wake_fd_[1]);
        return -1;
    }
    wake_ = wfd;
    assert(this->modr(wake_fd_[0], true) == 0);
    return 0;
}

int ez_poll::mod(int fd)
{
    ez_data *data = fd2data_[fd];
    struct epoll_event event = {0};
    event.data.ptr = data;
    event.events = (data->event_ & ez_read ? EPOLLIN : 0) | (data->event_ & ez_write ? EPOLLOUT : 0);
    return epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &event);
}

int ez_poll::wakeup()
{
    assert(epfd_ != -1);
    assert(wake_fd_[1] != -1);
    int ret = write(wake_fd_[1], "", 1);
    if (ret == 1 || errno == EAGAIN)
        return 0;
    return -1;
}

void ez_poll::ez_wakefd::on_event(ez_poll *poll, int fd, short event)
{
    char buf[32];
    read(fd, buf, sizeof(buf));
}

uint64_t ez_poll::get_current_time()
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    uint64_t now = (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
    return now;
}

ez_timer_id ez_poll::add_timer(ez_timer *timer, int after)
{
    assert(timer);
    assert(after >= 0);

    uint64_t id = get_timer_id();
    uint64_t when = get_current_time() + after;
    if (in_thread())
    {
        add_timer_in_thread(id, timer, when);
    }
    else
    {
        ez_add_timer *t = new ez_add_timer(id, timer, when);
        run_task_async(t);
    }
    return id;
}

int ez_poll::del_timer(ez_timer_id id)
{
    if (in_thread())
    {
        del_timer_in_thread(id);
    }
    else
    {
        ez_del_timer *t = new ez_del_timer(id);
        run_task_async(t);
    }
    return 0;
}

int ez_poll::run_timer()
{
    uint64_t now = get_current_time();
    while (!timer_map_.empty())
    {
        timer_iter iter = timer_map_.begin();
        if (iter->first >= now) // >= to avoid infinite loop if user call "add_timer(timer, 0)"
            break;
        timer_info info = iter->second;
        timer_map_.erase(iter);
        ez_timer_id id = info.first;
        id_map_.erase(id);
        ez_timer *timer = info.second;
        timer->on_timer(this);
    }
    return 0;
}

void ez_poll::set_thread_id(pthread_t tid)
{
    tid_ = tid;
}

bool ez_poll::in_thread()
{
    return pthread_self() == tid_;
}

void ez_poll::run_task(ez_task *task)
{
    assert(task);
    if (in_thread())
        task->on_action(this);
    else
        run_task_async(task);
}

void ez_poll::run_task_async(ez_task *task)
{
    pthread_mutex_lock(&lock_);
    tasks_.push_back(task);
    pthread_mutex_unlock(&lock_);
    assert(wakeup() == 0);
}
    
uint64_t ez_poll::get_timer_id()
{
    pthread_mutex_lock(&lock_);
    uint64_t id = timer_id_++;
    pthread_mutex_unlock(&lock_);
    return id;
}

ez_add_timer::ez_add_timer(uint64_t id, ez_timer *timer, uint64_t when): id_(id), timer_(timer), when_(when)
{
}
void ez_add_timer::on_action(ez_poll *poll)
{
    poll->add_timer_in_thread(id_, timer_, when_);
    delete this;
}

void ez_poll::add_timer_in_thread(ez_timer_id id, ez_timer *timer, uint64_t when)
{
    timer_iter iter = timer_map_.insert(std::make_pair(when, std::make_pair(id, timer)));
    assert(id_map_.insert(std::make_pair(id, iter)).second);
}

void ez_poll::del_timer_in_thread(ez_timer_id id)
{
    id_iter iter = id_map_.find(id); 
    if (iter != id_map_.end())
    {
        timer_iter titer = iter->second;
        id_map_.erase(iter);
        timer_map_.erase(titer);
    }
}

ez_del_timer::ez_del_timer(uint64_t id): id_(id)
{
}

void ez_del_timer::on_action(ez_poll *poll)
{
    poll->del_timer_in_thread(id_);
    delete this;
}
