// how to run & bug appearance:
//     ./system-test-peerdb-bug upstream 10086
//     ./system-test-peerdb-bug downstream localhost 10086 1
// currently, the "downstream" will hang.
// it seems that the response to "downstream_req_4_read_from_upstream_disk"
// has never been received by the "downstream".
// but if you
//   * uncomment `downstream->receive_response(op_id);` in `fake_ledger_put`, or
//   * comment out the sleep in `Peer::handle_op_forwarded_request`
// the bug will disappear. i believe doing this will only cover up the bug.
// both shouldn't affect the correctness.
// i'm suspecting that the event loop indeed misses some messages.
//
// to give yael some pressure, run:
//     ./system-test-peerdb-bug downstream localhost 10086 200

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
#include "yael/NetworkSocketListener.h"
#include "yael/EventLoop.h"

using namespace yael;
using namespace std::chrono_literals;
typedef uint32_t remote_party_id;

class PeerHandler : public yael::NetworkSocketListener
{
public:
    PeerHandler(const std::string &host, uint16_t port);
    PeerHandler(std::unique_ptr<network::Socket> &&s);
    ~PeerHandler();
    remote_party_id identifier() const;
    void send(const std::string &msg);
    void wait();
    void notify_all();

protected:
    void on_network_message(network::Socket::message_in_t &msg) override;

private:
    static std::atomic<remote_party_id> m_next_remote_party_id;
    remote_party_id m_identifier;
    std::condition_variable_any m_condition_var;
};

remote_party_id g_upstream_id;
std::atomic<remote_party_id> PeerHandler::m_next_remote_party_id;
std::unordered_map<remote_party_id, std::shared_ptr<PeerHandler>> g_peer_handlers;
std::mutex g_peer_handlers_mutex;

class PeerAcceptor : public yael::NetworkSocketListener
{
public:
    void listen(uint16_t port);
    void connect(const std::string &host, uint16_t port);

protected:
    void on_new_connection(std::unique_ptr<network::Socket> &&s) override;
};

std::shared_ptr<PeerAcceptor> g_peer_acceptor = nullptr;

class Peer
{
public:
    Peer(remote_party_id local_identifier);
    ~Peer();
    void send(const std::string &msg);
    std::string receive_response(const std::string &op_id);
    void handle_message(const std::string msg);
    void notify_all();
    void wait();
    remote_party_id get_local_identifier() const;
    void lock();
    void unlock();
private:
    Peer(const Peer &other) = delete;
    void handle_op_request(const std::string &op_id, const std::string &content);
    void handle_op_forwarded_request(const std::string &op_id, const std::string &content);
    void handle_op_response(const std::string &op_id, const std::string &content);
    remote_party_id m_local_identifier;
    std::unordered_map<std::string, std::string> m_responses;
};

std::unordered_map<remote_party_id, Peer*> g_peers;
std::mutex g_peers_mutex;

std::shared_ptr<PeerHandler> find_peer_handler(remote_party_id id)
{
    std::lock_guard<std::mutex> lock(g_peer_handlers_mutex);
    auto it = g_peer_handlers.find(id);
    if (it == g_peer_handlers.end())
    {
        LOG(ERROR) << "g_peer_handlers[" << id << "] doesn't exists.";
        abort();
    }
    return it->second;
}

Peer* find_peer(remote_party_id id)
{
    std::lock_guard<std::mutex> lock(g_peers_mutex);
    auto it = g_peers.find(id);
    if (it == g_peers.end())
    {
        LOG(ERROR) << "g_peers[" << id << "] doesn't exists.";
        abort();
    }
    return it->second;
}

bool is_downstream()
{
    return g_upstream_id != 0;
}

PeerHandler::PeerHandler(const std::string &host, uint16_t port)
   : NetworkSocketListener(), m_identifier(++m_next_remote_party_id)
{
    LOG(INFO) << "Connecting to host " << host << " port " << port;
    new Peer(m_identifier);
    auto sock = new yael::network::TcpSocket();
    auto addr = yael::network::resolve_URL(host, port);
    const bool success = sock->connect(addr);
    if(!success)
    {
        throw std::runtime_error("Failed to connect to other server");
    }
    
    NetworkSocketListener::set_socket(std::unique_ptr<network::Socket>{sock}, SocketType::Connection);
}

