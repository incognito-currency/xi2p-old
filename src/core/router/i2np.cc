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

#include "core/router/i2np.h"

#include <string.h>

#include <atomic>

#include "core/crypto/elgamal.h"
#include "core/crypto/rand.h"
#include "core/crypto/util/compression.h"

#include "core/router/context.h"
#include "core/router/garlic.h"
#include "core/router/net_db/impl.h"
#include "core/router/transports/impl.h"
#include "core/router/tunnel/impl.h"

#include "core/util/log.h"
#include "core/util/timestamp.h"

#include "version.h"  // NOLINT(build/include)

namespace xi2p {
namespace core {

// TODO(anonimal): bytestream refactor

std::unique_ptr<I2NPMessage> NewI2NPMessage() {
  return std::make_unique<I2NPMessageBuffer<I2NP_MAX_MESSAGE_SIZE>>();
}

std::unique_ptr<I2NPMessage> NewI2NPShortMessage() {
  return std::make_unique<I2NPMessageBuffer<I2NP_MAX_SHORT_MESSAGE_SIZE>>();
}

std::unique_ptr<I2NPMessage> NewI2NPMessage(
    std::size_t len) {
  return (len < I2NP_MAX_SHORT_MESSAGE_SIZE / 2) ?
      NewI2NPShortMessage() :
      NewI2NPMessage();
}

std::shared_ptr<I2NPMessage> ToSharedI2NPMessage(
    std::unique_ptr<I2NPMessage> msg) {
  return std::shared_ptr<I2NPMessage>(std::move(msg));
}

void I2NPMessage::FillI2NPMessageHeader(
    I2NPMessageType msg_type,
    std::uint32_t reply_msg_ID) {
  SetTypeID(msg_type);
  if (reply_msg_ID)  // for tunnel creation
    SetMsgID(reply_msg_ID);
  else
    SetMsgID(xi2p::core::Rand<std::uint32_t>());
  SetExpiration(
      xi2p::core::GetMillisecondsSinceEpoch() +
      I2NP_HEADER_DEFAULT_EXPIRATION_TIME);
  UpdateSize();
  UpdateChks();
}

void I2NPMessage::RenewI2NPMessageHeader() {
  SetMsgID(xi2p::core::Rand<std::uint32_t>());
  SetExpiration(
      xi2p::core::GetMillisecondsSinceEpoch() +
      I2NP_HEADER_DEFAULT_EXPIRATION_TIME);
}

std::unique_ptr<I2NPMessage> CreateI2NPMessage(
    I2NPMessageType msg_type,
    const std::uint8_t* buf,
    int len,
    std::uint32_t reply_msg_ID) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPMessage(len);
  if (msg->len + len < msg->max_len) {
    memcpy(msg->GetPayload(), buf, len);
    msg->len += len;
  } else {
    LOG(error) << "I2NPMessage: message length " << len << " exceeds max length";
    // TODO(anonimal): ...then don't return the message...
  }
  msg->FillI2NPMessageHeader(msg_type, reply_msg_ID);
  return msg;
}

// TDDO(anonimal):
//   Theoretically, and despite the signed type, len passed has the potential to be 64k (value given in 2 bytes 0xffff)
//   but, because of I2NP fragmentation + alignment, the max should be no more than 62708 bytes (61.2k for tunnel delivery mode).
//
//   Now, msg->max_len is at a minimum 4k ("short" message) or at most 32k depending on message type.
//   The below sanity test ensures that passed len is < (4k || 32k) *BUT* that still leaves potential for copying over
//   ((msg->max_len - 1) - len) amount of memory (memory after payload, potentially uninitialized) into the message buffer
//   which is then sent through the tunnel.  TODO(anonimal): ensure callers never send incorrect length to begin with.
//
//   Note: NTCP limits message size to to 16378 bytes and SSU limits (should) to 32k.
std::shared_ptr<I2NPMessage> CreateI2NPMessage(
    const std::uint8_t* buf,
    int len,  // TODO(anonimal): uint16_t, and caller should ensure no overflow
    std::shared_ptr<xi2p::core::InboundTunnel> from) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPMessage();
  if (msg->offset + len < msg->max_len) {
    memcpy(msg->GetBuffer(), buf, len);
    msg->len = msg->offset + len;
    msg->from = from;
  } else {
    LOG(error) << "I2NPMessage: message length " << len << " exceeds max length";
    // TODO(anonimal): now why would we want to send an empty message?!...
  }
  return ToSharedI2NPMessage(std::move(msg));
}

