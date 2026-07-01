#!/bin/bash

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

    REDIS=", \"redis\": {
            \"hostname\": \"${ZT_REDIS_HOST}\",
            \"port\": ${ZT_REDIS_PORT},
            \"clusterMode\": ${ZT_REDIS_CLUSTER_MODE},
            \"password\": \"${ZT_REDIS_PASSWORD}\"
        }
    "
else
    REDIS=", \"redis\": null"
fi

mkdir -p /var/lib/zerotier-one

pushd /var/lib/zerotier-one
if [ -d "$ZT_IDENTITY_PATH" ]; then
    echo '*** Using existing ZT identity from path $ZT_IDENTITY_PATH'

    ln -s $ZT_IDENTITY_PATH/identity.public identity.public
    ln -s $ZT_IDENTITY_PATH/identity.secret identity.secret
    if [ -L  "$ZT_IDENTITY_PATH/authtoken.secret" ] && [ -e "$ZT_IDENTITY_PATH/authtoken.secret" ]; then
        ln -s $ZT_IDENTITY_PATH/authtoken.secret authtoken.secret
        ln -s $ZT_IDENTITY_PATH/authtoken.secret metricstoken.secret
    fi
fi
popd

DEFAULT_PORT=9993
DEFAULT_LB_MODE=false

APP_NAME="controller-$(cat /var/lib/zerotier-one/identity.public | cut -d ':' -f 1)"

BIGTABLE_CONF=""
if [ "$ZT_USE_BIGTABLE" == "true" ]; then
    if [ -z "$ZT_BIGTABLE_PROJECT" ] || [ -z "$ZT_BIGTABLE_INSTANCE" ] || [ -z "$ZT_BIGTABLE_TABLE" ]; then
        echo '*** FAILED: ZT_BIGTABLE_PROJECT, ZT_BIGTABLE_INSTANCE, and ZT_BIGTABLE_TABLE environment variables must all be defined to use Bigtable as a controller backend'
        exit 1
    fi

    BIGTABLE_CONF=", \"bigtable\": {
        \"project_id\": \"${ZT_BIGTABLE_PROJECT}\",
        \"instance_id\": \"${ZT_BIGTABLE_INSTANCE}\",
        \"table_id\": \"${ZT_BIGTABLE_TABLE}\"
    }
    "
fi


PUBSUB_CONF=""
if [ "$ZT_USE_PUBSUB" == "true" ]; then
    if [ -z "$ZT_PUBSUB_PROJECT" ]; then
        echo '*** FAILED: ZT_PUBSUB_PROJECT environment variable must be defined to use PubSub as a controller backend'
        exit 1
    fi

    if [ -z "$ZT_PUBSUB_MEMBER_CHANGE_RECV_TOPIC" ] || [ -z "$ZT_PUBSUB_MEMBER_CHANGE_SEND_TOPIC" ] || [ -z "$ZT_PUBSUB_NETWORK_CHANGE_RECV_TOPIC" ] || [ -z "$ZT_PUBSUB_NETWORK_CHANGE_SEND_TOPIC" ]; then
        echo '*** FAILED: ZT_PUBSUB_MEMBER_CHANGE_RECV_TOPIC, ZT_PUBSUB_MEMBER_CHANGE_SEND_TOPIC, ZT_PUBSUB_NETWORK_CHANGE_RECV_TOPIC, and ZT_PUBSUB_NETWORK_CHANGE_SEND_TOPIC environment variables must all be defined to use PubSub as a controller backend'
        exit 1
    fi

    PUBSUB_CONF=", \"pubsub\": {
        \"project_id\": \"${ZT_PUBSUB_PROJECT}\",
        \"member_change_recv_topic\": \"${ZT_PUBSUB_MEMBER_CHANGE_RECV_TOPIC}\",
        \"member_change_send_topic\": \"${ZT_PUBSUB_MEMBER_CHANGE_SEND_TOPIC}\",
        \"network_change_recv_topic\": \"${ZT_PUBSUB_NETWORK_CHANGE_RECV_TOPIC}\",
        \"network_change_send_topic\": \"${ZT_PUBSUB_NETWORK_CHANGE_SEND_TOPIC}\",
        \"sso_nonce_publish_topic\": \"${ZT_PUBSUB_SSO_NONCE_PUBLISH_TOPIC}\",
        \"sso_auth_subscribe_topic\": \"${ZT_PUBSUB_SSO_AUTH_SUBSCRIBE_TOPIC}\"
    }