PeerHandler::PeerHandler(std::unique_ptr<network::Socket> &&s)
    : NetworkSocketListener(std::move(s), SocketType::Connection),
      m_identifier(++m_next_remote_party_id)
{
    new Peer(m_identifier);
    send("response | connect | ok");
    LOG(INFO) << "New Peer " << m_identifier << " connected";
}

PeerHandler::~PeerHandler()
{
    LOG(INFO) << "Peer " << m_identifier << " disconnected";
}

remote_party_id PeerHandler::identifier() const
{
    return m_identifier;
}

void PeerHandler::send(const std::string &msg)
{
    LOG(INFO) << "Sending to Peer #" << m_identifier << ": " << msg;
    const uint8_t *data = reinterpret_cast<const uint8_t*>(msg.c_str());
    const uint32_t length = msg.size();
    bool result = NetworkSocketListener::send(data, length);
    if (!result)
    {
        throw std::runtime_error("Failed to send message to peer " + std::to_string(identifier()));
    }
}

std::atomic<int> g_num_waiting;

void PeerHandler::wait()
{
    ++g_num_waiting;
    m_condition_var.wait(mutex());
    --g_num_waiting;
}

void PeerHandler::notify_all()
{
    m_condition_var.notify_all();
}

void PeerHandler::on_network_message(network::Socket::message_in_t &pair)
{
    std::string msg(reinterpret_cast<const char*>(pair.data), pair.length);
    auto peer = find_peer(m_identifier);
    peer->handle_message(msg);
}

void PeerAcceptor::listen(uint16_t port)
{
    const std::string host = "0.0.0.0";
    auto socket = new yael::network::TcpSocket();
    bool res = socket->listen(host, port, 100);
    
    if(!res)
    {
        throw std::runtime_error("socket->listen failed");
    }

    yael::NetworkSocketListener::set_socket(std::unique_ptr<network::Socket>{socket}, yael::SocketType::Acceptor);
    LOG(INFO) << "Listening for peers on host " << host << " port " << port;
}

void PeerAcceptor::connect(const std::string &host, uint16_t port)
{
    auto &el = yael::EventLoop::get_instance();
    auto peer_handler = el.make_socket_listener<PeerHandler>(host, port);
    g_upstream_id = peer_handler->identifier();
    g_peer_handlers_mutex.lock();
    g_peer_handlers[peer_handler->identifier()] = peer_handler;
    g_peer_handlers_mutex.unlock();
    
    auto upstream = find_peer(g_upstream_id);
    upstream->lock();
    upstream->receive_response("connect");
    upstream->unlock();
    LOG(INFO) << "Successfully connected to upstream host " << host << " port " << port;
}

void PeerAcceptor::on_new_connection(std::unique_ptr<network::Socket>&& s)
{
    auto &el = yael::EventLoop::get_instance();
    std::lock_guard<std::mutex> lock(g_peer_handlers_mutex);
    auto peer_handler = el.make_socket_listener<PeerHandler>(std::move(s));
    g_peer_handlers[peer_handler->identifier()] = peer_handler;
}

Peer::Peer(remote_party_id local_identifier)
    : m_local_identifier(local_identifier)
{
    std::lock_guard<std::mutex> lock(g_peers_mutex);
    g_peers[m_local_identifier] = this;
}

Peer::~Peer()
{
    std::lock_guard<std::mutex> lock(g_peers_mutex);
    g_peers.erase(m_local_identifier);
}

void Peer::send(const std::string &msg)
{
    auto peer_handler = find_peer_handler(m_local_identifier);
    peer_handler->send(msg);
}