// TODO(anonimal): bytestream refactor
std::shared_ptr<I2NPMessage> CreateDeliveryStatusMsg(
    std::uint32_t msg_ID) {
  std::unique_ptr<I2NPMessage> m = NewI2NPShortMessage();
  std::uint8_t* buf = m->GetPayload();
  if (msg_ID) {
      core::OutputByteStream::Write<std::uint32_t>(
          buf + DELIVERY_STATUS_MSGID_OFFSET, msg_ID);
      core::OutputByteStream::Write<std::uint64_t>(
          buf + DELIVERY_STATUS_TIMESTAMP_OFFSET,
          core::GetMillisecondsSinceEpoch());
  } else {  // for SSU establishment
    core::OutputByteStream::Write<std::uint32_t>(
        buf + DELIVERY_STATUS_MSGID_OFFSET,
        xi2p::core::Rand<std::uint32_t>());
    core::OutputByteStream::Write<std::uint64_t>(
        buf + DELIVERY_STATUS_TIMESTAMP_OFFSET,
        I2P_NETWORK_ID);
  }
  m->len += DELIVERY_STATUS_SIZE;
  m->FillI2NPMessageHeader(I2NPDeliveryStatus);
  return ToSharedI2NPMessage(std::move(m));
}

std::shared_ptr<I2NPMessage> CreateRouterInfoDatabaseLookupMsg(
    const std::uint8_t* key,
    const std::uint8_t* from,
    std::uint32_t reply_tunnel_ID,
    bool exploratory,
    std::set<xi2p::core::IdentHash>* excluded_peers) {
  auto m = ToSharedI2NPMessage(
      excluded_peers ? NewI2NPMessage() : NewI2NPShortMessage());
  std::uint8_t* buf = m->GetPayload();
  memcpy(buf, key, 32);  // key
  buf += 32;
  memcpy(buf, from, 32);  // from
  buf += 32;
  std::uint8_t flag = exploratory ? DATABASE_LOOKUP_TYPE_EXPLORATORY_LOOKUP :
    DATABASE_LOOKUP_TYPE_ROUTERINFO_LOOKUP;
  if (reply_tunnel_ID) {
    *buf = flag | DATABASE_LOOKUP_DELIVERY_FLAG;  // set delivery flag
    core::OutputByteStream::Write<std::uint32_t>(buf + 1, reply_tunnel_ID);
    buf += 5;
  } else {
    *buf = flag;  // flag
    buf++;
  }
  if (excluded_peers) {
    int cnt = excluded_peers->size();
    core::OutputByteStream::Write<std::uint16_t>(buf, cnt);
    buf += 2;
    for (auto& it : *excluded_peers) {
      memcpy(buf, it, 32);
      buf += 32;
    }
  } else {
    // nothing to exclude
    core::OutputByteStream::Write<std::uint16_t>(buf, 0, false);
    buf += 2;
  }
  m->len += (buf - m->GetPayload());
  m->FillI2NPMessageHeader(I2NPDatabaseLookup);
  return m;
}

// TODO(anonimal: bytestream refactor
// TODO(anonimal): pass reference to structure, not FIVE arguments!
std::shared_ptr<I2NPMessage> CreateLeaseSetDatabaseLookupMsg(
    const xi2p::core::IdentHash& dest,
    const std::set<xi2p::core::IdentHash>& excluded_floodfills,
    const xi2p::core::InboundTunnel* reply_tunnel,
    const std::uint8_t* reply_key,
    const std::uint8_t* reply_tag) {
  int cnt = excluded_floodfills.size();
  auto m = ToSharedI2NPMessage(
      cnt > 0 ? NewI2NPMessage() : NewI2NPShortMessage());
  std::uint8_t* buf = m->GetPayload();
  memcpy(buf, dest, 32);  // key
  buf += 32;
  memcpy(buf, reply_tunnel->GetNextIdentHash(), 32);  // reply tunnel GW
  buf += 32;
  *buf = DATABASE_LOOKUP_DELIVERY_FLAG |
         DATABASE_LOOKUP_ENCYPTION_FLAG |
         DATABASE_LOOKUP_TYPE_LEASESET_LOOKUP;  // flags
  core::OutputByteStream::Write<std::uint32_t>(
      buf + 1, reply_tunnel->GetNextTunnelID());  // reply tunnel ID
  buf += 5;
  // excluded
  core::OutputByteStream::Write<std::uint16_t>(buf, cnt);
  buf += 2;
  if (cnt > 0) {
    for (auto& it : excluded_floodfills) {
      memcpy(buf, it, 32);
      buf += 32;
    }
  }
  // encryption
  memcpy(buf, reply_key, 32);
  buf[32] = 1;  // 1 tag
  memcpy(buf + 33, reply_tag, 32);
  buf += 65;
  m->len += (buf - m->GetPayload());
  m->FillI2NPMessageHeader(I2NPDatabaseLookup);
  return m;
}

