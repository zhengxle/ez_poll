#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "ez_proto.h"
#include "ez_poll.h"
#include "ez_server.h"
#include <map>

static sig_atomic_t stop_server = 0;

class my_server;

class my_client: public ez_handler
{
public:
    my_client(my_server *srv, uint64_t client_id, ez_conn *conn): client_id_(client_id), mysrv_(srv), conn_(conn) { }
    virtual void on_message(ez_poll *poll, ez_conn *conn)
    {
        int packlen;
        while ((packlen = proto_.unpack_message(conn)) > 0)
        {
            const std::string &req = proto_.get_body();
            printf("id=%"PRIu64" command=%"PRIu64" bodylen=%"PRIu64" body=%.*s\n", proto_.get_id(), proto_.get_command(), proto_.get_bodylen(), (int)req.size(), req.c_str());

            const std::string &res = proto_.pack_message(
                    proto_.get_id(), proto_.get_command(), "sample_server", "pong\n", 5);
            if (conn->send_message(res.c_str(), res.size()) == -1)
            {
                conn->close();
                return;
            }
        }
        if (packlen == -1)
        {
            printf("packlen=-1 reason=proto_invalid\n");
            conn->close();
        }
    }
    virtual void on_error(ez_poll *poll, ez_conn *conn)
    {
        printf("on_error\n");
        conn->close();
    }
    virtual void on_close(ez_poll *poll, ez_conn *conn); 
    ez_conn *get_conn()
    {
        return conn_;
    }
private:
    uint64_t client_id_;
    my_server *mysrv_;
    ez_conn *conn_;
    ez_proto proto_;
};

class my_server: public ez_listen_handler, public ez_timer
{
public:
    my_server(): cur_id_(0), srv_(&poll_) 
    {
        srv_.set_handler(this);
    }
    virtual void on_timer(ez_poll *poll)
    {
        printf("on_timer\n");
        poll->add_timer(this, 1000);
        if (stop_server)
        {
            poll->stop();
        }
    }
    int start(const char *ip, int port)
    {
        assert(poll_.init() >= 0);
        poll_.add_timer(this, 1000);
        return srv_.listen(ip, port);
    }
    int stop()
    {
        while (!clients_.empty())
        {
            std::map<uint64_t, my_client *>::iterator iter = clients_.begin();
            my_client *client = iter->second;
            client->get_conn()->close();
        }
        srv_.stop();
        poll_.shutdown();
        return 0;
    }
    virtual void on_accept(ez_poll *poll, ez_conn *conn)
    {
        // notice: you can call "conn->close();return;" to reject a connection directly here, it's safe"
        //
        ++cur_id_;
        my_client *client = new my_client(this, cur_id_, conn);
        conn->set_handler(client);
        clients_.insert(std::map<uint64_t, my_client *>::value_type(cur_id_, client));

        const char *sock_ip, *peer_ip;
        int sock_port, peer_port;
        if (conn->getsockname(&sock_ip, &sock_port) == -1)
            assert(false);
        if (conn->getpeername(&peer_ip, &peer_port) == -1)
            assert(false);
        printf("accept (%s:%d) on (%s:%d)\n", peer_ip, peer_port, sock_ip, sock_port);
    }
    int close_client(uint64_t client_id)
    {
        std::map<uint64_t, my_client *>::iterator iter = clients_.find(client_id);
        if (iter == clients_.end())
            return -1;
        my_client *client = iter->second;
        delete client;
        clients_.erase(iter);
        return 0;
    }
    ez_poll *get_poll()
    {
        return &poll_;
    }

private:
    uint64_t cur_id_;
    std::map<uint64_t, my_client *> clients_;
    ez_poll poll_;
    ez_server srv_;
};
    
void my_client::on_close(ez_poll *poll, ez_conn *conn)
{
    printf("on_close\n");
    mysrv_->close_client(client_id_);
}

static void stop_handler(int signo)
{
    stop_server = 1;
}
static void setup_handler()
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = stop_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

int main()
{
    setup_handler();

    my_server srv;
    
    if (srv.start("0.0.0.0", 8761) == -1)
    {
        printf("start=-1\n");
        return 1;
    }

    ez_poll *poll = srv.get_poll();
    poll->run();
    printf("stopping\n");
    srv.stop();
    return 0;
}
