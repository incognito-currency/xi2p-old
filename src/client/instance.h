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

#ifndef SRC_CLIENT_INSTANCE_H_
#define SRC_CLIENT_INSTANCE_H_

#include <string>
#include <vector>

#include "client/util/config.h"
#include "core/instance.h"

namespace xi2p
{
namespace client
{
// TODO(anonimal): we currently want to limit core interaction through client only for all apps and most API cases.
//  A member function getter can return the object which will (should) change mutable data via the core API.
//  In essence, the core and client Instance objects are essentially a preliminary API.

/// @class Instance
/// @brief Client instance implementation
/// @note Owns core instance object
/// @note It is currently implied that only a single configuration object will
///   be used by a single instance object.
class Instance
{
 public:
  // TODO(anonimal): upon further API development, we'll most likely want non-const reference
  explicit Instance(const core::Instance& core);

  // TODO(anonimal): overload ctor
  ~Instance();

  /// @brief Initializes the router's client context object
  /// @details Creates tunnels, proxies and I2PControl service
  void Initialize();

  /// @brief Starts instance
  void Start();

  /// @brief Stops instance
  void Stop();

  /// @brief Reloads configuration
  /// @notes TODO(unassigned): should also reload client context
  void Reload();

  /// @brief Get client configuration object
  /// @return Reference to configuration object
  const Configuration& GetConfig() const noexcept
  {
    return m_Config;
  }

 private:
  /// @brief Sets up (or reloads) client/server tunnels
  /// @warning Configuration files must be parsed prior to setup
  void SetupTunnels();

  /// @brief Should remove old tunnels after tunnels config is updated
  /// @param updated_client_tunnels List of client tunnels to keep
  /// @param updated_server_tunnels List of server tunnels to keep
  void RemoveOldTunnels(
      const std::vector<std::string>& updated_client_tunnels,
      const std::vector<std::string>& updated_server_tunnels);

 private:
  /// @brief Exception dispatcher
  core::Exception m_Exception;

  /// @brief Core instance
  core::Instance m_Core;

  /// @brief Client configuration implementation
  /// @note Must be initialized with core configuration
  Configuration m_Config;

  /// @brief Is client configuration in the process of reloading?
  /// TODO(unassigned): expand types of reloading
  bool m_IsReloading;
};

}  // namespace client
}  // namespace xi2p

#endif  // SRC_CLIENT_INSTANCE_H_
