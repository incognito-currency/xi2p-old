/**                                                                                           //
 * Copyright (c) 2017-2018, The Xi2p I2P Router Project                                      //
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
 */

#include "core/crypto/util/compression.h"

#include <cryptopp/crc.h>
#include <cryptopp/gzip.h>
#include <cryptopp/zinflate.h>

#include "core/util/log.h"

namespace xi2p {
namespace core {

/// @class DeflateDecompressorImpl
/// @brief RFC 1951 DEFLATE Decompressor
class DeflateDecompressor::DeflateDecompressorImpl {
 public:
  std::size_t Put(
      std::uint8_t* buffer,
      std::size_t length) {
    std::size_t unprocessed_bytes;
    unprocessed_bytes = m_Inflator.Put(buffer, length);
    // Signal the end of messages to the object
    m_Inflator.MessageEnd();
    return unprocessed_bytes;
  }

  std::size_t Get(
       std::uint8_t* buffer,
       std::size_t length) {
    return m_Inflator.Get(buffer, length);
  }

  std::size_t MaxRetrievable() {
    return m_Inflator.MaxRetrievable();
  }

  bool Verify(
      std::uint8_t* hash,
      std::uint8_t* data,
      std::size_t length) {
    return CryptoPP::CRC32().VerifyDigest(hash, data, length);
  }

 private:
  CryptoPP::Inflator m_Inflator;
};

DeflateDecompressor::DeflateDecompressor()
    : m_DeflateDecompressorPimpl(
          std::make_unique<DeflateDecompressorImpl>()) {}

DeflateDecompressor::~DeflateDecompressor() {}

std::size_t DeflateDecompressor::Put(
    std::uint8_t* buffer,
    std::size_t length) {
  return m_DeflateDecompressorPimpl->Put(buffer, length);
}

std::size_t DeflateDecompressor::Get(
    std::uint8_t* buffer,
    std::size_t length) {
  return m_DeflateDecompressorPimpl->Get(buffer, length);
}

std::size_t DeflateDecompressor::MaxRetrievable() {
  return m_DeflateDecompressorPimpl->MaxRetrievable();
}

bool DeflateDecompressor::Verify(
    std::uint8_t* hash,
    std::uint8_t* data,
    std::size_t length) {
  return m_DeflateDecompressorPimpl->Verify(hash, data, length);
}

/// @class GzipImpl
/// @brief RFC 1952 GZIP Compressor
class Gzip::GzipImpl {
 public:
  unsigned int GetMinDeflateLevel() {
    return CryptoPP::Gzip::MIN_DEFLATE_LEVEL;
  }
  unsigned int GetDefaultDeflateLevel() {
    return CryptoPP::Gzip::DEFAULT_DEFLATE_LEVEL;
  }

  unsigned int GetMaxDeflateLevel() {
    return CryptoPP::Gzip::MAX_DEFLATE_LEVEL;
  }

  void SetDeflateLevel(
      unsigned int deflate_level) {
    m_Gzip.SetDeflateLevel(deflate_level);
  }

  std::size_t Put(
      const std::uint8_t* buffer,
      std::size_t length) {
    std::size_t unprocessed_bytes;
    unprocessed_bytes = m_Gzip.Put(buffer, length);
    m_Gzip.MessageEnd();
    return unprocessed_bytes;
  }

  std::size_t Get(
       std::uint8_t* buffer,
       std::size_t length) {
    return m_Gzip.Get(buffer, length);
  }

  std::size_t MaxRetrievable() {
    return m_Gzip.MaxRetrievable();
  }

 private:
  CryptoPP::Gzip m_Gzip;
};

Gzip::Gzip()
    : m_GzipPimpl(
          std::make_unique<GzipImpl>()) {}

Gzip::~Gzip() {}

std::size_t Gzip::GetMinDeflateLevel() {
  return m_GzipPimpl->GetMinDeflateLevel();
}

std::size_t Gzip::GetDefaultDeflateLevel() {
  return m_GzipPimpl->GetDefaultDeflateLevel();
}

std::size_t Gzip::GetMaxDeflateLevel() {
  return m_GzipPimpl->GetMaxDeflateLevel();
}

void Gzip::SetDeflateLevel(
      std::size_t level) {
  m_GzipPimpl->SetDeflateLevel(level);
}

std::size_t Gzip::Put(
    const std::uint8_t* buffer,
    std::size_t length) {
  return m_GzipPimpl->Put(buffer, length);
}

std::size_t Gzip::Get(
    std::uint8_t* buffer,
    std::size_t length) {
  return m_GzipPimpl->Get(buffer, length);
}

std::size_t Gzip::MaxRetrievable() {
  return m_GzipPimpl->MaxRetrievable();
}

/// @class GunzipImpl
/// @brief RFC 1952 GZIP Decompressor
class Gunzip::GunzipImpl {
 public:
  std::size_t Put(
      const std::uint8_t* buffer,
      std::size_t length) {
    std::size_t unprocessed_bytes;
    unprocessed_bytes = m_Gunzip.Put(buffer, length);
    m_Gunzip.MessageEnd();
    return unprocessed_bytes;
  }

  std::size_t Get(
       std::uint8_t* buffer,
       std::size_t length) {
    return m_Gunzip.Get(buffer, length);
  }

  std::size_t MaxRetrievable() {
    return m_Gunzip.MaxRetrievable();
  }

 private:
  CryptoPP::Gunzip m_Gunzip;
};

Gunzip::Gunzip()
    : m_GunzipPimpl(
          std::make_unique<GunzipImpl>()) {}

Gunzip::~Gunzip() {}

std::size_t Gunzip::Put(
    const std::uint8_t* buffer,
    std::size_t length) {
  return m_GunzipPimpl->Put(buffer, length);
}

std::size_t Gunzip::Get(
    std::uint8_t* buffer,
    std::size_t length) {
  return m_GunzipPimpl->Get(buffer, length);
}

std::size_t Gunzip::MaxRetrievable() {
  return m_GunzipPimpl->MaxRetrievable();
}

}  // namespace core
}  // namespace xi2p
