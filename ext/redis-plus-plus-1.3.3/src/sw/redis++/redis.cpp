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

#include "redis.h"
#include <hiredis/hiredis.h>
#include "command.h"
#include "errors.h"
#include "queued_redis.h"

namespace sw {

namespace redis {

Redis::Redis(const std::string &uri) : Redis(ConnectionOptions(uri)) {}

Redis::Redis(const GuardedConnectionSPtr &connection) : _connection(connection) {
    assert(_connection);
}

Pipeline Redis::pipeline(bool new_connection) {
    if (!_pool) {
        throw Error("cannot create pipeline in single connection mode");
    }

    return Pipeline(_pool, new_connection);
}

Transaction Redis::transaction(bool piped, bool new_connection) {
    if (!_pool) {
        throw Error("cannot create transaction in single connection mode");
    }

    return Transaction(_pool, new_connection, piped);
}

Subscriber Redis::subscriber() {
    if (!_pool) {
        throw Error("cannot create subscriber in single connection mode");
    }

    return Subscriber(_pool->create());
}

// CONNECTION commands.

void Redis::auth(const StringView &password) {
    auto reply = command<void (*)(Connection &, const StringView &)>(cmd::auth, password);

    reply::parse<void>(*reply);
}

void Redis::auth(const StringView &user, const StringView &password) {
    auto reply = command<void (*)(Connection &, const StringView &, const StringView &)>(
                        cmd::auth, user, password);

    reply::parse<void>(*reply);
}

std::string Redis::echo(const StringView &msg) {
    auto reply = command(cmd::echo, msg);

    return reply::parse<std::string>(*reply);
}

std::string Redis::ping() {
    auto reply = command<void (*)(Connection &)>(cmd::ping);

    return reply::to_status(*reply);
}

std::string Redis::ping(const StringView &msg) {
    auto reply = command<void (*)(Connection &, const StringView &)>(cmd::ping, msg);

    return reply::parse<std::string>(*reply);
}

void Redis::swapdb(long long idx1, long long idx2) {
    auto reply = command(cmd::swapdb, idx1, idx2);

    reply::parse<void>(*reply);
}

// SERVER commands.

void Redis::bgrewriteaof() {
    auto reply = command(cmd::bgrewriteaof);

    reply::parse<void>(*reply);
}

void Redis::bgsave() {
    auto reply = command(cmd::bgsave);

    reply::parse<void>(*reply);
}

long long Redis::dbsize() {
    auto reply = command(cmd::dbsize);

    return reply::parse<long long>(*reply);
}

void Redis::flushall(bool async) {
    auto reply = command(cmd::flushall, async);

    reply::parse<void>(*reply);
}

void Redis::flushdb(bool async) {
    auto reply = command(cmd::flushdb, async);

    reply::parse<void>(*reply);
}

std::string Redis::info() {
    auto reply = command<void (*)(Connection &)>(cmd::info);

    return reply::parse<std::string>(*reply);
}

std::string Redis::info(const StringView &section) {
    auto reply = command<void (*)(Connection &, const StringView &)>(cmd::info, section);

    return reply::parse<std::string>(*reply);
}

long long Redis::lastsave() {
    auto reply = command(cmd::lastsave);

    return reply::parse<long long>(*reply);
}

void Redis::save() {
    auto reply = command(cmd::save);

    reply::parse<void>(*reply);
}

// KEY commands.

long long Redis::del(const StringView &key) {
    auto reply = command(cmd::del, key);

    return reply::parse<long long>(*reply);
}

OptionalString Redis::dump(const StringView &key) {
    auto reply = command(cmd::dump, key);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::exists(const StringView &key) {
    auto reply = command(cmd::exists, key);

    return reply::parse<long long>(*reply);
}

bool Redis::expire(const StringView &key, long long timeout) {
    auto reply = command(cmd::expire, key, timeout);

    return reply::parse<bool>(*reply);
}

bool Redis::expireat(const StringView &key, long long timestamp) {
    auto reply = command(cmd::expireat, key, timestamp);

    return reply::parse<bool>(*reply);
}

bool Redis::move(const StringView &key, long long db) {
    auto reply = command(cmd::move, key, db);

    return reply::parse<bool>(*reply);
}

bool Redis::persist(const StringView &key) {
    auto reply = command(cmd::persist, key);

    return reply::parse<bool>(*reply);
}

bool Redis::pexpire(const StringView &key, long long timeout) {
    auto reply = command(cmd::pexpire, key, timeout);

    return reply::parse<bool>(*reply);
}

bool Redis::pexpireat(const StringView &key, long long timestamp) {
    auto reply = command(cmd::pexpireat, key, timestamp);

    return reply::parse<bool>(*reply);
}

long long Redis::pttl(const StringView &key) {
    auto reply = command(cmd::pttl, key);

    return reply::parse<long long>(*reply);
}

OptionalString Redis::randomkey() {
    auto reply = command(cmd::randomkey);

    return reply::parse<OptionalString>(*reply);
}

void Redis::rename(const StringView &key, const StringView &newkey) {
    auto reply = command(cmd::rename, key, newkey);

    reply::parse<void>(*reply);
}

bool Redis::renamenx(const StringView &key, const StringView &newkey) {
    auto reply = command(cmd::renamenx, key, newkey);

    return reply::parse<bool>(*reply);
}

void Redis::restore(const StringView &key,
                    const StringView &val,
                    long long ttl,
                    bool replace) {
    auto reply = command(cmd::restore, key, val, ttl, replace);

    reply::parse<void>(*reply);
}

long long Redis::touch(const StringView &key) {
    auto reply = command(cmd::touch, key);

    return reply::parse<long long>(*reply);
}

long long Redis::ttl(const StringView &key) {
    auto reply = command(cmd::ttl, key);

    return reply::parse<long long>(*reply);
}

std::string Redis::type(const StringView &key) {
    auto reply = command(cmd::type, key);

    return reply::parse<std::string>(*reply);
}

long long Redis::unlink(const StringView &key) {
    auto reply = command(cmd::unlink, key);

    return reply::parse<long long>(*reply);
}

long long Redis::wait(long long numslaves, long long timeout) {
    auto reply = command(cmd::wait, numslaves, timeout);

    return reply::parse<long long>(*reply);
}

// STRING commands.

long long Redis::append(const StringView &key, const StringView &val) {
    auto reply = command(cmd::append, key, val);

    return reply::parse<long long>(*reply);
}

long long Redis::bitcount(const StringView &key, long long start, long long end) {
    auto reply = command(cmd::bitcount, key, start, end);

    return reply::parse<long long>(*reply);
}

long long Redis::bitop(BitOp op, const StringView &destination, const StringView &key) {
    auto reply = command(cmd::bitop, op, destination, key);

    return reply::parse<long long>(*reply);
}

long long Redis::bitpos(const StringView &key,
                            long long bit,
                            long long start,
                            long long end) {
    auto reply = command(cmd::bitpos, key, bit, start, end);

    return reply::parse<long long>(*reply);
}

long long Redis::decr(const StringView &key) {
    auto reply = command(cmd::decr, key);

    return reply::parse<long long>(*reply);
}

long long Redis::decrby(const StringView &key, long long decrement) {
    auto reply = command(cmd::decrby, key, decrement);

    return reply::parse<long long>(*reply);
}

OptionalString Redis::get(const StringView &key) {
    auto reply = command(cmd::get, key);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::getbit(const StringView &key, long long offset) {
    auto reply = command(cmd::getbit, key, offset);

    return reply::parse<long long>(*reply);
}

std::string Redis::getrange(const StringView &key, long long start, long long end) {
    auto reply = command(cmd::getrange, key, start, end);

    return reply::parse<std::string>(*reply);
}

OptionalString Redis::getset(const StringView &key, const StringView &val) {
    auto reply = command(cmd::getset, key, val);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::incr(const StringView &key) {
    auto reply = command(cmd::incr, key);

    return reply::parse<long long>(*reply);
}

long long Redis::incrby(const StringView &key, long long increment) {
    auto reply = command(cmd::incrby, key, increment);

    return reply::parse<long long>(*reply);
}

double Redis::incrbyfloat(const StringView &key, double increment) {
    auto reply = command(cmd::incrbyfloat, key, increment);

    return reply::parse<double>(*reply);
}

void Redis::psetex(const StringView &key,
                        long long ttl,
                        const StringView &val) {
    auto reply = command(cmd::psetex, key, ttl, val);

    reply::parse<void>(*reply);
}

bool Redis::set(const StringView &key,
                    const StringView &val,
                    const std::chrono::milliseconds &ttl,
                    UpdateType type) {
    auto reply = command(cmd::set, key, val, ttl.count(), type);

    reply::rewrite_set_reply(*reply);

    return reply::parse<bool>(*reply);
}

void Redis::setex(const StringView &key,
                    long long ttl,
                    const StringView &val) {
    auto reply = command(cmd::setex, key, ttl, val);

    reply::parse<void>(*reply);
}

bool Redis::setnx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::setnx, key, val);

    return reply::parse<bool>(*reply);
}

long long Redis::setrange(const StringView &key, long long offset, const StringView &val) {
    auto reply = command(cmd::setrange, key, offset, val);

    return reply::parse<long long>(*reply);
}

long long Redis::strlen(const StringView &key) {
    auto reply = command(cmd::strlen, key);

    return reply::parse<long long>(*reply);
}

// LIST commands.

OptionalStringPair Redis::blpop(const StringView &key, long long timeout) {
    auto reply = command(cmd::blpop, key, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

OptionalStringPair Redis::blpop(const StringView &key, const std::chrono::seconds &timeout) {
    return blpop(key, timeout.count());
}

OptionalStringPair Redis::brpop(const StringView &key, long long timeout) {
    auto reply = command(cmd::brpop, key, timeout);

    return reply::parse<OptionalStringPair>(*reply);
}

OptionalStringPair Redis::brpop(const StringView &key, const std::chrono::seconds &timeout) {
    return brpop(key, timeout.count());
}

OptionalString Redis::brpoplpush(const StringView &source,
                                    const StringView &destination,
                                    long long timeout) {
    auto reply = command(cmd::brpoplpush, source, destination, timeout);

    return reply::parse<OptionalString>(*reply);
}

OptionalString Redis::lindex(const StringView &key, long long index) {
    auto reply = command(cmd::lindex, key, index);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::linsert(const StringView &key,
                            InsertPosition position,
                            const StringView &pivot,
                            const StringView &val) {
    auto reply = command(cmd::linsert, key, position, pivot, val);

    return reply::parse<long long>(*reply);
}

long long Redis::llen(const StringView &key) {
    auto reply = command(cmd::llen, key);

    return reply::parse<long long>(*reply);
}

OptionalString Redis::lpop(const StringView &key) {
    auto reply = command(cmd::lpop, key);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::lpush(const StringView &key, const StringView &val) {
    auto reply = command(cmd::lpush, key, val);

    return reply::parse<long long>(*reply);
}

long long Redis::lpushx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::lpushx, key, val);

    return reply::parse<long long>(*reply);
}

long long Redis::lrem(const StringView &key, long long count, const StringView &val) {
    auto reply = command(cmd::lrem, key, count, val);

    return reply::parse<long long>(*reply);
}

void Redis::lset(const StringView &key, long long index, const StringView &val) {
    auto reply = command(cmd::lset, key, index, val);

    reply::parse<void>(*reply);
}

void Redis::ltrim(const StringView &key, long long start, long long stop) {
    auto reply = command(cmd::ltrim, key, start, stop);

    reply::parse<void>(*reply);
}

OptionalString Redis::rpop(const StringView &key) {
    auto reply = command(cmd::rpop, key);

    return reply::parse<OptionalString>(*reply);
}

OptionalString Redis::rpoplpush(const StringView &source, const StringView &destination) {
    auto reply = command(cmd::rpoplpush, source, destination);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::rpush(const StringView &key, const StringView &val) {
    auto reply = command(cmd::rpush, key, val);

    return reply::parse<long long>(*reply);
}

long long Redis::rpushx(const StringView &key, const StringView &val) {
    auto reply = command(cmd::rpushx, key, val);

    return reply::parse<long long>(*reply);
}

long long Redis::hdel(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hdel, key, field);

    return reply::parse<long long>(*reply);
}

bool Redis::hexists(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hexists, key, field);

    return reply::parse<bool>(*reply);
}

OptionalString Redis::hget(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hget, key, field);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::hincrby(const StringView &key, const StringView &field, long long increment) {
    auto reply = command(cmd::hincrby, key, field, increment);

    return reply::parse<long long>(*reply);
}

double Redis::hincrbyfloat(const StringView &key, const StringView &field, double increment) {
    auto reply = command(cmd::hincrbyfloat, key, field, increment);

    return reply::parse<double>(*reply);
}

long long Redis::hlen(const StringView &key) {
    auto reply = command(cmd::hlen, key);

    return reply::parse<long long>(*reply);
}

bool Redis::hset(const StringView &key, const StringView &field, const StringView &val) {
    auto reply = command(cmd::hset, key, field, val);

    return reply::parse<bool>(*reply);
}

bool Redis::hset(const StringView &key, const std::pair<StringView, StringView> &item) {
    return hset(key, item.first, item.second);
}

bool Redis::hsetnx(const StringView &key, const StringView &field, const StringView &val) {
    auto reply = command(cmd::hsetnx, key, field, val);

    return reply::parse<bool>(*reply);
}

bool Redis::hsetnx(const StringView &key, const std::pair<StringView, StringView> &item) {
    return hsetnx(key, item.first, item.second);
}

long long Redis::hstrlen(const StringView &key, const StringView &field) {
    auto reply = command(cmd::hstrlen, key, field);

    return reply::parse<long long>(*reply);
}

// SET commands.

long long Redis::sadd(const StringView &key, const StringView &member) {
    auto reply = command(cmd::sadd, key, member);

    return reply::parse<long long>(*reply);
}

long long Redis::scard(const StringView &key) {
    auto reply = command(cmd::scard, key);

    return reply::parse<long long>(*reply);
}

long long Redis::sdiffstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sdiffstore, destination, key);

    return reply::parse<long long>(*reply);
}

long long Redis::sinterstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sinterstore, destination, key);

