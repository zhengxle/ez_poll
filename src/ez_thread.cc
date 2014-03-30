#include "ez_thread.h"
#include <signal.h>
#include <assert.h>

ez_thread::ez_thread()
{
    assert(poll_.init() >= 0);
}

ez_thread::~ez_thread()
{
    assert(poll_.shutdown() == 0);
}
    
int ez_thread::start()
{
    stop_ = false;
    pthread_create(&tid_, NULL, thread_main, this);
    poll_.set_thread_id(tid_);
    return 0;
}

int ez_thread::stop()
{
    poll_.run_task(this);
    pthread_join(tid_, NULL);
    poll_.set_thread_id(pthread_self()); // reset tid, so that any operation of poll can work directly
    return 0;
}

void ez_thread::on_action(ez_poll *poll)
{
    poll->stop();
}

void *ez_thread::thread_main(void *arg)
{
    ez_thread *t = (ez_thread *)arg;
    t->main();
    return NULL;
}

void ez_thread::main()
{
    poll_.run();
}

ez_poll *ez_thread::get_poll()
{
    return &poll_;
}

int ez_thread_pool::init_threads(size_t num_thread)
{
    assert(num_thread > 0);
    index_ = 0;
    
    // block all signals for new created threads
    sigset_t block_all;
    sigset_t cur;
    sigfillset(&block_all);
    sigprocmask(SIG_SETMASK, &block_all, &cur); 

    for (size_t i = 0; i < num_thread; ++i)
    {
        ez_thread *t = new ez_thread();
        threads_.push_back(t);
    }
    // recover main-thread's mask 
    sigprocmask(SIG_SETMASK, &cur, NULL);
    return 0;
}
int ez_thread_pool::start_threads()
{
    assert(threads_.size() > 0);
    for (size_t i = 0; i < threads_.size(); ++i)
    {
        assert(threads_[i]->start() == 0);
    }
    return 0;
}

int ez_thread_pool::join_threads()
{
    assert(threads_.size() > 0);
    for (size_t i = 0; i < threads_.size(); ++i)
    {
        assert(threads_[i]->stop() == 0);
    }
    return 0;
}

int ez_thread_pool::free_threads()
{
    assert(threads_.size() > 0);
    for (size_t i = 0; i < threads_.size(); ++i)
    {
        ez_thread *t = threads_[i];
        delete t;
    }
    threads_.clear();
    return 0;
}

std::vector<ez_thread *> ez_thread_pool::get_threads()
{
    assert(threads_.size() > 0);
    return threads_;
}

std::pair<uint64_t, ez_thread *> ez_thread_pool::choose_thread()
{
    ez_thread *t = threads_[index_];
    std::pair<uint64_t, ez_thread *> ret(index_, t); 
    index_ = (index_ + 1) % threads_.size();
    return ret;
}
