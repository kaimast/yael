[![ci-badge](https://github.com/kaimast/yael/actions/workflows/ci.yml/badge.svg)](https://github.com/kaimast/yael/actions)

# Yet Another Event Loop
An object-oriented event loop implementation built on top of epoll.

Core Features:
* Written in modern C++ and avoids raw pointers whenever possible
* The event loop will spawn worker threads for you, no need to start your own
* Thread-safe and highly concurrent
* Networking abstraction for TCP and TLS
* Supprot for timer events

## Building
This project depends on the google testing (gtest)  and logging frameworks (glog), as well as libbotan for encryption (TLS).

To compile the project you further need meson-build, ninja, and a recent (>= C++17) compiler.

After running `meson build`, you can use `ninja` for development.
`ninja tests` runs all unit tests and `ninja lint` exeuctes lint checks if clang-tidy is available.

## Usage
To make your socket handler compatible with the event loop just use the NetworkSocketListener interface

```cpp
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
```cpp
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

For more examples, please take a look at the tests and benchmarks.