    return reply::parse<long long>(*reply);
}

bool Redis::sismember(const StringView &key, const StringView &member) {
    auto reply = command(cmd::sismember, key, member);

    return reply::parse<bool>(*reply);
}

bool Redis::smove(const StringView &source,
                    const StringView &destination,
                    const StringView &member) {
    auto reply = command(cmd::smove, source, destination, member);

    return reply::parse<bool>(*reply);
}

OptionalString Redis::spop(const StringView &key) {
    auto reply = command(cmd::spop, key);

    return reply::parse<OptionalString>(*reply);
}

OptionalString Redis::srandmember(const StringView &key) {
    auto reply = command(cmd::srandmember, key);

    return reply::parse<OptionalString>(*reply);
}

long long Redis::srem(const StringView &key, const StringView &member) {
    auto reply = command(cmd::srem, key, member);

    return reply::parse<long long>(*reply);
}

long long Redis::sunionstore(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::sunionstore, destination, key);

    return reply::parse<long long>(*reply);
}

// SORTED SET commands.

auto Redis::bzpopmax(const StringView &key, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmax, key, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

auto Redis::bzpopmin(const StringView &key, long long timeout)
    -> Optional<std::tuple<std::string, std::string, double>> {
    auto reply = command(cmd::bzpopmin, key, timeout);

    return reply::parse<Optional<std::tuple<std::string, std::string, double>>>(*reply);
}

long long Redis::zadd(const StringView &key,
                        const StringView &member,
                        double score,
                        UpdateType type,
                        bool changed) {
    auto reply = command(cmd::zadd, key, member, score, type, changed);

    return reply::parse<long long>(*reply);
}

long long Redis::zcard(const StringView &key) {
    auto reply = command(cmd::zcard, key);

    return reply::parse<long long>(*reply);
}

double Redis::zincrby(const StringView &key, double increment, const StringView &member) {
    auto reply = command(cmd::zincrby, key, increment, member);

    return reply::parse<double>(*reply);
}

long long Redis::zinterstore(const StringView &destination, const StringView &key, double weight) {
    auto reply = command(cmd::zinterstore, destination, key, weight);

    return reply::parse<long long>(*reply);
}

Optional<std::pair<std::string, double>> Redis::zpopmax(const StringView &key) {
    auto reply = command(cmd::zpopmax, key, 1);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<Optional<std::pair<std::string, double>>>(*reply);
}

Optional<std::pair<std::string, double>> Redis::zpopmin(const StringView &key) {
    auto reply = command(cmd::zpopmin, key, 1);

    reply::rewrite_empty_array_reply(*reply);

    return reply::parse<Optional<std::pair<std::string, double>>>(*reply);
}

OptionalLongLong Redis::zrank(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrank, key, member);

    return reply::parse<OptionalLongLong>(*reply);
}

