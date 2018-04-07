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

#include "core/router/context.h"
#include "core/util/exception.h"
#include "core/util/log.h"
#include "util/base.h"
#include "util/benchmark.h"
#include "util/command.h"
#ifdef WITH_CRYPTOPP
#include "util/cpuid.h"
#endif // WITH_CRYPTOPP
#ifdef WITH_FUZZ_TESTS
#include "util/fuzz.h"
#endif  // WITH_FUZZ_TESTS
#include "util/i2pcontrol.h"
#include "util/routerinfo.h"
#include "util/su3file.h"

#ifndef _WIN32
#include "util/xi2p.h"
#endif

namespace bpo = boost::program_options;
typedef std::map<std::string, Command*> ListCommands;

void PrintUsage(
    const std::string& name,
    const bpo::options_description& desc,
    const ListCommands& list_cmd)
{
  LOG(info) << "Syntax: " << name << " <options> command";
  LOG(info) << desc;
  LOG(info) << "Available commands:";
  for (const auto& c : list_cmd)
    LOG(info) << "\t" << c.first;
}

int main(int argc, const char* argv[])
{
  ListCommands list_cmd;

  Base32Command base32_cmd;
  list_cmd[base32_cmd.GetName()] = &base32_cmd;

  Base64Command base64_cmd;
  list_cmd[base64_cmd.GetName()] = &base64_cmd;

  SU3FileCommand su3file_cmd;
  list_cmd[su3file_cmd.GetName()] = &su3file_cmd;

  RouterInfoCommand routerinfo_cmd;
  list_cmd[routerinfo_cmd.GetName()] = &routerinfo_cmd;

  Benchmark benchmark_cmd;
  list_cmd[benchmark_cmd.GetName()] = &benchmark_cmd;

  I2PControlCommand i2pcontrol_cmd;
  list_cmd[i2pcontrol_cmd.GetName()] = &i2pcontrol_cmd;

#ifndef _WIN32
  Xi2pCommand xi2p_cmd;
  list_cmd[xi2p_cmd.GetName()] = &xi2p_cmd;
#endif

#ifdef WITH_CRYPTOPP
  CpuidCommand cpuid_cmd;
  list_cmd[cpuid_cmd.GetName()] = &cpuid_cmd;
#endif // WITH_CRYPTOPP

#ifdef WITH_FUZZ_TESTS
  FuzzCommand fuzz_cmd;
  list_cmd[fuzz_cmd.GetName()] = &fuzz_cmd;
#endif  // WITH_FUZZ_TESTS

  bpo::options_description general_desc("General options");
  // See src/app/config.cc for log options
  general_desc.add_options()("help,h", "produce this help message")(
      "log-to-console",
      bpo::value<bool>()->default_value(true)->value_name("bool"))(
      "log-to-file",
      bpo::value<bool>()->default_value(false)->value_name("bool"))(
      "log-file-name",
      bpo::value<std::string>()->default_value("")->value_name("path"))(
      "log-level", bpo::value<std::uint16_t>()->default_value(3))(
      "log-auto-flush",
      bpo::value<bool>()->default_value(false)->value_name("bool"))(
      "log-enable-color",
      bpo::value<bool>()->default_value(true)->value_name("bool"));

  bpo::options_description spec("Specific options");
  spec.add_options()(
      "args", bpo::value<std::vector<std::string> >()->multitoken());
  bpo::options_description config_options;
  config_options.add(general_desc).add(spec);

  bpo::positional_options_description pos;
  pos.add("args", -1);

  bpo::variables_map vm;
  std::vector<std::string> args, opts;
  try
    {
      bpo::parsed_options parsed = bpo::command_line_parser(argc, argv)
                                       .options(config_options)
                                       .allow_unregistered()
                                       .positional(pos)
                                       .run();
      opts = bpo::collect_unrecognized(parsed.options, bpo::include_positional);

      bpo::store(parsed, vm);
      bpo::notify(vm);
    }
  catch (...)
    {
      xi2p::core::Exception ex;
      ex.Dispatch(__func__);
      return EXIT_FAILURE;
    }

  // Setup logging options
  xi2p::core::SetupLogging(vm);

  if (vm.count("args"))
    args = vm["args"].as<std::vector<std::string> >();

  if (vm.count("help"))
    {
      if (!args.empty())
        {
          const auto& c = list_cmd.find(args.front());
          if (c != list_cmd.end())
            {  // print only sub command help
              list_cmd[c->first]->PrintUsage(
                  std::string(argv[0]) + " " + c->first);
              return EXIT_SUCCESS;
            }
        }
      PrintUsage(argv[0], general_desc, list_cmd);
      return EXIT_SUCCESS;
    }

  if (opts.empty())
    {
      LOG(error) << "Not enough arguments !";
      PrintUsage(argv[0], general_desc, list_cmd);
      return EXIT_FAILURE;
    }

  std::string sub_cmd = opts.front();
  // If the first argument is not a command
  if (list_cmd.find(sub_cmd) == list_cmd.end())
    {
      LOG(error) << "Invalid command or option \"" << sub_cmd << "\"";
      PrintUsage(argv[0], general_desc, list_cmd);
      return EXIT_FAILURE;
    }

  for (const auto& a : opts)
    LOG(trace) << "-- OPTS : " << a;
  opts.erase(opts.begin());  // Remove sub command
  // Process
  if (list_cmd[sub_cmd]->Impl(std::string(argv[0]) + " " + sub_cmd, opts))
    return EXIT_SUCCESS;
  return EXIT_FAILURE;
}
