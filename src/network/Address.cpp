#include "yael/network/Address.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <stdexcept>

namespace yael::network {

Address::Address(const sockaddr_in6 &addr) {
    std::array<char, INET6_ADDRSTRLEN> ip_as_string;

    if (inet_ntop(AF_INET6, &addr.sin6_addr, ip_as_string.data(), 25) ==
        nullptr) {
        throw std::runtime_error("invalid sock address");
    }

    IPv6 = true;
    IP = std::string(ip_as_string.data());
    PortNumber = ntohs(addr.sin6_port);
}

Address::Address(const sockaddr_in &addr) {
    std::array<char, INET6_ADDRSTRLEN> ip_as_string;

    if (inet_ntop(AF_INET, &addr.sin_addr, ip_as_string.data(), 16) ==
        nullptr) {
        throw std::runtime_error("invalid sock address");
    }

    IPv6 = false;
    IP = std::string(ip_as_string.data());
    PortNumber = ntohs(addr.sin_port);
}

bool Address::get_sock_address6(sockaddr_in6 &addr) const {
    if (!IPv6) {
        return false;
    }

    addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, IP.c_str(), &addr.sin6_addr.s6_addr);
    addr.sin6_port = htons(PortNumber);

    return true;
}

bool Address::get_sock_address(sockaddr_in &addr) const {
    if (IPv6) {
        return false;
    }

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, IP.c_str(), &addr.sin_addr.s_addr);
    addr.sin_port = htons(PortNumber);

    return true;
}

Address resolve_URL(const std::string &url, uint16_t port_number, bool IPv6) {
    struct addrinfo *host, *hosti;
    Address address;

    const auto error = getaddrinfo(url.c_str(), nullptr, nullptr, &host);

    if (error != 0) {
        throw std::runtime_error(std::string("Error getting address: ") +
                                 gai_strerror(error));
    }

    // Look for the correct address.
    for (hosti = host; hosti != nullptr; hosti = hosti->ai_next) {
        if ((IPv6 && hosti->ai_family != AF_INET6) ||
            (!IPv6 && hosti->ai_family == AF_INET6)) {
            continue;
        }

        if (IPv6) {
            auto saddr = *(reinterpret_cast<sockaddr_in6 *>(hosti->ai_addr));
            address = Address(saddr);
        } else {
            auto saddr = *(reinterpret_cast<sockaddr_in *>(hosti->ai_addr));
            address = Address(saddr);
        }

        break;
    }

    freeaddrinfo(host);

    address.PortNumber = port_number;
    return address;
}

} // namespace yael::network
