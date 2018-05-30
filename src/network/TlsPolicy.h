#pragma once

#include <botan/tls_policy.h>
#include <vector>
#include <string>

namespace yael
{
namespace network
{

class TlsPolicy : public Botan::TLS::Strict_Policy
{
public:
    std::vector<std::string> allowed_signature_methods() const override
    {
        return { "RSA" };
    }
};

}
}
