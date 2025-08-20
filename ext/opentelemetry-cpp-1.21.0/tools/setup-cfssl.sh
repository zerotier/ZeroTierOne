#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

[ -z "${CFSSL_VERSION}" ] && export CFSSL_VERSION="1.6.3"

curl -s -L -o /bin/cfssl https://github.com/cloudflare/cfssl/releases/download/v${CFSSL_VERSION}/cfssl_${CFSSL_VERSION}_linux_amd64
curl -s -L -o /bin/cfssljson https://github.com/cloudflare/cfssl/releases/download/v${CFSSL_VERSION}/cfssljson_${CFSSL_VERSION}_linux_amd64
chmod +x /bin/cfssl
chmod +x /bin/cfssljson
