#include "ez_proto.h"
#include "ez_server.h"
#include "ez_poll.h"
#include <string.h>
#include <stdio.h>

static sig_atomic_t stop = 0;

class worker_thread;

class my_client: public ez_handler
{
public:
    my_client(uint64_t cli_id, worker_thread *thread, ez_conn *conn): cli_id_(cli_id), thread_(thread), conn_(conn)
    {

    }
    virtual void on_message(ez_poll *poll, ez_conn *conn)
    {
        int packlen;
        while ((packlen = proto_.unpack_message(conn)) > 0)
        {
            const std::string &req = proto_.get_body();
            printf("id=%"PRIu64" command=%"PRIu64" bodylen=%"PRIu64" body=%.*s\n", proto_.get_id(), proto_.get_command(), proto_.get_bodylen(), (int)req.size(), req.c_str());
            
            const std::string &res = proto_.pack_message(
                    proto_.get_id(), proto_.get_command(), "sample_multi_server", "pong\n", 5);
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
    virtual void on_close(ez_poll *poll, ez_conn *conn);

    ez_conn *get_conn() { return conn_; } 

private:
    uint64_t cli_id_;
    worker_thread *thread_;
    ez_conn *conn_;
    ez_proto proto_;
};

class worker_thread: public ez_listen_handler, public ez_timer
{
public:
    virtual void on_timer(ez_poll *poll)
    {
        fprintf(stderr, "This is thread - %lu\n", pthread_self());
        timerid_ = poll->add_timer(this, 1000);
    }
    worker_thread(ez_thread *thread): cli_id_(0), thread_(thread)
    {
        timerid_ = thread_->get_poll()->add_timer(this, 1000);
    }
    ~worker_thread()
    {
        while (!clients_.empty())
        {
            my_client *cli = clients_.begin()->second;
            cli->get_conn()->close();
        }
        thread_->get_poll()->del_timer(timerid_);
    }
    void close_client(uint64_t cli_id)
    {
        client_map::iterator iter = clients_.find(cli_id);
        my_client *cli = iter->second;
        clients_.erase(iter);
        delete cli;
    }
    virtual void on_accept(ez_poll *poll, ez_conn *conn)
    {
        my_client *cli = new my_client(cli_id_, this, conn);
        clients_.insert(client_map::value_type(cli_id_, cli));
        conn->set_handler(cli);
        ++cli_id_;
    }
    ez_thread *get_thread()
    {
        return thread_;
    }
private:
    typedef std::map<uint64_t, my_client *> client_map;
    client_map clients_;
    uint64_t cli_id_;
    ez_thread *thread_;
    ez_timer_id timerid_;
};
    
void my_client::on_close(ez_poll *poll, ez_conn *conn)
{
    thread_->close_client(cli_id_);
}

class multi_server: public ez_timer
{
public:
    multi_server(): srv_(&poll_)
    {
    }

    virtual void on_timer(ez_poll *poll)
    {
        poll->add_timer(this, 1000);
        if (::stop)
        {
            poll->stop();
        }
    }

    int start(int num_thread)
    {
        assert(poll_.init() >= 0); // init main thread's ez_poll
        poll_.add_timer(this, 1000);
        if (srv_.listen("0.0.0.0", 8761) == -1)
        {
            poll_.shutdown();
            return -1;
        }
        assert(srv_.init_threads(num_thread) == 0);
        std::vector<ez_thread *> threads = srv_.get_thread_pool()->get_threads();
        std::vector<ez_listen_handler *> handlers;
        for (int i = 0; i < num_thread; ++i)
        {
            worker_thread *thread = new worker_thread(threads[i]);
            threads_.push_back(thread);
            handlers.push_back(thread);
        }
        assert(srv_.start_threads(handlers) == 0);
        return 0;
    }

    int stop()
    {
        assert(srv_.join_threads() == 0);
        for (size_t i = 0; i < threads_.size(); ++i)
        {
            delete threads_[i];
        }
        threads_.clear();
        assert(srv_.free_threads() == 0);
        assert(srv_.stop() == 0);
        assert(poll_.shutdown() == 0);
        return 0;
    }

    ez_poll * get_poll() { return &poll_; }
private:
    std::vector<worker_thread *> threads_;
    ez_poll poll_;
    ez_server srv_;
};

static void stop_handler(int signo)
{
    stop = 1;
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

    multi_server srv;
    
    assert(srv.start(8) == 0);
    
    ez_poll *poll = srv.get_poll();

    poll->run();

    assert(srv.stop() == 0);
    return 0;
}
