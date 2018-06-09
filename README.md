[![Build Status](https://travis-ci.org/kaimast/yael.svg?branch=master)](https://travis-ci.org/kaimast/yael)

# Yet Another Event Loop
An object-oriented event loop implementation built on top of epoll.

Core Features:
* Written in modern C++
* Thread-safe but mostly lock free
* Supports both time and network events
* Networking abstaction for TCP and TLS

## Building
This project depends on the google testing and logging frameworks.

For building you further need meson-build, libbotan, ninja, and a recent (>= C++17) compiler.

## Usage
To make your socket handler compatible with the event loop just use the NetworkSocketListener interface

```
class MyServer : protected NetworkSocketListener
{
public:
    bool init()
    {
        auto socket = new network::Socket();
        bool res = socket->listen("localhost", 4242, 100);

        if(!res)
        {
            std::cerr << "Failed to bind port" << std::endl;
            return false;
        }

        NetworkSocketListener::set_socket(socket);
        return true;
    }

protected:
    void on_new_connection(std::unique_ptr<yael::network::Socket> &&socket) override
    {
        auto &el = EventLoop::get_instance();
        el.make_socket_listener<MyClientHandler>(std::move(socket));
    }
};
```

The event loop follows the singleton pattern.
```
EventLoop::initialize();
auto &loop = EventLoop::get_instance();

// Start worker threads
// This will automatically trigger update() on SocketListeners 
loop.run();

// Tell worker threads to terminate
loop.stop();

// Wait for the system to shut down
loop.wait();

// Destroy the event loop
EventLoop::destroy();
```

For more examples look at the tests and benchmarks.
