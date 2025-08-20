#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

which cfssl
which cfssljson

cfssl version
cfssljson -version

echo "Generating CA"
cfssl genkey -initca ca_csr.json | cfssljson -bare ca

echo "Generating CLIENT CERT"
cfssl gencert -ca ca.pem -ca-key ca-key.pem client_csr.json | cfssljson -bare client_cert
echo "Generating SERVER CERT"
cfssl gencert -ca ca.pem -ca-key ca-key.pem server_csr.json | cfssljson -bare server_cert

echo "Generating CA B"
cfssl genkey -initca ca_csr_b.json | cfssljson -bare ca_b

echo "Generating CLIENT CERT B"
cfssl gencert -ca ca_b.pem -ca-key ca_b-key.pem client_csr_b.json | cfssljson -bare client_cert_b
echo "Generating SERVER CERT B"
cfssl gencert -ca ca_b.pem -ca-key ca_b-key.pem server_csr_b.json | cfssljson -bare server_cert_b

echo "UNREADABLE" > unreadable.pem
chmod 0 unreadable.pem

# Needed to copy this key inside docker (different owner)
chmod +r server_cert-key.pem

# Debug
ls -l

