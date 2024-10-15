#include "yael/NetworkSocketListener.h"

#include "yael/EventLoop.h"

using namespace yael;

NetworkSocketListener::NetworkSocketListener() = default;

NetworkSocketListener::NetworkSocketListener(
    std::unique_ptr<network::Socket> &&socket, SocketType type) {
    if (socket) {
        NetworkSocketListener::set_socket(
            std::forward<std::unique_ptr<network::Socket>>(socket), type);

        m_socket->wait_connection_established();
    }
}

std::unique_ptr<network::Socket> NetworkSocketListener::release_socket() {
    const std::unique_lock lock(m_mutex);

    // Move socket before we unregistered so socket doesn't get closed
    auto sock = std::move(m_socket);

    /// FIXME we should tell event listener to remap the socket
    // the current approach can cause race conditions...
    auto &el = EventLoop::get_instance();
    el.unregister_event_listener(
        std::dynamic_pointer_cast<EventListener>(shared_from_this()));

    return sock;
}

void NetworkSocketListener::re_register(bool first_time) {
    // send queue decides the mode of the listener
    const std::unique_lock lock(m_send_mutex);

    if (!m_socket->is_valid()) {
        // we're done
        return;
    }

    auto &el = EventLoop::get_instance();
    el.notify_listener_mode_change(shared_from_this(), m_mode, first_time);
}

void NetworkSocketListener::set_mode(EventListener::Mode mode) {
    if (mode == m_mode) {
        return;
    }

    m_mode = mode;

    auto &el = EventLoop::get_instance();
    el.notify_listener_mode_change(shared_from_this(), mode, false);
}

void NetworkSocketListener::set_socket(
    std::unique_ptr<network::Socket> &&socket, SocketType type) {
    const std::unique_lock lock(m_mutex);

    if (m_socket) {
        throw std::runtime_error(
            "There is already a socket assigned to this listener!");
    }

    if (!socket->is_valid()) {
        throw std::runtime_error("Not a valid socket!");
    }

    m_socket = std::move(socket);
    m_socket_type = type;
    m_fileno = m_socket->get_fileno();
}

bool NetworkSocketListener::is_valid() {
    const std::unique_lock lock(m_mutex);

    if (!m_socket) {
        return false;
    }

    return m_socket->is_valid();
}

bool NetworkSocketListener::is_connected() {
    const std::unique_lock lock(m_mutex);

    if (!m_socket) {
        return false;
    }

    return m_socket->is_connected();
}

void NetworkSocketListener::on_write_ready() {
    std::unique_lock lock(m_send_mutex);

    bool has_more;

    try {
        has_more = m_socket->do_send();
    } catch (const network::socket_error &e) {
        LOG(WARNING) << "Failed to send data to "
                     << m_socket->get_remote_address() << ": " << e.what();

        has_more = false;

        lock.unlock();
        close_socket();
    }

    if (!has_more && is_valid()) {
        set_mode(EventListener::Mode::ReadOnly);
    }
}

void NetworkSocketListener::send(std::shared_ptr<uint8_t[]> &&data,
                                 size_t length, bool blocking, bool async) {
    std::unique_lock send_lock(m_send_mutex);

    bool has_more;

    while (true) {
        try {
            has_more = m_socket->send(data, length, async);
            break;
        } catch (const network::socket_error &e) {
            DLOG(WARNING) << "Failed to send data to "
                          << m_socket->get_remote_address() << ": " << e.what();

            has_more = false;
            close_socket();
            break;
        } catch (const network::send_queue_full &) {
            if (blocking) {
                DLOG(WARNING)
                    << "Send queue to " << m_socket->get_remote_address()
                    << " is full. Thread is blocking...";

                send_lock.unlock();
                m_socket->wait_send_queue_empty();
                send_lock.lock();
            } else {
                LOG(ERROR) << "Failed to send data to "
                           << m_socket->get_remote_address()
                           << ": send queue is full";
                has_more = false;
                close_socket();
                break;
            }
        }
    }

    if (is_valid()) {
        if (has_more) {
            set_mode(Mode::ReadWrite);
        } else {
            set_mode(Mode::ReadOnly);
        }
    } else {
        close_socket();
    }
}

