#include "network/Address.h"

#include <stdexcept>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace yael
{
namespace network
{

Address::Address(const sockaddr_in6& addr)
{
    char ipAsString[INET6_ADDRSTRLEN];

    if (!inet_ntop( AF_INET6, &addr.sin6_addr, &ipAsString[0], 25))
    {
        throw std::runtime_error("invalid sock address");
    }

    IPv6 = true;
    IP = std::string(&ipAsString[0]);
    PortNumber = ntohs(addr.sin6_port);
}

Address::Address(const sockaddr_in& addr)
{
    char ipAsString[INET_ADDRSTRLEN];

    if (!inet_ntop( AF_INET, &addr.sin_addr, &ipAsString[0], 16))
    {
        throw std::runtime_error("invalid sock address");
    }

    IPv6 = false;
    IP = std::string(&ipAsString[0]);
    PortNumber = ntohs(addr.sin_port);
}

bool Address::get_sock_address6(sockaddr_in6& addr) const
{
    if (!IPv6)
        return false;

    addr.sin6_family = AF_INET6;
    inet_pton(AF_INET6, IP.c_str(), &addr.sin6_addr.s6_addr);
    addr.sin6_port = htons(PortNumber);

    return true;
}

bool Address::get_sock_address(sockaddr_in& addr) const
{
    if (IPv6)
        return false;

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, IP.c_str(), &addr.sin_addr.s_addr);
    addr.sin_port = htons(PortNumber);

    return true;
}

Address resolve_URL(std::string url, uint16_t port_number, bool IPv6_)
{
    struct addrinfo *host, *hosti;
    Address address;

    int error = getaddrinfo(url.c_str(), 0, 0, &host );

    if (error != 0)
    {
        throw std::runtime_error(std::string("Error getting address: ") + gai_strerror( error ));
    }

    // Look for the correct address.
    for (hosti = host; hosti != 0; hosti = hosti->ai_next)
    {
        if((IPv6_ && hosti->ai_family != AF_INET6) || (!IPv6_ && hosti->ai_family == AF_INET6))
            continue;

        if(IPv6_)
        {
            sockaddr_in6 saddr = *(reinterpret_cast<sockaddr_in6*>(hosti->ai_addr));
            address = Address(saddr);
        }
        else
        {
            sockaddr_in saddr = *(reinterpret_cast<sockaddr_in*>(hosti->ai_addr));
            address = Address(saddr);
        }

        break;
    }

    if(host)
    {
        freeaddrinfo(host);
    }

    address.PortNumber = port_number;
    return address;
}

}
}