std::string Peer::receive_response(const std::string &op_id)
{
    auto it = m_responses.find(op_id);

    while (it == m_responses.end())
    {
        this->wait();
        it = m_responses.find(op_id);
    }

    std::string value = it->second;
    m_responses.erase(it);

    return value;
}

void Peer::handle_message(const std::string msg)
{
    const std::string::size_type pos1 = msg.find(" | ");
    const std::string::size_type pos2 = msg.find(" | ", pos1+3);
    const std::string type = msg.substr(0, pos1);
    const std::string op_id = msg.substr(pos1+3, pos2 - pos1 - 3);
    const std::string content = msg.substr(pos2+3);
    LOG(INFO) << "got message: type=" << type << " op_id=" << op_id << " content=" << content;
    if (type == "request") handle_op_request(op_id, content);
    else if (type == "response") handle_op_response(op_id, content);
    else if (type == "forwarded") handle_op_forwarded_request(op_id, content);
    else
    {
        LOG(ERROR) << "unknown message type: " << type;
        abort();
    }
}

void fake_ledger_put(remote_party_id downstream_id)
{
    // pretending that we've done with the ledger things
    // let's push the index update to downstreams!

    Peer *downstream = find_peer(downstream_id);
    downstream->lock();
    const std::string op_id = "upstream_req_1_put_object_index";
    downstream->send("request | " + op_id + " | PutObjectIndex");

    // this corresponds to the "p->wait();" in Ledger::put_next_version of peerdb
    // there is a "FIXME" that comment this "p->wait();" out.
    // commenting out this statement or not shouldn't affect the correctness.
    // but in current version, uncomment this makes the bug disappear.
    // otherwise, it'll hang.
    // downstream->receive_response(op_id);

    downstream->unlock();
}

void fake_enclave_read_from_upstream_disk()
{
    Peer *upstream = find_peer(g_upstream_id);
    const std::string op_id = "downstream_req_4_read_from_upstream_disk";
    upstream->lock();
    upstream->send("request | " + op_id + " | ReadFromUpstreamDisk");
    upstream->receive_response(op_id);
    upstream->unlock();
}

void fake_ledger_put_object_index_from_upstream()
{
    // pretending we are changing downstream object index now,
    // there'll be a ReadFromUpstreamDisk to fetch the children node of the index
    fake_enclave_read_from_upstream_disk();
}

void Peer::handle_op_request(const std::string &op_id, const std::string &content)
{
    unlock();
    std::string output;
    if (content == "ReadFromUpstreamDisk")
    {
        std::this_thread::sleep_for(100ms);
        output = "<disk content blahblah>";
    }
    else if (content == "PutObjectIndex")
    {
        fake_ledger_put_object_index_from_upstream();
        output = "ok";
    }
    else 
    {
        LOG(ERROR) << "unknown op_request type";
        abort();
    }
    lock();
    send("response | " + op_id + " | " + output);
}

void Peer::handle_op_response(const std::string &op_id, const std::string &content)
{
    m_responses[op_id] = content;
    notify_all();
}

void Peer::handle_op_forwarded_request(const std::string &op_id, const std::string &content)
{
    unlock();
    std::string output;
    if (content == "PutObject")
    {
        fake_ledger_put(m_local_identifier);

        // wait and let the PutObjectIndex send first.
        // if don't wait, the client will receive two messages in a single update().
        // commenting out this statement or not should also not affect the correctness.
        // but it works if this is commented out. otherwise, it hangs.
        std::this_thread::sleep_for(10ms);

        output = "ok";
    }
    else 
    {
        LOG(ERROR) << "unknown op_forwarded_request type";
        abort();
    }
    lock();
    send("response | " + op_id + " | " + output);
}

void Peer::notify_all()
{
    auto peer_handler = find_peer_handler(m_local_identifier);
    peer_handler->notify_all();
}

void Peer::wait()
{
    auto peer_handler = find_peer_handler(m_local_identifier);
    peer_handler->wait();
}

remote_party_id Peer::get_local_identifier() const
{
    return m_local_identifier;
}

