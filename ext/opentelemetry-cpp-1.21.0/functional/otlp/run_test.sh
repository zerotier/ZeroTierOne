#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

# To run tests in a local dev environment:
# - make sure docker is running
# - set BUILD_DIR to the top level build directory,

[ -z "${BUILD_DIR}" ] && export BUILD_DIR="${HOME}/build"

export CERT_DIR=../cert

export TEST_BIN_DIR="${BUILD_DIR}/functional/otlp/"

# SELINUX
# https://docs.docker.com/storage/bind-mounts/#configure-the-selinux-label

USE_MOUNT_OPTION=""

if [ -x "$(command -v getenforce)" ]; then
  SELINUXSTATUS=$(getenforce);
  if [ "${SELINUXSTATUS}" == "Enforcing" ]; then
    echo "Detected SELINUX"
    USE_MOUNT_OPTION=":z"
  fi;
fi

#
# Prepare docker image
#

docker build -t otelcpp-func-test .

echo "REPORT:" > report.log

#
# Exercising HTTP functional tests
#

export TEST_EXECUTABLE="func_otlp_http"
export TEST_URL="localhost:4318/v1/traces"

#
# MODE 'NONE'
#

export SERVER_MODE="none"
./run_test_mode.sh

#
# MODE 'HTTP'
#

echo ""
echo "###############################################################"
echo "Starting otelcol --config otel-config-http.yaml"
echo "###############################################################"
echo ""


docker run -d \
  -v `pwd`/otel-docker-config-http.yaml:/otel-cpp/otel-config.yaml${USE_MOUNT_OPTION} \
  -p 4318:4318 \
  --name otelcpp-test-http \
  otelcpp-func-test

sleep 5;

export SERVER_MODE="http"
./run_test_mode.sh

echo ""
echo "###############################################################"
echo "Stopping otelcol (http)"
echo "###############################################################"
echo ""

docker stop otelcpp-test-http
docker rm otelcpp-test-http

#
# MODE 'SSL'
#

echo ""
echo "###############################################################"
echo "Starting otelcol --config otel-config-https.yaml"
echo "###############################################################"
echo ""

docker run -d \
  -v `pwd`/otel-docker-config-https.yaml:/otel-cpp/otel-config.yaml${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/ca.pem:/otel-cpp/ca.pem${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/server_cert.pem:/otel-cpp/server_cert.pem${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/server_cert-key.pem:/otel-cpp/server_cert-key.pem${USE_MOUNT_OPTION} \
  -p 4318:4318 \
  --name otelcpp-test-https \
  otelcpp-func-test

sleep 5;

export SERVER_MODE="https"
./run_test_mode.sh

echo ""
echo "###############################################################"
echo "Stopping otelcol (https)"
echo "###############################################################"
echo ""

docker stop otelcpp-test-https
docker rm otelcpp-test-https

#
# Exercising gRPC functional tests
#
export TEST_EXECUTABLE="func_otlp_grpc"
export TEST_URL="localhost:4317"

#
# MODE 'SSL'
#

echo ""
echo "###############################################################"
echo "Starting otelcol --config otel-config-https-mtls.yaml"
echo "###############################################################"
echo ""

docker run -d \
  -v `pwd`/otel-docker-config-https-mtls.yaml:/otel-cpp/otel-config.yaml${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/ca.pem:/otel-cpp/ca.pem${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/client_cert.pem:/otel-cpp/client_cert.pem${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/server_cert.pem:/otel-cpp/server_cert.pem${USE_MOUNT_OPTION} \
  -v `pwd`/../cert/server_cert-key.pem:/otel-cpp/server_cert-key.pem${USE_MOUNT_OPTION} \
  -p 4317:4317 \
  --name otelcpp-test-grpc-mtls \
  otelcpp-func-test

sleep 5;

export SERVER_MODE="https"
./run_test_mode.sh

echo ""
echo "###############################################################"
echo "Stopping otelcol (https / mTLS)"
echo "###############################################################"
echo ""

docker stop otelcpp-test-grpc-mtls
docker rm otelcpp-test-grpc-mtls

echo ""
echo "###############################################################"
echo "TEST REPORT"
echo "###############################################################"
echo ""

cat report.log

export PASSED_COUNT=`grep -c "PASSED" report.log`
export FAILED_COUNT=`grep -c "FAILED" report.log`

echo ""
echo "###############################################################"
echo "TEST VERDICT: ${PASSED_COUNT} PASSED, ${FAILED_COUNT} FAILED"
echo "###############################################################"
echo ""

if [ "${FAILED_COUNT}" != "0" ]; then
  #
  # CI FAILED
  #

  exit 1
fi;

#
# CI PASSED
#
exit 0