std::shared_ptr<I2NPMessage> CreateDatabaseSearchReply(
    const xi2p::core::IdentHash& ident,
    std::vector<xi2p::core::IdentHash> routers) {
  auto m =  ToSharedI2NPMessage(NewI2NPShortMessage());
  std::uint8_t* buf = m->GetPayload();
  std::size_t len = 0;
  memcpy(buf, ident, 32);
  len += 32;
  buf[len] = routers.size();
  len++;
  for (auto it : routers) {
    memcpy(buf + len, it, 32);
    len += 32;
  }
  memcpy(buf + len, context.GetRouterInfo().GetIdentHash(), 32);
  len += 32;
  m->len += len;
  m->FillI2NPMessageHeader(I2NPDatabaseSearchReply);
  return m;
}

// TODO(anonimal): bytestream refactor
std::shared_ptr<I2NPMessage> CreateDatabaseStoreMsg(
    std::shared_ptr<const xi2p::core::RouterInfo> router,
    std::uint32_t reply_token) {
  if (!router)  // we send own RouterInfo
    router = context.GetSharedRouterInfo();
  auto m = ToSharedI2NPMessage(NewI2NPShortMessage());
  try {
    std::uint8_t* payload = m->GetPayload();
    memcpy(payload + DATABASE_STORE_KEY_OFFSET, router->GetIdentHash(), 32);
    payload[DATABASE_STORE_TYPE_OFFSET] = 0;  // RouterInfo
    core::OutputByteStream::Write<std::uint32_t>(
        payload + DATABASE_STORE_REPLY_TOKEN_OFFSET, reply_token);
    std::uint8_t* buf = payload + DATABASE_STORE_HEADER_SIZE;
    if (reply_token) {
      memset(buf, 0, 4);  // zero tunnel_ID means direct reply
      buf += 4;
      memcpy(buf, router->GetIdentHash(), 32);
      buf += 32;
    }
    xi2p::core::Gzip compressor;
    compressor.Put(router->GetBuffer(), router->GetBufferLen());
    auto size = compressor.MaxRetrievable();
    core::OutputByteStream::Write<std::uint16_t>(buf, size);  // size
    buf += 2;
    m->len += (buf - payload);  // payload size
    if (m->len + size > m->max_len) {
      LOG(debug)
        << "I2NPMessage: DatabaseStore message size is not enough for "
        << m->len + size;
      auto new_msg =  ToSharedI2NPMessage(NewI2NPMessage());
      *new_msg = *m;
      m = new_msg;
      buf = m->buf + m->len;
    }
    compressor.Get(buf, size);
    m->len += size;
    m->FillI2NPMessageHeader(I2NPDatabaseStore);
  } catch (...) {
    xi2p::core::Exception ex;
    ex.Dispatch(__func__);
    // TODO(anonimal): review if we need to safely break control, ensure exception handling by callers
  }
  return m;
}

// TODO(anonimal): bytestream refactor
std::shared_ptr<I2NPMessage> CreateDatabaseStoreMsg(
    std::shared_ptr<const xi2p::core::LeaseSet> lease_set,
    std::uint32_t reply_token) {
  if (!lease_set)
    return nullptr;
  auto m = ToSharedI2NPMessage(NewI2NPShortMessage());
  std::uint8_t* payload = m->GetPayload();
  memcpy(payload + DATABASE_STORE_KEY_OFFSET, lease_set->GetIdentHash(), 32);
  payload[DATABASE_STORE_TYPE_OFFSET] = 1;  // LeaseSet
  core::OutputByteStream::Write<std::uint32_t>(payload + DATABASE_STORE_REPLY_TOKEN_OFFSET, reply_token);
  std::size_t size = DATABASE_STORE_HEADER_SIZE;
  if (reply_token) {
    auto leases = lease_set->GetNonExpiredLeases();
    if (leases.size() > 0) {
      core::OutputByteStream::Write<std::uint32_t>(payload + size, leases[0].tunnel_ID);
      size += 4;  // reply tunnel_ID
      memcpy(payload + size, leases[0].tunnel_gateway, 32);
      size += 32;  // reply tunnel gateway
    } else {
      core::OutputByteStream::Write<std::uint32_t>(payload + DATABASE_STORE_REPLY_TOKEN_OFFSET, 0);
    }
  }
  memcpy(payload + size, lease_set->GetBuffer(), lease_set->GetBufferLen());
  size += lease_set->GetBufferLen();
  m->len += size;
  m->FillI2NPMessageHeader(I2NPDatabaseStore);
  return m;
}

