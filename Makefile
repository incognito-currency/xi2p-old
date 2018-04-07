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

# Get custom Xi2p data path + set appropriate CMake generator.
# If no path is given, set default path
system := $(shell uname)
ifeq ($(XI2P_DATA_PATH),)
  ifeq ($(system), Linux)
    data-path = $(HOME)/.xi2p
  endif
  ifeq ($(system), Darwin)
    data-path = $(HOME)/Library/Application\ Support/Xi2p
  endif
  ifneq (, $(findstring BSD, $(system))) # We should support other BSD's
    data-path = $(HOME)/.xi2p
  endif
  ifeq ($(system), DragonFly)
    data-path = $(HOME)/.xi2p
  endif
  ifneq (, $(findstring MINGW, $(system)))
    data-path = "$(APPDATA)"\\Xi2p
    cmake-gen = -G 'MSYS Makefiles'
  endif
else
  data-path = $(XI2P_DATA_PATH)
endif

# Our base cmake command
cmake = cmake $(cmake-gen)

# Release types
cmake-debug = $(cmake) -D CMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake-release = $(cmake) -D CMAKE_BUILD_TYPE=Release

# TODO(unassigned): cmake-release when we're out of alpha
cmake-xi2p = $(cmake-debug)
cmake-xi2p-util = -D WITH_XI2P_UTIL=ON

# Current off-by-default Xi2p build options
cmake-upnp       = -D WITH_UPNP=ON
cmake-optimize   = -D WITH_OPTIMIZE=ON
cmake-hardening  = -D WITH_HARDENING=ON
cmake-tests      = -D WITH_TESTS=ON
cmake-fuzz-tests = -D WITH_FUZZ_TESTS=ON
cmake-static     = -D WITH_STATIC=ON
cmake-doxygen    = -D WITH_DOXYGEN=ON
cmake-coverage   = -D WITH_COVERAGE=ON
cmake-python     = -D WITH_PYTHON=ON

# Currently, our dependencies are static but cpp-netlib's dependencies are not (by default)
cmake-cpp-netlib-static = -D CPP-NETLIB_STATIC_OPENSSL=ON -D CPP-NETLIB_STATIC_BOOST=ON

# cpp-netlib shared
cmake-cpp-netlib-shared = -D CPP-NETLIB_BUILD_SHARED_LIBS=ON

# Android-specific
cmake-android = -D ANDROID=1 -D XI2P_DATA_PATH="/data/local/tmp/.xi2p"

# Native
cmake-native = -DCMAKE_CXX_FLAGS="-march=native"
cryptopp-native = CXXFLAGS="-march=native -DCRYPTOPP_NO_CPU_FEATURE_PROBES=1"  # Refs #699

# Filesystem
build = build/
build-cpp-netlib = deps/cpp-netlib/$(build)
build-cryptopp = deps/cryptopp/  # No longer using CMake
build-doxygen = doc/Doxygen
build-fuzzer = contrib/Fuzzer/$(build)

# CMake builder macros
define CMAKE
  mkdir -p $1
  cd $1 && $2 ../
endef

define CMAKE_CPP-NETLIB
  @echo "=== Building cpp-netlib ==="
  $(eval cmake-cpp-netlib = $(cmake-release) -D CPP-NETLIB_BUILD_TESTS=OFF -D CPP-NETLIB_BUILD_EXAMPLES=OFF $1)
  $(call CMAKE,$(build-cpp-netlib),$(cmake-cpp-netlib))
endef

define MAKE_CRYPTOPP
  @echo "=== Building cryptopp ==="
  cd $(build-cryptopp) && $1
endef

define CMAKE_FUZZER
  @echo "=== Building fuzzer ==="
  $(eval cmake-fuzzer = $(cmake-release) -DLLVM_USE_SANITIZER=Address -DLLVM_USE_SANITIZE_COVERAGE=YES \
      -DCMAKE_CXX_FLAGS="-g -O2 -fno-omit-frame-pointer -std=c++11" $1)
  $(call CMAKE,$(build-fuzzer),$(cmake-fuzzer))
endef

# Targets
all: dynamic

#--------------------------------#
# Dependency build types/options #
#--------------------------------#

deps:
	$(call CMAKE_CPP-NETLIB,$(cmake-native)) && $(MAKE)
	$(call MAKE_CRYPTOPP, $(MAKE) $(cryptopp-native) static)

shared-deps:
	$(call CMAKE_CPP-NETLIB,$(cmake-native) $(cmake-cpp-netlib-shared)) && $(MAKE)
	$(call MAKE_CRYPTOPP, $(MAKE) shared)