void Peer::lock()
{
    auto peer_handler = find_peer_handler(m_local_identifier);
    peer_handler->lock();
}

void Peer::unlock()
{
    auto peer_handler = find_peer_handler(m_local_identifier);
    peer_handler->unlock();
}

void stop_handler(int)
{
    LOG(INFO) << "Received signal. Stopping...";
    yael::EventLoop::get_instance().stop();
}

void print_help()
{
    std::cout << "usage: " << std::endl
              << "   ./system-test-peerdb-bug   upstream <listen-port>" << std::endl
              << "   ./system-test-peerdb-bug downstream <upstream-host> <upstream-port> <num-processes>" << std::endl
              << std::endl;
    exit(1);
}

void do_child(const std::string &host, uint16_t port)
{
    FLAGS_logbufsecs = 0; 
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_logtostderr = 1;
    google::InitGoogleLogging("do_upstream");

    yael::EventLoop::initialize();
    auto &event_loop = yael::EventLoop::get_instance();

    g_peer_acceptor = std::shared_ptr<PeerAcceptor>{new PeerAcceptor()};//el.make_socket_listener<PeerAcceptor>();
    std::this_thread::sleep_for(2000ms);
    g_peer_acceptor->connect(host, port);
    
    signal(SIGSTOP, stop_handler);
    signal(SIGTERM, stop_handler);

    auto upstream = find_peer(g_upstream_id);
    upstream->lock();
    const std::string op_id = "downstream_req_3_forward_put_object";
    upstream->send("forwarded | " + op_id + " | PutObject");
    upstream->receive_response(op_id);
    upstream->unlock();

    bool ok = false;
    std::this_thread::sleep_for(100ms);
    for (int i = 0; i < 20; ++i)
    {
        int waiting = g_num_waiting;
        LOG(INFO) << "trying for the " << i << "th times: " << waiting << " threads waiting...";
        if (waiting)
        {
            std::this_thread::sleep_for(500ms);
        }
        else
        {
            ok = true;
            break;
        }
    }
    if (ok)
    {
        LOG(INFO) << "success!";
        event_loop.stop();
    }
    else
    {
        LOG(ERROR) << "failed!";
    }

    event_loop.wait();
    event_loop.destroy();
    exit(ok ? 0 : 1);
}

void do_downstream(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    const std::string &host = argv[2];
    const uint16_t port = std::atoi(argv[3]);
    const int num = std::atoi(argv[4]);
    
    for (int i = 0; i < num; ++i)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("error");
            abort();
        }
        else if (pid == 0)
        {
            do_child(host, port);
            exit(0);
        }
    }

    bool ok = true;
    for (int i = 0; i < num; ++i)
    {
        int status;
        pid_t pid = wait(&status);
        ok = ok && status == 0;
        printf("[%d/%d] Child with PID %ld exited with status 0x%x.\n", i+1, num, (long)pid, status);
    }
    if (ok)
        puts("Success!");
    else
    {
        puts("Failed!");
        exit(1);
    }
}

void do_upstream(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    FLAGS_logbufsecs = 0; 
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_logtostderr = 1;
    google::InitGoogleLogging("do_upstream");

    yael::EventLoop::initialize();
    auto &event_loop = yael::EventLoop::get_instance();

    g_peer_acceptor = std::shared_ptr<PeerAcceptor>{new PeerAcceptor()};//el.make_socket_listener<PeerAcceptor>();

    const uint16_t port = std::atoi(argv[2]);
    g_peer_acceptor->listen(port);
    event_loop.register_socket_listener(g_peer_acceptor);

    signal(SIGSTOP, stop_handler);
    signal(SIGTERM, stop_handler);
    event_loop.wait();
    event_loop.destroy();
}

int main(int argc, char** argv)
{
    if (argc < 3)
        print_help();
    if (!strcmp(argv[1], "upstream") && argc == 3)
        do_upstream(argc, argv);
    else if (!strcmp(argv[1], "downstream") && argc == 5)
        do_downstream(argc, argv);
    else
        print_help();

}
