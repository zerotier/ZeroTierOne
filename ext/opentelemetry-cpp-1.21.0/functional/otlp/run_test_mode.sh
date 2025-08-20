#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#
# This script can be invoked as follows:
#
# export SERVER_MODE="none" (default)
# No collector listening on endpoint.
#
# export SERVER_MODE="http"
# Http collector listening on endpoint.
#
# export SERVER_MODE="https"
# Https collector listening on endpoint.
#

set -e

[ -z "${BUILD_DIR}" ] && export BUILD_DIR=$HOME/build

[ -z "${SERVER_MODE}" ] && export SERVER_MODE="none"

[ -z "${TEST_EXECUTABLE}" ] && echo "Please specify TEST_EXECUTABLE name" && exit 1

[ -z "${TEST_URL}" ] && echo "Please specify TEST_URL endpoint (without scheme)" && exit 1

export CERT_DIR=../cert

export TEST_BIN_DIR="${BUILD_DIR}/functional/otlp/"

[ ! -f "${TEST_BIN_DIR}/${TEST_EXECUTABLE}" ] && echo "::notice::Executable ${TEST_EXECUTABLE} not built in this configuration" && exit 0

"${TEST_BIN_DIR}/${TEST_EXECUTABLE}" --list > test_list.txt

export TEST_FULL_NAME=""

#
# Connect with no security
#

export TEST_ENDPOINT="http://${TEST_URL}"
export TEST_RUN="insecure"

for T in `cat test_list.txt`
do
  echo "====================================================================="
  echo "Running test ${T} on ${TEST_RUN} ${TEST_ENDPOINT} with server ${SERVER_MODE}"
  TEST_FULL_NAME="${T}-${TEST_RUN}-${SERVER_MODE}"
  "${TEST_BIN_DIR}/${TEST_EXECUTABLE}" --debug --mode ${SERVER_MODE} --cert-dir ${CERT_DIR} --endpoint ${TEST_ENDPOINT} ${T}
  RC=$?
  if [ ${RC} -eq 0 ]; then
    echo "TEST ${TEST_FULL_NAME}: PASSED" | tee -a report.log
  else
    echo "TEST ${TEST_FULL_NAME}: FAILED" | tee -a report.log
  fi
done

#
# Connect with security
#

export TEST_ENDPOINT="https://${TEST_URL}"
export TEST_RUN="secure"

for T in `cat test_list.txt`
do
  echo "====================================================================="
  echo "Running test ${T} on ${TEST_RUN} ${TEST_ENDPOINT} with server ${SERVER_MODE}"
  TEST_FULL_NAME="${T}-${TEST_RUN}-${SERVER_MODE}"
  "${TEST_BIN_DIR}/${TEST_EXECUTABLE}" --debug --mode ${SERVER_MODE} --cert-dir ${CERT_DIR} --endpoint ${TEST_ENDPOINT} ${T}
  RC=$?
  if [ ${RC} -eq 0 ]; then
    echo "TEST ${TEST_FULL_NAME}: PASSED" | tee -a report.log
  else
    echo "TEST ${TEST_FULL_NAME}: FAILED" | tee -a report.log
  fi
done

