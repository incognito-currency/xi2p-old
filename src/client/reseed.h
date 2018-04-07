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
 *                                                                                            //
 * Parts of the project are originally copyright (c) 2013-2015 The PurpleI2P Project          //
 */

#ifndef SRC_CLIENT_RESEED_H_
#define SRC_CLIENT_RESEED_H_

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "client/util/zip.h"

#include "core/crypto/util/x509.h"

#include "core/router/context.h"

#include "core/util/filesystem.h"

namespace xi2p {
namespace client {

/**
 * Reseed/SU3 specification can be found at
 * https://geti2p.net/en/docs/spec/updates
 */

/**
 * @class Reseed
 * @brief Reseed implementation
 * @details Will construct an SU3 using constructed argument
 * @param String URI directed to an SU3 stream
 */
class Reseed {
 public:
  /// @brief Constructs stream from string
  explicit Reseed(
      const std::string& stream =
          core::context.GetOpts()["reseed-from"].as<std::string>())
      : m_Stream(stream)
  {
  }

  /// @brief Reseed implementation
  /// @details Oversees fetching and processing of SU3
  /// @return false on failure
  bool Start();

  /// @brief Processes SU3 certificates for SU3 verification
  /// @param Container to populate
  /// @param Path for certificate directory
  /// @return False on failure
  static bool ProcessCerts(
      std::map<std::string, xi2p::core::PublicKey>* map,
      const boost::filesystem::path& cert_dir);

 private:
  /// @brief Fetches the stream to reseed from
  /// @return false on failure
  bool FetchStream();

  /// @brief Downloads stream via HTTP/S
  /// @param URL
  /// @return false on failure
  bool FetchStream(
      const std::string& url);

  /// @brief Opens stream from file
  /// @param URI (local file, NFS, sshfs, Samba, etc.)
  /// @return false on failure
  bool FetchStream(
      std::ifstream& ifs);

 private:
  // X.509 object used for SU3 verification
  xi2p::core::X509 m_X509;

  // X.509 signing keys for SU3 verification
  std::map<std::string, xi2p::core::PublicKey> m_SigningKeys;

  // The URI which will be the SU3
  std::string m_Stream;

  // Spec constant
  const std::string m_Filename = "i2pseeds.su3";

  // I2P-approved reseed hosts
  const std::vector<std::string> m_Hosts = {
    "https://download.xxlspeed.com/",  // Requires SNI
    "https://i2p.mooo.com/netDb/",
    "https://i2pseed.creativecowpat.net:8443/",
    "https://netdb.i2p2.no/",  // Requires SNI
    "https://reseed.i2p-projekt.de/",
    "https://reseed.onion.im/",
    // Note: for "Let's Encrypt" certs,
    // we currently require DST Root CA X3 (not ISRG Root X1)
    "https://i2p-0.manas.ca:8443/",
    "https://i2p.manas.ca:8443/",
    "https://i2p.novg.net/",
    "https://reseed.atomike.ninja/",  // Requires SNI
    "https://reseed.memcpy.io/",  // Requires SNI
  };
};

/**
 * @class SU3
 * @brief SU3 implementation
 * @param su3 String of bytes that *must* be an SU3
 * @param keys Pubkeys to verify with
 * @param disable_verification Disable signed SU3 verification?
 */
class SU3 {
 public:
  SU3(const std::string& su3,
      std::map<std::string, xi2p::core::PublicKey>& keys,
      bool verify = true)
      : m_Stream(su3),
        m_SigningKeys(keys),
        m_Verify(verify),
        m_Data(std::make_unique<Data>()) {}

  // Extracted RI's (map of router info files)
  std::unordered_map<std::size_t, std::vector<std::uint8_t>> m_RouterInfos;

  /// @brief SU3 implementation
  /// @return False on failure
  bool SU3Impl();

  /// @brief Extracts Embedded file from SU3 stream
  /// @param Output stream
  /// @return False on failure
  bool Extract(xi2p::core::OutputFileStream* output);

  /// @brief Get Version
  /// @return version
  std::string GetVersion() const
  {
    std::stringstream ss;
    ss << std::dec;
    for (std::size_t i(0); i < m_Data->version_length; ++i)
      ss << m_Data->version[i];
    return ss.str();
  }

  /// @brief Get Signer Id
  /// @return signer_id
  std::string GetSignerId() const
  {
    return std::string(m_Data->signer_id.data());
  }

  /// @brief Get Signature type
  /// @return signature type
  xi2p::core::SigningKeyType GetSignatureType() const
  {
    return m_Data->signature_type;
  }

  /// @brief Get Content type
  /// @return content type
  std::uint8_t GetContentType() const
  {
    return m_Data->content_type;
  }

  /// @brief Get Embedded File type
  /// @return file type
  std::uint8_t GetFileType() const
  {
    return m_Data->file_type;
  }

  /// @return Get human readable string for FileType
  /// @param FileType
  /// @return human readable string
  static const std::string FileTypeToString(std::uint8_t type);

  /// @brief Get human readable string for ContentType
  /// @param ContentType
  /// @return human readable string
  static const std::string ContentTypeToString(std::uint8_t type);

 private:
  /// @brief Prepares/parses SU3 stream
  /// @return False on failure
  bool PrepareStream();

  /// @brief Verifies signature of SU3 stream
  /// @return False on failure
  bool VerifySignature();

  /// @brief Extracts content (RI's) from SU3 stream
  /// @return False on failure
  bool ExtractContent();

 private:
  const std::string m_MagicValue = "I2Psu3";

  enum struct FileType : const std::uint8_t {
    zip_file = 0x00,     // Supported
    xml_file = 0x01,     // Not supported
    html_file = 0x02,    // Not supported
    xml_gz_file = 0x03,  // Not supported
  };

  enum struct ContentType : const std::uint8_t {
    unknown = 0x00,         // Not supported
    router_update = 0x01,   // Not supported
    plugin_related = 0x02,  // Not supported
    reseed_data = 0x03,     // Supported
    news_feed = 0x04,       // Not supported
  };

  enum struct Offset : const std::uint8_t {
    version = 1,  // SU3
    unused = 1,
  };

  enum struct Size : const std::uint8_t {
    magic_number = 7,
    signature_type = 2,
    signature_length = 2,
    minimal_version = 16,
    version_length = 1,
    signer_id_length = 1,
    content_length = 8,
    content_type = 1,
    file_type = 1,
  };

  struct Data {
    std::array<char, static_cast<std::uint8_t>(Size::magic_number)> magic_number;
    xi2p::core::SigningKeyType signature_type;
    std::uint16_t signature_length;
    std::uint8_t version_length;  // Seconds since epoch, in ASCII. $(date +%s)
    std::uint8_t signer_id_length;
    std::uint8_t file_type, content_type;
    std::array<char, std::numeric_limits<std::uint8_t>::max()> version;
    std::array<char, std::numeric_limits<std::uint8_t>::max()> signer_id;
    std::uint64_t content_length;
    std::size_t content_position;  // ZIP/Router Infos/etc.
    std::size_t signature_position;
    std::vector<std::uint8_t> content, signature;
  };

  // Complete SU3 Stream
  xi2p::core::StringStream m_Stream;

  // X.509 signing keys for SU3 verification
  std::map<std::string, xi2p::core::PublicKey> m_SigningKeys;

  /// @brief Enable SU3 verification?
  bool m_Verify;

  // Spec-defined data
  std::unique_ptr<Data> m_Data;
};

}  // namespace client
}  // namespace xi2p

#endif  // SRC_CLIENT_RESEED_H_
