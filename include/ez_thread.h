#ifndef _EZ_THREAD_H
#define _EZ_THREAD_H

#include <vector>
#include <inttypes.h>
#include <pthread.h>
#include <utility>
#include "ez_poll.h"

class ez_thread: public ez_task
{
public:
    ez_thread();
    ~ez_thread();
    int start();
    int stop();
    void main();
    ez_poll *get_poll();
    virtual void on_action(ez_poll *poll);
    static void *thread_main(void *arg);

private:
    pthread_t tid_;
    ez_poll poll_;
    bool stop_;
};

class ez_thread_pool
{
public:
    int init_threads(size_t num_thread);
    int free_threads();
    int start_threads();
    int join_threads();
    std::vector<ez_thread *> get_threads();
    std::pair<uint64_t, ez_thread *> choose_thread();
private:
    uint64_t index_;
    std::vector<ez_thread *> threads_;
};

#endif
