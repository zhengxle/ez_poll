#include "ez_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int ez_server::listen(const char *ip, int port)
{
    assert(lis_sock_ == -1);
    assert(ip);
    assert(port > 0);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) return -1;
    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, (socklen_t)sizeof(on)) == -1)
    {
        close(sock);
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1 || ::listen(sock, 1024) == -1)
    {
        close(sock);
        return -1;
    }
    if (poll_->add(sock, this) == -1)
    {
        close(sock);
        return -1;
    } 
    assert(poll_->modr(sock, true) == 0);
    lis_sock_ = sock;
    return 0;
}

int ez_server::stop()
{
    assert(lis_sock_ != -1);
    assert(poll_->del(lis_sock_) == 0);
    close(lis_sock_);
    lis_sock_ = -1;
    handler_ = NULL;
    return 0;
}

void ez_server::on_event(ez_poll *poll, int fd, short event)
{
    assert(handler_);
    int sock = accept(fd, NULL, NULL);
    if (sock >= 0)
    {
        ez_conn *conn = new ez_conn(poll);
        if (conn->accept(sock, NULL) == -1)
        {
            delete conn;
            ::close(sock);
            return;
        }
        handler_->on_accept(poll, conn);
    }
}

int ez_server::schedule(ez_conn *conn)
{
    std::pair<uint64_t, ez_thread *> p = pool_.choose_thread();
    p.second->get_poll()->run_task(new ez_schedule_task(conn, handlers_[p.first]));
    return 0;
}

void ez_listen_proxy::on_accept(ez_poll *poll, ez_conn *conn)
{
    assert(conn->detach() == 0);
    srv_->schedule(conn);
}

int ez_server::init_threads(size_t num_thread)
{
    assert(threads_.size() == 0);
    assert(0 == pool_.init_threads(num_thread));
    threads_ = pool_.get_threads();
    handler_ = new ez_listen_proxy(this);
    return 0;
}
int ez_server::free_threads()
{
    assert(threads_.size() > 0);
    assert(pool_.free_threads() == 0);
    threads_.clear();
    handlers_.clear();
    delete handler_;
    handler_ = NULL;
    return 0;
}

int ez_server::start_threads(const std::vector<ez_listen_handler *> &handlers)
{
    assert(0 == pool_.start_threads());
    handlers_ = handlers;
    return 0;
}

int ez_server::join_threads()
{
    assert(threads_.size() > 0);
    assert(0 == pool_.join_threads());
    handlers_.clear();
    return 0;
}

    int join_threads();

void ez_schedule_task::on_action(ez_poll *poll)
{
    assert(conn_->attach(poll) == 0);
    handler_->on_accept(poll, conn_);
    delete this;
}
    
ez_schedule_task::ez_schedule_task(ez_conn *conn, ez_listen_handler *handler): conn_(conn), handler_(handler)
{
}