"
fi

# Backwards compatibility: if the per-frontend URLs aren't set but the legacy
# ZT_SSO_REDIRECT_URL and ZT_ASSIGNED_CENTRAL_VERSION are both populated, map
# the legacy URL into the new format keyed by the assigned Central version.
if [ -z "$ZT_SSO_REDIRECT_URL_CV1" ] && [ -z "$ZT_SSO_REDIRECT_URL_CV2" ]; then
    if [ -n "$ZT_SSO_REDIRECT_URL" ] && [ -n "$ZT_ASSIGNED_CENTRAL_VERSION" ]; then
        case "$ZT_ASSIGNED_CENTRAL_VERSION" in
            cv1)
                ZT_SSO_REDIRECT_URL_CV1="$ZT_SSO_REDIRECT_URL"
                ;;
            cv2)
                ZT_SSO_REDIRECT_URL_CV2="$ZT_SSO_REDIRECT_URL"
                ;;
            all)
                echo '*** FAILED: controllers assigned to "all" must define ZT_SSO_REDIRECT_URL_CV1 and ZT_SSO_REDIRECT_URL_CV2; the legacy ZT_SSO_REDIRECT_URL cannot serve both Central versions'
                exit 1
                ;;
        esac
    fi
fi

# Per-frontend Central SSO redirect URLs. SSO-enabled networks are routed to the
# SSO endpoint of the Central version ("cv1" or "cv2") they belong to.
SSO_REDIRECT_URLS_CONF=""
if [ -n "$ZT_SSO_REDIRECT_URL_CV1" ] || [ -n "$ZT_SSO_REDIRECT_URL_CV2" ]; then
    SSO_REDIRECT_URLS=""
    if [ -n "$ZT_SSO_REDIRECT_URL_CV1" ]; then
        SSO_REDIRECT_URLS="\"cv1\": \"${ZT_SSO_REDIRECT_URL_CV1}\""
    fi
    if [ -n "$ZT_SSO_REDIRECT_URL_CV2" ]; then
        if [ -n "$SSO_REDIRECT_URLS" ]; then
            SSO_REDIRECT_URLS="${SSO_REDIRECT_URLS},
        "
        fi
        SSO_REDIRECT_URLS="${SSO_REDIRECT_URLS}\"cv2\": \"${ZT_SSO_REDIRECT_URL_CV2}\""
    fi
    SSO_REDIRECT_URLS_CONF=", \"ssoRedirectURLs\": {
        ${SSO_REDIRECT_URLS}
    }
"
fi

