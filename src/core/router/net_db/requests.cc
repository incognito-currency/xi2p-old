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

#include "core/router/net_db/requests.h"

#include "core/router/net_db/impl.h"
#include "core/router/transports/impl.h"

#include "core/util/log.h"

namespace xi2p {
namespace core {

std::shared_ptr<I2NPMessage> RequestedDestination::CreateRequestMessage(
    std::shared_ptr<const RouterInfo> router,
    std::shared_ptr<const xi2p::core::InboundTunnel> reply_tunnel) {
  auto msg = xi2p::core::CreateRouterInfoDatabaseLookupMsg(
      m_Destination,
      reply_tunnel->GetNextIdentHash(),
      reply_tunnel->GetNextTunnelID(),
      m_IsExploratory,
      &m_ExcludedPeers);
  m_ExcludedPeers.insert(router->GetIdentHash());
  m_CreationTime = xi2p::core::GetSecondsSinceEpoch();
  return msg;
}

std::shared_ptr<I2NPMessage> RequestedDestination::CreateRequestMessage(
    const IdentHash& floodfill) {
  auto msg = xi2p::core::CreateRouterInfoDatabaseLookupMsg(
      m_Destination,
      context.GetRouterInfo().GetIdentHash(),
      0,
      false,
      &m_ExcludedPeers);
  m_ExcludedPeers.insert(floodfill);
  m_CreationTime = xi2p::core::GetSecondsSinceEpoch();
  return msg;
}

void RequestedDestination::ClearExcludedPeers() {
  m_ExcludedPeers.clear();
}

void RequestedDestination::Success(
    std::shared_ptr<RouterInfo> r) {
  if (m_RequestComplete) {
    m_RequestComplete(r);
    m_RequestComplete = nullptr;
  }
}

void RequestedDestination::Fail() {
  if (m_RequestComplete) {
    m_RequestComplete(nullptr);
    m_RequestComplete = nullptr;
  }
}

void NetDbRequests::Start() {}  // TODO(unassigned): ???

void NetDbRequests::Stop() {
  m_RequestedDestinations.clear();
}

std::shared_ptr<RequestedDestination> NetDbRequests::CreateRequest(
    const IdentHash& destination,
    bool is_exploratory,
    RequestedDestination::RequestComplete request_complete) {
  // request RouterInfo directly
  auto dest =
    std::make_shared<RequestedDestination>(destination, is_exploratory);
  dest->SetRequestComplete(request_complete); {
    std::unique_lock<std::mutex> l(m_RequestedDestinationsMutex);
    if (!m_RequestedDestinations.insert(
          std::make_pair(
            destination,
            std::shared_ptr<RequestedDestination>(dest))).second)  // not inserted
      return nullptr;
  }
  return dest;
}

void NetDbRequests::RequestComplete(
    const IdentHash& ident,
    std::shared_ptr<RouterInfo> r) {
  auto it = m_RequestedDestinations.find(ident);
  if (it != m_RequestedDestinations.end()) {
    if (r)
      it->second->Success(r);
    else
      it->second->Fail();
    std::unique_lock<std::mutex> l(m_RequestedDestinationsMutex);
    m_RequestedDestinations.erase(it);
  }
}

std::shared_ptr<RequestedDestination> NetDbRequests::FindRequest(
    const IdentHash& ident) const {
  auto it = m_RequestedDestinations.find(ident);
  if (it != m_RequestedDestinations.end())
    return it->second;
  return nullptr;
}

void NetDbRequests::ManageRequests() {
  std::uint64_t ts = xi2p::core::GetSecondsSinceEpoch();
  std::unique_lock<std::mutex> l(m_RequestedDestinationsMutex);
  for (auto it = m_RequestedDestinations.begin();
      it != m_RequestedDestinations.end();) {
    auto& dest = it->second;
    bool is_complete = false;
    // request is worthless after 1 minute
    if (ts < dest->GetCreationTime() + 60) {
      // no response for 5 seconds
      if (ts > dest->GetCreationTime() + 5)  {
        auto count = dest->GetExcludedPeers().size();
        std::size_t attempts(7);
        if (!dest->IsExploratory() && count < attempts) {
          auto pool = xi2p::core::tunnels.GetExploratoryPool();
          auto outbound = pool->GetNextOutboundTunnel();
          auto inbound = pool->GetNextInboundTunnel();
          auto next_floodfill = netdb.GetClosestFloodfill(
              dest->GetDestination(),
              dest->GetExcludedPeers());
          if (next_floodfill && outbound && inbound) {
            outbound->SendTunnelDataMsg(
                next_floodfill->GetIdentHash(),
                0,
                dest->CreateRequestMessage(
                  next_floodfill,
                  inbound));
          } else {
            is_complete = true;
            if (!inbound)
              LOG(warning) << "NetDbRequests: no inbound tunnels";
            if (!outbound)
              LOG(warning) << "NetDbRequests: no outbound tunnels";
            if (!next_floodfill)
              LOG(warning) << "NetDbRequests: no more floodfills";
          }
        } else {
          if (!dest->IsExploratory())
            LOG(warning)
              << "NetDbRequests: " << dest->GetDestination().ToBase64()
              << " not found after " << attempts << " attempts";
          is_complete = true;
        }
      }
    } else {  // delete obsolete request
      is_complete = true;
    }
    if (is_complete)
      it = m_RequestedDestinations.erase(it);
    else
      it++;
  }
}

}  // namespace core
}  // namespace xi2p