bool HandleBuildRequestRecords(
    int num,
    std::uint8_t* records,
    std::uint8_t* clear_text) {
  // TODO(anonimal): this try block should be handled entirely by caller
  try {
    /**
     * When a hop receives a tunnel build message, it looks through the records
     * contained within it for one starting with their own identity hash
     * (trimmed to 16 bytes). It then decrypts the ElGamal block from that record
     * and retrieves the protected cleartext.
     *
     *   bytes   0-15 : First 16 bytes of the SHA-256 of
     *                  the current hop's router identity
     *   bytes 16-527 : ElGamal-2048 encrypted request record
     *
     *   Total: 528 byte record
     *
     * TODO(unassigned): implement bloom filter, see #58 + #77
     */
    for (int i = 0; i < num; i++) {
      std::uint8_t* record = records + i * TUNNEL_BUILD_RECORD_SIZE;
      // Test if current hop's router identity is ours
      if (!memcmp(
              record + BUILD_REQUEST_RECORD_TO_PEER_OFFSET,
              (const std::uint8_t *)context.GetRouterInfo().GetIdentHash(),
              16)) {
        LOG(debug) << "I2NPMessage: record " << i << " is ours";
        // Get session key from encrypted block
        xi2p::core::ElGamalDecrypt(
            context.GetEncryptionPrivateKey(),
            record + BUILD_REQUEST_RECORD_ENCRYPTED_OFFSET,
            clear_text);
        /**
         * After the current hop reads their record, we replace it with
         * a reply record stating whether or not they agree to participate
         * in the tunnel, and if we do not, we classify a reason for rejection
         * This is simply a 1 byte value, with 0x0 meaning that we agree to
         * participate in the tunnel, and higher values meaning
         * higher levels of rejection.
         */
        if (context.AcceptsTunnels() &&
            xi2p::core::tunnels.GetTransitTunnels().size() <=
            MAX_NUM_TRANSIT_TUNNELS &&
            !xi2p::core::transports.IsBandwidthExceeded()) {
          xi2p::core::TransitTunnel* transit_tunnel =
            xi2p::core::CreateTransitTunnel(
                core::InputByteStream::Read<std::uint32_t>(clear_text + BUILD_REQUEST_RECORD_RECEIVE_TUNNEL_OFFSET),
                clear_text + BUILD_REQUEST_RECORD_NEXT_IDENT_OFFSET,
                core::InputByteStream::Read<std::uint32_t>(clear_text + BUILD_REQUEST_RECORD_NEXT_TUNNEL_OFFSET),
                clear_text + BUILD_REQUEST_RECORD_LAYER_KEY_OFFSET,
                clear_text + BUILD_REQUEST_RECORD_IV_KEY_OFFSET,
                clear_text[BUILD_REQUEST_RECORD_FLAG_OFFSET] & 0x80,
                clear_text[BUILD_REQUEST_RECORD_FLAG_OFFSET] & 0x40);
          xi2p::core::tunnels.AddTransitTunnel(transit_tunnel);
          record[BUILD_RESPONSE_RECORD_RET_OFFSET] = 0;
        } else {
          /**
           * The following rejection codes are defined:
           *
           * TUNNEL_REJECT_PROBABALISTIC_REJECT = 10
           * TUNNEL_REJECT_TRANSIENT_OVERLOAD = 20
           * TUNNEL_REJECT_BANDWIDTH = 30
           * TUNNEL_REJECT_CRIT = 50
           *
           * To hide other causes from peers (such as router shutdown),
           * the current implementation uses TUNNEL_REJECT_BANDWIDTH
           * for *all* rejections.
           *
           * TODO(unassigned): review use-case for implementing other rejections
           */
          record[BUILD_RESPONSE_RECORD_RET_OFFSET] = 30;
        }
        /**
         * The reply is encrypted using the AES session key delivered to it in
         * the encrypted block, padded with 495 bytes of random data to reach
         * the full record size. The padding is placed before the status byte:
         *
         *  AES-256-CBC(SHA-256(padding + status) + padding + status, key, IV)
         *
         *   bytes   0-31 : SHA-256 of bytes 32-527
         *   bytes 32-526 : Padding (random generated)
         *   byte 527     : Status byte / reply value
         *
         *   Total: 528 byte record
         */
        // Fill random padding
        xi2p::core::RandBytes(
            record + BUILD_RESPONSE_RECORD_RANDPAD_OFFSET,
            BUILD_RESPONSE_RECORD_RANDPAD_SIZE);
        // Get SHA256 of complete record
        xi2p::core::SHA256().CalculateDigest(
            record + BUILD_RESPONSE_RECORD_SHA256HASH_OFFSET,
            record + BUILD_RESPONSE_RECORD_RANDPAD_OFFSET,
            BUILD_RESPONSE_RECORD_RANDPAD_SIZE + 1);  // + 1 byte for status/reply
        /**
         * After deciding whether they will agree to participate
         * in the tunnel or not, they replace the record that had contained
         * the request with an encrypted reply block. All other records are
         * AES-256 encrypted with the included reply key and IV.
         * Each is AES/CBC encrypted separately with the same reply key
         * and reply IV. The CBC mode is not continued (chained) across records.
         */
        xi2p::core::CBCEncryption encryption;
        for (int j = 0; j < num; j++) {
          encryption.SetKey(clear_text + BUILD_REQUEST_RECORD_REPLY_KEY_OFFSET);
          encryption.SetIV(clear_text + BUILD_REQUEST_RECORD_REPLY_IV_OFFSET);
          std::uint8_t* reply = records + j * TUNNEL_BUILD_RECORD_SIZE;
          encryption.Encrypt(reply, TUNNEL_BUILD_RECORD_SIZE, reply);
        }
        return true;
      }
    }
  } catch (...) {
    core::Exception ex;
    ex.Dispatch(__func__);
    // TODO(anonimal): review if we need to safely break control, ensure exception handling by callers
    throw;
  }
  return false;
}

