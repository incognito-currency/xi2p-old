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

#include "client/instance.h"

#include <cstdint>
#include <memory>
#include <stdexcept>

#include "client/context.h"

#include "core/util/log.h"

namespace xi2p
{
namespace client
{
Instance::Instance(const core::Instance& core) try
    : m_Exception(__func__),
      m_Core(core),
      m_Config(m_Core.GetConfig()),
      m_IsReloading(false)
  {
  }
catch (...)
  {
    m_Exception.Dispatch();
  }

Instance::~Instance() {}

// Note: we'd love Instance RAII but singleton needs to be daemonized (if applicable) before initialization
// TODO(unassigned): see TODO's for client context and singleton
void Instance::Initialize()
{
  // Initialize core
  m_Core.Initialize();

  LOG(debug) << "Instance: initializing client";
  // TODO(unassigned): a useful shutdown handler but needs to callback to daemon
  // singleton's member. It's only used for I2PControl (and currently doesn't work)
  // so we'll have to figure out another way to *not* rely on the singleton to
  // tell the contexts to shutdown. Note: previous to refactor work, the shutdown handler
  // (or related) was not fully functional (for possible threading reasons), so
  // commenting out this function does not provide any loss in functionality.
  /*context.RegisterShutdownHandler(
    [this]() { m_IsRunning = false; });*/

  // Initialize proxies
  std::shared_ptr<ClientDestination> local_destination;
  auto const& map = m_Config.GetCoreConfig().GetMap();
  auto const proxy_keys = map["proxykeys"].as<std::string>();

  if (!proxy_keys.empty())
    local_destination = context.LoadLocalDestination(proxy_keys, false);

  context.SetHTTPProxy(std::make_unique<HTTPProxy>(
      "HTTP Proxy",  // TODO(unassigned): what if we want to change the name?
      map["httpproxyaddress"].as<std::string>(),
      map["httpproxyport"].as<int>(),
      local_destination));

  context.SetSOCKSProxy(std::make_unique<SOCKSProxy>(
      map["socksproxyaddress"].as<std::string>(),
      map["socksproxyport"].as<int>(),
      local_destination));

  // Initialize I2PControl
  auto const i2pcontrol_port = map["i2pcontrolport"].as<int>();
  if (i2pcontrol_port)
    {
      context.SetI2PControlService(std::make_unique<I2PControlService>(
          context.GetIoService(),
          map["i2pcontroladdress"].as<std::string>(),
          i2pcontrol_port,
          map["i2pcontrolpassword"].as<std::string>()));
    }

  // Setup client and server tunnels
  SetupTunnels();
}

void Instance::SetupTunnels()
{
  // List of tunnels that exist after update
  std::vector<std::string> updated_client_tunnels, updated_server_tunnels;
  // Count number of tunnels
  std::size_t client_count{}, server_count{};
  // Iterate through each section in tunnels config
  for (auto const& tunnel : m_Config.GetParsedTunnelsConfig())
    {
      try
        {
          // Test which type of tunnel (client or server)
          if (tunnel.type == m_Config.GetAttribute(Key::Client)
              || tunnel.type == m_Config.GetAttribute(Key::IRC))
            {  // TODO(unassigned): see #9
              if (m_IsReloading)
                {
                  auto const* client_tunnel =
                      context.GetClientTunnel(tunnel.port);
                  if (client_tunnel && client_tunnel->GetName() != tunnel.name)
                    {
                      // Check for conflicting port done in ParseTunnelsConfig
                      // early deletion of client_tunnel to avoid temp duplicate port bind
                      std::string name = client_tunnel->GetName();
                      LOG(debug)
                          << "ClientContext: Premature delete tunnel " << name;
                      context.RemoveClientTunnels(
                          [&name](I2PClientTunnel* old_tunnel) {
                            return name == old_tunnel->GetName();
                          });
                    }
                  context.UpdateClientTunnel(tunnel);
                  updated_client_tunnels.push_back(tunnel.name);
                  ++client_count;
                  continue;
                }
              // Create client tunnel
              if (context.AddClientTunnel(tunnel))
                ++client_count;
              else
                LOG(error) << "Instance: client tunnel with port "
                           << tunnel.port << " already exists";
            }
          else
            {  // TODO(unassigned): currently, anything that's not client
              bool const is_http =
                  (tunnel.type == m_Config.GetAttribute(Key::HTTP));
              if (m_IsReloading)
                {
                  context.UpdateServerTunnel(tunnel, is_http);
                  updated_server_tunnels.push_back(tunnel.name);
                  ++server_count;
                  continue;
                }
              if (context.AddServerTunnel(tunnel, is_http))
                ++server_count;
              else
                LOG(error) << "Instance: Failed to add server tunnel";
            }
        }
      catch (...)
        {
          core::Exception ex;
          ex.Dispatch(__func__);
          return;
        }
    }  // end of iteration block
  if (m_IsReloading)
    {
      LOG(info) << "Instance: " << client_count << " client tunnels updated";
      LOG(info) << "Instance: " << server_count << " server tunnels updated";
      RemoveOldTunnels(updated_client_tunnels, updated_server_tunnels);
      return;
    }
  LOG(info) << "Instance: " << client_count << " client tunnels created";
  LOG(info) << "Instance: " << server_count << " server tunnels created";
}

void Instance::RemoveOldTunnels(
    const std::vector<std::string>& updated_client_tunnels,
    const std::vector<std::string>& updated_server_tunnels)
{
  context.RemoveServerTunnels(
      [&updated_server_tunnels](I2PServerTunnel* tunnel) {
        return std::find(
                   updated_server_tunnels.begin(),
                   updated_server_tunnels.end(),
                   tunnel->GetTunnelAttributes().name)
               == updated_server_tunnels.end();
      });
  context.RemoveClientTunnels(
      [&updated_client_tunnels](I2PClientTunnel* tunnel) {
        return std::find(
                   updated_client_tunnels.begin(),
                   updated_client_tunnels.end(),
                   tunnel->GetTunnelAttributes().name)
               == updated_client_tunnels.end();
      });
}

void Instance::Start()
{
  try
    {
      LOG(debug) << "Instance: starting core";
      m_Core.Start();

      LOG(debug) << "Instance: starting client";
      context.Start();
    }
  catch (...)
    {
      m_Exception.Dispatch(__func__);
      throw;
    }

  LOG(info) << "Instance: client successfully started";
}

void Instance::Stop()
{
  try
    {
      LOG(debug) << "Instance: stopping client";
      context.Stop();

      LOG(debug) << "Instance: stopping core";
      m_Core.Stop();
    }
  catch (...)
    {
      m_Exception.Dispatch(__func__);
      throw;
    }

  LOG(info) << "Instance: client successfully stopped";
}

void Instance::Reload()
{
  LOG(info) << "Instance: reloading client";
  // TODO(unassigned): locking etc.
  // TODO(unassigned): core instance
  m_IsReloading = true;
  m_Config.ParseConfig();
  SetupTunnels();
  m_IsReloading = false;
}

}  // namespace client
}  // namespace xi2p
