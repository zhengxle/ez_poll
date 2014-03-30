#ifndef _EZ_CONN_H
#define _EZ_CONN_H

#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include "ez_buffer.h"
#include "ez_poll.h"
#include <string>

class ez_poll;
class ez_conn;
class ez_handler
{
public:
    virtual ~ez_handler() { }
    virtual void on_message(ez_poll *poll, ez_conn *conn) = 0;
    virtual void on_error(ez_poll *poll, ez_conn *conn) = 0;
    virtual void on_close(ez_poll *poll, ez_conn *conn) = 0;
};

class ez_conn: public ez_fd
{
    enum conn_status { none, connecting, connected };
public:
    ez_conn(ez_poll *poll):
        sock_(-1), poll_(poll), handler_(NULL), status_(none), del_myself_(false), close_tag_(0),
        sock_ip_(NULL), sock_port_(0), peer_ip_(NULL), peer_port_(0)
    { assert(poll); }

    virtual void on_event(ez_poll *poll, int fd, short event);
    
    // info are cached inside, it's fast to call 
    int getsockname(const char **ip, int *port);
    // notice: fetching peer address may fail if called after connect immediately, because connecting is in progress.
    // but who wants to get the address that is connecting to(you must already know it)?
    int getpeername(const char **ip, int *port); 

    int send_message(const char *msg, int size);
    int get_message(const char **msg, size_t *size); 
    int use_message(size_t size); 
    int connect(const char *ip, int port);
    int accept(int fd, struct sockaddr_in *addr);
    int set_sock(int sock) { sock_ = sock; assert(sock_ >= 0); return 0;}
    int set_handler(ez_handler *handler) { handler_ = handler; return 0;}
    int close();
    int valid() { return status_ != none; }
    
    // for multi-thread schedule design
    int detach();
    int attach(ez_poll *poll);

private:
    void handle_read();
    void handle_write();
    void handle_error();
    int sock_;
    ez_poll *poll_;
    ez_handler *handler_;
    conn_status status_;
    ez_buffer inbuf_;
    ez_buffer outbuf_;
    bool del_myself_;
    uint64_t close_tag_;
    std::string *sock_ip_;
    int sock_port_;
    std::string *peer_ip_;
    int peer_port_;
};

#endif
