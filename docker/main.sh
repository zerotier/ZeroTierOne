#!/usr/bin/env bash

if [ -z "$ZT_IDENTITY_PATH" ]; then
    echo '*** FAILED: ZT_IDENTITY_PATH environment variable is not defined'
    exit 1
fi
if [ -z "$ZT_DB_HOST" ]; then
    echo '*** FAILED: ZT_DB_HOST environment variable not defined'
    exit 1
fi
if [ -z "$ZT_DB_PORT" ]; then
    echo '*** FAILED: ZT_DB_PORT environment variable not defined'
    exit 1
fi
if [ -z "$ZT_DB_USER" ]; then
    echo '*** FAILED: ZT_DB_USER environment variable not defined'
    exit 1
fi
if [ -z "$ZT_DB_PASSWORD" ]; then
    echo '*** FAILED: ZT_DB_PASSWORD environment variable not defined'
    exit 1
fi

mkdir -p /var/lib/zerotier-one

pushd /var/lib/zerotier-one
ln -s $ZT_IDENTITY_PATH/identity.public identity.public
ln -s $ZT_IDENTITY_PATH/identity.secret identity.secret
popd

echo "{
    \"settings\": {
        \"portMappingEnabled\": true,
        \"softwareUpdate\": \"disable\",
        \"interfadePrefixBlacklist\": [
            \"inot\",
            \"nat64\"
        ],
        \"controllerDbPath\": \"postgres:host=${ZT_DB_HOST} port=${ZT_DB_PORT} dbname=ztc user=${ZT_DB_USER} password=${ZT_DB_PASSWORD}\"
    }
}    
" > /var/lib/zerotier-one/local.conf

# export ASAN_OPTIONS=detect_leaks=1
# export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
# export MSAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer
exec /usr/local/bin/zerotier-one /var/lib/zerotier-one