long long Redis::zrem(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrem, key, member);

    return reply::parse<long long>(*reply);
}

long long Redis::zremrangebyrank(const StringView &key, long long start, long long stop) {
    auto reply = command(cmd::zremrangebyrank, key, start, stop);

    return reply::parse<long long>(*reply);
}

OptionalLongLong Redis::zrevrank(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zrevrank, key, member);

    return reply::parse<OptionalLongLong>(*reply);
}

OptionalDouble Redis::zscore(const StringView &key, const StringView &member) {
    auto reply = command(cmd::zscore, key, member);

    return reply::parse<OptionalDouble>(*reply);
}

long long Redis::zunionstore(const StringView &destination, const StringView &key, double weight) {
    auto reply = command(cmd::zunionstore, destination, key, weight);

    return reply::parse<long long>(*reply);
}

// HYPERLOGLOG commands.

bool Redis::pfadd(const StringView &key, const StringView &element) {
    auto reply = command(cmd::pfadd, key, element);

    return reply::parse<bool>(*reply);
}

long long Redis::pfcount(const StringView &key) {
    auto reply = command(cmd::pfcount, key);

    return reply::parse<long long>(*reply);
}

void Redis::pfmerge(const StringView &destination, const StringView &key) {
    auto reply = command(cmd::pfmerge, destination, key);

    reply::parse<void>(*reply);
}

