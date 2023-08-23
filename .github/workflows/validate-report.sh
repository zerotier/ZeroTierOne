#!/bin/bash

################################################################################
# Set exit code depending on tool reports                                      #
################################################################################

DEFINITELY_LOST=$(cat *test-results/*summary.json | jq .num_definite_bytes_lost)
EXIT_CODE=$(cat *test-results/*summary.json | jq .exit_code)
EXIT_REASON=$(cat *test-results/*summary.json | jq .exit_reason)

cat *test-results/*summary.json

echo -e "\nBytes of memory definitely lost: $DEFINITELY_LOST"

if [[ "$DEFINITELY_LOST" -gt 0 ]]; then
      exit 1
fi

# Catch-all for other non-zero exit codes

if [[ "$EXIT_CODE" -gt 0 ]]; then
      echo "Test failed: $EXIT_REASON"
      exit 1
fi