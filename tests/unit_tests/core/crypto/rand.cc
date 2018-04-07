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

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "core/crypto/rand.h"

/// Note:
///
///  These tests were needed for home-brewed crypto. Now that we use cryptopp
///  for random ranges, these unit-tests are no longer needed. Referencing #515
///

BOOST_AUTO_TEST_SUITE(RandInRange)

/// @class Range
template <class T>
struct Range {
  T repeated, tally;
  T min, max, result;
  void WithinBounds();
  void OuterBounds(T low, T high);
  Range()
      : repeated(0),
        tally(0),
        min(0),
        max(std::numeric_limits<T>::max()),
        result(0) {}
};

/// @brief Test if random range of type T is within given thresholds
/// @notes This not a test for entropy but rather to ensure a reasonable
///  implementation (e.g., not stuck repeating the same value 100 times, etc.)
template <class T>
void Range<T>::WithinBounds() {
  for (T i = 0; i < /*Arbitrary value*/ 100; i++) {
    repeated = result;
    result = xi2p::core::RandInRange32(min, max);
    if (result < min || result > max)
      throw std::out_of_range(std::string("RandInRange: returned ") + std::to_string(result));
    if (result == repeated)
      tally++;
    if (tally > 3)  // Arbitrary threshold, small to allow probability for smaller types
      throw std::length_error(std::string("RandInRange: tally exceeded threshold"));
  }
}

/// @brief Check outer bounds
/// @notes We don't use the code above because we need more iterations
/// @notes Referencing #515
template <class T>
void Range<T>::OuterBounds(T low, T high) {
  for (T i = 0; i < max; i++) {
    result = xi2p::core::RandInRange32(low, high);
    if (result != low && result != high)
      tally++;
    if (tally == max)
      throw std::range_error(std::string("RandInRange: never outer bounds"));
  }
}

BOOST_AUTO_TEST_CASE(_uint8_t) {
  Range<std::uint8_t> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

BOOST_AUTO_TEST_CASE(_uint16_t) {
  Range<std::uint16_t> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

BOOST_AUTO_TEST_CASE(_uint32_t) {
  Range<std::uint32_t> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

BOOST_AUTO_TEST_CASE(_uint64_t) {
  Range<std::uint64_t> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

// Signed, so test for negative results regardless of initialized lowerbound
BOOST_AUTO_TEST_CASE(_int) {
  Range<int> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

BOOST_AUTO_TEST_CASE(_long) {
  Range<long> range;
  BOOST_CHECK_NO_THROW(range.WithinBounds());
}

BOOST_AUTO_TEST_CASE(Outer) {
  Range<std::uint8_t> range;
  BOOST_CHECK_NO_THROW(range.OuterBounds(3, 8));
}

BOOST_AUTO_TEST_SUITE_END()
