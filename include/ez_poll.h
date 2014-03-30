#ifndef _EZ_POLL_H
#define _EZ_POLL_H

#include <map>
#include <deque>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

enum ez_event
{
    ez_none = 0x00,
    ez_read = 0x01, 
    ez_write = 0x02,
    ez_error = 0x04,
};

class ez_poll;

class ez_fd
{
public:
    virtual void on_event(ez_poll *poll, int fd, short event) = 0;
    virtual ~ez_fd() { };
};

class ez_timer
{
public:
    virtual void on_timer(ez_poll *poll) = 0;
    virtual ~ez_timer() { }
};

// support for async task(added from a different thread) for ez_poll
class ez_task
{
public:
    virtual void on_action(ez_poll *poll) = 0;
    virtual ~ez_task() {} 
};

class ez_add_timer: public ez_task
{
public:
    ez_add_timer(uint64_t id, ez_timer *timer, uint64_t when);
    virtual void on_action(ez_poll *poll);
private:
    uint64_t id_;
    ez_timer *timer_;
    uint64_t when_;
};

class ez_del_timer: public ez_task
{
public:
    ez_del_timer(uint64_t id);
    virtual void on_action(ez_poll *poll);
private:
    uint64_t id_;
};

typedef uint64_t ez_timer_id;

class ez_poll 
{
public:
    typedef std::pair<uint64_t, ez_timer *> timer_info;
    typedef std::multimap<uint64_t, timer_info> timer_map;// time -> (id, ez_timer *)
    typedef timer_map::iterator timer_iter;
    typedef std::map<uint64_t/*id*/, timer_iter> id_map; // id -> iterator of timer_map
    typedef id_map::iterator id_iter; // id -> iterator of timer_map

    ez_poll();
    ~ez_poll();
    int init();
    int run();
    int stop();
    int shutdown();
    int add(int fd, ez_fd *ezfd);
    int del(int fd);
    int modr(int fd, bool set);
    int modw(int fd, bool set);
    int poll(int timeout/*msecs*/);
    int init_wakeup();
    int wakeup();
    
    /// new api for async tasks 
    //  
    //  it's all thread-safe to call !!
    //////
    void run_task(ez_task *task);
    void set_thread_id(pthread_t tid);
    ez_timer_id add_timer(ez_timer *timer, int after);
    int del_timer(ez_timer_id id);
    ///////////////////////////////
    void add_timer_in_thread(ez_timer_id id, ez_timer *timer, uint64_t when);
    void del_timer_in_thread(ez_timer_id id);
    
private:
    bool in_thread();
    uint64_t get_current_time();
    uint64_t get_timer_id();
    void run_task_async(ez_task *task);
    
    int mod(int fd);
    int run_task();
    int run_timer();
    int setnonblock(int fd);
    class ez_data
    {
    public:
        int fd_;
        ez_fd *ezfd_;
        short event_;
        bool closed_;
    };
    class ez_wakefd: public ez_fd
    {
    public:
        virtual void on_event(ez_poll *poll, int fd, short event);
    };
    bool run_;
    bool inloop_;
    int epfd_;
    int maxfd_;
    ez_data **fd2data_;
    int wake_fd_[2];
    ez_wakefd *wake_;
    int closed_size_;
    int closed_count_;
    ez_data **closed_;

    pthread_t tid_;
    id_map id_map_;
    timer_map timer_map_;
    uint64_t timer_id_;
    pthread_mutex_t lock_;
    std::deque<ez_task *> tasks_;
};

#endif
