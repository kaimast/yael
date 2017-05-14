# Yet Another Event Loop

A very simple event loop for C++14 built on top of epoll.

## Building
This requires the google, testing, mocking, and logging frameworks.
For building you further need CMake, ninja, and a recent (=C++14) compiler.

## Usage
To make your socket handler compatible with the event loop just use the SocketListener interface
```
class MyServer : protected NetworkSocketListener
{
public:
    bool init()
    {
        auto socket = new network::Socket();
        bool res = socket->listen("localhost", BENCHMARK_PORT, 100);

        if(!res)
        {
            std::cerr << "Failed to bind port" << std::endl;
            return false;
        }

        socket->set_blocking(false);

        NetworkSocketListener::set_socket(socket);
        return true;
    }

protected:
    void update() override
    {
       // Call accept() on the socket here...
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
