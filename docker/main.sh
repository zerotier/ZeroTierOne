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
if [ -z "$ZT_DB_NAME" ]; then
    echo '*** FAILED: ZT_DB_NAME environment variable not defined'
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

RMQ=""
if [ "$ZT_USE_RABBITMQ" == "true" ]; then
    if [ -z "$RABBITMQ_HOST" ]; then
        echo '*** FAILED: RABBITMQ_HOST environment variable not defined'
        exit 1
    fi
    if [ -z "$RABBITMQ_PORT" ]; then
        echo '*** FAILED: RABBITMQ_PORT environment variable not defined'
        exit 1
    fi
    if [ -z "$RABBITMQ_USERNAME" ]; then
        echo '*** FAILED: RABBITMQ_USERNAME environment variable not defined'
        exit 1
    fi
    if [ -z "$RABBITMQ_PASSWORD" ]; then
        echo '*** FAILED: RABBITMQ_PASSWORD environment variable not defined'
        exit 1
    fi
    RMQ=", \"rabbitmq\": {
        \"host\": \"${RABBITMQ_HOST}\",
        \"port\": ${RABBITMQ_PORT},
        \"username\": \"${RABBITMQ_USERNAME}\",
        \"password\": \"${RABBITMQ_PASSWORD}\"
    }"
fi

mkdir -p /var/lib/zerotier-one

pushd /var/lib/zerotier-one
ln -s $ZT_IDENTITY_PATH/identity.public identity.public
ln -s $ZT_IDENTITY_PATH/identity.secret identity.secret
popd

DEFAULT_PORT=9993

echo "{
    \"settings\": {
        \"portMappingEnabled\": true,
        \"softwareUpdate\": \"disable\",
        \"interfacePrefixBlacklist\": [
            \"inot\",
            \"nat64\"
        ],
        \"controllerDbPath\": \"postgres:host=${ZT_DB_HOST} port=${ZT_DB_PORT} dbname=${ZT_DB_NAME} user=${ZT_DB_USER} password=${ZT_DB_PASSWORD} sslmode=prefer sslcert=${DB_CLIENT_CERT} sslkey=${DB_CLIENT_KEY} sslrootcert=${DB_SERVER_CA}\"
        ${RMQ}
    }
}    
" > /var/lib/zerotier-one/local.conf

export GLIBCXX_FORCE_NEW=1
export GLIBCPP_FORCE_NEW=1
export LD_PRELOAD="/usr/lib64/libjemalloc.so"
exec /usr/local/bin/zerotier-one -p${ZT_CONTROLLER_PORT:-$DEFAULT_PORT} /var/lib/zerotier-one
