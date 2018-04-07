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

#include "client/util/json.h"

#include <iomanip>

namespace xi2p
{
namespace client
{
JsonObject::JsonObject(
    const std::string& value)
    : m_Children(),
      m_Value("\"" + value + "\"") {}

JsonObject::JsonObject(int value)
    : m_Children(), m_Value("\"" + std::to_string(value) + "\"")
{
}

JsonObject::JsonObject(
    double v)
    : m_Children(),
      m_Value() {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << v;
        m_Value = "\"" + oss.str() + "\"";
}

JsonObject::JsonObject(const ptree& node)
{
  m_Value = node.get_value<std::string>();
  if (m_Value == "null")
    m_Value = "";
  else
    m_Value = "\"" + m_Value + "\"";

  for (const auto& child : node)
    m_Children[child.first] = JsonObject(child.second);
}

JsonObject& JsonObject::operator[](
    const std::string& key) {
  return m_Children[key];
}

bool JsonObject::operator==(const JsonObject& other) const
{
  return ToString() == other.ToString();
}

bool JsonObject::operator<(const JsonObject& other) const
{
  return m_Value == other.GetValue();
}

std::ostream& operator<<(std::ostream& os, const JsonObject& object)
{
  os << object.ToString();
  return os;
}

const std::string& JsonObject::GetValue() const
{
  return m_Value;
}

std::string JsonObject::ToString() const {
  if (m_Children.empty())
    return m_Value;
  std::ostringstream oss;
  oss << '{';
  for (auto it = m_Children.begin(); it != m_Children.end(); ++it) {
    if (it != m_Children.begin())
      oss << ',';
    oss << '"' << it->first << "\":" << it->second.ToString();
  }
  oss << '}';
  return oss.str();
}

}  // namespace client
}  // namespace xi2p
