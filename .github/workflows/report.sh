#!/bin/bash

################################################################################
# Set exit code depending on tool reports                                      #
################################################################################

DEFINITELY_LOST=$(cat *test-results/*summary.json | jq .num_definite_bytes_lost)

cat *test-results/*summary.json

echo -e "\nBytes of memory definitely lost: $DEFINITELY_LOST"

if [[ "$DEFINITELY_LOST" -gt 0 ]]; then
      exit 1
fi
