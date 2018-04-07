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

#include <boost/test/unit_test.hpp>

#include "client/util/parse.h"

#include <set>

#include "core/crypto/rand.h"
#include "core/router/identity.h"
#include "core/util/log.h"

BOOST_AUTO_TEST_SUITE(ClientParsing)

BOOST_AUTO_TEST_CASE(ParseACL)
{
  std::set<xi2p::core::IdentHash> idents;
  std::uint8_t count(3);
  std::string acl;

  // Create hashes + construct malformed ACL
  for (std::uint8_t i(0); i < count; i++)
    {
      // Note: not a "real" (key-generated) ident hash
      std::array<std::uint8_t, sizeof(xi2p::core::IdentHash)> rand{{}};
      xi2p::core::RandBytes(rand.data(), rand.size());

      // Create hash + insert into set
      xi2p::core::IdentHash hash(rand.data());
      idents.insert(hash);

      // Log valid b32
      const std::string b32 = hash.ToBase32();
      LOG(debug) << "ParseACL: " << b32;

      // Construct malformed ACL delimiters
      // TODO(unassigned): extend test for malformed ACLs?
      acl += b32;
      if (i != (count - 1))
        acl += ",,,,";
    }

  // Log our malformed ACL
  LOG(debug) << "ParseACL: " << acl;

  // Parse our malformed ACL
  auto const& parsed_idents = xi2p::client::ParseACL(acl);

  // Check if parser fixed the ACL and if parsed ACL matches the correct ACL
  BOOST_CHECK(parsed_idents == idents);
}

struct TunnelFixture {
  xi2p::client::TunnelAttributes tunnel{};
};

// Test for correct delimiter parsing against plain configuration
BOOST_AUTO_TEST_CASE(ParseClientDestination) {
  // Create plain destination
  auto plain = std::make_unique<TunnelFixture>();

  plain->tunnel.dest = "anonimal.i2p";
  plain->tunnel.dest_port = 80;

  xi2p::client::ParseClientDestination(&plain->tunnel);

  // Create delimited destination
  auto delimited = std::make_unique<TunnelFixture>();

  delimited->tunnel.dest = "anonimal.i2p:80";
  delimited->tunnel.dest_port = 12345;

  xi2p::client::ParseClientDestination(&delimited->tunnel);

  // Both destinations should be equal after being parsed
  BOOST_CHECK_EQUAL(delimited->tunnel.dest, plain->tunnel.dest);
  BOOST_CHECK_EQUAL(delimited->tunnel.dest_port, plain->tunnel.dest_port);
}

// Test for bad port length
BOOST_AUTO_TEST_CASE(CatchBadClientDestination) {
  // Create bad destination
  auto bad = std::make_unique<TunnelFixture>();

  bad->tunnel.dest = "anonimal.i2p:111111111";
  bad->tunnel.dest_port = 80;

  BOOST_REQUIRE_THROW(
      xi2p::client::ParseClientDestination(&bad->tunnel),
      std::exception);

  // TODO(unassigned): expand test-case (see TODO in function definition)
}

BOOST_AUTO_TEST_SUITE_END()
