/**                                                                                           //
 * Copyright (c) 2013-2018, The Xi2p I2P Router Project                                      //
 *                                                                                            //
 * All rights reserved.                                                                       //
 *                                                                                            //
 * Redistribution and use in source and binary forms, with or without modification, are       //
 * permitted provided that the following conditions are met:                                  //
 *                                                                                            //
 * 1. Redistributions of source code must retain the above copyright notice, this list of     //
 *    conditions and the following disclaimer.                                                //
 *                                                                                            //
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list     //
 *    of conditions and the following disclaimer in the documentation and/or other            //
 *    materials provided with the distribution.                                               //
 *                                                                                            //
 * 3. Neither the name of the copyright holder nor the names of its contributors may be       //
 *    used to endorse or promote products derived from this software without specific         //
 *    prior written permission.                                                               //
 *                                                                                            //
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY        //
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF    //
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL     //
 * THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,       //
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,               //
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    //
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,          //
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF    //
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.               //
 *                                                                                            //
 * Parts of the project are originally copyright (c) 2013-2015 The PurpleI2P Project          //
 */

#ifndef SRC_CORE_ROUTER_IDENTITY_H_
#define SRC_CORE_ROUTER_IDENTITY_H_

#include <string.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "core/crypto/elgamal.h"
#include "core/crypto/radix.h"
#include "core/crypto/signature_base.h"

#include "core/util/exception.h"
#include "core/util/tag.h"

namespace xi2p {
namespace core {

typedef Tag<32> IdentHash;

inline std::string GetB32Address(
    const xi2p::core::IdentHash& ident) {
  return ident.ToBase32().append(".b32.i2p");
}

#pragma pack(1)
struct Keys {
  std::uint8_t private_key[256];
  std::uint8_t signing_private_key[20];
  std::uint8_t public_key[256];
  std::uint8_t signing_key[128];
};

const std::uint8_t CERTIFICATE_TYPE_NULL = 0;
const std::uint8_t CERTIFICATE_TYPE_HASHCASH = 1;
const std::uint8_t CERTIFICATE_TYPE_HIDDEN = 2;
const std::uint8_t CERTIFICATE_TYPE_SIGNED = 3;
const std::uint8_t CERTIFICATE_TYPE_MULTIPLE = 4;
const std::uint8_t CERTIFICATE_TYPE_KEY = 5;

/// @brief Human readable description of this struct
/// @param CERTIFICATE_TYPE
/// @returns human readable string
std::string GetCertificateTypeName(std::uint8_t type);

struct Identity {
  std::uint8_t public_key[256];
  std::uint8_t signing_key[128];

  struct {
    std::uint8_t type;
    std::uint16_t length;
  } certificate;

  Identity() = default;

  explicit Identity(
      const Keys& keys) {
    *this = keys;
  }

  Identity& operator=(const Keys& keys);

  std::size_t FromBuffer(
      const std::uint8_t* buf,
      std::size_t len);

  IdentHash Hash() const;
};
#pragma pack()
Keys CreateRandomKeys();

const std::size_t DEFAULT_IDENTITY_SIZE = sizeof(Identity);  // 387 bytes

const std::uint16_t CRYPTO_KEY_TYPE_ELGAMAL = 0;
const std::uint16_t SIGNING_KEY_TYPE_DSA_SHA1 = 0;
const std::uint16_t SIGNING_KEY_TYPE_ECDSA_SHA256_P256 = 1;
const std::uint16_t SIGNING_KEY_TYPE_ECDSA_SHA384_P384 = 2;
const std::uint16_t SIGNING_KEY_TYPE_ECDSA_SHA512_P521 = 3;
const std::uint16_t SIGNING_KEY_TYPE_RSA_SHA256_2048 = 4;
const std::uint16_t SIGNING_KEY_TYPE_RSA_SHA384_3072 = 5;
const std::uint16_t SIGNING_KEY_TYPE_RSA_SHA512_4096 = 6;
const std::uint16_t SIGNING_KEY_TYPE_EDDSA_SHA512_ED25519 = 7;

// TODO(anonimal): review/implement to bump client type to EdDSA-SHA512-Ed25519
const std::uint16_t DEFAULT_CLIENT_SIGNING_KEY_TYPE = SIGNING_KEY_TYPE_ECDSA_SHA256_P256;
const std::uint16_t DEFAULT_ROUTER_SIGNING_KEY_TYPE = SIGNING_KEY_TYPE_EDDSA_SHA512_ED25519;

typedef std::uint16_t SigningKeyType;
typedef std::uint16_t CryptoKeyType;

/// @brief SigningKeyType to human readable string
/// @param type of the signature key
/// @returns human readable string for the key type, empty string if unknown
const std::string GetSigningKeyTypeName(const SigningKeyType key);

class IdentityEx {
 public:
  IdentityEx();
  ~IdentityEx();

  IdentityEx(
      const std::uint8_t* public_key,
      const std::uint8_t* signing_key,
      SigningKeyType type = SIGNING_KEY_TYPE_DSA_SHA1);

  IdentityEx(
      const std::uint8_t* buf,
      std::size_t len);

  IdentityEx(
      const IdentityEx& other);

  IdentityEx& operator=(const IdentityEx& other);

  IdentityEx& operator=(const Identity& standard);