release-deps:
	$(call CMAKE_CPP-NETLIB) && $(MAKE)
	$(call MAKE_CRYPTOPP, $(MAKE) static)

release-static-deps:
	$(call CMAKE_CPP-NETLIB,$(cmake-cpp-netlib-static)) && $(MAKE)
	$(call MAKE_CRYPTOPP, $(MAKE) static)

#-----------------------------------#
# For local, end-user cloned builds #
#-----------------------------------#

dynamic: shared-deps
	$(eval cmake-xi2p += $(cmake-native))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

static: deps
	$(eval cmake-xi2p += $(cmake-native) $(cmake-static))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

#-----------------------------------#
# For  dynamic distribution release #
#-----------------------------------#

release: release-deps
	# TODO(unassigned): cmake release flags + optimizations/hardening when we're out of alpha
	$(eval cmake-xi2p += $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

#--------------------------------------------------------------#
# For static distribution release (website and nightly builds) #
#--------------------------------------------------------------#

release-static: release-static-deps
        # TODO(unassigned): cmake release flags + optimizations/hardening when we're out of alpha
	$(eval cmake-xi2p += $(cmake-static) $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

release-static-android: release-static-deps
	$(eval cmake-xi2p += $(cmake-static) $(cmake-android) $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# TODO(unassigned): static UPnP once our UPnP implementation is release-ready

#-----------------#
# Optional builds #
#-----------------#

# Utility binary
util: deps
	$(eval cmake-xi2p += $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# For API/testnet development
python: shared-deps
	$(eval cmake-xi2p += $(cmake-python))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce vanilla binary with UPnP support
upnp: deps
	$(eval cmake-xi2p += $(cmake-upnp))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce optimized, hardened binary *with* UPnP
all-options: deps
	$(eval cmake-xi2p += $(cmake-optimize) $(cmake-hardening) $(cmake-upnp) $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce optimized, hardened binary *without* UPnP. Note: we need (or very much should have) optimizations with hardening
optimized-hardened: deps
	$(eval cmake-xi2p += $(cmake-optimize) $(cmake-hardening))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce all unit-tests with optimized hardening
optimized-hardened-tests: deps
	$(eval cmake-xi2p += $(cmake-optimize) $(cmake-hardening) $(cmake-tests))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce build with coverage. Note: leaving out hardening because of need for optimizations
coverage: deps
	$(eval cmake-xi2p += $(cmake-coverage) $(cmake-upnp) $(cmake-xi2p-util))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce unit-tests with coverage
coverage-tests: deps
	$(eval cmake-xi2p += $(cmake-coverage) $(cmake-tests))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce vanilla unit-tests
tests: deps
	$(eval cmake-xi2p += $(cmake-tests))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce vanilla fuzzer-tests
fuzz-tests: deps
	$(call CMAKE_FUZZER) && $(MAKE)
	$(eval cmake-xi2p += $(cmake-fuzz-tests))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE)

# Produce Doxygen documentation
doxygen:
	$(eval cmake-xi2p += $(cmake-doxygen))
	$(call CMAKE,$(build),$(cmake-xi2p)) && $(MAKE) doc

# Produce available CMake build options
help:
	$(call CMAKE,$(build),$(cmake-xi2p) -LH)

# Clean all build directories and Doxygen output
clean:
	$(eval remove-build = rm -fR $(build) $(build-cpp-netlib) $(build-doxygen) $(build-fuzzer) && cd $(build-cryptopp) && $(MAKE) clean)
	@if [ "$$FORCE_CLEAN" = "yes" ]; then $(remove-build); \
	else echo "CAUTION: This will remove the build directories for Xi2p and all submodule dependencies, and remove all Doxygen output"; \
	read -r -p "Is this what you wish to do? (y/N)?: " CONFIRM; \
	  if [ $$CONFIRM = "y" ] || [ $$CONFIRM = "Y" ]; then $(remove-build); \
          else echo "Exiting."; exit 1; \
          fi; \
        fi

# Install binaries and package
install:
	@_install="./pkg/installers/xi2p-install.sh"; \
	if [ -e $$_install ]; then $$_install; else echo "Unable to find $$_install"; exit 1; fi

# Un-install binaries and package
uninstall:
	@_install="./pkg/installers/xi2p-install.sh"; \
	if [ -e $$_install ]; then $$_install -u; else echo "Unable to find $$_install"; exit 1; fi

.PHONY: all deps release-deps release-static-deps dynamic static release release-static release-static-android all-options optimized-hardened optimized-hardened-tests coverage coverage-tests tests doxygen help clean install uninstall
