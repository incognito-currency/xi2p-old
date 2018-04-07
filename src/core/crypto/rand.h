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

#ifndef SRC_CORE_CRYPTO_RAND_H_
#define SRC_CORE_CRYPTO_RAND_H_

#include <algorithm>
#include <cstdint>

namespace xi2p {
namespace core {

  /// @brief Generates CSPRNG bytes
  /// @param data Buffer to store result
  /// @param length Size of buffer
  void RandBytes(
      std::uint8_t* data,
      std::size_t length);

  /// @brief Generates a random of type T
  /// @return Random value of type T
  template<class T>
  T Rand() {
    T ret;
    // TODO(unassigned): alignment
    RandBytes(
        reinterpret_cast<std::uint8_t *>(&ret),
        sizeof(ret));
    return ret;
  }

  /// @brief Generates a random unsigned integer in given range
  /// @warning If min/max < 0, results are undefined
  /// @warning If type is greater than unsigned int, results are undefined
  /// @param min Lowerbound
  /// @param max Upperbound
  /// @return Random number in range [min, max]
  std::uint32_t RandInRange32(std::uint32_t min, std::uint32_t max);

  /// @brief CSPRNG shuffle of a sequence container
  /// @param begin iterator to the first element in container
  /// @param end iterator beyond the last element in container
  /// @note IT must meet the requirements of ForwardIterator.
  ///   begin, end must meet the requirements of Swappable.
  template <class IT>
  void Shuffle(IT begin, IT end) {
    for (; begin != end; ++begin)
      std::iter_swap(
          begin,
          begin + RandInRange32(0, end - begin - 1));
  }

}  // namespace core
}  // namespace xi2p

#endif  // SRC_CORE_CRYPTO_RAND_H_
