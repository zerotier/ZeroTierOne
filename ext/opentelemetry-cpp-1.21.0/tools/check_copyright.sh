#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if [[ ! -e tools/check_copyright.sh ]]; then
  echo "This tool must be run from the topmost directory." >&2
  exit 1
fi

set -e

#
# Process input file .copyright-ignore,
# - remove comments
# - remove blank lines
# to create file /tmp/all_ignored
#

grep -v "^#" < .copyright-ignore | \
grep -v "^[[:space:]]*$" > /tmp/all_ignored

#
# Find all files from the repository
# to create file /tmp/all_checked
#

find . -type f -print | sort -u > /tmp/all_checked

#
# Filter out /tmp/all_checked,
# remove all ignored patterns from /tmp/all_ignored
# When the pattern is *.md,
# make sure to filter *\.md to avoid hiding *.cmd
# Then, *\.md needs to be escaped to *\\.md,
# to be given to egrep, hence the sed.
# 

while IFS= read -r PATTERN; do
  SAFE_PATTERN=`echo "${PATTERN}" | sed "s!\.!\\\\\.!g"`
  echo "Filtering out ${SAFE_PATTERN}"
  egrep -v "${SAFE_PATTERN}" < /tmp/all_checked > /tmp/all_checked-tmp
  mv /tmp/all_checked-tmp /tmp/all_checked
done < /tmp/all_ignored

#
# For all files in /tmp/all_checked
# - verify there is copyright
# - verify there is a license
# and append to /tmp/all_missing
#
# Valid copyright strings are:
# - Copyright The OpenTelemetry Authors
#
# Valid license strings are:
# - SPDX-License-Identifier: Apache-2.0
#

rm -rf /tmp/all_missing
touch /tmp/all_missing

for FILE in `cat /tmp/all_checked`
do
  echo "Checking ${FILE}"
  export COPYRIGHT=`head -10 ${FILE} | grep -c "Copyright The OpenTelemetry Authors"`
  export LICENSE=`head -10 ${FILE} | grep -c "SPDX-License-Identifier: Apache-2.0"`
  if [ "$COPYRIGHT" == "0" ]; then
    echo "Missing copyright in ${FILE}" >> /tmp/all_missing
  fi;
  if [ "${LICENSE}" == "0" ]; then
    echo "Missing license in ${FILE}" >> /tmp/all_missing
  fi;
done

#
# Final report
#

FAIL_COUNT=`wc -l < /tmp/all_missing`

if [ ${FAIL_COUNT} != "0" ]; then
  #
  # CI FAILED
  #

  cat /tmp/all_missing

  echo "Total number of failed checks: ${FAIL_COUNT}"
  exit 1
fi;

#
# CI PASSED
#

exit 0

