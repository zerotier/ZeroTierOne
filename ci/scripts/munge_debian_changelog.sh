#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

export FILE=$1
export VERSION=$2
export NAME=$3
export MESSAGE=$4
export DATE=$(date "+%a, %d %b %Y %T %z")
# export DATE=$(date "+%a %b %d %Y")

set +e
grep --version | grep BSD &> /dev/null
if [ $? == 0 ]; then BSDGREP=true ; else BSDGREP=false ; fi
set -e

# echo "#~~~~~~~~~~~~~~~~~~~~"
# echo "$0 variables:"
# echo "VERSION: ${VERSION}"
# echo "NAME: ${NAME}"
# echo "MESSAGE: ${MESSAGE}"
# echo "DATE: ${DATE}"
# echo "BSDGREP: ${BSDGREP}"
# echo "#~~~~~~~~~~~~~~~~~~~~"
# echo

if $BSDGREP ; then    
    sed -i '' s/^Version:.*/"Version:        ${VERSION}"/ ${FILE}
else
    sed -i s/^Version:.*/"Version:        ${VERSION}"/ ${FILE}
fi

awk -v version=${VERSION} -v date=${DATE} -v name=${NAME} -v message=${MESSAGE} \
    'BEGIN{print "bytey (" version ") unstable; urgency=medium\n\n  * " message "\n\n -- " name "  " date "\n" }{ print }' \
    ${FILE} > ${FILE}.new

mv ${FILE}.new ${FILE}
