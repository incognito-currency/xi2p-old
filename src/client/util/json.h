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
 */

#ifndef SRC_CLIENT_UTIL_JSON_H_
#define SRC_CLIENT_UTIL_JSON_H_

#include <boost/property_tree/ptree.hpp>

#include <map>
#include <sstream>
#include <string>

namespace xi2p
{
namespace client
{
/**
 * @class JsonObject
 * @brief Represents a Json object, provides functionality to convert to string.
 */
class JsonObject {
  typedef boost::property_tree::ptree ptree;

 public:
  JsonObject() = default;

  explicit JsonObject(
      const std::string& value);

  explicit JsonObject(
      int value);

  explicit JsonObject(
      double value);

  explicit JsonObject(const ptree& value);

  JsonObject& operator[](
      const std::string& key);

  // @return JsonObject associated with key
  const JsonObject& Get(const std::string& key) const;

  // @return current node value
  const std::string& GetValue() const;

  std::string ToString() const;

  // For usage with boost::variant
  bool operator==(const JsonObject&) const;
  bool operator<(const JsonObject&) const;

 private:
  std::map<std::string, JsonObject> m_Children;
  std::string m_Value;
};

std::ostream& operator<<(std::ostream& os, const JsonObject&);

}  // namespace client
}  // namespace xi2p

#endif  // SRC_CLIENT_UTIL_JSON_H_
