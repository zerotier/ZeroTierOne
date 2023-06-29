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

#ifndef SEWENEW_REDISPLUSPLUS_COMMAND_H
#define SEWENEW_REDISPLUSPLUS_COMMAND_H

#include <cassert>
#include <ctime>
#include <string>
#include <chrono>
#include "connection.h"
#include "command_options.h"
#include "command_args.h"
#include "utils.h"

namespace sw {

namespace redis {

namespace cmd {

// CONNECTION command.
inline void auth(Connection &connection, const StringView &password) {
    connection.send("AUTH %b", password.data(), password.size());
}

inline void auth(Connection &connection, const StringView &user, const StringView &password) {
    connection.send("AUTH %b %b",
                    user.data(), user.size(),
                    password.data(), password.size());
}

inline void echo(Connection &connection, const StringView &msg) {
    connection.send("ECHO %b", msg.data(), msg.size());
}

inline void ping(Connection &connection) {
    connection.send("PING");
}

inline void quit(Connection &connection) {
    connection.send("QUIT");
}

inline void ping(Connection &connection, const StringView &msg) {
    // If *msg* is empty, Redis returns am empty reply of REDIS_REPLY_STRING type.
    connection.send("PING %b", msg.data(), msg.size());
}

inline void select(Connection &connection, long long idx) {
    connection.send("SELECT %lld", idx);
}

inline void swapdb(Connection &connection, long long idx1, long long idx2) {
    connection.send("SWAPDB %lld %lld", idx1, idx2);
}

// SERVER commands.

inline void bgrewriteaof(Connection &connection) {
    connection.send("BGREWRITEAOF");
}

inline void bgsave(Connection &connection) {
    connection.send("BGSAVE");
}

inline void dbsize(Connection &connection) {
    connection.send("DBSIZE");
}

inline void flushall(Connection &connection, bool async) {
    if (async) {
        connection.send("FLUSHALL ASYNC");
    } else {
        connection.send("FLUSHALL");
    }
}

inline void flushdb(Connection &connection, bool async) {
    if (async) {
        connection.send("FLUSHDB ASYNC");
    } else {
        connection.send("FLUSHDB");
    }
}

inline void info(Connection &connection) {
    connection.send("INFO");
}

inline void info(Connection &connection, const StringView &section) {
    connection.send("INFO %b", section.data(), section.size());
}

inline void lastsave(Connection &connection) {
    connection.send("LASTSAVE");
}

inline void save(Connection &connection) {
    connection.send("SAVE");
}

// KEY commands.

inline void del(Connection &connection, const StringView &key) {
    connection.send("DEL %b", key.data(), key.size());
}

template <typename Input>
inline void del_range(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "DEL" << std::make_pair(first, last);

    connection.send(args);
}

inline void dump(Connection &connection, const StringView &key) {
    connection.send("DUMP %b", key.data(), key.size());
}

inline void exists(Connection &connection, const StringView &key) {
    connection.send("EXISTS %b", key.data(), key.size());
}

template <typename Input>
inline void exists_range(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "EXISTS" << std::make_pair(first, last);

    connection.send(args);
}

inline void expire(Connection &connection,
                    const StringView &key,
                    long long timeout) {
    connection.send("EXPIRE %b %lld",
                    key.data(), key.size(),
                    timeout);
}

inline void expireat(Connection &connection,
                        const StringView &key,
                        long long timestamp) {
    connection.send("EXPIREAT %b %lld",
                    key.data(), key.size(),
                    timestamp);
}

inline void keys(Connection &connection, const StringView &pattern) {
    connection.send("KEYS %b", pattern.data(), pattern.size());
}

inline void move(Connection &connection, const StringView &key, long long db) {
    connection.send("MOVE %b %lld",
                    key.data(), key.size(),
                    db);
}

inline void persist(Connection &connection, const StringView &key) {
    connection.send("PERSIST %b", key.data(), key.size());
}

inline void pexpire(Connection &connection,
                    const StringView &key,
                    long long timeout) {
    connection.send("PEXPIRE %b %lld",
                    key.data(), key.size(),
                    timeout);
}

inline void pexpireat(Connection &connection,
                        const StringView &key,
                        long long timestamp) {
    connection.send("PEXPIREAT %b %lld",
                    key.data(), key.size(),
                    timestamp);
}

inline void pttl(Connection &connection, const StringView &key) {
    connection.send("PTTL %b", key.data(), key.size());
}

inline void randomkey(Connection &connection) {
    connection.send("RANDOMKEY");
}

inline void rename(Connection &connection,
                    const StringView &key,
                    const StringView &newkey) {
    connection.send("RENAME %b %b",
                    key.data(), key.size(),
                    newkey.data(), newkey.size());
}

inline void renamenx(Connection &connection,
                        const StringView &key,
                        const StringView &newkey) {
    connection.send("RENAMENX %b %b",
                    key.data(), key.size(),
                    newkey.data(), newkey.size());
}

void restore(Connection &connection,
                const StringView &key,
                const StringView &val,
                long long ttl,
                bool replace);

inline void scan(Connection &connection,
                    long long cursor,
                    const StringView &pattern,
                    long long count) {
    connection.send("SCAN %lld MATCH %b COUNT %lld",
                    cursor,
                    pattern.data(), pattern.size(),
                    count);
}

inline void touch(Connection &connection, const StringView &key) {
    connection.send("TOUCH %b", key.data(), key.size());
}

template <typename Input>
inline void touch_range(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "TOUCH" << std::make_pair(first, last);

    connection.send(args);
}

inline void ttl(Connection &connection, const StringView &key) {
    connection.send("TTL %b", key.data(), key.size());
}

inline void type(Connection &connection, const StringView &key) {
    connection.send("TYPE %b", key.data(), key.size());
}

inline void unlink(Connection &connection, const StringView &key) {
    connection.send("UNLINK %b", key.data(), key.size());
}

template <typename Input>
inline void unlink_range(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "UNLINK" << std::make_pair(first, last);

    connection.send(args);
}

inline void wait(Connection &connection, long long numslave, long long timeout) {
    connection.send("WAIT %lld %lld", numslave, timeout);
}

// STRING commands.

inline void append(Connection &connection, const StringView &key, const StringView &str) {
    connection.send("APPEND %b %b",
                    key.data(), key.size(),
                    str.data(), str.size());
}

inline void bitcount(Connection &connection,
                        const StringView &key,
                        long long start,
                        long long end) {
    connection.send("BITCOUNT %b %lld %lld",
                    key.data(), key.size(),
                    start, end);
}

void bitop(Connection &connection,
            BitOp op,
            const StringView &destination,
            const StringView &key);

template <typename Input>
void bitop_range(Connection &connection,
                    BitOp op,
                    const StringView &destination,
                    Input first,
                    Input last);

inline void bitpos(Connection &connection,
                    const StringView &key,
                    long long bit,
                    long long start,
                    long long end) {
    connection.send("BITPOS %b %lld %lld %lld",
                    key.data(), key.size(),
                    bit,
                    start,
                    end);
}

inline void decr(Connection &connection, const StringView &key) {
    connection.send("DECR %b", key.data(), key.size());
}

inline void decrby(Connection &connection, const StringView &key, long long decrement) {
    connection.send("DECRBY %b %lld",
                    key.data(), key.size(),
                    decrement);
}

inline void get(Connection &connection, const StringView &key) {
    connection.send("GET %b",
                    key.data(), key.size());
}

inline void getbit(Connection &connection, const StringView &key, long long offset) {
    connection.send("GETBIT %b %lld",
                    key.data(), key.size(),
                    offset);
}

inline void getrange(Connection &connection,
                        const StringView &key,
                        long long start,
                        long long end) {
    connection.send("GETRANGE %b %lld %lld",
                    key.data(), key.size(),
                    start,
                    end);
}

inline void getset(Connection &connection,
                    const StringView &key,
                    const StringView &val) {
    connection.send("GETSET %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

inline void incr(Connection &connection, const StringView &key) {
    connection.send("INCR %b", key.data(), key.size());
}

inline void incrby(Connection &connection, const StringView &key, long long increment) {
    connection.send("INCRBY %b %lld",
                    key.data(), key.size(),
                    increment);
}

inline void incrbyfloat(Connection &connection, const StringView &key, double increment) {
    connection.send("INCRBYFLOAT %b %f",
                    key.data(), key.size(),
                    increment);
}

template <typename Input>
inline void mget(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "MGET" << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
inline void mset(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "MSET" << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
inline void msetnx(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "MSETNX" << std::make_pair(first, last);

    connection.send(args);
}

inline void psetex(Connection &connection,
                    const StringView &key,
                    long long ttl,
                    const StringView &val) {
    connection.send("PSETEX %b %lld %b",
                    key.data(), key.size(),
                    ttl,
                    val.data(), val.size());
}

void set(Connection &connection,
            const StringView &key,
            const StringView &val,
            long long ttl,
            UpdateType type);

inline void setex(Connection &connection,
                    const StringView &key,
                    long long ttl,
                    const StringView &val) {
    connection.send("SETEX %b %lld %b",
                    key.data(), key.size(),
                    ttl,
                    val.data(), val.size());
}

inline void setnx(Connection &connection,
                    const StringView &key,
                    const StringView &val) {
    connection.send("SETNX %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

inline void setrange(Connection &connection,
                        const StringView &key,
                        long long offset,
                        const StringView &val) {
    connection.send("SETRANGE %b %lld %b",
                    key.data(), key.size(),
                    offset,
                    val.data(), val.size());
}

inline void strlen(Connection &connection, const StringView &key) {
    connection.send("STRLEN %b", key.data(), key.size());
}

// LIST commands.

inline void blpop(Connection &connection, const StringView &key, long long timeout) {
    connection.send("BLPOP %b %lld",
                    key.data(), key.size(),
                    timeout);
}

template <typename Input>
inline void blpop_range(Connection &connection,
                        Input first,
                        Input last,
                        long long timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BLPOP" << std::make_pair(first, last) << timeout;

    connection.send(args);
}

inline void brpop(Connection &connection, const StringView &key, long long timeout) {
    connection.send("BRPOP %b %lld",
                    key.data(), key.size(),
                    timeout);
}

template <typename Input>
inline void brpop_range(Connection &connection,
                        Input first,
                        Input last,
                        long long timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BRPOP" << std::make_pair(first, last) << timeout;

    connection.send(args);
}

inline void brpoplpush(Connection &connection,
                        const StringView &source,
                        const StringView &destination,
                        long long timeout) {
    connection.send("BRPOPLPUSH %b %b %lld",
                    source.data(), source.size(),
                    destination.data(), destination.size(),
                    timeout);
}

inline void lindex(Connection &connection, const StringView &key, long long index) {
    connection.send("LINDEX %b %lld",
                    key.data(), key.size(),
                    index);
}

void linsert(Connection &connection,
                const StringView &key,
                InsertPosition position,
                const StringView &pivot,
                const StringView &val);

inline void llen(Connection &connection,
                    const StringView &key) {
    connection.send("LLEN %b", key.data(), key.size());
}

inline void lpop(Connection &connection, const StringView &key) {
    connection.send("LPOP %b",
                    key.data(), key.size());
}

inline void lpush(Connection &connection, const StringView &key, const StringView &val) {
    connection.send("LPUSH %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

template <typename Input>
inline void lpush_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "LPUSH" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void lpushx(Connection &connection, const StringView &key, const StringView &val) {
    connection.send("LPUSHX %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

inline void lrange(Connection &connection,
                    const StringView &key,
                    long long start,
                    long long stop) {
    connection.send("LRANGE %b %lld %lld",
                    key.data(), key.size(),
                    start,
                    stop);
}

inline void lrem(Connection &connection,
                    const StringView &key,
                    long long count,
                    const StringView &val) {
    connection.send("LREM %b %lld %b",
                    key.data(), key.size(),
                    count,
                    val.data(), val.size());
}

inline void lset(Connection &connection,
                    const StringView &key,
                    long long index,
                    const StringView &val) {
    connection.send("LSET %b %lld %b",
                    key.data(), key.size(),
                    index,
                    val.data(), val.size());
}

inline void ltrim(Connection &connection,
                    const StringView &key,
                    long long start,
                    long long stop) {
    connection.send("LTRIM %b %lld %lld",
                    key.data(), key.size(),
                    start,
                    stop);
}

inline void rpop(Connection &connection, const StringView &key) {
    connection.send("RPOP %b", key.data(), key.size());
}

inline void rpoplpush(Connection &connection,
                        const StringView &source,
                        const StringView &destination) {
    connection.send("RPOPLPUSH %b %b",
                    source.data(), source.size(),
                    destination.data(), destination.size());
}

inline void rpush(Connection &connection, const StringView &key, const StringView &val) {
    connection.send("RPUSH %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

template <typename Input>
inline void rpush_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "RPUSH" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void rpushx(Connection &connection, const StringView &key, const StringView &val) {
    connection.send("RPUSHX %b %b",
                    key.data(), key.size(),
                    val.data(), val.size());
}

// HASH commands.

inline void hdel(Connection &connection, const StringView &key, const StringView &field) {
    connection.send("HDEL %b %b",
                    key.data(), key.size(),
                    field.data(), field.size());
}

template <typename Input>
inline void hdel_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HDEL" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void hexists(Connection &connection, const StringView &key, const StringView &field) {
    connection.send("HEXISTS %b %b",
                    key.data(), key.size(),
                    field.data(), field.size());
}

inline void hget(Connection &connection, const StringView &key, const StringView &field) {
    connection.send("HGET %b %b",
                    key.data(), key.size(),
                    field.data(), field.size());
}

inline void hgetall(Connection &connection, const StringView &key) {
    connection.send("HGETALL %b", key.data(), key.size());
}

inline void hincrby(Connection &connection,
                    const StringView &key,
                    const StringView &field,
                    long long increment) {
    connection.send("HINCRBY %b %b %lld",
                    key.data(), key.size(),
                    field.data(), field.size(),
                    increment);
}

inline void hincrbyfloat(Connection &connection,
                            const StringView &key,
                            const StringView &field,
                            double increment) {
    connection.send("HINCRBYFLOAT %b %b %f",
                    key.data(), key.size(),
                    field.data(), field.size(),
                    increment);
}

inline void hkeys(Connection &connection, const StringView &key) {
    connection.send("HKEYS %b", key.data(), key.size());
}

inline void hlen(Connection &connection, const StringView &key) {
    connection.send("HLEN %b", key.data(), key.size());
}

template <typename Input>
inline void hmget(Connection &connection,
                    const StringView &key,
                    Input first,
                    Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HMGET" << key << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
inline void hmset(Connection &connection,
                    const StringView &key,
                    Input first,
                    Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HMSET" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void hscan(Connection &connection,
                    const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count) {
    connection.send("HSCAN %b %lld MATCH %b COUNT %lld",
                    key.data(), key.size(),
                    cursor,
                    pattern.data(), pattern.size(),
                    count);
}

inline void hset(Connection &connection,
                    const StringView &key,
                    const StringView &field,
                    const StringView &val) {
    connection.send("HSET %b %b %b",
                    key.data(), key.size(),
                    field.data(), field.size(),
                    val.data(), val.size());
}

template <typename Input>
void hset_range(Connection &connection, const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HSET" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void hsetnx(Connection &connection,
                    const StringView &key,
                    const StringView &field,
                    const StringView &val) {
    connection.send("HSETNX %b %b %b",
                    key.data(), key.size(),
                    field.data(), field.size(),
                    val.data(), val.size());
}

inline void hstrlen(Connection &connection,
                    const StringView &key,
                    const StringView &field) {
    connection.send("HSTRLEN %b %b",
                    key.data(), key.size(),
                    field.data(), field.size());
}

inline void hvals(Connection &connection, const StringView &key) {
    connection.send("HVALS %b", key.data(), key.size());
}

// SET commands

inline void sadd(Connection &connection,
                    const StringView &key,
                    const StringView &member) {
    connection.send("SADD %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

template <typename Input>
inline void sadd_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SADD" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void scard(Connection &connection, const StringView &key) {
    connection.send("SCARD %b", key.data(), key.size());
}

template <typename Input>
inline void sdiff(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SDIFF" << std::make_pair(first, last);

    connection.send(args);
}

inline void sdiffstore(Connection &connection,
                        const StringView &destination,
                        const StringView &key) {
    connection.send("SDIFFSTORE %b %b",
                    destination.data(), destination.size(),
                    key.data(), key.size());
}

template <typename Input>
inline void sdiffstore_range(Connection &connection,
                                const StringView &destination,
                                Input first,
                                Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SDIFFSTORE" << destination << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
inline void sinter(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SINTER" << std::make_pair(first, last);

    connection.send(args);
}

inline void sinterstore(Connection &connection,
                        const StringView &destination,
                        const StringView &key) {
    connection.send("SINTERSTORE %b %b",
                    destination.data(), destination.size(),
                    key.data(), key.size());
}

template <typename Input>
inline void sinterstore_range(Connection &connection,
                                const StringView &destination,
                                Input first,
                                Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SINTERSTORE" << destination << std::make_pair(first, last);

    connection.send(args);
}

inline void sismember(Connection &connection,
                        const StringView &key,
                        const StringView &member) {
    connection.send("SISMEMBER %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

inline void smembers(Connection &connection, const StringView &key) {
    connection.send("SMEMBERS %b", key.data(), key.size());
}

inline void smove(Connection &connection,
                    const StringView &source,
                    const StringView &destination,
                    const StringView &member) {
    connection.send("SMOVE %b %b %b",
                    source.data(), source.size(),
                    destination.data(), destination.size(),
                    member.data(), member.size());
}

inline void spop(Connection &connection, const StringView &key) {
    connection.send("SPOP %b", key.data(), key.size());
}

inline void spop_range(Connection &connection, const StringView &key, long long count) {
    connection.send("SPOP %b %lld",
                    key.data(), key.size(),
                    count);
}

inline void srandmember(Connection &connection, const StringView &key) {
    connection.send("SRANDMEMBER %b", key.data(), key.size());
}

inline void srandmember_range(Connection &connection,
                                const StringView &key,
                                long long count) {
    connection.send("SRANDMEMBER %b %lld",
                    key.data(), key.size(),
                    count);
}

inline void srem(Connection &connection,
                    const StringView &key,
                    const StringView &member) {
    connection.send("SREM %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

template <typename Input>
inline void srem_range(Connection &connection,
                    const StringView &key,
                    Input first,
                    Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SREM" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void sscan(Connection &connection,
                    const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count) {
    connection.send("SSCAN %b %lld MATCH %b COUNT %lld",
                    key.data(), key.size(),
                    cursor,
                    pattern.data(), pattern.size(),
                    count);
}

template <typename Input>
inline void sunion(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SUNION" << std::make_pair(first, last);

    connection.send(args);
}

inline void sunionstore(Connection &connection,
                        const StringView &destination,
                        const StringView &key) {
    connection.send("SUNIONSTORE %b %b",
                    destination.data(), destination.size(),
                    key.data(), key.size());
}

template <typename Input>
inline void sunionstore_range(Connection &connection,
                                const StringView &destination,
                                Input first,
                                Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SUNIONSTORE" << destination << std::make_pair(first, last);

    connection.send(args);
}

// Sorted Set commands.

inline void bzpopmax(Connection &connection, const StringView &key, long long timeout) {
    connection.send("BZPOPMAX %b %lld", key.data(), key.size(), timeout);
}

template <typename Input>
void bzpopmax_range(Connection &connection,
                    Input first,
                    Input last,
                    long long timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BZPOPMAX" << std::make_pair(first, last) << timeout;

    connection.send(args);
}

inline void bzpopmin(Connection &connection, const StringView &key, long long timeout) {
    connection.send("BZPOPMIN %b %lld", key.data(), key.size(), timeout);
}

template <typename Input>
void bzpopmin_range(Connection &connection,
                    Input first,
                    Input last,
                    long long timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BZPOPMIN" << std::make_pair(first, last) << timeout;

    connection.send(args);
}

template <typename Input>
void zadd_range(Connection &connection,
                const StringView &key,
                Input first,
                Input last,
                UpdateType type,
                bool changed);

inline void zadd(Connection &connection,
                    const StringView &key,
                    const StringView &member,
                    double score,
                    UpdateType type,
                    bool changed) {
    auto tmp = {std::make_pair(member, score)};

    zadd_range(connection, key, tmp.begin(), tmp.end(), type, changed);
}

inline void zcard(Connection &connection, const StringView &key) {
    connection.send("ZCARD %b", key.data(), key.size());
}

template <typename Interval>
inline void zcount(Connection &connection,
                    const StringView &key,
                    const Interval &interval) {
    connection.send("ZCOUNT %b %s %s",
                    key.data(), key.size(),
                    interval.min().c_str(),
                    interval.max().c_str());
}

inline void zincrby(Connection &connection,
                    const StringView &key,
                    double increment,
                    const StringView &member) {
    connection.send("ZINCRBY %b %f %b",
                    key.data(), key.size(),
                    increment,
                    member.data(), member.size());
}

inline void zinterstore(Connection &connection,
                        const StringView &destination,
                        const StringView &key,
                        double weight) {
    connection.send("ZINTERSTORE %b 1 %b WEIGHTS %f",
                    destination.data(), destination.size(),
                    key.data(), key.size(),
                    weight);
}

template <typename Input>
void zinterstore_range(Connection &connection,
                        const StringView &destination,
                        Input first,
                        Input last,
                        Aggregation aggr);

template <typename Interval>
inline void zlexcount(Connection &connection,
                        const StringView &key,
                        const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    connection.send("ZLEXCOUNT %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

inline void zpopmax(Connection &connection, const StringView &key, long long count) {
    connection.send("ZPOPMAX %b %lld",
                        key.data(), key.size(),
                        count);
}

inline void zpopmin(Connection &connection, const StringView &key, long long count) {
    connection.send("ZPOPMIN %b %lld",
                        key.data(), key.size(),
                        count);
}

inline void zrange(Connection &connection,
                    const StringView &key,
                    long long start,
                    long long stop,
                    bool with_scores) {
    if (with_scores) {
        connection.send("ZRANGE %b %lld %lld WITHSCORES",
                        key.data(), key.size(),
                        start,
                        stop);
    } else {
        connection.send("ZRANGE %b %lld %lld",
                        key.data(), key.size(),
                        start,
                        stop);
    }
}

template <typename Interval>
inline void zrangebylex(Connection &connection,
                        const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    connection.send("ZRANGEBYLEX %b %b %b LIMIT %lld %lld",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size(),
                    opts.offset,
                    opts.count);
}

template <typename Interval>
void zrangebyscore(Connection &connection,
                    const StringView &key,
                    const Interval &interval,
                    const LimitOptions &opts,
                    bool with_scores) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    if (with_scores) {
        connection.send("ZRANGEBYSCORE %b %b %b WITHSCORES LIMIT %lld %lld",
                        key.data(), key.size(),
                        min.data(), min.size(),
                        max.data(), max.size(),
                        opts.offset,
                        opts.count);
    } else {
        connection.send("ZRANGEBYSCORE %b %b %b LIMIT %lld %lld",
                        key.data(), key.size(),
                        min.data(), min.size(),
                        max.data(), max.size(),
                        opts.offset,
                        opts.count);
    }
}

inline void zrank(Connection &connection,
                    const StringView &key,
                    const StringView &member) {
    connection.send("ZRANK %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

inline void zrem(Connection &connection,
                    const StringView &key,
                    const StringView &member) {
    connection.send("ZREM %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

template <typename Input>
inline void zrem_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "ZREM" << key << std::make_pair(first, last);

    connection.send(args);
}

template <typename Interval>
inline void zremrangebylex(Connection &connection,
                            const StringView &key,
                            const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    connection.send("ZREMRANGEBYLEX %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

inline void zremrangebyrank(Connection &connection,
                            const StringView &key,
                            long long start,
                            long long stop) {
    connection.send("zremrangebyrank %b %lld %lld",
                    key.data(), key.size(),
                    start,
                    stop);
}

template <typename Interval>
inline void zremrangebyscore(Connection &connection,
                                const StringView &key,
                                const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    connection.send("ZREMRANGEBYSCORE %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

inline void zrevrange(Connection &connection,
                        const StringView &key,
                        long long start,
                        long long stop,
                        bool with_scores) {
    if (with_scores) {
        connection.send("ZREVRANGE %b %lld %lld WITHSCORES",
                        key.data(), key.size(),
                        start,
                        stop);
    } else {
        connection.send("ZREVRANGE %b %lld %lld",
                        key.data(), key.size(),
                        start,
                        stop);
    }
}

template <typename Interval>
inline void zrevrangebylex(Connection &connection,
                            const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    connection.send("ZREVRANGEBYLEX %b %b %b LIMIT %lld %lld",
                    key.data(), key.size(),
                    max.data(), max.size(),
                    min.data(), min.size(),
                    opts.offset,
                    opts.count);
}

template <typename Interval>
void zrevrangebyscore(Connection &connection,
                        const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        bool with_scores) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    if (with_scores) {
        connection.send("ZREVRANGEBYSCORE %b %b %b WITHSCORES LIMIT %lld %lld",
                        key.data(), key.size(),
                        max.data(), max.size(),
                        min.data(), min.size(),
                        opts.offset,
                        opts.count);
    } else {
        connection.send("ZREVRANGEBYSCORE %b %b %b LIMIT %lld %lld",
                        key.data(), key.size(),
                        max.data(), max.size(),
                        min.data(), min.size(),
                        opts.offset,
                        opts.count);
    }
}

inline void zrevrank(Connection &connection,
                        const StringView &key,
                        const StringView &member) {
    connection.send("ZREVRANK %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

inline void zscan(Connection &connection,
                    const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count) {
    connection.send("ZSCAN %b %lld MATCH %b COUNT %lld",
                    key.data(), key.size(),
                    cursor,
                    pattern.data(), pattern.size(),
                    count);
}

inline void zscore(Connection &connection,
                    const StringView &key,
                    const StringView &member) {
    connection.send("ZSCORE %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

inline void zunionstore(Connection &connection,
                        const StringView &destination,
                        const StringView &key,
                        double weight) {
    connection.send("ZUNIONSTORE %b 1 %b WEIGHTS %f",
                    destination.data(), destination.size(),
                    key.data(), key.size(),
                    weight);
}

template <typename Input>
void zunionstore_range(Connection &connection,
                        const StringView &destination,
                        Input first,
                        Input last,
                        Aggregation aggr);

// HYPERLOGLOG commands.

inline void pfadd(Connection &connection,
                    const StringView &key,
                    const StringView &element) {
    connection.send("PFADD %b %b",
                    key.data(), key.size(),
                    element.data(), element.size());
}

template <typename Input>
inline void pfadd_range(Connection &connection,
                        const StringView &key,
                        Input first,
                        Input last) {
    assert(first != last);

    CmdArgs args;
    args << "PFADD" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void pfcount(Connection &connection, const StringView &key) {
    connection.send("PFCOUNT %b", key.data(), key.size());
}

template <typename Input>
inline void pfcount_range(Connection &connection,
                            Input first,
                            Input last) {
    assert(first != last);

    CmdArgs args;
    args << "PFCOUNT" << std::make_pair(first, last);

    connection.send(args);
}

inline void pfmerge(Connection &connection, const StringView &destination, const StringView &key) {
    connection.send("PFMERGE %b %b",
                    destination.data(), destination.size(),
                    key.data(), key.size());
}

template <typename Input>
inline void pfmerge_range(Connection &connection,
                            const StringView &destination,
                            Input first,
                            Input last) {
    assert(first != last);

    CmdArgs args;
    args << "PFMERGE" << destination << std::make_pair(first, last);

    connection.send(args);
}

// GEO commands.

inline void geoadd(Connection &connection,
                    const StringView &key,
                    const std::tuple<StringView, double, double> &member) {
    const auto &mem = std::get<0>(member);

    connection.send("GEOADD %b %f %f %b",
                    key.data(), key.size(),
                    std::get<1>(member),
                    std::get<2>(member),
                    mem.data(), mem.size());
}

template <typename Input>
inline void geoadd_range(Connection &connection,
                            const StringView &key,
                            Input first,
                            Input last) {
    assert(first != last);

    CmdArgs args;
    args << "GEOADD" << key;

    while (first != last) {
        const auto &member = *first;
        args << std::get<1>(member) << std::get<2>(member) << std::get<0>(member);
        ++first;
    }

    connection.send(args);
}

void geodist(Connection &connection,
                const StringView &key,
                const StringView &member1,
                const StringView &member2,
                GeoUnit unit);

inline void geohash(Connection &connection, const StringView &key, const StringView &member) {
    connection.send("GEOHASH %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

template <typename Input>
inline void geohash_range(Connection &connection,
                            const StringView &key,
                            Input first,
                            Input last) {
    assert(first != last);

    CmdArgs args;
    args << "GEOHASH" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void geopos(Connection &connection, const StringView &key, const StringView &member) {
    connection.send("GEOPOS %b %b",
                    key.data(), key.size(),
                    member.data(), member.size());
}

template <typename Input>
inline void geopos_range(Connection &connection,
                            const StringView &key,
                            Input first,
                            Input last) {
    assert(first != last);

    CmdArgs args;
    args << "GEOPOS" << key << std::make_pair(first, last);

    connection.send(args);
}

void georadius(Connection &connection,
                const StringView &key,
                const std::pair<double, double> &loc,
                double radius,
                GeoUnit unit,
                long long count,
                bool asc,
                bool with_coord,
                bool with_dist,
                bool with_hash);

void georadius_store(Connection &connection,
                        const StringView &key,
                        const std::pair<double, double> &loc,
                        double radius,
                        GeoUnit unit,
                        const StringView &destination,
                        bool store_dist,
                        long long count);

void georadiusbymember(Connection &connection,
                        const StringView &key,
                        const StringView &member,
                        double radius,
                        GeoUnit unit,
                        long long count,
                        bool asc,
                        bool with_coord,
                        bool with_dist,
                        bool with_hash);

void georadiusbymember_store(Connection &connection,
                                const StringView &key,
                                const StringView &member,
                                double radius,
                                GeoUnit unit,
                                const StringView &destination,
                                bool store_dist,
                                long long count);

// SCRIPTING commands.

template <typename Keys, typename Args>
inline void eval(Connection &connection,
                   const StringView &script,
                   Keys keys_first,
                   Keys keys_last,
                   Args args_first,
                   Args args_last) {
    CmdArgs cmd_args;

    auto keys_num = std::distance(keys_first, keys_last);

    cmd_args << "EVAL" << script << keys_num
            << std::make_pair(keys_first, keys_last)
            << std::make_pair(args_first, args_last);

    connection.send(cmd_args);
}

template <typename Keys, typename Args>
inline void evalsha(Connection &connection,
                       const StringView &script,
                       Keys keys_first,
                       Keys keys_last,
                       Args args_first,
                       Args args_last) {
    CmdArgs cmd_args;

    auto keys_num = std::distance(keys_first, keys_last);

    cmd_args << "EVALSHA" << script << keys_num
            << std::make_pair(keys_first, keys_last)
            << std::make_pair(args_first, args_last);

    connection.send(cmd_args);
}

inline void script_exists(Connection &connection, const StringView &sha) {
    connection.send("SCRIPT EXISTS %b", sha.data(), sha.size());
}

template <typename Input>
inline void script_exists_range(Connection &connection, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SCRIPT" << "EXISTS" << std::make_pair(first, last);

    connection.send(args);
}

inline void script_flush(Connection &connection) {
    connection.send("SCRIPT FLUSH");
}

inline void script_kill(Connection &connection) {
    connection.send("SCRIPT KILL");
}

inline void script_load(Connection &connection, const StringView &script) {
    connection.send("SCRIPT LOAD %b", script.data(), script.size());
}

// PUBSUB commands.

inline void psubscribe(Connection &connection, const StringView &pattern) {
    connection.send("PSUBSCRIBE %b", pattern.data(), pattern.size());
}

template <typename Input>
inline void psubscribe_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("PSUBSCRIBE: no key specified");
    }

    CmdArgs args;
    args << "PSUBSCRIBE" << std::make_pair(first, last);

    connection.send(args);
}

inline void publish(Connection &connection,
                    const StringView &channel,
                    const StringView &message) {
    connection.send("PUBLISH %b %b",
                    channel.data(), channel.size(),
                    message.data(), message.size());
}

inline void punsubscribe(Connection &connection) {
    connection.send("PUNSUBSCRIBE");
}

inline void punsubscribe(Connection &connection, const StringView &pattern) {
    connection.send("PUNSUBSCRIBE %b", pattern.data(), pattern.size());
}

template <typename Input>
inline void punsubscribe_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("PUNSUBSCRIBE: no key specified");
    }

    CmdArgs args;
    args << "PUNSUBSCRIBE" << std::make_pair(first, last);

    connection.send(args);
}

inline void subscribe(Connection &connection, const StringView &channel) {
    connection.send("SUBSCRIBE %b", channel.data(), channel.size());
}

template <typename Input>
inline void subscribe_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("SUBSCRIBE: no key specified");
    }

    CmdArgs args;
    args << "SUBSCRIBE" << std::make_pair(first, last);

    connection.send(args);
}

inline void unsubscribe(Connection &connection) {
    connection.send("UNSUBSCRIBE");
}

inline void unsubscribe(Connection &connection, const StringView &channel) {
    connection.send("UNSUBSCRIBE %b", channel.data(), channel.size());
}

template <typename Input>
inline void unsubscribe_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("UNSUBSCRIBE: no key specified");
    }

    CmdArgs args;
    args << "UNSUBSCRIBE" << std::make_pair(first, last);

    connection.send(args);
}

// Transaction commands.

inline void discard(Connection &connection) {
    connection.send("DISCARD");
}

inline void exec(Connection &connection) {
    connection.send("EXEC");
}

inline void multi(Connection &connection) {
    connection.send("MULTI");
}

inline void unwatch(Connection &connection) {
    connection.send("UNWATCH");
}

template <typename Input>
inline void unwatch_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("UNWATCH: no key specified");
    }

    CmdArgs args;
    args << "UNWATCH" << std::make_pair(first, last);

    connection.send(args);
}

inline void watch(Connection &connection, const StringView &key) {
    connection.send("WATCH %b", key.data(), key.size());
}

template <typename Input>
inline void watch_range(Connection &connection, Input first, Input last) {
    if (first == last) {
        throw Error("WATCH: no key specified");
    }

    CmdArgs args;
    args << "WATCH" << std::make_pair(first, last);

    connection.send(args);
}

// Stream commands.

inline void xack(Connection &connection,
                    const StringView &key,
                    const StringView &group,
                    const StringView &id) {
    connection.send("XACK %b %b %b",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    id.data(), id.size());
}

template <typename Input>
void xack_range(Connection &connection,
                const StringView &key,
                const StringView &group,
                Input first,
                Input last) {
    CmdArgs args;
    args << "XACK" << key << group << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
void xadd_range(Connection &connection,
                const StringView &key,
                const StringView &id,
                Input first,
                Input last) {
    CmdArgs args;
    args << "XADD" << key << id << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
void xadd_maxlen_range(Connection &connection,
                        const StringView &key,
                        const StringView &id,
                        Input first,
                        Input last,
                        long long count,
                        bool approx) {
    CmdArgs args;
    args << "XADD" << key << "MAXLEN";

    if (approx) {
        args << "~";
    }

    args << count << id << std::make_pair(first, last);

    connection.send(args);
}

inline void xclaim(Connection &connection,
                    const StringView &key,
                    const StringView &group,
                    const StringView &consumer,
                    long long min_idle_time,
                    const StringView &id) {
    connection.send("XCLAIM %b %b %b %lld %b",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    consumer.data(), consumer.size(),
                    min_idle_time,
                    id.data(), id.size());
}

template <typename Input>
void xclaim_range(Connection &connection,
                    const StringView &key,
                    const StringView &group,
                    const StringView &consumer,
                    long long min_idle_time,
                    Input first,
                    Input last) {
    CmdArgs args;
    args << "XCLAIM" << key << group << consumer << min_idle_time << std::make_pair(first, last);

    connection.send(args);
}

inline void xdel(Connection &connection, const StringView &key, const StringView &id) {
    connection.send("XDEL %b %b", key.data(), key.size(), id.data(), id.size());
}

template <typename Input>
void xdel_range(Connection &connection, const StringView &key, Input first, Input last) {
    CmdArgs args;
    args << "XDEL" << key << std::make_pair(first, last);

    connection.send(args);
}

inline void xgroup_create(Connection &connection,
                            const StringView &key,
                            const StringView &group,
                            const StringView &id,
                            bool mkstream) {
    CmdArgs args;
    args << "XGROUP" << "CREATE" << key << group << id;

    if (mkstream) {
        args << "MKSTREAM";
    }

    connection.send(args);
}

inline void xgroup_setid(Connection &connection,
                            const StringView &key,
                            const StringView &group,
                            const StringView &id) {
    connection.send("XGROUP SETID %b %b %b",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    id.data(), id.size());
}

inline void xgroup_destroy(Connection &connection,
                            const StringView &key,
                            const StringView &group) {
    connection.send("XGROUP DESTROY %b %b",
                    key.data(), key.size(),
                    group.data(), group.size());
}

inline void xgroup_delconsumer(Connection &connection,
                                const StringView &key,
                                const StringView &group,
                                const StringView &consumer) {
    connection.send("XGROUP DELCONSUMER %b %b %b",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    consumer.data(), consumer.size());
}

inline void xlen(Connection &connection, const StringView &key) {
    connection.send("XLEN %b", key.data(), key.size());
}

inline void xpending(Connection &connection, const StringView &key, const StringView &group) {
    connection.send("XPENDING %b %b",
                    key.data(), key.size(),
                    group.data(), group.size());
}

inline void xpending_detail(Connection &connection,
                            const StringView &key,
                            const StringView &group,
                            const StringView &start,
                            const StringView &end,
                            long long count) {
    connection.send("XPENDING %b %b %b %b %lld",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    start.data(), start.size(),
                    end.data(), end.size(),
                    count);
}

inline void xpending_per_consumer(Connection &connection,
                                    const StringView &key,
                                    const StringView &group,
                                    const StringView &start,
                                    const StringView &end,
                                    long long count,
                                    const StringView &consumer) {
    connection.send("XPENDING %b %b %b %b %lld %b",
                    key.data(), key.size(),
                    group.data(), group.size(),
                    start.data(), start.size(),
                    end.data(), end.size(),
                    count,
                    consumer.data(), consumer.size());
}

inline void xrange(Connection &connection,
                    const StringView &key,
                    const StringView &start,
                    const StringView &end) {
    connection.send("XRANGE %b %b %b",
                    key.data(), key.size(),
                    start.data(), start.size(),
                    end.data(), end.size());
}

inline void xrange_count(Connection &connection,
                            const StringView &key,
                            const StringView &start,
                            const StringView &end,
                            long long count) {
    connection.send("XRANGE %b %b %b COUNT %lld",
                    key.data(), key.size(),
                    start.data(), start.size(),
                    end.data(), end.size(),
                    count);
}

inline void xread(Connection &connection,
                    const StringView &key,
                    const StringView &id,
                    long long count) {
    connection.send("XREAD COUNT %lld STREAMS %b %b",
                    count,
                    key.data(), key.size(),
                    id.data(), id.size());
}

template <typename Input>
void xread_range(Connection &connection, Input first, Input last, long long count) {
    CmdArgs args;
    args << "XREAD" << "COUNT" << count << "STREAMS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    connection.send(args);
}

inline void xread_block(Connection &connection,
                        const StringView &key,
                        const StringView &id,
                        long long timeout,
                        long long count) {
    connection.send("XREAD COUNT %lld BLOCK %lld STREAMS %b %b",
                    count,
                    timeout,
                    key.data(), key.size(),
                    id.data(), id.size());
}

template <typename Input>
void xread_block_range(Connection &connection,
                        Input first,
                        Input last,
                        long long timeout,
                        long long count) {
    CmdArgs args;
    args << "XREAD" << "COUNT" << count << "BLOCK" << timeout << "STREAMS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    connection.send(args);
}

inline void xreadgroup(Connection &connection,
                        const StringView &group,
                        const StringView &consumer,
                        const StringView &key,
                        const StringView &id,
                        long long count,
                        bool noack) {
    CmdArgs args;
    args << "XREADGROUP" << "GROUP" << group << consumer << "COUNT" << count;

    if (noack) {
        args << "NOACK";
    }

    args << "STREAMS" << key << id;

    connection.send(args);
}

template <typename Input>
void xreadgroup_range(Connection &connection,
                        const StringView &group,
                        const StringView &consumer,
                        Input first,
                        Input last,
                        long long count,
                        bool noack) {
    CmdArgs args;
    args << "XREADGROUP" << "GROUP" << group << consumer << "COUNT" << count;

    if (noack) {
        args << "NOACK";
    }

    args << "STREAMS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    connection.send(args);
}

inline void xreadgroup_block(Connection &connection,
                                const StringView &group,
                                const StringView &consumer,
                                const StringView &key,
                                const StringView &id,
                                long long timeout,
                                long long count,
                                bool noack) {
    CmdArgs args;
    args << "XREADGROUP" << "GROUP" << group << consumer
        << "COUNT" << count << "BLOCK" << timeout;

    if (noack) {
        args << "NOACK";
    }

    args << "STREAMS" << key << id;

    connection.send(args);
}

template <typename Input>
void xreadgroup_block_range(Connection &connection,
                            const StringView &group,
                            const StringView &consumer,
                            Input first,
                            Input last,
                            long long timeout,
                            long long count,
                            bool noack) {
    CmdArgs args;
    args << "XREADGROUP" << "GROUP" << group << consumer
        << "COUNT" << count << "BLOCK" << timeout;

    if (noack) {
        args << "NOACK";
    }

    args << "STREAMS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    connection.send(args);
}

inline void xrevrange(Connection &connection,
                    const StringView &key,
                    const StringView &end,
                    const StringView &start) {
    connection.send("XREVRANGE %b %b %b",
                    key.data(), key.size(),
                    end.data(), end.size(),
                    start.data(), start.size());
}

inline void xrevrange_count(Connection &connection,
                            const StringView &key,
                            const StringView &end,
                            const StringView &start,
                            long long count) {
    connection.send("XREVRANGE %b %b %b COUNT %lld",
                    key.data(), key.size(),
                    end.data(), end.size(),
                    start.data(), start.size(),
                    count);
}

void xtrim(Connection &connection, const StringView &key, long long count, bool approx);

namespace detail {

void set_bitop(CmdArgs &args, BitOp op);

void set_update_type(CmdArgs &args, UpdateType type);

void set_aggregation_type(CmdArgs &args, Aggregation type);

template <typename Input>
void zinterstore(std::false_type,
                    Connection &connection,
                    const StringView &destination,
                    Input first,
                    Input last,
                    Aggregation aggr) {
    CmdArgs args;
    args << "ZINTERSTORE" << destination << std::distance(first, last)
        << std::make_pair(first, last);

    set_aggregation_type(args, aggr);

    connection.send(args);
}

template <typename Input>
void zinterstore(std::true_type,
                    Connection &connection,
                    const StringView &destination,
                    Input first,
                    Input last,
                    Aggregation aggr) {
    CmdArgs args;
    args << "ZINTERSTORE" << destination << std::distance(first, last);

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    args << "WEIGHTS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    set_aggregation_type(args, aggr);

    connection.send(args);
}

template <typename Input>
void zunionstore(std::false_type,
                    Connection &connection,
                    const StringView &destination,
                    Input first,
                    Input last,
                    Aggregation aggr) {
    CmdArgs args;
    args << "ZUNIONSTORE" << destination << std::distance(first, last)
        << std::make_pair(first, last);

    set_aggregation_type(args, aggr);

    connection.send(args);
}

template <typename Input>
void zunionstore(std::true_type,
                    Connection &connection,
                    const StringView &destination,
                    Input first,
                    Input last,
                    Aggregation aggr) {
    CmdArgs args;
    args << "ZUNIONSTORE" << destination << std::distance(first, last);

    for (auto iter = first; iter != last; ++iter) {
        args << iter->first;
    }

    args << "WEIGHTS";

    for (auto iter = first; iter != last; ++iter) {
        args << iter->second;
    }

    set_aggregation_type(args, aggr);

    connection.send(args);
}

void set_geo_unit(CmdArgs &args, GeoUnit unit);

void set_georadius_store_parameters(CmdArgs &args,
                                    double radius,
                                    GeoUnit unit,
                                    const StringView &destination,
                                    bool store_dist,
                                    long long count);

void set_georadius_parameters(CmdArgs &args,
                                double radius,
                                GeoUnit unit,
                                long long count,
                                bool asc,
                                bool with_coord,
                                bool with_dist,
                                bool with_hash);

}

}

}

}

namespace sw {

namespace redis {

namespace cmd {

template <typename Input>
void bitop_range(Connection &connection,
                    BitOp op,
                    const StringView &destination,
                    Input first,
                    Input last) {
    assert(first != last);

    CmdArgs args;

    detail::set_bitop(args, op);

    args << destination << std::make_pair(first, last);

    connection.send(args);
}

template <typename Input>
void zadd_range(Connection &connection,
                const StringView &key,
                Input first,
                Input last,
                UpdateType type,
                bool changed) {
    assert(first != last);

    CmdArgs args;

    args << "ZADD" << key;

    detail::set_update_type(args, type);

    if (changed) {
        args << "CH";
    }

    while (first != last) {
        // Swap the <member, score> pair to <score, member> pair.
        args << first->second << first->first;
        ++first;
    }

    connection.send(args);
}

template <typename Input>
void zinterstore_range(Connection &connection,
                        const StringView &destination,
                        Input first,
                        Input last,
                        Aggregation aggr) {
    assert(first != last);

    detail::zinterstore(typename IsKvPairIter<Input>::type(),
                        connection,
                        destination,
                        first,
                        last,
                        aggr);
}

template <typename Input>
void zunionstore_range(Connection &connection,
                        const StringView &destination,
                        Input first,
                        Input last,
                        Aggregation aggr) {
    assert(first != last);

    detail::zunionstore(typename IsKvPairIter<Input>::type(),
                        connection,
                        destination,
                        first,
                        last,
                        aggr);
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_COMMAND_H
