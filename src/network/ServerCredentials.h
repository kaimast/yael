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
    ServerCredentials(const std::string &key_path, const std::string &cert_path)
        : m_key(Botan::PKCS8::load_key(key_path, m_rng)), m_certificate(cert_path)
    {}

     std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(
            const std::string& type,
            const std::string& context) override
     {
        (void)type;
        (void)context;

         return std::vector<Botan::Certificate_Store*>();
     }

  std::vector<Botan::X509_Certificate> cert_chain(
     const std::vector<std::string>& cert_key_types,
     const std::string& type,
     const std::string& context) override
     {
        (void)cert_key_types;
        (void)type;
        (void)context;

        return { m_certificate };
     }

  Botan::Private_Key* private_key_for(const Botan::X509_Certificate& cert,
     const std::string& type,
     const std::string& context) override
     {
        (void)cert;
        (void)type;
        (void)context;

         return m_key.get();
     }
  
private:
     Botan::AutoSeeded_RNG m_rng;
     std::unique_ptr<Botan::Private_Key> m_key;
     Botan::X509_Certificate m_certificate;
};

}
}
