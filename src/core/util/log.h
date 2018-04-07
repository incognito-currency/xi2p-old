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

#ifndef SRC_CORE_UTIL_LOG_H_
#define SRC_CORE_UTIL_LOG_H_

#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/network/include/http/client.hpp>
#include <boost/program_options.hpp>
#include <sstream>
#include <string>
#include "core/util/byte_stream.h"

// TODO(anonimal):
// Boost.Log uses an "application-wide singleton" (note: our logger/sink setup applies globally from instance configuration)
// As a result, logging will not work when in daemon mode. http://www.boost.org/doc/libs/1_63_0/libs/log/doc/html/log/rationale/fork_support.html
// We've worked around this problem in the past by using some very gross hacking but we may be able to apply a cleaner work-around so we can set this up entirely in the core namespace
// (we could create an (inheritable?) logging class with overloaded stream operator and adjust our logging initialization and macro accordingly, or consider other options)
// Also note that a singleton will effect having multiple logging library options (there's no need to do that though when we have huge flexibility with sinks)

BOOST_LOG_GLOBAL_LOGGER(g_Logger, boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>)
#define LOG(severity) BOOST_LOG_SEV(g_Logger::get(), boost::log::trivial::severity)

namespace xi2p
{
namespace core
{
/// @details This configures/sets up the global logger.
/// @param Parsed xi2p variable map
/// @warning Xi2p config must first be parsed
void SetupLogging(
    const boost::program_options::variables_map& parsed_xi2p_config);

/// @brief Log source and destination of a network request or response
/// @param boost::network::http::client:: request or response
/// @return human readable string
template <typename Type>
std::string LogNetEndpointsToString(const Type& req)
{
  std::stringstream ss;
  ss << "Source : \""
     << static_cast<std::string>(boost::network::http::source(req))
     << "\" Dest : \""
     << static_cast<std::string>(boost::network::http::destination(req))
     << "\"";
  return ss.str();
}

/// @brief Log headers of a network request or response
/// @param boost::network::http::client:: request or response
/// @return human readable string
template <typename Type>
std::string LogNetHeaderToString(const Type& req)
{
  std::stringstream ss;
  ss << "Headers : ";
  for (auto const& header : req.headers())
    ss << "\"" << header.first << "\"  : \"" << header.second << "\" | ";
  return ss.str();
}

/// @brief Log body of a network request or response
/// @param boost::network::http::client:: request or response
/// @return human readable string
template <typename Type>
std::string LogNetBodyToString(const Type& req)
{
  std::stringstream ss;
  std::string body(static_cast<std::string>(boost::network::http::body(req)));
  ss << "Body : "
     << xi2p::core::GetFormattedHex(
            reinterpret_cast<const uint8_t*>(body.data()), body.length());
  return ss.str();
}

/// @brief Log entire message (endpoints + headers + body)
/// @param boost::network::http::client:: request or response
/// @return human readable string
template <typename Type>
std::string LogNetMessageToString(const Type& req)
{
  std::stringstream ss;
  ss << LogNetEndpointsToString(req) << std::endl
     << LogNetHeaderToString(req) << std::endl
     << LogNetBodyToString(req);
  return ss.str();
}

}  // namespace core
}  // namespace xi2p

#endif  // SRC_CORE_UTIL_LOG_H_