void HandleVariableTunnelBuildMsg(
    std::uint32_t reply_msg_ID,
    std::uint8_t* buf,
    std::size_t len) {
  int num = buf[0];
  LOG(debug) << "I2NPMessage: VariableTunnelBuild " << num << " records";
  auto tunnel = xi2p::core::tunnels.GetPendingInboundTunnel(reply_msg_ID);
  if (tunnel) {
    // endpoint of inbound tunnel
    LOG(debug)
      << "I2NPMessage: VariableTunnelBuild reply for tunnel "
      << tunnel->GetTunnelID();
    if (tunnel->HandleTunnelBuildResponse(buf, len)) {
      LOG(debug)
        << "I2NPMessage: inbound tunnel "
        << tunnel->GetTunnelID() << " has been created";
      tunnel->SetState(xi2p::core::e_TunnelStateEstablished);
      xi2p::core::tunnels.AddInboundTunnel(tunnel);
    } else {
      LOG(debug)
        << "I2NPMessage: inbound tunnel "
        << tunnel->GetTunnelID() << " has been declined";
      tunnel->SetState(xi2p::core::e_TunnelStateBuildFailed);
    }
  } else {
    std::uint8_t clear_text[BUILD_REQUEST_RECORD_CLEAR_TEXT_SIZE] = {};
    if (HandleBuildRequestRecords(num, buf + 1, clear_text)) {
      // we are endpoint of outboud tunnel
      if (clear_text[BUILD_REQUEST_RECORD_FLAG_OFFSET] & 0x40) {
        // So, we send it to reply tunnel
        xi2p::core::transports.SendMessage(
            clear_text + BUILD_REQUEST_RECORD_NEXT_IDENT_OFFSET,
            ToSharedI2NPMessage(
                CreateTunnelGatewayMsg(
                    core::InputByteStream::Read<std::uint32_t>(
                        clear_text + BUILD_REQUEST_RECORD_NEXT_TUNNEL_OFFSET),
                    I2NPVariableTunnelBuildReply,
                    buf,
                    len,
                    core::InputByteStream::Read<std::uint32_t>(
                        clear_text + BUILD_REQUEST_RECORD_SEND_MSG_ID_OFFSET))));
      } else {
        xi2p::core::transports.SendMessage(
            clear_text + BUILD_REQUEST_RECORD_NEXT_IDENT_OFFSET,
            ToSharedI2NPMessage(
                CreateI2NPMessage(
                    I2NPVariableTunnelBuild,
                    buf,
                    len,
                    core::InputByteStream::Read<std::uint32_t>(
                        clear_text + BUILD_REQUEST_RECORD_SEND_MSG_ID_OFFSET))));
      }
    }
  }
}

