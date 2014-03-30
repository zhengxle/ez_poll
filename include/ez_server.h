#ifndef _EZ_SERVER_H
#define _EZ_SERVER_H

#include "ez_poll.h"
#include "ez_conn.h"
#include "ez_thread.h"
#include <vector>
#include <deque>
#include <inttypes.h>
#include <signal.h>

class ez_listen_handler
{
public:
    virtual void on_accept(ez_poll *poll, ez_conn *conn) = 0;
    virtual ~ez_listen_handler() {}
};

class ez_server;

class ez_listen_proxy: public ez_listen_handler
{
public:
    ez_listen_proxy(ez_server *srv): srv_(srv) 
    {
        assert(srv_);
    }
    virtual void on_accept(ez_poll *poll, ez_conn *conn);
private:
    ez_server *srv_;
};

class ez_schedule_task: public ez_task
{
public:
    ez_schedule_task(ez_conn *conn, ez_listen_handler *handler);
    virtual void on_action(ez_poll *poll);    
private:
    ez_conn *conn_;
    ez_listen_handler *handler_;
};

class ez_server: public ez_fd
{
public:
    ez_server(ez_poll *poll): poll_(poll), lis_sock_(-1), handler_(NULL)
    {
        assert(poll); 
    }
    int listen(const char *ip, int port);
    int stop();
    int set_handler(ez_listen_handler *handler) { handler_ = handler; return 0;}
    virtual void on_event(ez_poll *poll, int fd, short event);
    
    int init_threads(size_t num_thread);
    int free_threads();
    int start_threads(const std::vector<ez_listen_handler *> &handlers);
    int join_threads();

    ez_thread_pool *get_thread_pool() { return &pool_; }

    int schedule(ez_conn *conn);

private:
    ez_poll *poll_;
    int lis_sock_;
    ez_listen_handler *handler_;
    std::vector<ez_listen_handler *> handlers_;
    std::vector<ez_thread *> threads_;
    ez_thread_pool pool_;
};

#endif
