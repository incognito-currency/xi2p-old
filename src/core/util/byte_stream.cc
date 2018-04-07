/**                                                                                           //
 * Copyright (c) 2015-2018, The Xi2p I2P Router Project                                      //
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

#include "core/util/byte_stream.h"

#include <boost/asio/ip/address.hpp>

#include <cassert>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "core/util/log.h"

// TODO(anonimal): a secure bytestream implementation that ensures wiped memory when needed.
//   Otherwise, and preferably, use existing standard library containers with crypto++.

namespace xi2p
{
namespace core
{
// Input
ByteStream::ByteStream(const std::uint8_t* data, const std::size_t len)
    : m_DataPtr(const_cast<std::uint8_t*>(data)), m_Length(len), m_Counter{}
// TODO(anonimal): remove const cast!
{
  assert(data || len);

  if (!data)
    throw std::invalid_argument("ByteStream: null data");
  if (!len)
    throw std::length_error("ByteStream: null length");
}

// Output
ByteStream::ByteStream(std::uint8_t* data, const std::size_t len)
    : m_DataPtr(data), m_Length(len), m_Counter{}
{
  assert(data || len);

  if (!data)
    throw std::invalid_argument("ByteStream: null data");
  if (!len)
    throw std::length_error("ByteStream: null length");
}

ByteStream::ByteStream(const std::size_t len) : m_Length(len), m_Counter{}
{
  assert(len);

  if (!len)
    throw std::length_error("ByteStream: null length");

  m_Data.reserve(len);
  m_DataPtr = m_Data.data();
}

void ByteStream::Advance(const std::size_t len)
{
  assert(len && len <= m_Length);

  if (!len || len > m_Length)
    throw std::length_error("ByteStream: invalid length advancement");

  m_DataPtr += len;
  m_Counter += len;
  m_Length -= len;
}

// Input

InputByteStream::InputByteStream(
    const std::uint8_t* data,
    const std::size_t len)
    : ByteStream(data, len)
{
}

std::uint8_t* InputByteStream::ReadBytes(const std::size_t len)
{
  std::uint8_t* ptr = m_DataPtr;
  Advance(len);
  return ptr;
}

void InputByteStream::SkipBytes(const std::size_t len)
{
  Advance(len);
}

// Output

OutputByteStream::OutputByteStream(std::uint8_t* data, const std::size_t len)
    : ByteStream(data, len)
{
}

OutputByteStream::OutputByteStream(const std::size_t len) : ByteStream(len) {}

void OutputByteStream::WriteData(
    const std::uint8_t* data,
    const std::size_t len,
    const bool allow_null_data)
{
  assert((data && allow_null_data) || len);

  if (!data && !allow_null_data)
    throw std::invalid_argument("OutputByteStream: null data");
  if (!len)
    throw std::length_error("OutputByteStream: null length");

  std::uint8_t* ptr = m_DataPtr;
  Advance(len);

  if (!data)
    std::memset(ptr, 0, len);
  else
    std::memcpy(ptr, data, len);
}

void OutputByteStream::SkipBytes(const std::size_t len)
{
  WriteData(nullptr, len, true);
}

// Misc

const std::string GetFormattedHex(
    const std::uint8_t* data,
    const std::size_t size)
{
  std::ostringstream hex;
  hex << "\n\t"
      << " |  ";
  std::size_t count{}, sub_count{};
  for (std::size_t i = 0; i < size; i++)
    {
      hex << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<std::uint16_t>(data[i]) << " ";
      count++;
      if (count == 32 || (i == size - 1))
        {
          hex << " |"
              << "\n\t";
          count = 0;
        }
      sub_count++;
      if (sub_count == 8 && (i != size - 1))
        {
          hex << " |  ";
          sub_count = 0;
        }
    }
  return hex.str() + "\n";
}

std::vector<std::uint8_t> AddressToByteVector(
    const boost::asio::ip::address& address)
{
  bool is_v4(address.is_v4());
  std::vector<std::uint8_t> data(is_v4 ? 4 : 16);
  std::memcpy(
      data.data(),
      is_v4 ? address.to_v4().to_bytes().data()
            : address.to_v6().to_bytes().data(),
      data.size());
  return data;
}

boost::asio::ip::address BytesToAddress(
    const std::uint8_t* host,
    const std::uint8_t size)
{
  boost::asio::ip::address address;

  assert(size == 4 || size == 16);
  switch (size)
    {
      case 4:
        {
          boost::asio::ip::address_v4::bytes_type bytes;
          std::memcpy(bytes.data(), host, size);
          address = boost::asio::ip::address_v4(bytes);
        }
        break;
      case 16:
        {
          boost::asio::ip::address_v6::bytes_type bytes;
          std::memcpy(bytes.data(), host, size);
          address = boost::asio::ip::address_v6(bytes);
        }
        break;
      default:
        throw std::length_error("invalid address size");
    };

  return address;
}

}  // namespace core
}  // namespace xi2p