void HandleTunnelBuildMsg(
    std::uint8_t* buf,
    std::size_t len) {
  std::uint8_t clear_text[BUILD_REQUEST_RECORD_CLEAR_TEXT_SIZE];
  if (HandleBuildRequestRecords(NUM_TUNNEL_BUILD_RECORDS, buf, clear_text)) {
    // we are endpoint of outbound tunnel
    if (clear_text[BUILD_REQUEST_RECORD_FLAG_OFFSET] & 0x40) {
      // so we send it to reply tunnel
      xi2p::core::transports.SendMessage(
          clear_text + BUILD_REQUEST_RECORD_NEXT_IDENT_OFFSET,
          ToSharedI2NPMessage(
              CreateTunnelGatewayMsg(
                  core::InputByteStream::Read<std::uint32_t>(
                      clear_text + BUILD_REQUEST_RECORD_NEXT_TUNNEL_OFFSET),
                  I2NPTunnelBuildReply,
                  buf,
                  len,
                  core::InputByteStream::Read<std::uint32_t>(
                      clear_text + BUILD_REQUEST_RECORD_SEND_MSG_ID_OFFSET))));
    } else {
      xi2p::core::transports.SendMessage(
          clear_text + BUILD_REQUEST_RECORD_NEXT_IDENT_OFFSET,
          ToSharedI2NPMessage(
              CreateI2NPMessage(
                  I2NPTunnelBuild,
                  buf,
                  len,
                  core::InputByteStream::Read<std::uint32_t>(
                      clear_text + BUILD_REQUEST_RECORD_SEND_MSG_ID_OFFSET))));
    }
  }
}

void HandleVariableTunnelBuildReplyMsg(
    std::uint32_t reply_msg_ID,
    std::uint8_t* buf,
    std::size_t len) {
  LOG(debug) << "I2NPMessage: " << __func__ << " reply_msg_ID=" << reply_msg_ID;
  auto tunnel = xi2p::core::tunnels.GetPendingOutboundTunnel(reply_msg_ID);
  if (tunnel) {
    // reply for outbound tunnel
    if (tunnel->HandleTunnelBuildResponse(buf, len)) {
      LOG(debug)
        << "I2NPMessage: outbound tunnel "
        << tunnel->GetTunnelID() << " has been created";
      tunnel->SetState(xi2p::core::e_TunnelStateEstablished);
      xi2p::core::tunnels.AddOutboundTunnel(tunnel);
    } else {
      LOG(warning)
        << "I2NPMessage: outbound tunnel "
        << tunnel->GetTunnelID() << " has been declined";
      tunnel->SetState(xi2p::core::e_TunnelStateBuildFailed);
    }
  } else {
    LOG(warning)
      << "I2NPMessage: pending tunnel for message "
      << reply_msg_ID << " not found";
  }
}


std::unique_ptr<I2NPMessage> CreateTunnelDataMsg(
    const std::uint8_t * buf) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPShortMessage();
  memcpy(msg->GetPayload(), buf, xi2p::core::TUNNEL_DATA_MSG_SIZE);
  msg->len += xi2p::core::TUNNEL_DATA_MSG_SIZE;
  msg->FillI2NPMessageHeader(I2NPTunnelData);
  return msg;
}

std::unique_ptr<I2NPMessage> CreateTunnelDataMsg(
    std::uint32_t tunnel_ID,
    const std::uint8_t* payload) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPShortMessage();
  memcpy(msg->GetPayload() + 4, payload, xi2p::core::TUNNEL_DATA_MSG_SIZE - 4);
  core::OutputByteStream::Write<std::uint32_t>(msg->GetPayload(), tunnel_ID);
  msg->len += xi2p::core::TUNNEL_DATA_MSG_SIZE;
  msg->FillI2NPMessageHeader(I2NPTunnelData);
  return msg;
}

