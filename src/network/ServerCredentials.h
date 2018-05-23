#pragma once

#include <botan/certstor.h>
#include <botan/credentials_manager.h>

namespace yael
{
namespace network
{

class ServerCredentials : public Botan::Credentials_Manager
{
public:
     std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
            const std::string& type,
            const std::string& context) override
     {
        // return a list of certificates of CAs we trust for tls server certificates,
        // e.g., all the certificates in the local directory "cas"
        return { new Botan::Certificate_Store_In_Memory("cas") };
     }

  std::vector<Botan::X509_Certificate> cert_chain(
     const std::vector<std::string>& cert_key_types,
     const std::string& type,
     const std::string& context) override
     {
         // when using tls client authentication (optional), return
         // a certificate chain being sent to the tls server,
         // else an empty list
         return std::vector<Botan::X509_Certificate>();
     }

  Botan::Private_Key* private_key_for(const Botan::X509_Certificate& cert,
     const std::string& type,
     const std::string& context) override
     {
         // when returning a chain in cert_chain(), return the private key
         // associated with the leaf certificate here
         return nullptr;
     }
};

}
}