  std::size_t FromBuffer(
      const std::uint8_t* buf,
      std::size_t len);

  std::size_t ToBuffer(
      std::uint8_t* buf,
      std::size_t len) const;

  void FromBase64(const std::string& s);

  std::string ToBase64() const;

  /// @brief Human readable description of this struct
  /// @param prefix for tabulations
  /// @returns human readable string
  std::string GetDescription(const std::string& = std::string()) const;

  const Identity& GetStandardIdentity() const {
    return m_StandardIdentity;
  }

  const IdentHash& GetIdentHash() const {
    return m_IdentHash;
  }

  std::size_t GetFullLen() const {
    return m_ExtendedLen + DEFAULT_IDENTITY_SIZE;
  }

  std::size_t GetSigningPublicKeyLen() const;

  std::size_t GetSigningPrivateKeyLen() const;

  std::size_t GetSignatureLen() const;

  bool Verify(
      const std::uint8_t* buf,
      std::size_t len,
      const std::uint8_t* signature) const;

  SigningKeyType GetSigningKeyType() const;

  CryptoKeyType GetCryptoKeyType() const;

  void DropVerifier();  // to save memory

 private:
  void CreateVerifier() const;

 private:
  Identity m_StandardIdentity;
  IdentHash m_IdentHash;
  mutable std::unique_ptr<xi2p::core::Verifier> m_Verifier;
  std::uint16_t m_ExtendedLen;
  std::unique_ptr<std::uint8_t[]> m_ExtendedBuffer;
  core::Exception m_Exception;
};

class PrivateKeys {  // for eepsites
 public:
  PrivateKeys();
  ~PrivateKeys();

  PrivateKeys(
      const PrivateKeys& other)
      : m_Signer(nullptr) {
        *this = other;
      }

  explicit PrivateKeys(
      const Keys& keys)
      : m_Signer(nullptr) {
        *this = keys;
      }

  PrivateKeys& operator=(const Keys& keys);

  PrivateKeys& operator=(const PrivateKeys& other);

  const IdentityEx& GetPublic() const {
    return m_Public;
  }

  const std::uint8_t* GetPrivateKey() const {
    return m_PrivateKey;
  }

  const std::uint8_t* GetSigningPrivateKey() const {
    return m_SigningPrivateKey;
  }

  void Sign(
      const std::uint8_t* buf,
      int len,
      std::uint8_t* signature) const;

  std::size_t GetFullLen() const {
    return m_Public.GetFullLen() + 256 + m_Public.GetSigningPrivateKeyLen();
  }

  std::size_t FromBuffer(
      const std::uint8_t* buf,
      std::size_t len);

  std::size_t ToBuffer(
      std::uint8_t* buf,
      std::size_t len) const;

  static PrivateKeys CreateRandomKeys(
      SigningKeyType type = DEFAULT_CLIENT_SIGNING_KEY_TYPE);

 private:
  void CreateSigner();

 private:
  IdentityEx m_Public;
  std::uint8_t m_PrivateKey[256];
  // assume private key doesn't exceed 1024 bytes
  std::uint8_t m_SigningPrivateKey[1024];
  std::unique_ptr<xi2p::core::Signer> m_Signer;
};

// kademlia
struct XORMetric {
  union {
    std::uint8_t metric[32];
    std::uint64_t metric_ll[4];
  };

  void SetMin() {
    memset(metric, 0, 32);
  }

  void SetMax() {
    memset(metric, 0xFF, 32);
  }

  bool operator<(const XORMetric& other) const {
    return memcmp(metric, other.metric, 32) < 0;
  }
};

IdentHash CreateRoutingKey(
    const IdentHash& ident);

XORMetric operator^(
    const IdentHash& key1,
    const IdentHash& key2);

// destination for delivery instructions
class RoutingDestination {
 public:
  RoutingDestination() {}
  virtual ~RoutingDestination() {}

  virtual const IdentHash& GetIdentHash() const = 0;

  virtual const std::uint8_t* GetEncryptionPublicKey() const = 0;

  virtual bool IsDestination() const = 0;  // for garlic

  std::unique_ptr<const xi2p::core::ElGamalEncryption>& GetElGamalEncryption() const {
    if (!m_ElGamalEncryption)
      m_ElGamalEncryption.reset(
          new xi2p::core::ElGamalEncryption(GetEncryptionPublicKey()));
    return m_ElGamalEncryption;
  }

 private:
  // use lazy initialization
  mutable std::unique_ptr<const xi2p::core::ElGamalEncryption> m_ElGamalEncryption;
};

class LocalDestination {
 public:
  virtual ~LocalDestination() {}

  virtual const PrivateKeys& GetPrivateKeys() const = 0;

  virtual const std::uint8_t* GetEncryptionPrivateKey() const = 0;

  virtual const std::uint8_t* GetEncryptionPublicKey() const = 0;

  const IdentityEx& GetIdentity() const {
    return GetPrivateKeys().GetPublic();
  }

  const IdentHash& GetIdentHash() const {
    return GetIdentity().GetIdentHash();
  }

  void Sign(
      const std::uint8_t* buf,
      int len,
      std::uint8_t* signature) const {
    GetPrivateKeys().Sign(
        buf,
        len,
        signature);
  }
};

}  // namespace core
}  // namespace xi2p

#endif  // SRC_CORE_ROUTER_IDENTITY_H_