void NetworkSocketListener::send(std::unique_ptr<uint8_t[]> &&data,
                                 size_t length, bool blocking, bool async) {
    std::unique_lock send_lock(m_send_mutex);

    bool has_more;

    while (true) {
        try {
            has_more = m_socket->send(data, length, async);
            break;
        } catch (const network::socket_error &e) {
            LOG(WARNING) << "Failed to send data to "
                         << m_socket->get_remote_address() << ": " << e.what();

            has_more = false;
            close_socket();
            break;
        } catch (const network::send_queue_full &) {
            if (blocking) {
                LOG(WARNING)
                    << "Send queue to " << m_socket->get_remote_address()
                    << " is full. Thread is blocking...";

                send_lock.unlock();
                m_socket->wait_send_queue_empty();
                send_lock.lock();
            } else {
                LOG(ERROR) << "Failed to send data to "
                           << m_socket->get_remote_address()
                           << ": send queue is full";
                has_more = false;
                close_socket();
                break;
            }
        }
    }

    if (is_valid()) {
        if (has_more) {
            set_mode(Mode::ReadWrite);
        } else {
            set_mode(Mode::ReadOnly);
        }
    } else {
        close_socket();
    }
}

void NetworkSocketListener::send(const uint8_t *data, size_t length,
                                 bool blocking, bool async) {
    std::unique_lock send_lock(m_send_mutex);

    bool has_more;

    while (true) {
        try {
            has_more = m_socket->send(data, length, async);
            break;
        } catch (const network::socket_error &e) {
            LOG(ERROR) << "Failed to send data to "
                       << m_socket->get_remote_address() << ": " << e.what();
            has_more = false;
            close_socket();
            break;
        } catch (const network::send_queue_full &) {
            if (blocking) {
                LOG(WARNING)
                    << "Send queue to " << m_socket->get_remote_address()
                    << " is full. Thread is blocking...";

                send_lock.unlock();
                m_socket->wait_send_queue_empty();
                send_lock.lock();
            } else {
                LOG(ERROR) << "Failed to send data to "
                           << m_socket->get_remote_address()
                           << ": send queue is full";
                has_more = false;
                close_socket();
                break;
            }
        }
    }

    if (is_valid()) {
        if (has_more) {
            set_mode(Mode::ReadWrite);
        } else {
            set_mode(Mode::ReadOnly);
        }
    } else {
        close_socket();
    }
}

void NetworkSocketListener::wait_for_connection() {
    while (!is_connected()) {
        if (!m_socket) {
            // busy wait while socket doesn't exist
            continue;
        }

        if (m_socket->is_listening()) {
            throw std::runtime_error(
                "Cannot wait for connection. Is listening.");
        }

        m_socket->wait_connection_established();
    }
}

void NetworkSocketListener::on_error() {
    LOG(WARNING) << "Got error; closing socket";
    close_socket();
}

void NetworkSocketListener::on_read_ready() {
    std::unique_lock lock(m_mutex);

    switch (m_socket_type) {
    case SocketType::Acceptor: {
        auto result = m_socket->accept();
        lock.unlock();

        for (auto &s : result) {
            this->on_new_connection(std::move(s));
        }

        break;
    }
    case SocketType::Connection: {
        try {
            while (m_socket) {
                auto message = m_socket->receive();

                if (message) {
                    lock.unlock();
                    this->on_network_message(*message);
                    lock.lock();
                } else {
                    // no more data
                    break;
                }
            }
        } catch (const network::socket_error &e) {
            LOG(WARNING) << e.what();
        }

        // After processing the last message we will notify the user that
        // the socket is closed
        if (m_socket && !m_socket->is_valid()) {
            close_socket_internal(lock);
        }

        break;
    }
    default:
        throw std::runtime_error("Unknown socket type!");
    }
}

void NetworkSocketListener::close_socket_internal(
    std::unique_lock<std::mutex> &lock) {
    bool done = true;

    if (m_socket && m_socket->is_valid()) {
        done = m_socket->close();
        lock.unlock();
    }

    if (done && !m_has_disconnected) {
        // make sure we invoke the callback at most once
        m_has_disconnected = true;

        if (m_socket_type == SocketType::Connection) {
            this->on_disconnect();
        }

        if (EventLoop::is_initialized()) {
            auto &el = EventLoop::get_instance();
            el.unregister_event_listener(shared_from_this());
        }
    }
}

int32_t NetworkSocketListener::get_fileno() const { return m_fileno; }
