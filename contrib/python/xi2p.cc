// Copyright (c) 2017-2018, The Xi2p I2P Router Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "contrib/python/instance.h"

#include <boost/python.hpp>

#include "src/client/instance.h"

namespace py = boost::python;

void test_run()
{
  xi2p::core::Instance core;
  xi2p::client::Instance client(core);

  client.Initialize();
  client.Start();
  client.Stop();
}

void wrapper_test_run()
{
  Core core;
  Client client(core);

  client.init();
  client.start();
  client.stop();
}

// TODO(anonimal): in-tandem API development
BOOST_PYTHON_MODULE(xi2p_python)
{
  py::def("test_run", test_run);
  py::def("wrapper_test_run", wrapper_test_run);

  py::class_<Core>("Core", py::init<std::string>())  // string for convenience
    .def(py::init<py::list>())
    .def("init", &Core::init)
    .def("start", &Core::start)
    .def("stop", &Core::stop);

  py::class_<Client>("Client", py::init<Core>())
    .def("init", &Client::init)
    .def("start", &Client::start)
    .def("stop", &Client::stop);
}
