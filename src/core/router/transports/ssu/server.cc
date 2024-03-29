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

#include "core/router/transports/ssu/server.h"

#include <boost/bind.hpp>

#include <array>

#include "core/crypto/rand.h"

#include "core/router/context.h"
#include "core/router/net_db/impl.h"

#include "core/util/log.h"
#include "core/util/timestamp.h"

namespace xi2p {
namespace core {

SSUServer::SSUServer(
    boost::asio::io_service& service,
    std::size_t port)
    : m_Service(service),
      m_Endpoint(boost::asio::ip::udp::v4(), port),
      m_EndpointV6(boost::asio::ip::udp::v6(), port),
      m_Socket(m_Service, m_Endpoint),
      m_SocketV6(m_Service),
      m_IntroducersUpdateTimer(m_Service),
      m_PeerTestsCleanupTimer(m_Service),
      m_IsRunning(false) {
  m_Socket.set_option(boost::asio::socket_base::receive_buffer_size(65535));
  m_Socket.set_option(boost::asio::socket_base::send_buffer_size(65535));
  if (context.SupportsV6()) {
    m_SocketV6.open(boost::asio::ip::udp::v6());
    m_SocketV6.set_option(boost::asio::ip::v6_only(true));
    m_SocketV6.set_option(boost::asio::socket_base::receive_buffer_size(65535));
    m_SocketV6.set_option(boost::asio::socket_base::send_buffer_size(65535));
    m_SocketV6.bind(m_EndpointV6);
  }
}

SSUServer::~SSUServer() {}

void SSUServer::Start() {
  LOG(debug) << "SSUServer: starting";
  m_IsRunning = true;
  m_Service.post(
      std::bind(
          &SSUServer::Receive,
          this));
  if (context.SupportsV6()) {
    m_Service.post(
        std::bind(
            &SSUServer::ReceiveV6,
            this));
  }
  SchedulePeerTestsCleanupTimer();
  // wait for 30 seconds and decide if we need introducers
  ScheduleIntroducersUpdateTimer();
}

void SSUServer::Stop() {
  LOG(debug) << "SSUServer: stopping";
  DeleteAllSessions();
  m_IsRunning = false;
  m_Socket.close();
  m_SocketV6.close();
}

void SSUServer::AddRelay(
    std::uint32_t tag,
    const boost::asio::ip::udp::endpoint& relay) {
  LOG(debug) << "SSUServer: adding relay";
  m_Relays[tag] = relay;
}

std::shared_ptr<SSUSession> SSUServer::FindRelaySession(
    std::uint32_t tag) {
  LOG(debug) << "SSUServer: finding relay session";
  auto it = m_Relays.find(tag);
  if (it != m_Relays.end())
    return FindSession(it->second);
  return nullptr;
}

void SSUServer::Send(
    const std::uint8_t* buf,
    std::size_t len,
    const boost::asio::ip::udp::endpoint& to) {
  LOG(debug) << "SSUServer: sending data";
  if (to.protocol() == boost::asio::ip::udp::v4()) {
    try {
      m_Socket.send_to(boost::asio::buffer(buf, len), to);
    } catch (const std::exception& ex) {
      LOG(error) << "SSUServer: send error: '" << ex.what() << "'";
    }
  } else {
    try {
      m_SocketV6.send_to(boost::asio::buffer(buf, len), to);
    } catch (const std::exception& ex) {
      LOG(error) << "SSUServer: V6 send error: '" << ex.what() << "'";
    }
  }
}

void SSUServer::Receive() {
  LOG(debug) << "SSUServer: receiving data";
  RawSSUPacket* packet = new RawSSUPacket();  // always freed in ensuing handlers
  m_Socket.async_receive_from(
      boost::asio::buffer(
          packet->buf,
          SSUSize::MTUv4),
      packet->from,
      std::bind(
          &SSUServer::HandleReceivedFrom,
          this,
          std::placeholders::_1,
          std::placeholders::_2,
          packet));
}

void SSUServer::ReceiveV6() {
  LOG(debug) << "SSUServer: V6: receiving data";
  RawSSUPacket* packet = new RawSSUPacket();  // always freed in ensuing handlers
  m_SocketV6.async_receive_from(
      boost::asio::buffer(
          packet->buf,
          SSUSize::MTUv6),
      packet->from,
      std::bind(
          &SSUServer::HandleReceivedFromV6,
          this,
          std::placeholders::_1,
          std::placeholders::_2,
          packet));
}

// coverity[+free : arg-2]
void SSUServer::HandleReceivedFrom(
    const boost::system::error_code& ecode,
    std::size_t bytes_transferred,
    RawSSUPacket* packet) {
  LOG(debug) << "SSUServer: handling received data";
  if (!ecode) {
    packet->len = bytes_transferred;
    std::vector<RawSSUPacket *> packets;
    packets.push_back(packet);
    boost::system::error_code ec;
    std::size_t more_bytes = m_Socket.available(ec);
    // TODO(anonimal): but what about 0 length HolePunch?
    //   Current handler's null length check done in vain?
    while (more_bytes && packets.size() < 25) {
      packet = new RawSSUPacket();
      packet->len = m_Socket.receive_from(
          boost::asio::buffer(
              packet->buf,
              SSUSize::MTUv4),
          packet->from);
      packets.push_back(packet);
      more_bytes = m_Socket.available();
    }
    // packet is freed in ensuing handler
    m_Service.post(
        std::bind(
            &SSUServer::HandleReceivedPackets,
            this,
            packets));
    Receive();
  } else {
    LOG(error) << "SSUServer: receive error: " << ecode.message();
    delete packet;  // free packet, now
  }
}

// coverity[+free : arg-2]
void SSUServer::HandleReceivedFromV6(
    const boost::system::error_code& ecode,
    std::size_t bytes_transferred,
    RawSSUPacket* packet) {
  LOG(debug) << "SSUServer: V6: handling received data";
  if (!ecode) {
    packet->len = bytes_transferred;
    std::vector<RawSSUPacket *> packets;
    packets.push_back(packet);
    std::size_t more_bytes = m_SocketV6.available();
    while (more_bytes && packets.size() < 25) {
      packet = new RawSSUPacket();
      packet->len = m_SocketV6.receive_from(
          boost::asio::buffer(
              packet->buf,
              SSUSize::MTUv6),
          packet->from);
      packets.push_back(packet);
      more_bytes = m_SocketV6.available();
    }
    // packet is freed in ensuing handler
    m_Service.post(
        std::bind(
            &SSUServer::HandleReceivedPackets,
            this,
            packets));
    ReceiveV6();
  } else {
    LOG(error) << "SSUServer: V6 receive error: " << ecode.message();
    delete packet;  // free packet, now
  }
}

void SSUServer::HandleReceivedPackets(
    std::vector<RawSSUPacket *> packets) {
  LOG(debug) << "SSUServer: handling received packets";
  std::shared_ptr<SSUSession> session;
  for (auto packet : packets) {
    auto pkt = packet;
    try {
      // we received pkt for other session than previous
      if (!session || session->GetRemoteEndpoint() != pkt->from) {
        if (session)
          session->FlushData();
        auto session_it = m_Sessions.find(pkt->from);
        if (session_it != m_Sessions.end())
          {
            session = session_it->second;
          }
        else
          {
          session = std::make_shared<SSUSession>(*this, pkt->from);
          session->WaitForConnect(); {
            std::unique_lock<std::mutex> l(m_SessionsMutex);
            // TODO(anonimal): assuming we get this far with 0 length HolePunch,
            //   why would we add a session with Charlie *before* sending a SessionRequest?
            m_Sessions[pkt->from] = session;
          }
          LOG(debug)
            << "SSUServer: created new SSU session from "
            << session->GetRemoteEndpoint();
        }
      }
      session->ProcessNextMessage(pkt->buf, pkt->len, pkt->from);
    } catch (const std::exception& ex) {
      LOG(error) << "SSUServer: " << __func__ << ": '" << ex.what() << "'";
      if (session)
        session->FlushData();
      session = nullptr;
    }
    delete pkt;  // free received packet
  }
  if (session)
    session->FlushData();
}

std::shared_ptr<SSUSession> SSUServer::FindSession(
    std::shared_ptr<const xi2p::core::RouterInfo> router) const {
  LOG(debug) << "SSUServer: finding session from RI";
  if (!router)
    return nullptr;
  auto address = router->GetSSUAddress();  // v4 only
  if (!address)
    return nullptr;
  auto session =
    FindSession(boost::asio::ip::udp::endpoint(address->host, address->port));
  if (session || !context.SupportsV6())
    return session;
  // try v6
  address = router->GetSSUAddress(true);
  if (!address)
    return nullptr;
  return FindSession(boost::asio::ip::udp::endpoint(address->host, address->port));
}

std::shared_ptr<SSUSession> SSUServer::FindSession(
    const boost::asio::ip::udp::endpoint& ep) const {
  LOG(debug) << "SSUServer: finding session from endpoint";
  auto it = m_Sessions.find(ep);
  if (it != m_Sessions.end())
    return it->second;
  else
    return nullptr;
}

std::shared_ptr<SSUSession> SSUServer::GetSession(
    std::shared_ptr<const xi2p::core::RouterInfo> router,
    bool peer_test) {
  LOG(debug) << "SSUServer: getting session";
  std::shared_ptr<SSUSession> session;
  if (router) {
    auto address = router->GetSSUAddress(context.SupportsV6());
    if (address) {
      boost::asio::ip::udp::endpoint remote_endpoint(
          address->host,
          address->port);
      auto it = m_Sessions.find(remote_endpoint);
      if (it != m_Sessions.end()) {
        session = it->second;
      } else {
        // otherwise create new session
        session = std::make_shared<SSUSession>(
            *this,
            remote_endpoint,
            router,
            peer_test); {
          std::unique_lock<std::mutex> l(m_SessionsMutex);
          m_Sessions[remote_endpoint] = session;
        }
        session->SetRemoteIdentHashAbbreviation();
        if (!router->UsesIntroducer()) {
          // connect directly
          LOG(debug)
            << "SSUServer: creating new session to"
            << session->GetFormattedSessionInfo();
          session->Connect();
        } else {
          // connect through introducer
          auto num_introducers = address->introducers.size();
          if (num_introducers > 0) {
            std::shared_ptr<SSUSession> introducer_session;
            const xi2p::core::RouterInfo::Introducer* introducer = nullptr;
            // we might have a session to introducer already
            for (std::size_t i = 0; i < num_introducers; i++) {
              introducer = &(address->introducers[i]);
              it = m_Sessions.find(
                       boost::asio::ip::udp::endpoint(
                           introducer->host,
                           introducer->port));
              if (it != m_Sessions.end()) {
                introducer_session = it->second;
                break;
              }
            }
            if (introducer_session) {  // session found
              LOG(debug)
                << "SSUServer: " << introducer->host << ":" << introducer->port
                << "session to introducer already exists";
            } else {  // create new
              LOG(debug) << "SSUServer: creating new session to introducer";
              introducer = &(address->introducers[0]);  // TODO(unassigned): ???
              boost::asio::ip::udp::endpoint introducerEndpoint(
                  introducer->host,
                  introducer->port);
              introducer_session = std::make_shared<SSUSession>(
                  *this,
                  introducerEndpoint,
                  router);
              std::unique_lock<std::mutex> l(m_SessionsMutex);
              m_Sessions[introducerEndpoint] = introducer_session;
            }
            // introduce
            LOG(debug)
              << "SSUServer: introducing new SSU session to "
              << "[" << router->GetIdentHashAbbreviation() << "] through introducer "
              << "[" << introducer_session->GetRemoteIdentHashAbbreviation() << "] "
              << introducer->host << ":" << introducer->port;
            session->WaitForIntroduction();
            // if we are unreachable
            if (context.GetRouterInfo().UsesIntroducer()) {
              std::array<std::uint8_t, 1> buf {{}};
              Send(buf.data(), 0, remote_endpoint);  // send HolePunch
            }
            introducer_session->Introduce(introducer->tag, introducer->key);
          } else {
            LOG(warning)
              << "SSUServer: can't connect to unreachable router."
              << "No introducers presented";
            std::unique_lock<std::mutex> l(m_SessionsMutex);
            m_Sessions.erase(remote_endpoint);
            session.reset();
          }
        }
      }
    } else {
      LOG(warning)
        << "SSUServer: router "
        << "[" << router->GetIdentHashAbbreviation() << "] "
        << "doesn't have SSU address";
    }
  }
  return session;
}

void SSUServer::DeleteSession(
    std::shared_ptr<SSUSession> session) {
  LOG(debug) << "SSUServer: deleting session";
  if (session) {
    session->Close();
    std::unique_lock<std::mutex> l(m_SessionsMutex);
    m_Sessions.erase(session->GetRemoteEndpoint());
  }
}

void SSUServer::DeleteAllSessions() {
  LOG(debug) << "SSUServer: deleting all sessions";
  std::unique_lock<std::mutex> l(m_SessionsMutex);
  for (auto it : m_Sessions)
    it.second->Close();
  m_Sessions.clear();
}

template<typename Filter>
std::shared_ptr<SSUSession> SSUServer::GetRandomSession(
    Filter filter) {
  LOG(debug) << "SSUServer: getting random session";
  std::vector<std::shared_ptr<SSUSession>> filtered_sessions;
  for (auto session : m_Sessions)
    if (filter (session.second))
      filtered_sessions.push_back(session.second);
  if (filtered_sessions.size() > 0) {
    std::size_t s = filtered_sessions.size();
    std::size_t ind = xi2p::core::RandInRange32(0, s - 1);
    return filtered_sessions[ind];
  }
  return nullptr;
}

std::shared_ptr<SSUSession> SSUServer::GetRandomEstablishedSession(
    std::shared_ptr<const SSUSession> excluded) {
  LOG(debug) << "SSUServer: getting random established session";
  return GetRandomSession(
      [excluded](std::shared_ptr<SSUSession> session)->bool {
      return session->GetState() == SessionState::Established &&
      !session->IsV6() &&
      session != excluded; });
}

std::set<SSUSession *> SSUServer::FindIntroducers(
    std::size_t max_num_introducers) {
  LOG(debug) << "SSUServer: finding introducers";
  std::uint32_t ts = xi2p::core::GetSecondsSinceEpoch();
  std::set<SSUSession *> ret;
  for (std::size_t i = 0; i < max_num_introducers; i++) {
    auto session =
      GetRandomSession(
          [&ret, ts](std::shared_ptr<SSUSession> session)->bool {
          return session->GetRelayTag() &&
          !ret.count(session.get()) &&
          session->GetState() == SessionState::Established &&
          ts < session->GetCreationTime()
               + SSUDuration::ToIntroducerSessionDuration; });
    if (session) {
      ret.insert(session.get());
      break;
    }
  }
  return ret;
}

void SSUServer::ScheduleIntroducersUpdateTimer() {
  LOG(debug) << "SSUServer: scheduling introducers update timer";
  m_IntroducersUpdateTimer.expires_from_now(
      boost::posix_time::seconds(
          SSUDuration::KeepAliveInterval));
  m_IntroducersUpdateTimer.async_wait(
      std::bind(
          &SSUServer::HandleIntroducersUpdateTimer,
          this,
          std::placeholders::_1));
}

void SSUServer::HandleIntroducersUpdateTimer(
    const boost::system::error_code& ecode) {
  LOG(debug) << "SSUServer: handling introducers update timer";
  if (ecode != boost::asio::error::operation_aborted) {
    // timeout expired
    if (context.GetState() == RouterState::Testing) {
      // we still don't know if we need introducers
      ScheduleIntroducersUpdateTimer();
      return;
    }
    if (context.GetState () == RouterState::OK)
      return;  // we don't need introducers anymore
    // we are firewalled
    if (!context.IsUnreachable()) context.SetUnreachable();
    std::list<boost::asio::ip::udp::endpoint> new_list;
    std::size_t num_introducers = 0;
    std::uint32_t ts = xi2p::core::GetSecondsSinceEpoch();  // Timestamp
    for (auto introducer : m_Introducers) {
      auto session = FindSession(introducer);
      if (session &&
          ts < session->GetCreationTime()
             + SSUDuration::ToIntroducerSessionDuration) {
        session->SendKeepAlive();
        new_list.push_back(introducer);
        num_introducers++;
      } else {
        context.RemoveIntroducer(introducer);
      }
    }
    auto max_introducers = SSUSize::MaxIntroducers;
    if (num_introducers < max_introducers) {
      // create new
      auto introducers = FindIntroducers(max_introducers);
      if (introducers.size() > 0) {
        for (auto it : introducers) {
          auto router = it->GetRemoteRouter();
          if (router &&
              context.AddIntroducer(*router, it->GetRelayTag())) {
            new_list.push_back(it->GetRemoteEndpoint());
            if (new_list.size() >= max_introducers)
              break;
          }
        }
      }
    }
    m_Introducers = new_list;
    if (m_Introducers.empty()) {
      auto introducer = xi2p::core::netdb.GetRandomIntroducer();
      if (introducer)
        GetSession(introducer);
    }
    ScheduleIntroducersUpdateTimer();
  }
}

void SSUServer::NewPeerTest(
    std::uint32_t nonce,
    PeerTestParticipant role,
    std::shared_ptr<SSUSession> session) {
  LOG(debug) << "SSUServer: new peer test";
  m_PeerTests[nonce] = {
    xi2p::core::GetMillisecondsSinceEpoch(),
    role,
    session
  };
}

PeerTestParticipant SSUServer::GetPeerTestParticipant(
    std::uint32_t nonce) {
  LOG(debug) << "SSUServer: getting PeerTest participant";
  auto it = m_PeerTests.find(nonce);
  if (it != m_PeerTests.end())
    return it->second.role;
  else
    return PeerTestParticipant::Unknown;
}

std::shared_ptr<SSUSession> SSUServer::GetPeerTestSession(
    std::uint32_t nonce) {
  LOG(debug) << "SSUServer: getting PeerTest session";
  auto it = m_PeerTests.find(nonce);
  if (it != m_PeerTests.end())
    return it->second.session;
  else
    return nullptr;
}

void SSUServer::UpdatePeerTest(
    std::uint32_t nonce,
    PeerTestParticipant role) {
  LOG(debug) << "SSUServer: updating PeerTest";
  auto it = m_PeerTests.find(nonce);
  if (it != m_PeerTests.end())
    it->second.role = role;
}

void SSUServer::RemovePeerTest(
    std::uint32_t nonce) {
  LOG(debug) << "SSUServer: removing PeerTest";
  m_PeerTests.erase(nonce);
}

void SSUServer::SchedulePeerTestsCleanupTimer() {
  LOG(debug) << "SSUServer: scheduling PeerTests cleanup timer";
  m_PeerTestsCleanupTimer.expires_from_now(
      boost::posix_time::seconds(
          SSUDuration::PeerTestTimeout));
  m_PeerTestsCleanupTimer.async_wait(
      std::bind(
          &SSUServer::HandlePeerTestsCleanupTimer,
          this,
          std::placeholders::_1));
}

void SSUServer::HandlePeerTestsCleanupTimer(
    const boost::system::error_code& ecode) {
  LOG(debug) << "SSUServer: handling PeerTests cleanup timer";
  if (ecode != boost::asio::error::operation_aborted) {
    std::size_t num_deleted = 0;
    std::uint64_t ts = xi2p::core::GetMillisecondsSinceEpoch();
    for (auto it = m_PeerTests.begin(); it != m_PeerTests.end();) {
      if (ts > it->second.creationTime
               + SSUDuration::PeerTestTimeout
               * 1000LL) {
        num_deleted++;
        it = m_PeerTests.erase(it);
      } else {
        it++;
      }
    }
    if (num_deleted > 0)
      LOG(debug)
        << "SSUServer: " << num_deleted << " peer tests have been expired";
    SchedulePeerTestsCleanupTimer();
  }
}

}  // namespace core
}  // namespace xi2p

