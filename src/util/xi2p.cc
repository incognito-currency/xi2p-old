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

#include "util/xi2p.h"

#include <chrono>
#include <thread>
#include <utility>

#include "client/instance.h"

#include "core/instance.h"
#include "core/util/log.h"

// TODO(unassigned):
//
//  Known issues when running xi2p from xi2p-util:
//
//  1. Because of(?) the Boost.Log singleton (unresolved, see TODOs),
//  there are duplicate log records. Simply removing the log-to-console
//  option for either xi2p-util or xi2p will prevent dups.
//
//  2. Passing log-level to xi2p from xi2p-util doesn't work (it's not
//  picked up in the xi2p args list...). Possibly another Boost.Log
//  conflict with xi2p-util's implementation.
//
//  3. cpp-netlib's HTTPS is unavailable (initialization issue?). In the
//  meantime, reseeding from file works. Note: Boost.Beast will replace
//  out reliance on the recently abandoned cpp-netlib.
//
//  4. Passing --help to xi2p option will not provide `xi2p --help`
//  options. Some interface work may be needed for resolution.
//
//  Other than that, if resources are properly installed
//  (via `make install` for example), xi2p will run as expected.

/// @brief Global for signal handler
bool g_IsRunning;

/// @brief Global for instance reloading
bool g_IsReloading;

Xi2pCommand::Xi2pCommand() : m_Exception(__func__)
{
  // Signal handler
  struct sigaction sa{};
  sa.sa_handler = Signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGHUP, &sa, 0);
  sigaction(SIGINT, &sa, 0);
  sigaction(SIGABRT, &sa, 0);
  sigaction(SIGTERM, &sa, 0);

  // Begin run
  g_IsRunning = true;
  g_IsReloading = false;
}

void Xi2pCommand::PrintUsage(const std::string& name) const
{
  LOG(info) << "Syntax: " << name;
  LOG(info) << "\t--help (show help options)";
}

bool Xi2pCommand::Impl(
    const std::string& cmd_name,
    const std::vector<std::string>& args)
{
  try
    {
      // Create instances
      xi2p::core::Instance core(args);
      m_Client = std::make_unique<xi2p::client::Instance>(core);

      // TODO(anonimal): we want true RAII. See TODOs.
      // Initialize core/client
      m_Client->Initialize();

      // Start core/client
      m_Client->Start();

      // Keep instances running
      // TODO(anonimal): PoC for WIP API
      while (g_IsRunning)
        {
          if (g_IsReloading)
            {
              m_Client->Reload();
              g_IsReloading = false;
            }
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }

      // Stop client/core
      m_Client->Stop();
    }
  catch (...)
    {
      m_Exception.Dispatch(__func__);
      try
        {
          m_Client->Stop();
        }
      catch (...)
        {
          m_Exception.Dispatch(__func__);
        }
      LOG(error) << __func__ << ": could not run " << cmd_name;
      return false;
    }
  return true;
}

void Xi2pCommand::Signal(int signal)
{
  switch (signal)
    {
      case SIGHUP:
        g_IsReloading = true;
        break;
      case SIGINT:
      case SIGABRT:
      case SIGTERM:
        g_IsRunning = false;
        break;
    }
}