std::shared_ptr<I2NPMessage> CreateEmptyTunnelDataMsg() {
  std::unique_ptr<I2NPMessage> msg = NewI2NPShortMessage();
  msg->len += xi2p::core::TUNNEL_DATA_MSG_SIZE;
  return ToSharedI2NPMessage(std::move(msg));
}

// TODO(anonimal): bytestream refactor
std::unique_ptr<I2NPMessage> CreateTunnelGatewayMsg(
    std::uint32_t tunnel_ID,
    const std::uint8_t* buf,
    std::size_t len) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPMessage(len);
  std::uint8_t* payload = msg->GetPayload();
  core::OutputByteStream::Write<std::uint32_t>(
      payload + TUNNEL_GATEWAY_HEADER_TUNNELID_OFFSET, tunnel_ID);
  core::OutputByteStream::Write<std::uint16_t>(
      payload + TUNNEL_GATEWAY_HEADER_LENGTH_OFFSET, len);
  memcpy(payload + TUNNEL_GATEWAY_HEADER_SIZE, buf, len);
  msg->len += TUNNEL_GATEWAY_HEADER_SIZE + len;
  msg->FillI2NPMessageHeader(I2NPTunnelGateway);
  return msg;
}

std::shared_ptr<I2NPMessage> CreateTunnelGatewayMsg(
    std::uint32_t tunnel_ID,
    std::shared_ptr<I2NPMessage> msg) {
  if (msg->offset >= I2NP_HEADER_SIZE + TUNNEL_GATEWAY_HEADER_SIZE) {
    // message is capable to be used without copying
    std::uint8_t* payload = msg->GetBuffer() - TUNNEL_GATEWAY_HEADER_SIZE;
    core::OutputByteStream::Write<std::uint32_t>(
        payload + TUNNEL_GATEWAY_HEADER_TUNNELID_OFFSET, tunnel_ID);
    int len = msg->GetLength();
    core::OutputByteStream::Write<std::uint16_t>(
        payload + TUNNEL_GATEWAY_HEADER_LENGTH_OFFSET, len);
    msg->offset -= (I2NP_HEADER_SIZE + TUNNEL_GATEWAY_HEADER_SIZE);
    msg->len = msg->offset + I2NP_HEADER_SIZE + TUNNEL_GATEWAY_HEADER_SIZE +len;
    msg->FillI2NPMessageHeader(I2NPTunnelGateway);
    return msg;
  } else {
    std::unique_ptr<I2NPMessage> msg1 = CreateTunnelGatewayMsg(
        tunnel_ID,
        msg->GetBuffer(),
        msg->GetLength());
    return ToSharedI2NPMessage(std::move(msg1));
  }
}

std::unique_ptr<I2NPMessage> CreateTunnelGatewayMsg(
    std::uint32_t tunnel_ID,
    I2NPMessageType msg_type,
    const std::uint8_t* buf,
    std::size_t len,
    std::uint32_t reply_msg_ID) {
  std::unique_ptr<I2NPMessage> msg = NewI2NPMessage(len);
  std::size_t gateway_msg_offset = I2NP_HEADER_SIZE + TUNNEL_GATEWAY_HEADER_SIZE;
  msg->offset += gateway_msg_offset;
  msg->len += gateway_msg_offset;
  memcpy(msg->GetPayload(), buf, len);
  msg->len += len;
  msg->FillI2NPMessageHeader(msg_type, reply_msg_ID);  // create content message
  len = msg->GetLength();
  msg->offset -= gateway_msg_offset;
  std::uint8_t* payload = msg->GetPayload();
  core::OutputByteStream::Write<std::uint32_t>(
      payload + TUNNEL_GATEWAY_HEADER_TUNNELID_OFFSET, tunnel_ID);
  core::OutputByteStream::Write<std::uint16_t>(
      payload + TUNNEL_GATEWAY_HEADER_LENGTH_OFFSET, len);
  msg->FillI2NPMessageHeader(I2NPTunnelGateway);  // gateway message
  return msg;
}

// TODO(anonimal): s/size_t/uint16_t/
std::size_t GetI2NPMessageLength(
    const std::uint8_t* msg) {
  return core::InputByteStream::Read<std::uint16_t>(
             msg + I2NP_HEADER_SIZE_OFFSET)
         + I2NP_HEADER_SIZE;
}

