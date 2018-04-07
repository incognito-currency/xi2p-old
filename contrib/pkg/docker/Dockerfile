# Copyright (c) 2017-2018, The Xi2p I2P Router Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Example:
# $ docker build -t geti2p/xi2p .
# $ export XI2P_PORT=$((9111 + RANDOM & 30777))
# $ docker run -p ${XI2P_PORT}:${XI2P_PORT} --env XI2P_PORT=${XI2P_PORT} geti2p/xi2p

FROM alpine:3.6
ENV XI2P_HOST 0.0.0.0
ENV XI2P_PORT 24085
ADD . xi2p/
RUN apk add --update --no-cache \
      binutils \
      boost \
      boost-date_time \
      boost-dev \
      boost-program_options \
      boost-system \
      boost-unit_test_framework \
      cmake \
      g++ \
      libressl-dev \
      make \
      bash \
    && \
    cd xi2p && \
    make all tests && \
    adduser -D xi2p && \
    mkdir /home/xi2p/.xi2p && \
    cp -r /xi2p/pkg/* /home/xi2p/.xi2p && \
    strip /xi2p/build/xi2p && \
    strip /xi2p/build/xi2p-util && \
    strip /xi2p/build/xi2p-tests && \
    install /xi2p/build/xi2p /usr/bin/xi2p && \
    install /xi2p/build/xi2p-util /usr/bin/xi2p-util && \
    install /xi2p/build/xi2p-tests /usr/bin/xi2p-tests && \
    cd .. && \
    rm -rf ~/xi2p && \
    chown -R xi2p:xi2p /home/xi2p && \
    apk del \
      binutils \
      boost-dev \
      cmake \
      g++ \
      make \
      bash \
    && \
    rm -rf /xi2p && \
    rm -rf /var/cache/*
USER xi2p
RUN /usr/bin/xi2p-tests
CMD /usr/bin/xi2p --host ${XI2P_HOST} --port ${XI2P_PORT}
