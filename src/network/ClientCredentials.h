#pragma once

#include <botan/certstor.h>
#include <botan/credentials_manager.h>

namespace yael {
namespace network {

class ClientCredentials : public Botan::Credentials_Manager {
  public:
    std::vector<Botan::Certificate_Store *>
    trusted_certificate_authorities(const std::string &type,
                                    const std::string &context) override {
        (void)type;
        (void)context;

        return {new Botan::Certificate_Store_In_Memory("cas")};
    }

    std::vector<Botan::X509_Certificate>
    cert_chain(const std::vector<std::string> &cert_key_types,
               const std::string &type, const std::string &context) override {
        (void)cert_key_types;
        (void)type;
        (void)context;

        return std::vector<Botan::X509_Certificate>();
    }

    Botan::Private_Key *private_key_for(const Botan::X509_Certificate &cert,
                                        const std::string &type,
                                        const std::string &context) override {
        (void)cert;
        (void)type;
        (void)context;

        return nullptr;
    }
};

} // namespace network
} // namespace yael
