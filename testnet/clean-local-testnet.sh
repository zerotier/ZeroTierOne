#!/bin/bash

export PATH=/bin:/usr/bin:/usr/local/bin

rm -rfv local-testnet/n????
find local-testnet -type f ! -name 'identity.*' -print0 | xargs -0 rm -fv
rm -rfv local-testnet/sn????/networks.d