echo "{
    \"settings\": {
        \"controllerDbPath\": \"postgres:host=${ZT_DB_HOST} port=${ZT_DB_PORT} dbname=${ZT_DB_NAME} user=${ZT_DB_USER} password=${ZT_DB_PASSWORD} application_name=${APP_NAME} sslmode=prefer sslcert=${DB_CLIENT_CERT} sslkey=${DB_CLIENT_KEY} sslrootcert=${DB_SERVER_CA}\",
        \"portMappingEnabled\": true,
        \"softwareUpdate\": \"disable\",
        \"interfacePrefixBlacklist\": [
            \"inot\",
            \"nat64\"
        ],
        \"lowBandwidthMode\": ${ZT_LB_MODE:-$DEFAULT_LB_MODE},
        \"ssoRedirectURL\": \"${ZT_SSO_REDIRECT_URL}\",
        \"allowManagementFrom\": [\"127.0.0.1\", \"::1\", \"10.0.0.0/8\"],
        \"enableMetrics\": ${ZT_ENABLE_METRICS:-true},
        \"otel\": {
            \"exporterEndpoint\": \"${ZT_EXPORTER_ENDPOINT}\",
            \"exporterSampleRate\": ${ZT_EXPORTER_SAMPLE_RATE:-0}
        }
        ${REDIS}
    },
    \"controller\": {
        \"listenMode\": \"${ZT_LISTEN_MODE:-pgsql}\",
        \"statusMode\": \"${ZT_STATUS_MODE:-pgsql}\",
        \"assignedCentralVersion\": \"${ZT_ASSIGNED_CENTRAL_VERSION:-all}\",
        \"ssoEnabled\": ${ZT_SSO_ENABLED:-false}
        ${SSO_REDIRECT_URLS_CONF}
        ${REDIS}
        ${BIGTABLE_CONF}
        ${PUBSUB_CONF}
    }
}
" > /var/lib/zerotier-one/local.conf

if [ -n "$DB_SERVER_CA" ]; then
    echo "secret list"
    chmod 600 /secrets/db/*.pem
    ls -l /secrets/db/
    until pg_isready -h ${ZT_DB_HOST} -p ${ZT_DB_PORT} -d "sslmode=prefer sslcert=${DB_CLIENT_CERT} sslkey=${DB_CLIENT_KEY} sslrootcert=${DB_SERVER_CA}"; do
	    echo "Waiting for PostgreSQL...";
	    sleep 2;
    done
else
    until pg_isready -h ${ZT_DB_HOST} -p ${ZT_DB_PORT}; do
	    echo "Waiting for PostgreSQL...";
	    sleep 2;
    done
fi


# echo "Migrating database (if needed)..."
# if [ -n "$DB_SERVER_CA" ]; then
#     /usr/local/bin/migrate -source file:///migrations -database "postgres://$ZT_DB_USER:$ZT_DB_PASSWORD@$ZT_DB_HOST:$ZT_DB_PORT/$ZT_DB_NAME?x-migrations-table=controller_migrations&sslmode=verify-full&sslrootcert=$DB_SERVER_CA&sslcert=$DB_CLIENT_CERT&sslkey=$DB_CLIENT_KEY" up
# else
#     /usr/local/bin/migrate -source file:///migrations -database "postgres://$ZT_DB_USER:$ZT_DB_PASSWORD@$ZT_DB_HOST:$ZT_DB_PORT/$ZT_DB_NAME?x-migrations-table=controller_migrations&sslmode=disable" up
# fi

if [ -n "$ZT_TEMPORAL_HOST" ] && [ -n "$ZT_TEMPORAL_PORT" ]; then
    echo "waiting for temporal..."
    while ! nc -z ${ZT_TEMPORAL_HOST} ${ZT_TEMPORAL_PORT}; do
        echo "waiting...";
        sleep 1;
    done
    echo "Temporal is up"
fi

cat /var/lib/zerotier-one/local.conf

export GOOGLE_CLOUD_CPP_ENABLE_CLOG=yes
export LIBC_FATAL_STDERR_=1
export GLIBCXX_FORCE_NEW=1
export GLIBCPP_FORCE_NEW=1
# Preload jemalloc, located arch-agnostically (libjemalloc2 installs it under the
# multiarch dir, e.g. /usr/lib/<triple>/libjemalloc.so.2). Skip if not present.
JEMALLOC="$(ldconfig -p 2>/dev/null | awk '/libjemalloc\.so\.2/ {print $NF; exit}')"
if [ -n "$JEMALLOC" ]; then
    export LD_PRELOAD="$JEMALLOC"
fi
exec /usr/local/bin/zerotier-one -p${ZT_CONTROLLER_PORT:-$DEFAULT_PORT} /var/lib/zerotier-one
