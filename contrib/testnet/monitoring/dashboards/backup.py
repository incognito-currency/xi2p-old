#!/usr/bin/env python

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


import argparse
import json
import os
import requests
import sys

# Configuration
parser = argparse.ArgumentParser(description='Backup current grafana dashboards.')
parser.add_argument('--dir', default=os.path.dirname(os.path.realpath(__file__)), 
                    help='Directory to store the dashboards (%(default)s)')
parser.add_argument('--uri', default='http://admin:xi2p@127.0.0.1:3030',
                    help='URI to access grafana with credentials (%(default)s)')

args = parser.parse_args()
uri = args.uri
directory = args.dir

# List dashboards
req = requests.get(uri + '/api/search').json()

for r in req:
    print("Backuping... " + r['uri'])
    # Get dashboard
    dashboard = requests.get(uri + '/api/dashboards/' + r['uri']).json()
    # Change id with 'null' so that it can be imported
    dashboard['dashboard']['id'] = None
    # Save to file
    with open(directory + '/' + r['uri'][3:] + '.json', 'w') as output:
        output.write(json.dumps(dashboard, indent=4, sort_keys=True, separators=(',', ': ')))

