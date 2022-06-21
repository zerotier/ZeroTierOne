#!/bin/bash

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

REDIS=""
if [ "$ZT_USE_REDIS" == "true" ]; then
    if [ -z "$ZT_REDIS_HOST" ]; then
        echo '*** FAILED: ZT_REDIS_HOST environment variable not defined'
        exit 1
    fi

    if [ -z "$ZT_REDIS_PORT" ]; then
        echo '*** FAILED: ZT_REDIS_PORT enivronment variable not defined'
        exit 1
    fi

    if [ -z "$ZT_REDIS_CLUSTER_MODE" ]; then
        echo '*** FAILED: ZT_REDIS_CLUSTER_MODE environment variable not defined'
        exit 1
    fi

    REDIS="\"redis\": {
            \"hostname\": \"${ZT_REDIS_HOST}\",
            \"port\": ${ZT_REDIS_PORT},
            \"clusterMode\": ${ZT_REDIS_CLUSTER_MODE},
            \"password\": \"${ZT_REDIS_PASSWORD}\"
        }
    "
else
    REDIS="\"redis\": null"
fi

mkdir -p /var/lib/zerotier-one

pushd /var/lib/zerotier-one
ln -s $ZT_IDENTITY_PATH/identity.public identity.public
ln -s $ZT_IDENTITY_PATH/identity.secret identity.secret
popd

DEFAULT_PORT=9993

APP_NAME="controller-$(cat /var/lib/zerotier-one/identity.public | cut -d ':' -f 1)"

echo "{
    \"settings\": {
        \"controllerDbPath\": \"postgres:host=${ZT_DB_HOST} port=${ZT_DB_PORT} dbname=${ZT_DB_NAME} user=${ZT_DB_USER} password=${ZT_DB_PASSWORD} application_name=${APP_NAME} sslmode=prefer sslcert=${DB_CLIENT_CERT} sslkey=${DB_CLIENT_KEY} sslrootcert=${DB_SERVER_CA}\",
        \"portMappingEnabled\": true,
        \"softwareUpdate\": \"disable\",
        \"interfacePrefixBlacklist\": [
            \"inot\",
            \"nat64\"
        ],
        \"ssoRedirectURL\": \"${ZT_SSO_REDIRECT_URL}\",
        ${REDIS}
    }
}    
" > /var/lib/zerotier-one/local.conf

if [ -n "$DB_SERVER_CA" ]; then
    until /usr/pgsql-10/bin/pg_isready -h ${ZT_DB_HOST} -p ${ZT_DB_PORT} -d "sslmode=prefer sslcert=${DB_CLIENT_CERT} sslkey=${DB_CLIENT_KEY} sslrootcert=${DB_SERVER_CA}"; do
	    echo "Waiting for PostgreSQL...";
	    sleep 2;
    done
else
    until /usr/pgsql-10/bin/pg_isready -h ${ZT_DB_HOST} -p ${ZT_DB_PORT}; do
	    echo "Waiting for PostgreSQL...";
	    sleep 2;
    done
fi

export GLIBCXX_FORCE_NEW=1
export GLIBCPP_FORCE_NEW=1
export LD_PRELOAD="/usr/lib64/libjemalloc.so"
exec /usr/local/bin/zerotier-one -p${ZT_CONTROLLER_PORT:-$DEFAULT_PORT} /var/lib/zerotier-one
