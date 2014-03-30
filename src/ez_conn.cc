#include "ez_conn.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <errno.h>

int ez_conn::accept(int fd, struct sockaddr_in *addr)
{
    addr = addr; // no effect
    assert(sock_ == -1);
    assert(fd >= 0);
    if (poll_->add(fd, this) == -1)
        return -1;
    assert(poll_->modr(fd, true) == 0);
    sock_ = fd;
    status_ = connected;
    del_myself_ = true;
    return 0;
}

int ez_conn::connect(const char *ip, int port)
{
    assert(status_ == none);
    assert(handler_);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) return -1;
    int flag = fcntl(sock, F_GETFL);
    if (flag == -1 || fcntl(sock, F_SETFL, flag | O_NONBLOCK) == -1)
    {
        ::close(sock); return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    int ret = ::connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) 
    {
        if (errno == EINPROGRESS)
        {
            if (poll_->add(sock, this) == 0)
            {
                assert(poll_->modw(sock, true) == 0);
                sock_ = sock;
                status_ = connecting;
                return 0;
            }
        }
        ::close(sock);
        return -1;
    } 
    sock_ = sock;
    status_ = connected;
    return 0;
}

class conn_del_timer: public ez_timer
{
public:
    conn_del_timer(ez_conn *conn): conn_(conn) { assert(conn_); }
    ~conn_del_timer() { delete conn_; }
    virtual void on_timer(ez_poll *poll)
    {
        delete this;
    }
private:
    ez_conn *conn_;
};

int ez_conn::close()
{
    assert(status_ == connecting || status_ == connected);
    if (poll_) // ez_server may detach ez_conn from ez_poll temporarily
    {
        assert(poll_->del(sock_) == 0);
    }
    delete sock_ip_; // safe to delete NULL
    delete peer_ip_;
    sock_ip_ = peer_ip_ = NULL;
    sock_port_ = peer_port_ = 0;
    ::close(sock_);
    sock_ = -1;
    status_ = none;
    inbuf_.reset_buffer();
    outbuf_.reset_buffer();
    if (handler_)
    {   // bugfix: user may do reconnect in on_close
        // i should reset handler_=NULL before on_close called
        ez_handler *tmp = handler_; 
        handler_ = NULL;
        tmp->on_close(poll_, this);
    }
    if (del_myself_)
    {
        //defer delete
        poll_->add_timer(new conn_del_timer(this), 0);
    }
    del_myself_ = false;
    close_tag_++;
    return 0;
}

int ez_conn::send_message(const char *msg, int size)
{
    assert(msg);
    assert(size > 0);
    assert(status_ != none);
    if (status_ == connecting || outbuf_.get_buffer_length())
    {
        outbuf_.append_buffer(msg, size);
        return 0;
    }
    int ret = write(sock_, msg, size);
    if (ret < 0)
    {
        if (errno != EINTR && errno != EAGAIN)
            return -1;
        ret = 0;
    } 
    else if (size == ret)
            return 0;
    outbuf_.append_buffer(msg + ret, size - ret);
    assert(poll_->modw(sock_, true) == 0);
    return 0;
}

void ez_conn::on_event(ez_poll *poll, int fd, short event)
{
    uint64_t tag = close_tag_;
    if (event & ez_read)
    {
        handle_read();
    }
    if (close_tag_ == tag && (event & ez_write))
    {
        handle_write();
    }
    if (close_tag_ == tag && (event & ez_error))
    {
        handle_error();
    }
}

void ez_conn::handle_read()
{
    assert(handler_);
    assert(inbuf_.reserve_space(4096));

    char *buf;
    size_t len;
    inbuf_.get_space_begin(&buf, &len);
    int ret = read(sock_, buf, len);
    if (ret < 0)
    {
        if (errno != EINTR && errno != EAGAIN)
        {
            handler_->on_error(poll_, this);
        }
        return;
    }
    else if (ret == 0) 
    {
        this->close();
    }
    else
    {
        inbuf_.append_buffer_ex(ret);
        handler_->on_message(poll_, this);
    }
}

void ez_conn::handle_write()
{
    if (status_ == connecting)
    {
        status_ = connected;
        if (!outbuf_.get_buffer_length())
            assert(poll_->modw(sock_, false) == 0);
        assert(poll_->modr(sock_, true) == 0);
    } 
    else 
    {
        const char *buf;
        size_t len;
        outbuf_.get_buffer_begin(&buf, &len);
        int ret = write(sock_, buf, len);
        if (ret < 0)
        {
            if (errno != EINTR && errno != EAGAIN)
                handler_->on_error(poll_, this);
            return;
        }
        else
        {
            outbuf_.erase_buffer(ret);
            if (!outbuf_.get_buffer_length())
                assert(poll_->modw(sock_, false) == 0);
        }
    }
}

void ez_conn::handle_error()
{
    handler_->on_error(poll_, this);
}

int ez_conn::get_message(const char **msg, size_t *size)
{
    inbuf_.get_buffer_begin(msg, size);
    return 0;
}

int ez_conn::use_message(size_t size)
{
    if (!inbuf_.erase_buffer(size))
        return -1;
    return 0;
}

int ez_conn::getsockname(const char **ip, int *port)
{
    assert(sock_ >= 0 && ip && port);
    if (!sock_ip_)
    {
        char buf[32] = {0};
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if (::getsockname(sock_, (struct sockaddr *)&addr, &len) == -1)
            return -1;
        if (!inet_ntop(AF_INET, &addr.sin_addr.s_addr, buf, sizeof(buf)))
            return -1;
        this->sock_ip_ = new std::string(buf);
        this->sock_port_ = ntohs(addr.sin_port);
    }
    *ip = sock_ip_->c_str();
    *port = sock_port_;
    return 0;
}

int ez_conn::getpeername(const char **ip, int *port)
{
    assert(sock_ >= 0 && ip && port);
    // "del_myself_ == false" means this is used as a client, you must know peer's address, no need to call!
    // just crash fast!
    assert(del_myself_ == true); 
    if (!peer_ip_)
    {
        char buf[32] = {0};
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if (::getpeername(sock_, (struct sockaddr *)&addr, &len) == -1)
            return -1;
        if (!inet_ntop(AF_INET, &addr.sin_addr.s_addr, buf, sizeof(buf)))
            return -1;
        this->peer_ip_ = new std::string(buf);
        this->peer_port_ = ntohs(addr.sin_port);
    }
    *ip = peer_ip_->c_str();
    *port = peer_port_;
    return 0;
}

int ez_conn::detach()
{
    assert(status_ != none && poll_);
    assert(poll_->del(sock_) == 0);
    poll_ = NULL;
    return 0;
}

int ez_conn::attach(ez_poll *poll)
{
    assert(status_ != none && !poll_ && poll);
    if (poll->add(sock_, this) != 0)
        return -1;
    assert(poll->modr(sock_, true) == 0);
    if (outbuf_.get_buffer_length())
    {
        assert(poll->modw(sock_, true));
    }
    poll_ = poll;
    return 0;
}
