#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <unordered_map>
#include <glog/logging.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "yael/network/TcpSocket.h"
#include "yael/DelayedNetworkSocketListener.h"
#include "yael/EventLoop.h"

using namespace yael;
using namespace std::chrono_literals;

class Acceptor : public yael::NetworkSocketListener
{
public:
    Acceptor(uint16_t port);

protected:
    void on_new_connection(std::unique_ptr<yael::network::Socket> &&socket) override;
};

class Peer : public yael::DelayedNetworkSocketListener
{
public:
    Peer(const std::string &host, uint16_t port, uint32_t delay);
    Peer(std::unique_ptr<yael::network::Socket> &&s, uint32_t delay = 0);

protected:
    void on_network_message(yael::network::Socket::message_in_t &msg) override;
};

std::string to_string(yael::network::Socket::message_in_t &msg)
{
    return std::string(reinterpret_cast<const char*>(msg.data), msg.length);
}

void Acceptor::on_new_connection(std::unique_ptr<yael::network::Socket> &&socket)
{
    auto &el = EventLoop::get_instance();
    el.make_event_listener<Peer>(std::move(socket));
}

Acceptor::Acceptor(uint16_t port)
{
    const std::string host = "0.0.0.0";
    auto socket = new network::TcpSocket();

    bool res = socket->listen(host, port, 100);
    if(!res)
    {
        throw std::runtime_error("socket->listen failed");
    }

    yael::NetworkSocketListener::set_socket(std::unique_ptr<network::Socket>(socket), yael::SocketType::Acceptor);
    LOG(INFO) << "Listening for peers on host " << host << " port " << port;
}

Peer::Peer(const std::string &host, uint16_t port, uint32_t delay)
    : yael::DelayedNetworkSocketListener(delay, nullptr, yael::SocketType::Connection)
{
    auto sock = new yael::network::TcpSocket();
    auto addr = yael::network::resolve_URL(host, port);
    bool success = sock->connect(addr);
    if (!success)
    {
        throw std::runtime_error("Failed to connect to server");
    }

    set_socket(std::unique_ptr<network::Socket>{sock}, SocketType::Connection);
    LOG(INFO) << "connected to " << host << ":" << port;
}

Peer::Peer(std::unique_ptr<yael::network::Socket> &&s, uint32_t delay)
    : yael::DelayedNetworkSocketListener(delay, std::forward<std::unique_ptr<yael::network::Socket>>(s), yael::SocketType::Connection)
{
    LOG(INFO) << "new peer connected";
}

void Peer::on_network_message(yael::network::Socket::message_in_t &msg)
{
    (void)msg;
}

void stop_handler(int)
{
    LOG(INFO) << "Received signal. Stopping...";
    yael::EventLoop::get_instance().stop();
}

int do_child(const std::string &host, uint16_t port, uint32_t delay)
{
    FLAGS_logbufsecs = 0; 
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_logtostderr = 1;
    google::InitGoogleLogging("do_child");

    yael::EventLoop::initialize();
    auto &event_loop = yael::EventLoop::get_instance();
    signal(SIGSTOP, stop_handler);
    signal(SIGTERM, stop_handler);

    auto &el = EventLoop::get_instance();

    for(size_t i = 0; i < 1000; ++i)
    {
        auto c = event_loop.make_event_listener<Peer>(host, port, delay);
        c->wait_for_connection();
        c->close_socket();
    }

    el.stop();

    event_loop.wait();
    event_loop.destroy();

    return 0;
}

void do_connect(int argc, char** argv)
{
    (void)argc;

    const std::string &host = argv[2];
    const uint16_t port = std::atoi(argv[3]);
    const int num_children = std::atoi(argv[4]);
    const uint32_t delay = std::atoi(argv[5]);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_children; ++i)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("error");
            abort();
        }
        else if (pid == 0)
        {
            auto res = do_child(host, port, delay);
            exit(res);
        }
    }

    bool ok = true;
    for (int i = 0; i < num_children; ++i)
    {
        int status;
        pid_t pid = wait(&status);
        ok = ok && status == 0;

        std::cout << "[" << i+1 << "/" <<num_children << "] Child with PID " << (long)pid << " exited with status 0x" << status << "." << std::endl;
    }

    if (ok)
    {
        std::cerr << "All Done!" << std::endl;
    }
    else
    {
        std::cerr << "Failed!" << std::endl;
        exit(-1);
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds> 
                                    (std::chrono::steady_clock::now() - start).count();

    std::cout << "Duration was " << duration << " ms" << std::endl;

    if(duration < delay)
    {
        std::cerr << "Duration shorter than delay" << std::endl;
        exit(-1);
    }

    exit(0);
}

void do_listen(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    FLAGS_logbufsecs = 0; 
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_logtostderr = 1;
    google::InitGoogleLogging("do_listen");

    yael::EventLoop::initialize();
    auto &event_loop = yael::EventLoop::get_instance();
    signal(SIGSTOP, stop_handler);
    signal(SIGTERM, stop_handler);

    const uint16_t port = std::atoi(argv[2]);
    event_loop.make_event_listener<Acceptor>(port);

    event_loop.wait();
    event_loop.destroy();

    DLOG(INFO) << "Server terminated";
}

void print_help()
{
    std::cout << "usage: " << std::endl
              << "   ./churn-test listen  <listen-port>" << std::endl
              << "   ./churn-test connect <upstream-host> <upstream-port> <num_connection> <delay>" << std::endl
              << std::endl;
    exit(1);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        print_help();
    }
    if (!strcmp(argv[1], "listen") && argc == 3)
    {
        do_listen(argc, argv);
    }
    else if (!strcmp(argv[1], "connect") && argc == 6)
    {
        do_connect(argc, argv);
    }
    else
    {
        print_help();
    }
}
