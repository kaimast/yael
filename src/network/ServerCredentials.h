#pragma once

#include <botan/certstor.h>
#include <botan/credentials_manager.h>
#include <botan/pk_keys.h>
#include <botan/pkcs8.h>

namespace yael
{
namespace network
{

class ServerCredentials : public Botan::Credentials_Manager
{
public:
    ServerCredentials()
        : m_key(Botan::PKCS8::load_key("test.key", m_rng))
    {}

     std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
            const std::string& type,
            const std::string& context) override
     {
         return std::vector<Botan::Certificate_Store*>();
     }

  std::vector<Botan::X509_Certificate> cert_chain(
     const std::vector<std::string>& cert_key_types,
     const std::string& type,
     const std::string& context) override
     {
        return { Botan::X509_Certificate("test.cert") };
     }

  Botan::Private_Key* private_key_for(const Botan::X509_Certificate& cert,
     const std::string& type,
     const std::string& context) override
     {
         return m_key.get();
     }
  
private:
     Botan::AutoSeeded_RNG m_rng;
     std::unique_ptr<Botan::Private_Key> m_key;
};

}
}
