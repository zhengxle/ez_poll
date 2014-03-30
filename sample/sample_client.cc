#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "ez_proto.h"
#include "ez_poll.h"
#include "ez_client.h"
#include <vector>

class my_manager;

class my_client: public ez_handler
{
public:
    my_client(my_manager *mgr);
    ez_client *get_client()
    {
        return &client_;
    }
    virtual void on_message(ez_poll *poll, ez_conn *conn)
    {
        int packlen;
        while ((packlen = proto_.unpack_message(conn)) > 0)
        {
            const std::string &req = proto_.get_body();
            printf("id=%"PRIu64" command=%"PRIu64" bodylen=%"PRIu64" body=%.*s\n", proto_.get_id(), proto_.get_command(), proto_.get_bodylen(), (int)req.size(), req.c_str());
            
            // command=0 means this is a ping request
            const std::string &res = proto_.pack_message(get_reqid(), 0, "sample_client", "ping\n", 5);
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
        conn->close();
    }
    virtual void on_close(ez_poll *poll, ez_conn * conn)
    {
        // clear request queue or something ? 
    }
    uint64_t get_reqid()
    {
        return reqid_++;
    }
private:
    uint64_t reqid_;
    my_manager *mgr_;
    ez_client client_;
    ez_proto proto_;
};

class my_manager: public ez_timer
{
public:
    my_manager(int num_client)
    {
        for (int i = 0; i < num_client; ++i)
        {
            my_client *client = new my_client(this);
            clients_.push_back(client);
        }
    }
    ~my_manager()
    {
    }
    ez_poll *get_poll()
    {
        return &poll_;
    }
    int start()
    {
        assert(poll_.init() >= 0);
        timer_id_ = poll_.add_timer(this, 1000);
        return 0;
    }
    int stop()
    {
        for (size_t i = 0; i < clients_.size();++i)
        {
            ez_client *ecli = clients_[i]->get_client();
            ez_conn *conn = ecli->get_conn();
            if (conn->valid()) 
                conn->close();
            delete clients_[i];
        }
        clients_.clear();
        poll_.del_timer(timer_id_);
        poll_.shutdown();
        return 0;
    }
    virtual void on_timer(ez_poll *poll)
    {
        for (size_t i = 0; i < clients_.size(); ++i)
        {
            ez_client *ecli = clients_[i]->get_client();
            ez_conn *conn = ecli->get_conn();
            if (!conn->valid())
            {
                conn->set_handler(clients_[i]); // reset handler(because conn->close will reset it)
                if (conn->connect("127.0.0.1", 8761) == 0)
                {
                    const std::string &ping = proto_.pack_message(clients_[i]->get_reqid(), 0, "sample_client", "ping\n", 5);
                    if (conn->send_message(ping.c_str(), ping.size()) == -1)
                        conn->close();
                }
            }
        }
        timer_id_ = poll->add_timer(this, 1000);
    }
private:
    ez_timer_id timer_id_;
    std::vector<my_client *> clients_;
    ez_poll poll_;
    ez_proto proto_;
};
    
my_client::my_client(my_manager *mgr): reqid_(0), mgr_(mgr), client_(mgr->get_poll())
{
    client_.get_conn()->set_handler(this);
}

static sig_atomic_t stop_mgr = 0;
static void stop_handler(int signo)
{
    stop_mgr = 1;
}
static void setup_handler()
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = stop_handler;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}
int main(int argc, char *argv[])
{
    setup_handler();
    my_manager mgr(10);
    
    if (mgr.start() == -1)
    {
        printf("mgr.start=-1\n");
        return 1;
    }

    ez_poll *poll = mgr.get_poll();
    while (!stop_mgr)
    {
        poll->poll(1000);
    }
    mgr.stop();
    printf("stoping\n");
    return 0;
}