// GEO commands.

long long Redis::geoadd(const StringView &key,
                        const std::tuple<StringView, double, double> &member) {
    auto reply = command(cmd::geoadd, key, member);

    return reply::parse<long long>(*reply);
}

OptionalDouble Redis::geodist(const StringView &key,
                                const StringView &member1,
                                const StringView &member2,
                                GeoUnit unit) {
    auto reply = command(cmd::geodist, key, member1, member2, unit);

    return reply::parse<OptionalDouble>(*reply);
}

OptionalString Redis::geohash(const StringView &key, const StringView &member) {
    auto reply = command(cmd::geohash, key, member);

    return reply::parse_leniently<OptionalString>(*reply);
}

Optional<std::pair<double, double>> Redis::geopos(const StringView &key, const StringView &member) {
    auto reply = command(cmd::geopos, key, member);

    return reply::parse_leniently<Optional<std::pair<double, double>>>(*reply);
}

OptionalLongLong Redis::georadius(const StringView &key,
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

OptionalLongLong Redis::georadiusbymember(const StringView &key,
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

// SCRIPTING commands.

bool Redis::script_exists(const StringView &sha1) {
    auto reply = command(cmd::script_exists, sha1);

    return reply::parse_leniently<bool>(*reply);
}

void Redis::script_flush() {
    auto reply = command(cmd::script_flush);

    reply::parse<void>(*reply);
}

void Redis::script_kill() {
    auto reply = command(cmd::script_kill);

    reply::parse<void>(*reply);
}

std::string Redis::script_load(const StringView &script) {
    auto reply = command(cmd::script_load, script);

    return reply::parse<std::string>(*reply);
}

// PUBSUB commands.

long long Redis::publish(const StringView &channel, const StringView &message) {
    auto reply = command(cmd::publish, channel, message);

    return reply::parse<long long>(*reply);
}

// Transaction commands.

void Redis::watch(const StringView &key) {
    auto reply = command(cmd::watch, key);

    reply::parse<void>(*reply);
}

void Redis::unwatch() {
    auto reply = command(cmd::unwatch);

    reply::parse<void>(*reply);
}

// Stream commands.

long long Redis::xack(const StringView &key, const StringView &group, const StringView &id) {
    auto reply = command(cmd::xack, key, group, id);

    return reply::parse<long long>(*reply);
}

long long Redis::xdel(const StringView &key, const StringView &id) {
    auto reply = command(cmd::xdel, key, id);

    return reply::parse<long long>(*reply);
}

void Redis::xgroup_create(const StringView &key,
                            const StringView &group,
                            const StringView &id,
                            bool mkstream) {
    auto reply = command(cmd::xgroup_create, key, group, id, mkstream);

    reply::parse<void>(*reply);
}

void Redis::xgroup_setid(const StringView &key, const StringView &group, const StringView &id) {
    auto reply = command(cmd::xgroup_setid, key, group, id);

    reply::parse<void>(*reply);
}

long long Redis::xgroup_destroy(const StringView &key, const StringView &group) {
    auto reply = command(cmd::xgroup_destroy, key, group);

    return reply::parse<long long>(*reply);
}

long long Redis::xgroup_delconsumer(const StringView &key,
                                const StringView &group,
                                const StringView &consumer) {
    auto reply = command(cmd::xgroup_delconsumer, key, group, consumer);

    return reply::parse<long long>(*reply);
}

long long Redis::xlen(const StringView &key) {
    auto reply = command(cmd::xlen, key);

    return reply::parse<long long>(*reply);
}

long long Redis::xtrim(const StringView &key, long long count, bool approx) {
    auto reply = command(cmd::xtrim, key, count, approx);

    return reply::parse<long long>(*reply);
}

}

}
