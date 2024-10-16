#pragma once

#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <iostream>

namespace yael::network {

/**
 * @brief Structure representing a network address
 * @note This is nothing more than a pair of port and IP
 */
struct Address
{
    constexpr static uint16_t InvalidPort = 0;

    explicit Address(const std::string ip = "", uint16_t portNumber = InvalidPort, bool IPv6_ = false)
        : IP(ip), PortNumber(portNumber), IPv6(IPv6_)
    {
    }

    Address(const Address &other) = default;

    Address(Address &&other) noexcept
        : IP(std::move(other.IP)), PortNumber(other.PortNumber), IPv6(other.IPv6)
    {}

    explicit Address(const sockaddr_in6& addr);
    explicit Address(const sockaddr_in& addr);

    [[nodiscard]]
    bool valid() const
    {
        return IP.size() > 0 && PortNumber != 0;
    }

    //! The string holding the id 4 or 16 chars
    std::string IP;

    //! The portNumber
    uint16_t PortNumber;

    //! Determines wheter the address is IPv6 or not
    bool IPv6;

    /**
     * @brief Is this network address equal to other?
     */
    [[nodiscard]]
    bool equals(const Address& other) const
    {
        if (other.PortNumber != PortNumber) {
            return false;
        }

        if (other.IPv6 != IPv6) {
            return false;
        }

        if (other.IP != IP) {
           return false;
        }

        return true;
    }

    /**
     * @brief Same as equals()
     */
    bool operator== (const Address& other) const
    {
        return this->equals(other);
    }

    bool operator!= (const Address& other) const
    {
        return !this->equals(other);
    }

    Address& operator=(const Address& other) = default;

    void reset()
    {
        IP = "";
        PortNumber = InvalidPort;
    }

    bool get_sock_address6(sockaddr_in6& addr) const;
    bool get_sock_address(sockaddr_in& addr) const;
};

/**
 * @brief Instantiate a network Address by URL instead of IP
 */
Address resolve_URL(const std::string &url, uint16_t port_number, bool IPv6 = false);

inline std::ostream& operator<<(std::ostream &os, const Address &addr)
{
    return os << addr.IP << ':' << addr.PortNumber;
}

}
