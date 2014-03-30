#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "ez_proto.h"
#include "ez_thread.h"
#include "ez_client.h"
#include <vector>

static sig_atomic_t stop_mgr = 0;

static void stop_handler(int signo)
{
    stop_mgr = 1;
}

class client: public ez_handler
{
public:
    client(ez_poll *poll): cli_(poll), req_id_(0)
    {
    }
    ~client() 
    {
        if (cli_.get_conn()->valid())
        {
            cli_.get_conn()->close();
        }
    }
    uint64_t get_reqid() { return req_id_++;}
    virtual void on_message(ez_poll *poll, ez_conn *conn)
    {
        int packlen;
        while ((packlen = proto_.unpack_message(conn)) > 0)
        {
            const std::string &req = proto_.get_body();
            printf("id=%"PRIu64" command=%"PRIu64" bodylen=%"PRIu64" body=%.*s\n", proto_.get_id(), proto_.get_command(), proto_.get_bodylen(), (int)req.size(), req.c_str());
            
            // command=0 means this is a ping request
            const std::string &res = proto_.pack_message(get_reqid(), 0, "sample_multi_client", "ping\n", 5);
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
    virtual void on_close(ez_poll *poll, ez_conn *conn)
    {
    }
    ez_client *get_client() { return &cli_; }
private:
    ez_client cli_;
    ez_proto proto_;
    uint64_t req_id_;
};

class client_thread: public ez_timer, public ez_task
{
public:
    client_thread()
    {
    }
    ~client_thread()
    {
        for (size_t i = 0; i < clients_.size(); ++i)
        {
            delete clients_[i];
        }
        assert(poll_->del_timer(id_) == 0);
    }
    virtual void on_timer(ez_poll *poll)
    {
        printf("client_thread=%lu\n", pthread_self());
        id_ = poll->add_timer(this, 1000);
        for (size_t i = 0; i < clients_.size(); ++i)
        {
            ez_client *cli = clients_[i]->get_client();
            ez_conn *conn = cli->get_conn();
            if (!conn->valid())
            {
                conn->set_handler(clients_[i]);
                if (conn->connect("127.0.0.1", 8761) == 0)
                {
                    const std::string &ping = proto_.pack_message(clients_[i]->get_reqid(), 0, "sample_multi_client", "ping\n", 5);
                    if (conn->send_message(ping.c_str(), ping.size()))
                        conn->close();
                }
            }
        }
    }
    // init task
    virtual void on_action(ez_poll *poll)
    {
        poll_ = poll;
        for (int i = 0 ; i < 10; ++i) // 10 connection to server
        {
            clients_.push_back(new client(poll));
        }
        id_ = poll->add_timer(this, 1000);
    }
private:
    std::vector<client *> clients_;
    ez_proto proto_;
    ez_timer_id id_;
    ez_poll *poll_;
};

class client_mgr: public ez_timer
{
public:
    client_mgr()
    {
        assert(poll_.init() >= 0); // main's poll non-used 
        assert(poll_.add_timer(this, 1000) == 0);
    }
    ~client_mgr()
    {
        assert(poll_.shutdown() == 0);
    }
    int start(int num_thread)
    {
        assert(0 == pool_.init_threads(num_thread));
        std::vector<ez_thread *> threads = pool_.get_threads();
        assert(0 == pool_.start_threads());
        
        for (size_t i = 0; i < threads.size(); ++i)
        {
            client_thread *t = new client_thread();
            threads_.push_back(t);
            // init timer in that thread, just an example of "run_task"!
            threads[i]->get_poll()->run_task(t); 
        }
        return 0;
    }
    int stop()
    {
        assert(0 == pool_.join_threads());
        for (size_t i = 0; i < threads_.size(); ++i)
        {
            delete threads_[i];
        }
        threads_.clear();
        assert(0 == pool_.free_threads());
        return 0;
    }
    virtual void on_timer(ez_poll *poll)
    {
        printf("on_timer\n");
        poll->add_timer(this, 1000);
        if (stop_mgr)
        {
            poll->stop();
        }
    }
    ez_poll *get_poll() { return &poll_; }
private:
    ez_thread_pool pool_;
    ez_poll poll_;
    std::vector<client_thread *> threads_;
};

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
    
    client_mgr mgr;
    assert(mgr.start(8) == 0);

    ez_poll *poll = mgr.get_poll();
    
    poll->run();
    mgr.stop();
    printf("stoped\n");
    return 0;
}
