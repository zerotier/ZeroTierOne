/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#include "redis_cluster.h"
#include <hiredis/hiredis.h>
#include "command.h"
#include "errors.h"
#include "queued_redis.h"

namespace sw {

namespace redis {

RedisCluster::RedisCluster(const std::string &uri) : RedisCluster(ConnectionOptions(uri)) {}

Redis RedisCluster::redis(const StringView &hash_tag, bool new_connection) {
    auto pool = _pool.fetch(hash_tag);
    if (new_connection) {
        // Create a new pool
        pool = std::make_shared<ConnectionPool>(pool->clone());
    }

    return Redis(std::make_shared<GuardedConnection>(pool));
}

Pipeline RedisCluster::pipeline(const StringView &hash_tag, bool new_connection) {
    auto pool = _pool.fetch(hash_tag);
    if (new_connection) {
        // Create a new pool
        pool = std::make_shared<ConnectionPool>(pool->clone());
    }

    return Pipeline(pool, new_connection);
}

Transaction RedisCluster::transaction(const StringView &hash_tag, bool piped, bool new_connection) {
    auto pool = _pool.fetch(hash_tag);
    if (new_connection) {
        // Create a new pool
        pool = std::make_shared<ConnectionPool>(pool->clone());
    }

    return Transaction(pool, new_connection, piped);
}

Subscriber RedisCluster::subscriber() {
    auto opts = _pool.connection_options();
    return Subscriber(Connection(opts));
}

// KEY commands.

long long RedisCluster::del(const StringView &key) {
    auto reply = command(cmd::del, key);

    return reply::parse<long long>(*reply);
}

OptionalString RedisCluster::dump(const StringView &key) {
    auto reply = command(cmd::dump, key);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::exists(const StringView &key) {
    auto reply = command(cmd::exists, key);

    return reply::parse<long long>(*reply);
}

bool RedisCluster::expire(const StringView &key, long long timeout) {
    auto reply = command(cmd::expire, key, timeout);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::expireat(const StringView &key, long long timestamp) {
    auto reply = command(cmd::expireat, key, timestamp);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::persist(const StringView &key) {
    auto reply = command(cmd::persist, key);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::pexpire(const StringView &key, long long timeout) {
    auto reply = command(cmd::pexpire, key, timeout);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::pexpireat(const StringView &key, long long timestamp) {
    auto reply = command(cmd::pexpireat, key, timestamp);

    return reply::parse<bool>(*reply);
}

long long RedisCluster::pttl(const StringView &key) {
    auto reply = command(cmd::pttl, key);

    return reply::parse<long long>(*reply);
}

void RedisCluster::rename(const StringView &key, const StringView &newkey) {
    auto reply = command(cmd::rename, key, newkey);

    reply::parse<void>(*reply);
}

bool RedisCluster::renamenx(const StringView &key, const StringView &newkey) {
    auto reply = command(cmd::renamenx, key, newkey);

    return reply::parse<bool>(*reply);
}

void RedisCluster::restore(const StringView &key,
                    const StringView &val,
                    long long ttl,
                    bool replace) {
    auto reply = command(cmd::restore, key, val, ttl, replace);

    reply::parse<void>(*reply);
}

long long RedisCluster::touch(const StringView &key) {
    auto reply = command(cmd::touch, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::ttl(const StringView &key) {
    auto reply = command(cmd::ttl, key);

    return reply::parse<long long>(*reply);
}

std::string RedisCluster::type(const StringView &key) {
    auto reply = command(cmd::type, key);

    return reply::parse<std::string>(*reply);
}

long long RedisCluster::unlink(const StringView &key) {
    auto reply = command(cmd::unlink, key);

    return reply::parse<long long>(*reply);
}

// STRING commands.

long long RedisCluster::append(const StringView &key, const StringView &val) {
    auto reply = command(cmd::append, key, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::bitcount(const StringView &key, long long start, long long end) {
    auto reply = command(cmd::bitcount, key, start, end);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::bitop(BitOp op, const StringView &destination, const StringView &key) {
    auto reply = _command(cmd::bitop, destination, op, destination, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::bitpos(const StringView &key,
                            long long bit,
                            long long start,
                            long long end) {
    auto reply = command(cmd::bitpos, key, bit, start, end);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::decr(const StringView &key) {
    auto reply = command(cmd::decr, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::decrby(const StringView &key, long long decrement) {
    auto reply = command(cmd::decrby, key, decrement);

    return reply::parse<long long>(*reply);
}

OptionalString RedisCluster::get(const StringView &key) {
    auto reply = command(cmd::get, key);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::getbit(const StringView &key, long long offset) {
    auto reply = command(cmd::getbit, key, offset);

    return reply::parse<long long>(*reply);
}

std::string RedisCluster::getrange(const StringView &key, long long start, long long end) {
    auto reply = command(cmd::getrange, key, start, end);

    return reply::parse<std::string>(*reply);
}

OptionalString RedisCluster::getset(const StringView &key, const StringView &val) {
    auto reply = command(cmd::getset, key, val);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::incr(const StringView &key) {
    auto reply = command(cmd::incr, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::incrby(const StringView &key, long long increment) {
    auto reply = command(cmd::incrby, key, increment);

    return reply::parse<long long>(*reply);
}

double RedisCluster::incrbyfloat(const StringView &key, double increment) {
    auto reply = command(cmd::incrbyfloat, key, increment);

    return reply::parse<double>(*reply);
}

void RedisCluster::psetex(const StringView &key,
                        long long ttl,
                        const StringView &val) {
    auto reply = command(cmd::psetex, key, ttl, val);

    reply::parse<void>(*reply);
}

bool RedisCluster::set(const StringView &key,
                    const StringView &val,
                    const std::chrono::milliseconds &ttl,
                    UpdateType type) {
    auto reply = command(cmd::set, key, val, ttl.count(), type);

    reply::rewrite_set_reply(*reply);

    return reply::parse<bool>(*reply);
}

void RedisCluster::setex(const StringView &key,
                    long long ttl,
                    const StringView &val) {
    auto reply = command(cmd::setex, key, ttl, val);

    reply::parse<void>(*reply);
}

bool RedisCluster::setnx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::setnx, key, val);

    return reply::parse<bool>(*reply);
}

long long RedisCluster::setrange(const StringView &key, long long offset, const StringView &val) {
    auto reply = command(cmd::setrange, key, offset, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::strlen(const StringView &key) {
    auto reply = command(cmd::strlen, key);

    return reply::parse<long long>(*reply);
}

// LIST commands.

OptionalStringPair RedisCluster::blpop(const StringView &key, long long timeout) {
    auto reply = command(cmd::blpop, key, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

OptionalStringPair RedisCluster::blpop(const StringView &key, const std::chrono::seconds &timeout) {
    return blpop(key, timeout.count());
}

OptionalStringPair RedisCluster::brpop(const StringView &key, long long timeout) {
    auto reply = command(cmd::brpop, key, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

OptionalStringPair RedisCluster::brpop(const StringView &key, const std::chrono::seconds &timeout) {
    return brpop(key, timeout.count());
}

OptionalString RedisCluster::brpoplpush(const StringView &source,
                                    const StringView &destination,
                                    long long timeout) {
    auto reply = command(cmd::brpoplpush, source, destination, timeout);

    return reply::parse<OptionalString>(*reply);
}

OptionalString RedisCluster::lindex(const StringView &key, long long index) {
    auto reply = command(cmd::lindex, key, index);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::linsert(const StringView &key,
                            InsertPosition position,
                            const StringView &pivot,
                            const StringView &val) {
    auto reply = command(cmd::linsert, key, position, pivot, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::llen(const StringView &key) {
    auto reply = command(cmd::llen, key);

    return reply::parse<long long>(*reply);
}

OptionalString RedisCluster::lpop(const StringView &key) {
    auto reply = command(cmd::lpop, key);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::lpush(const StringView &key, const StringView &val) {
    auto reply = command(cmd::lpush, key, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::lpushx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::lpushx, key, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::lrem(const StringView &key, long long count, const StringView &val) {
    auto reply = command(cmd::lrem, key, count, val);

    return reply::parse<long long>(*reply);
}

void RedisCluster::lset(const StringView &key, long long index, const StringView &val) {
    auto reply = command(cmd::lset, key, index, val);

    reply::parse<void>(*reply);
}

void RedisCluster::ltrim(const StringView &key, long long start, long long stop) {
    auto reply = command(cmd::ltrim, key, start, stop);

    reply::parse<void>(*reply);
}

OptionalString RedisCluster::rpop(const StringView &key) {
    auto reply = command(cmd::rpop, key);

    return reply::parse<OptionalString>(*reply);
}

OptionalString RedisCluster::rpoplpush(const StringView &source, const StringView &destination) {
    auto reply = command(cmd::rpoplpush, source, destination);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::rpush(const StringView &key, const StringView &val) {
    auto reply = command(cmd::rpush, key, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::rpushx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::rpushx, key, val);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::hdel(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hdel, key, field);

    return reply::parse<long long>(*reply);
}

bool RedisCluster::hexists(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hexists, key, field);

    return reply::parse<bool>(*reply);
}

OptionalString RedisCluster::hget(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hget, key, field);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::hincrby(const StringView &key, const StringView &field, long long increment) {
    auto reply = command(cmd::hincrby, key, field, increment);

    return reply::parse<long long>(*reply);
}

double RedisCluster::hincrbyfloat(const StringView &key, const StringView &field, double increment) {
    auto reply = command(cmd::hincrbyfloat, key, field, increment);

    return reply::parse<double>(*reply);
}

long long RedisCluster::hlen(const StringView &key) {
    auto reply = command(cmd::hlen, key);

    return reply::parse<long long>(*reply);
}

bool RedisCluster::hset(const StringView &key, const StringView &field, const StringView &val) {
    auto reply = command(cmd::hset, key, field, val);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::hset(const StringView &key, const std::pair<StringView, StringView> &item) {
    return hset(key, item.first, item.second);
}

bool RedisCluster::hsetnx(const StringView &key, const StringView &field, const StringView &val) {
    auto reply = command(cmd::hsetnx, key, field, val);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::hsetnx(const StringView &key, const std::pair<StringView, StringView> &item) {
    return hsetnx(key, item.first, item.second);
}

long long RedisCluster::hstrlen(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hstrlen, key, field);

    return reply::parse<long long>(*reply);
}

// SET commands.

long long RedisCluster::sadd(const StringView &key, const StringView &member) {
    auto reply = command(cmd::sadd, key, member);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::scard(const StringView &key) {
    auto reply = command(cmd::scard, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::sdiffstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sdiffstore, destination, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::sinterstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sinterstore, destination, key);

    return reply::parse<long long>(*reply);
}

bool RedisCluster::sismember(const StringView &key, const StringView &member) {
    auto reply = command(cmd::sismember, key, member);

    return reply::parse<bool>(*reply);
}

bool RedisCluster::smove(const StringView &source,
                    const StringView &destination,
                    const StringView &member) {
    auto reply = command(cmd::smove, source, destination, member);

    return reply::parse<bool>(*reply);
}

OptionalString RedisCluster::spop(const StringView &key) {
    auto reply = command(cmd::spop, key);

    return reply::parse<OptionalString>(*reply);
}

OptionalString RedisCluster::srandmember(const StringView &key) {
    auto reply = command(cmd::srandmember, key);

    return reply::parse<OptionalString>(*reply);
}

long long RedisCluster::srem(const StringView &key, const StringView &member) {
    auto reply = command(cmd::srem, key, member);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::sunionstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sunionstore, destination, key);

    return reply::parse<long long>(*reply);
}

// SORTED SET commands.

auto RedisCluster::bzpopmax(const StringView &key, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmax, key, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

auto RedisCluster::bzpopmin(const StringView &key, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmin, key, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

long long RedisCluster::zadd(const StringView &key,
                        const StringView &member,
                        double score,
                        UpdateType type,
                        bool changed) {
    auto reply = command(cmd::zadd, key, member, score, type, changed);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::zcard(const StringView &key) {
    auto reply = command(cmd::zcard, key);

    return reply::parse<long long>(*reply);
}

double RedisCluster::zincrby(const StringView &key, double increment, const StringView &member) {
    auto reply = command(cmd::zincrby, key, increment, member);

    return reply::parse<double>(*reply);
}

long long RedisCluster::zinterstore(const StringView &destination,
                                    const StringView &key,
                                    double weight) {
    auto reply = command(cmd::zinterstore, destination, key, weight);

    return reply::parse<long long>(*reply);
}

Optional<std::pair<std::string, double>> RedisCluster::zpopmax(const StringView &key) {
    auto reply = command(cmd::zpopmax, key, 1);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<Optional<std::pair<std::string, double>>>(*reply);
}

Optional<std::pair<std::string, double>> RedisCluster::zpopmin(const StringView &key) {
    auto reply = command(cmd::zpopmin, key, 1);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<Optional<std::pair<std::string, double>>>(*reply);
}

OptionalLongLong RedisCluster::zrank(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrank, key, member);

    return reply::parse<OptionalLongLong>(*reply);
}

long long RedisCluster::zrem(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrem, key, member);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::zremrangebyrank(const StringView &key, long long start, long long stop) {
    auto reply = command(cmd::zremrangebyrank, key, start, stop);

    return reply::parse<long long>(*reply);
}

OptionalLongLong RedisCluster::zrevrank(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrevrank, key, member);

    return reply::parse<OptionalLongLong>(*reply);
}

OptionalDouble RedisCluster::zscore(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zscore, key, member);

    return reply::parse<OptionalDouble>(*reply);
}

long long RedisCluster::zunionstore(const StringView &destination,
                                    const StringView &key,
                                    double weight) {
    auto reply = command(cmd::zunionstore, destination, key, weight);

    return reply::parse<long long>(*reply);
}

// HYPERLOGLOG commands.

bool RedisCluster::pfadd(const StringView &key, const StringView &element) {
    auto reply = command(cmd::pfadd, key, element);

    return reply::parse<bool>(*reply);
}

long long RedisCluster::pfcount(const StringView &key) {
    auto reply = command(cmd::pfcount, key);

    return reply::parse<long long>(*reply);
}

void RedisCluster::pfmerge(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::pfmerge, destination, key);

    reply::parse<void>(*reply);
}

// GEO commands.

long long RedisCluster::geoadd(const StringView &key,
                        const std::tuple<StringView, double, double> &member) {
    auto reply = command(cmd::geoadd, key, member);

    return reply::parse<long long>(*reply);
}

OptionalDouble RedisCluster::geodist(const StringView &key,
                                const StringView &member1,
                                const StringView &member2,
                                GeoUnit unit) {
    auto reply = command(cmd::geodist, key, member1, member2, unit);

    return reply::parse<OptionalDouble>(*reply);
}

OptionalString RedisCluster::geohash(const StringView &key, const StringView &member) {
    auto reply = command(cmd::geohash, key, member);

    return reply::parse_leniently<OptionalString>(*reply);
}

Optional<std::pair<double, double>> RedisCluster::geopos(const StringView &key,
                                                            const StringView &member) {
    auto reply = command(cmd::geopos, key, member);

    return reply::parse_leniently<Optional<std::pair<double, double>>>(*reply);
}


OptionalLongLong RedisCluster::georadius(const StringView &key,
                                    const std::pair<double, double> &loc,
                                    double radius,
                                    GeoUnit unit,
                                    const StringView &destination,
                                    bool store_dist,
                                    long long count) {
    auto reply = command(cmd::georadius_store,
                            key,
                            loc,
                            radius,
                            unit,
                            destination,
                            store_dist,
                            count);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<OptionalLongLong>(*reply);
}

OptionalLongLong RedisCluster::georadiusbymember(const StringView &key,
                                            const StringView &member,
                                            double radius,
                                            GeoUnit unit,
                                            const StringView &destination,
                                            bool store_dist,
                                            long long count) {
    auto reply = command(cmd::georadiusbymember_store,
                            key,
                            member,
                            radius,
                            unit,
                            destination,
                            store_dist,
                            count);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<OptionalLongLong>(*reply);
}

// PUBSUB commands.

long long RedisCluster::publish(const StringView &channel, const StringView &message) {
    auto reply = command(cmd::publish, channel, message);

    return reply::parse<long long>(*reply);
}

// Stream commands.

long long RedisCluster::xack(const StringView &key, const StringView &group, const StringView &id) {
    auto reply = command(cmd::xack, key, group, id);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::xdel(const StringView &key, const StringView &id) {
    auto reply = command(cmd::xdel, key, id);

    return reply::parse<long long>(*reply);
}

void RedisCluster::xgroup_create(const StringView &key,
                                    const StringView &group,
                                    const StringView &id,
                                    bool mkstream) {
    auto reply = command(cmd::xgroup_create, key, group, id, mkstream);

    reply::parse<void>(*reply);
}

void RedisCluster::xgroup_setid(const StringView &key,
                                const StringView &group,
                                const StringView &id) {
    auto reply = command(cmd::xgroup_setid, key, group, id);

    reply::parse<void>(*reply);
}

long long RedisCluster::xgroup_destroy(const StringView &key, const StringView &group) {
    auto reply = command(cmd::xgroup_destroy, key, group);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::xgroup_delconsumer(const StringView &key,
                                            const StringView &group,
                                            const StringView &consumer) {
    auto reply = command(cmd::xgroup_delconsumer, key, group, consumer);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::xlen(const StringView &key) {
    auto reply = command(cmd::xlen, key);

    return reply::parse<long long>(*reply);
}

long long RedisCluster::xtrim(const StringView &key, long long count, bool approx) {
    auto reply = command(cmd::xtrim, key, count, approx);

    return reply::parse<long long>(*reply);
}

void RedisCluster::_asking(Connection &connection) {
    // Send ASKING command.
    connection.send("ASKING");

    auto reply = connection.recv();

    assert(reply);

    reply::parse<void>(*reply);
}

}

}