void HandleI2NPMessage(
    std::uint8_t* msg,
    std::size_t len) {
  std::uint8_t type_ID = msg[I2NP_HEADER_TYPEID_OFFSET];
  std::uint32_t const msg_ID = core::InputByteStream::Read<std::uint32_t>(
      msg + I2NP_HEADER_MSGID_OFFSET);
  LOG(debug)
    << "I2NPMessage: msg received len=" << len
    << ", type=" << static_cast<int>(type_ID)
    << ", msg_ID=" << msg_ID;
  std::uint8_t* buf = msg + I2NP_HEADER_SIZE;
  std::uint16_t const size =
      core::InputByteStream::Read<std::uint16_t>(msg + I2NP_HEADER_SIZE_OFFSET);
  switch (type_ID) {
    case I2NPVariableTunnelBuild:
      LOG(debug) << "I2NPMessage: VariableTunnelBuild";
      HandleVariableTunnelBuildMsg(msg_ID, buf, size);
    break;
    case I2NPVariableTunnelBuildReply:
      LOG(debug) << "I2NPMessage: VariableTunnelBuildReply";
      HandleVariableTunnelBuildReplyMsg(msg_ID, buf, size);
    break;
    case I2NPTunnelBuild:
      LOG(debug) << "I2NPMessage: TunnelBuild";
      HandleTunnelBuildMsg(buf, size);
    break;
    case I2NPTunnelBuildReply:
      LOG(debug) << "I2NPMessage: TunnelBuildReply";
      // TODO(unassigned): ???
    break;
    default:
      LOG(warning)
        << "I2NPMessage: unexpected message " << static_cast<int>(type_ID);
  }
}

void HandleI2NPMessage(
    std::shared_ptr<I2NPMessage> msg) {
  if (msg) {
    switch (msg->GetTypeID()) {
      case I2NPTunnelData:
        LOG(debug) << "I2NPMessage: TunnelData";
        xi2p::core::tunnels.PostTunnelData(msg);
      break;
      case I2NPTunnelGateway:
        LOG(debug) << "I2NPMessage: TunnelGateway";
        xi2p::core::tunnels.PostTunnelData(msg);
      break;
      case I2NPGarlic: {
        LOG(debug) << "I2NPMessage: Garlic";
        if (msg->from) {
          if (msg->from->GetTunnelPool())
            msg->from->GetTunnelPool()->ProcessGarlicMessage(msg);
          else
            LOG(debug)
              << "I2NPMessage: local destination for garlic doesn't exist anymore";
        } else {
          context.ProcessGarlicMessage(msg);
        }
        break;
      }
      case I2NPDatabaseStore:
      case I2NPDatabaseSearchReply:
      case I2NPDatabaseLookup:
        // forward to netDb
        xi2p::core::netdb.PostI2NPMsg(msg);
      break;
      case I2NPDeliveryStatus: {
        LOG(debug) << "I2NPMessage: DeliveryStatus";
        if (msg->from && msg->from->GetTunnelPool())
          msg->from->GetTunnelPool()->ProcessDeliveryStatus(msg);
        else
          context.ProcessDeliveryStatusMessage(msg);
        break;
      }
      case I2NPVariableTunnelBuild:
      case I2NPVariableTunnelBuildReply:
      case I2NPTunnelBuild:
      case I2NPTunnelBuildReply:
        // forward to tunnel thread
        xi2p::core::tunnels.PostTunnelData(msg);
      break;
      default:
        HandleI2NPMessage(msg->GetBuffer(), msg->GetLength());
    }
  }
}

I2NPMessagesHandler::~I2NPMessagesHandler() {
  Flush();
}

void I2NPMessagesHandler::PutNextMessage(
    std::shared_ptr<I2NPMessage> msg) {
  if (msg) {
    switch (msg->GetTypeID()) {
      case I2NPTunnelData:
        m_TunnelMsgs.push_back(msg);
      break;
      case I2NPTunnelGateway:
        m_TunnelGatewayMsgs.push_back(msg);
      break;
      default:
        HandleI2NPMessage(msg);
    }
  }
}

void I2NPMessagesHandler::Flush() {
  if (!m_TunnelMsgs.empty()) {
    xi2p::core::tunnels.PostTunnelData(m_TunnelMsgs);
    m_TunnelMsgs.clear();
  }
  if (!m_TunnelGatewayMsgs.empty()) {
    xi2p::core::tunnels.PostTunnelData(m_TunnelGatewayMsgs);
    m_TunnelGatewayMsgs.clear();
  }
}
}  // namespace core
}  // namespace xi2p
