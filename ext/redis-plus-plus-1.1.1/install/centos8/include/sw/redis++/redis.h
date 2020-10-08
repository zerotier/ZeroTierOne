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

#ifndef SEWENEW_REDISPLUSPLUS_REDIS_H
#define SEWENEW_REDISPLUSPLUS_REDIS_H

#include <string>
#include <chrono>
#include <memory>
#include <initializer_list>
#include <tuple>
#include "connection_pool.h"
#include "reply.h"
#include "command_options.h"
#include "utils.h"
#include "subscriber.h"
#include "pipeline.h"
#include "transaction.h"
#include "sentinel.h"

namespace sw {

namespace redis {

template <typename Impl>
class QueuedRedis;

using Transaction = QueuedRedis<TransactionImpl>;

using Pipeline = QueuedRedis<PipelineImpl>;

class Redis {
public:
    Redis(const ConnectionOptions &connection_opts,
            const ConnectionPoolOptions &pool_opts = {}) : _pool(pool_opts, connection_opts) {}

    // Construct Redis instance with URI:
    // "tcp://127.0.0.1", "tcp://127.0.0.1:6379", or "unix://path/to/socket"
    explicit Redis(const std::string &uri);

    Redis(const std::shared_ptr<Sentinel> &sentinel,
            const std::string &master_name,
            Role role,
            const ConnectionOptions &connection_opts,
            const ConnectionPoolOptions &pool_opts = {}) :
                _pool(SimpleSentinel(sentinel, master_name, role), pool_opts, connection_opts) {}

    Redis(const Redis &) = delete;
    Redis& operator=(const Redis &) = delete;

    Redis(Redis &&) = default;
    Redis& operator=(Redis &&) = default;

    Pipeline pipeline();

    Transaction transaction(bool piped = false);

    Subscriber subscriber();

    template <typename Cmd, typename ...Args>
    auto command(Cmd cmd, Args &&...args)
        -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value, ReplyUPtr>::type;

    template <typename ...Args>
    auto command(const StringView &cmd_name, Args &&...args)
        -> typename std::enable_if<!IsIter<typename LastType<Args...>::type>::value,
                                    ReplyUPtr>::type;

    template <typename ...Args>
    auto command(const StringView &cmd_name, Args &&...args)
        -> typename std::enable_if<IsIter<typename LastType<Args...>::type>::value, void>::type;

    template <typename Result, typename ...Args>
    Result command(const StringView &cmd_name, Args &&...args);

    template <typename Input>
    auto command(Input first, Input last)
        -> typename std::enable_if<IsIter<Input>::value, ReplyUPtr>::type;

    template <typename Result, typename Input>
    auto command(Input first, Input last)
        -> typename std::enable_if<IsIter<Input>::value, Result>::type;

    template <typename Input, typename Output>
    auto command(Input first, Input last, Output output)
        -> typename std::enable_if<IsIter<Input>::value, void>::type;

    // CONNECTION commands.

    void auth(const StringView &password);

    std::string echo(const StringView &msg);

    std::string ping();

    std::string ping(const StringView &msg);

    // After sending QUIT, only the current connection will be close, while
    // other connections in the pool is still open. This is a strange behavior.
    // So we DO NOT support the QUIT command. If you want to quit connection to
    // server, just destroy the Redis object.
    //
    // void quit();

    // We get a connection from the pool, and send the SELECT command to switch
    // to a specified DB. However, when we try to send other commands to the
    // given DB, we might get a different connection from the pool, and these
    // commands, in fact, work on other DB. e.g.
    //
    // redis.select(1); // get a connection from the pool and switch to the 1th DB
    // redis.get("key"); // might get another connection from the pool,
    //                   // and try to get 'key' on the default DB
    //
    // Obviously, this is NOT what we expect. So we DO NOT support SELECT command.
    // In order to select a DB, we can specify the DB index with the ConnectionOptions.
    //
    // However, since Pipeline and Transaction always send multiple commands on a
    // single connection, these two classes have a *select* method.
    //
    // void select(long long idx);

    void swapdb(long long idx1, long long idx2);

    // SERVER commands.

    void bgrewriteaof();

    void bgsave();

    long long dbsize();

    void flushall(bool async = false);

    void flushdb(bool async = false);

    std::string info();

    std::string info(const StringView &section);

    long long lastsave();

    void save();

    // KEY commands.

    long long del(const StringView &key);

    template <typename Input>
    long long del(Input first, Input last);

    template <typename T>
    long long del(std::initializer_list<T> il) {
        return del(il.begin(), il.end());
    }

    OptionalString dump(const StringView &key);

    long long exists(const StringView &key);

    template <typename Input>
    long long exists(Input first, Input last);

    template <typename T>
    long long exists(std::initializer_list<T> il) {
        return exists(il.begin(), il.end());
    }

    bool expire(const StringView &key, long long timeout);

    bool expire(const StringView &key, const std::chrono::seconds &timeout);

    bool expireat(const StringView &key, long long timestamp);

    bool expireat(const StringView &key,
                    const std::chrono::time_point<std::chrono::system_clock,
                                                    std::chrono::seconds> &tp);

    template <typename Output>
    void keys(const StringView &pattern, Output output);

    bool move(const StringView &key, long long db);

    bool persist(const StringView &key);

    bool pexpire(const StringView &key, long long timeout);

    bool pexpire(const StringView &key, const std::chrono::milliseconds &timeout);

    bool pexpireat(const StringView &key, long long timestamp);

    bool pexpireat(const StringView &key,
                    const std::chrono::time_point<std::chrono::system_clock,
                                                    std::chrono::milliseconds> &tp);

    long long pttl(const StringView &key);

    OptionalString randomkey();

    void rename(const StringView &key, const StringView &newkey);

    bool renamenx(const StringView &key, const StringView &newkey);

    void restore(const StringView &key,
                    const StringView &val,
                    long long ttl,
                    bool replace = false);

    void restore(const StringView &key,
                    const StringView &val,
                    const std::chrono::milliseconds &ttl = std::chrono::milliseconds{0},
                    bool replace = false);

    // TODO: sort

    template <typename Output>
    long long scan(long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    template <typename Output>
    long long scan(long long cursor,
                    Output output);

    template <typename Output>
    long long scan(long long cursor,
                    const StringView &pattern,
                    Output output);

    template <typename Output>
    long long scan(long long cursor,
                    long long count,
                    Output output);

    long long touch(const StringView &key);

    template <typename Input>
    long long touch(Input first, Input last);

    template <typename T>
    long long touch(std::initializer_list<T> il) {
        return touch(il.begin(), il.end());
    }

    long long ttl(const StringView &key);

    std::string type(const StringView &key);

    long long unlink(const StringView &key);

    template <typename Input>
    long long unlink(Input first, Input last);

    template <typename T>
    long long unlink(std::initializer_list<T> il) {
        return unlink(il.begin(), il.end());
    }

    long long wait(long long numslaves, long long timeout);

    long long wait(long long numslaves, const std::chrono::milliseconds &timeout);

    // STRING commands.

    long long append(const StringView &key, const StringView &str);

    long long bitcount(const StringView &key, long long start = 0, long long end = -1);

    long long bitop(BitOp op, const StringView &destination, const StringView &key);

    template <typename Input>
    long long bitop(BitOp op, const StringView &destination, Input first, Input last);

    template <typename T>
    long long bitop(BitOp op, const StringView &destination, std::initializer_list<T> il) {
        return bitop(op, destination, il.begin(), il.end());
    }

    long long bitpos(const StringView &key,
                        long long bit,
                        long long start = 0,
                        long long end = -1);

    long long decr(const StringView &key);

    long long decrby(const StringView &key, long long decrement);

    OptionalString get(const StringView &key);

    long long getbit(const StringView &key, long long offset);

    std::string getrange(const StringView &key, long long start, long long end);

    OptionalString getset(const StringView &key, const StringView &val);

    long long incr(const StringView &key);

    long long incrby(const StringView &key, long long increment);

    double incrbyfloat(const StringView &key, double increment);

    template <typename Input, typename Output>
    void mget(Input first, Input last, Output output);

    template <typename T, typename Output>
    void mget(std::initializer_list<T> il, Output output) {
        mget(il.begin(), il.end(), output);
    }

    template <typename Input>
    void mset(Input first, Input last);

    template <typename T>
    void mset(std::initializer_list<T> il) {
        mset(il.begin(), il.end());
    }

    template <typename Input>
    bool msetnx(Input first, Input last);

    template <typename T>
    bool msetnx(std::initializer_list<T> il) {
        return msetnx(il.begin(), il.end());
    }

    void psetex(const StringView &key,
                long long ttl,
                const StringView &val);

    void psetex(const StringView &key,
                const std::chrono::milliseconds &ttl,
                const StringView &val);

    bool set(const StringView &key,
                const StringView &val,
                const std::chrono::milliseconds &ttl = std::chrono::milliseconds(0),
                UpdateType type = UpdateType::ALWAYS);

    void setex(const StringView &key,
                long long ttl,
                const StringView &val);

    void setex(const StringView &key,
                const std::chrono::seconds &ttl,
                const StringView &val);

    bool setnx(const StringView &key, const StringView &val);

    long long setrange(const StringView &key, long long offset, const StringView &val);

    long long strlen(const StringView &key);

    // LIST commands.

    OptionalStringPair blpop(const StringView &key, long long timeout);

    OptionalStringPair blpop(const StringView &key,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    template <typename Input>
    OptionalStringPair blpop(Input first, Input last, long long timeout);

    template <typename T>
    OptionalStringPair blpop(std::initializer_list<T> il, long long timeout) {
        return blpop(il.begin(), il.end(), timeout);
    }

    template <typename Input>
    OptionalStringPair blpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    template <typename T>
    OptionalStringPair blpop(std::initializer_list<T> il,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return blpop(il.begin(), il.end(), timeout);
    }

    OptionalStringPair brpop(const StringView &key, long long timeout);

    OptionalStringPair brpop(const StringView &key,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    template <typename Input>
    OptionalStringPair brpop(Input first, Input last, long long timeout);

    template <typename T>
    OptionalStringPair brpop(std::initializer_list<T> il, long long timeout) {
        return brpop(il.begin(), il.end(), timeout);
    }

    template <typename Input>
    OptionalStringPair brpop(Input first,
                                Input last,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    template <typename T>
    OptionalStringPair brpop(std::initializer_list<T> il,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0}) {
        return brpop(il.begin(), il.end(), timeout);
    }

    OptionalString brpoplpush(const StringView &source,
                                const StringView &destination,
                                long long timeout);

    OptionalString brpoplpush(const StringView &source,
                                const StringView &destination,
                                const std::chrono::seconds &timeout = std::chrono::seconds{0});

    OptionalString lindex(const StringView &key, long long index);

    long long linsert(const StringView &key,
                        InsertPosition position,
                        const StringView &pivot,
                        const StringView &val);

    long long llen(const StringView &key);

    OptionalString lpop(const StringView &key);

    long long lpush(const StringView &key, const StringView &val);

    template <typename Input>
    long long lpush(const StringView &key, Input first, Input last);

    template <typename T>
    long long lpush(const StringView &key, std::initializer_list<T> il) {
        return lpush(key, il.begin(), il.end());
    }

    long long lpushx(const StringView &key, const StringView &val);

    template <typename Output>
    void lrange(const StringView &key, long long start, long long stop, Output output);

    long long lrem(const StringView &key, long long count, const StringView &val);

    void lset(const StringView &key, long long index, const StringView &val);

    void ltrim(const StringView &key, long long start, long long stop);

    OptionalString rpop(const StringView &key);

    OptionalString rpoplpush(const StringView &source, const StringView &destination);

    long long rpush(const StringView &key, const StringView &val);

    template <typename Input>
    long long rpush(const StringView &key, Input first, Input last);

    template <typename T>
    long long rpush(const StringView &key, std::initializer_list<T> il) {
        return rpush(key, il.begin(), il.end());
    }

    long long rpushx(const StringView &key, const StringView &val);

    // HASH commands.

    long long hdel(const StringView &key, const StringView &field);

    template <typename Input>
    long long hdel(const StringView &key, Input first, Input last);

    template <typename T>
    long long hdel(const StringView &key, std::initializer_list<T> il) {
        return hdel(key, il.begin(), il.end());
    }

    bool hexists(const StringView &key, const StringView &field);

    OptionalString hget(const StringView &key, const StringView &field);

    template <typename Output>
    void hgetall(const StringView &key, Output output);

    long long hincrby(const StringView &key, const StringView &field, long long increment);

    double hincrbyfloat(const StringView &key, const StringView &field, double increment);

    template <typename Output>
    void hkeys(const StringView &key, Output output);

    long long hlen(const StringView &key);

    template <typename Input, typename Output>
    void hmget(const StringView &key, Input first, Input last, Output output);

    template <typename T, typename Output>
    void hmget(const StringView &key, std::initializer_list<T> il, Output output) {
        hmget(key, il.begin(), il.end(), output);
    }

    template <typename Input>
    void hmset(const StringView &key, Input first, Input last);

    template <typename T>
    void hmset(const StringView &key, std::initializer_list<T> il) {
        hmset(key, il.begin(), il.end());
    }

    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    template <typename Output>
    long long hscan(const StringView &key,
                    long long cursor,
                    Output output);

    bool hset(const StringView &key, const StringView &field, const StringView &val);

    bool hset(const StringView &key, const std::pair<StringView, StringView> &item);

    bool hsetnx(const StringView &key, const StringView &field, const StringView &val);

    bool hsetnx(const StringView &key, const std::pair<StringView, StringView> &item);

    long long hstrlen(const StringView &key, const StringView &field);

    template <typename Output>
    void hvals(const StringView &key, Output output);

    // SET commands.

    long long sadd(const StringView &key, const StringView &member);

    template <typename Input>
    long long sadd(const StringView &key, Input first, Input last);

    template <typename T>
    long long sadd(const StringView &key, std::initializer_list<T> il) {
        return sadd(key, il.begin(), il.end());
    }

    long long scard(const StringView &key);

    template <typename Input, typename Output>
    void sdiff(Input first, Input last, Output output);

    template <typename T, typename Output>
    void sdiff(std::initializer_list<T> il, Output output) {
        sdiff(il.begin(), il.end(), output);
    }

    long long sdiffstore(const StringView &destination, const StringView &key);

    template <typename Input>
    long long sdiffstore(const StringView &destination,
                            Input first,
                            Input last);

    template <typename T>
    long long sdiffstore(const StringView &destination,
                            std::initializer_list<T> il) {
        return sdiffstore(destination, il.begin(), il.end());
    }

    template <typename Input, typename Output>
    void sinter(Input first, Input last, Output output);

    template <typename T, typename Output>
    void sinter(std::initializer_list<T> il, Output output) {
        sinter(il.begin(), il.end(), output);
    }

    long long sinterstore(const StringView &destination, const StringView &key);

    template <typename Input>
    long long sinterstore(const StringView &destination,
                            Input first,
                            Input last);

    template <typename T>
    long long sinterstore(const StringView &destination,
                            std::initializer_list<T> il) {
        return sinterstore(destination, il.begin(), il.end());
    }

    bool sismember(const StringView &key, const StringView &member);

    template <typename Output>
    void smembers(const StringView &key, Output output);

    bool smove(const StringView &source,
                const StringView &destination,
                const StringView &member);

    OptionalString spop(const StringView &key);

    template <typename Output>
    void spop(const StringView &key, long long count, Output output);

    OptionalString srandmember(const StringView &key);

    template <typename Output>
    void srandmember(const StringView &key, long long count, Output output);

    long long srem(const StringView &key, const StringView &member);

    template <typename Input>
    long long srem(const StringView &key, Input first, Input last);

    template <typename T>
    long long srem(const StringView &key, std::initializer_list<T> il) {
        return srem(key, il.begin(), il.end());
    }

    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    template <typename Output>
    long long sscan(const StringView &key,
                    long long cursor,
                    Output output);

    template <typename Input, typename Output>
    void sunion(Input first, Input last, Output output);

    template <typename T, typename Output>
    void sunion(std::initializer_list<T> il, Output output) {
        sunion(il.begin(), il.end(), output);
    }

    long long sunionstore(const StringView &destination, const StringView &key);

    template <typename Input>
    long long sunionstore(const StringView &destination, Input first, Input last);

    template <typename T>
    long long sunionstore(const StringView &destination, std::initializer_list<T> il) {
        return sunionstore(destination, il.begin(), il.end());
    }

    // SORTED SET commands.

    auto bzpopmax(const StringView &key, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    auto bzpopmax(const StringView &key,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename Input>
    auto bzpopmax(Input first, Input last, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename Input>
    auto bzpopmax(Input first,
                    Input last,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename T>
    auto bzpopmax(std::initializer_list<T> il, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    template <typename T>
    auto bzpopmax(std::initializer_list<T> il,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmax(il.begin(), il.end(), timeout);
    }

    auto bzpopmin(const StringView &key, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    auto bzpopmin(const StringView &key,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename Input>
    auto bzpopmin(Input first, Input last, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename Input>
    auto bzpopmin(Input first,
                    Input last,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>>;

    template <typename T>
    auto bzpopmin(std::initializer_list<T> il, long long timeout)
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    template <typename T>
    auto bzpopmin(std::initializer_list<T> il,
                    const std::chrono::seconds &timeout = std::chrono::seconds{0})
        -> Optional<std::tuple<std::string, std::string, double>> {
        return bzpopmin(il.begin(), il.end(), timeout);
    }

    // We don't support the INCR option, since you can always use ZINCRBY instead.
    long long zadd(const StringView &key,
                    const StringView &member,
                    double score,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false);

    template <typename Input>
    long long zadd(const StringView &key,
                    Input first,
                    Input last,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false);

    template <typename T>
    long long zadd(const StringView &key,
                    std::initializer_list<T> il,
                    UpdateType type = UpdateType::ALWAYS,
                    bool changed = false) {
        return zadd(key, il.begin(), il.end(), type, changed);
    }

    long long zcard(const StringView &key);

    template <typename Interval>
    long long zcount(const StringView &key, const Interval &interval);

    double zincrby(const StringView &key, double increment, const StringView &member);

    // There's no aggregation type parameter for single key overload, since these 3 types
    // have the same effect.
    long long zinterstore(const StringView &destination, const StringView &key, double weight);

    // If *Input* is an iterator of a container of string,
    // we use the default weight, i.e. 1, and send
    // *ZINTERSTORE destination numkeys key [key ...] [AGGREGATE SUM|MIN|MAX]* command.
    // If *Input* is an iterator of a container of pair<string, double>, i.e. key-weight pair,
    // we send the command with the given weights:
    // *ZINTERSTORE destination numkeys key [key ...]
    // [WEIGHTS weight [weight ...]] [AGGREGATE SUM|MIN|MAX]*
    //
    // The following code use the default weight:
    //
    // vector<string> keys = {"k1", "k2", "k3"};
    // redis.zinterstore(destination, keys.begin(), keys.end());
    //
    // On the other hand, the following code use the given weights:
    //
    // vector<pair<string, double>> keys_with_weights = {{"k1", 1}, {"k2", 2}, {"k3", 3}};
    // redis.zinterstore(destination, keys_with_weights.begin(), keys_with_weights.end());
    //
    // NOTE: `keys_with_weights` can also be of type `unordered_map<string, double>`.
    // However, it will be slower than vector<pair<string, double>>, since we use
    // `distance(first, last)` to calculate the *numkeys* parameter.
    //
    // This also applies to *ZUNIONSTORE* command.
    template <typename Input>
    long long zinterstore(const StringView &destination,
                            Input first,
                            Input last,
                            Aggregation type = Aggregation::SUM);

    template <typename T>
    long long zinterstore(const StringView &destination,
                            std::initializer_list<T> il,
                            Aggregation type = Aggregation::SUM) {
        return zinterstore(destination, il.begin(), il.end(), type);
    }

    template <typename Interval>
    long long zlexcount(const StringView &key, const Interval &interval);

    Optional<std::pair<std::string, double>> zpopmax(const StringView &key);

    template <typename Output>
    void zpopmax(const StringView &key, long long count, Output output);

    Optional<std::pair<std::string, double>> zpopmin(const StringView &key);

    template <typename Output>
    void zpopmin(const StringView &key, long long count, Output output);

    // If *output* is an iterator of a container of string,
    // we send *ZRANGE key start stop* command.
    // If it's an iterator of a container of pair<string, double>,
    // we send *ZRANGE key start stop WITHSCORES* command.
    //
    // The following code sends *ZRANGE* without the *WITHSCORES* option:
    //
    // vector<string> result;
    // redis.zrange("key", 0, -1, back_inserter(result));
    //
    // On the other hand, the following code sends command with *WITHSCORES* option:
    //
    // unordered_map<string, double> with_score;
    // redis.zrange("key", 0, -1, inserter(with_score, with_score.end()));
    //
    // This also applies to other commands with the *WITHSCORES* option,
    // e.g. *ZRANGEBYSCORE*, *ZREVRANGE*, *ZREVRANGEBYSCORE*.
    template <typename Output>
    void zrange(const StringView &key, long long start, long long stop, Output output);

    template <typename Interval, typename Output>
    void zrangebylex(const StringView &key, const Interval &interval, Output output);

    template <typename Interval, typename Output>
    void zrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    // See *zrange* comment on how to send command with *WITHSCORES* option.
    template <typename Interval, typename Output>
    void zrangebyscore(const StringView &key, const Interval &interval, Output output);

    // See *zrange* comment on how to send command with *WITHSCORES* option.
    template <typename Interval, typename Output>
    void zrangebyscore(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    OptionalLongLong zrank(const StringView &key, const StringView &member);

    long long zrem(const StringView &key, const StringView &member);

    template <typename Input>
    long long zrem(const StringView &key, Input first, Input last);

    template <typename T>
    long long zrem(const StringView &key, std::initializer_list<T> il) {
        return zrem(key, il.begin(), il.end());
    }

    template <typename Interval>
    long long zremrangebylex(const StringView &key, const Interval &interval);

    long long zremrangebyrank(const StringView &key, long long start, long long stop);

    template <typename Interval>
    long long zremrangebyscore(const StringView &key, const Interval &interval);

    // See *zrange* comment on how to send command with *WITHSCORES* option.
    template <typename Output>
    void zrevrange(const StringView &key, long long start, long long stop, Output output);

    template <typename Interval, typename Output>
    void zrevrangebylex(const StringView &key, const Interval &interval, Output output);

    template <typename Interval, typename Output>
    void zrevrangebylex(const StringView &key,
                        const Interval &interval,
                        const LimitOptions &opts,
                        Output output);

    // See *zrange* comment on how to send command with *WITHSCORES* option.
    template <typename Interval, typename Output>
    void zrevrangebyscore(const StringView &key, const Interval &interval, Output output);

    // See *zrange* comment on how to send command with *WITHSCORES* option.
    template <typename Interval, typename Output>
    void zrevrangebyscore(const StringView &key,
                            const Interval &interval,
                            const LimitOptions &opts,
                            Output output);

    OptionalLongLong zrevrank(const StringView &key, const StringView &member);

    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    long long count,
                    Output output);

    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    const StringView &pattern,
                    Output output);

    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    long long count,
                    Output output);

    template <typename Output>
    long long zscan(const StringView &key,
                    long long cursor,
                    Output output);

    OptionalDouble zscore(const StringView &key, const StringView &member);

    // There's no aggregation type parameter for single key overload, since these 3 types
    // have the same effect.
    long long zunionstore(const StringView &destination, const StringView &key, double weight);

    // See *zinterstore* comment for how to use this method.
    template <typename Input>
    long long zunionstore(const StringView &destination,
                            Input first,
                            Input last,
                            Aggregation type = Aggregation::SUM);

    template <typename T>
    long long zunionstore(const StringView &destination,
                            std::initializer_list<T> il,
                            Aggregation type = Aggregation::SUM) {
        return zunionstore(destination, il.begin(), il.end(), type);
    }

    // HYPERLOGLOG commands.

    bool pfadd(const StringView &key, const StringView &element);

    template <typename Input>
    bool pfadd(const StringView &key, Input first, Input last);

    template <typename T>
    bool pfadd(const StringView &key, std::initializer_list<T> il) {
        return pfadd(key, il.begin(), il.end());
    }

    long long pfcount(const StringView &key);

    template <typename Input>
    long long pfcount(Input first, Input last);

    template <typename T>
    long long pfcount(std::initializer_list<T> il) {
        return pfcount(il.begin(), il.end());
    }

    void pfmerge(const StringView &destination, const StringView &key);

    template <typename Input>
    void pfmerge(const StringView &destination, Input first, Input last);

    template <typename T>
    void pfmerge(const StringView &destination, std::initializer_list<T> il) {
        pfmerge(destination, il.begin(), il.end());
    }

    // GEO commands.

    long long geoadd(const StringView &key,
                        const std::tuple<StringView, double, double> &member);

    template <typename Input>
    long long geoadd(const StringView &key,
                        Input first,
                        Input last);

    template <typename T>
    long long geoadd(const StringView &key,
                        std::initializer_list<T> il) {
        return geoadd(key, il.begin(), il.end());
    }

    OptionalDouble geodist(const StringView &key,
                            const StringView &member1,
                            const StringView &member2,
                            GeoUnit unit = GeoUnit::M);

    template <typename Input, typename Output>
    void geohash(const StringView &key, Input first, Input last, Output output);

    template <typename T, typename Output>
    void geohash(const StringView &key, std::initializer_list<T> il, Output output) {
        geohash(key, il.begin(), il.end(), output);
    }

    template <typename Input, typename Output>
    void geopos(const StringView &key, Input first, Input last, Output output);

    template <typename T, typename Output>
    void geopos(const StringView &key, std::initializer_list<T> il, Output output) {
        geopos(key, il.begin(), il.end(), output);
    }

    // TODO:
    // 1. since we have different overloads for georadius and georadius-store,
    //    we might use the GEORADIUS_RO command in the future.
    // 2. there're too many parameters for this method, we might refactor it.
    OptionalLongLong georadius(const StringView &key,
                                const std::pair<double, double> &loc,
                                double radius,
                                GeoUnit unit,
                                const StringView &destination,
                                bool store_dist,
                                long long count);

    // If *output* is an iterator of a container of string, we send *GEORADIUS* command
    // without any options and only get the members in the specified geo range.
    // If *output* is an iterator of a container of a tuple, the type of the tuple decides
    // options we send with the *GEORADIUS* command. If the tuple has an element of type
    // double, we send the *WITHDIST* option. If it has an element of type string, we send
    // the *WITHHASH* option. If it has an element of type pair<double, double>, we send
    // the *WITHCOORD* option. For example:
    //
    // The following code only gets the members in range, i.e. without any option.
    //
    // vector<string> members;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(members))
    //
    // The following code sends the command with *WITHDIST* option.
    //
    // vector<tuple<string, double>> with_dist;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist))
    //
    // The following code sends the command with *WITHDIST* and *WITHHASH* options.
    //
    // vector<tuple<string, double, string>> with_dist_hash;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist_hash))
    //
    // The following code sends the command with *WITHDIST*, *WITHCOORD* and *WITHHASH* options.
    //
    // vector<tuple<string, double, pair<double, double>, string>> with_dist_coord_hash;
    // redis.georadius("key", make_pair(10.1, 10.2), 10, GeoUnit::KM, 10, true,
    //                  back_inserter(with_dist_coord_hash))
    //
    // This also applies to *GEORADIUSBYMEMBER*.
    template <typename Output>
    void georadius(const StringView &key,
                    const std::pair<double, double> &loc,
                    double radius,
                    GeoUnit unit,
                    long long count,
                    bool asc,
                    Output output);

    OptionalLongLong georadiusbymember(const StringView &key,
                                        const StringView &member,
                                        double radius,
                                        GeoUnit unit,
                                        const StringView &destination,
                                        bool store_dist,
                                        long long count);

    // See comments on *GEORADIUS*.
    template <typename Output>
    void georadiusbymember(const StringView &key,
                            const StringView &member,
                            double radius,
                            GeoUnit unit,
                            long long count,
                            bool asc,
                            Output output);

    // SCRIPTING commands.

    template <typename Result>
    Result eval(const StringView &script,
                std::initializer_list<StringView> keys,
                std::initializer_list<StringView> args);

    template <typename Output>
    void eval(const StringView &script,
                std::initializer_list<StringView> keys,
                std::initializer_list<StringView> args,
                Output output);

    template <typename Result>
    Result evalsha(const StringView &script,
                    std::initializer_list<StringView> keys,
                    std::initializer_list<StringView> args);

    template <typename Output>
    void evalsha(const StringView &script,
                    std::initializer_list<StringView> keys,
                    std::initializer_list<StringView> args,
                    Output output);

    template <typename Input, typename Output>
    void script_exists(Input first, Input last, Output output);

    template <typename T, typename Output>
    void script_exists(std::initializer_list<T> il, Output output) {
        script_exists(il.begin(), il.end(), output);
    }

    void script_flush();

    void script_kill();

    std::string script_load(const StringView &script);

    // PUBSUB commands.

    long long publish(const StringView &channel, const StringView &message);

    // Transaction commands.
    void watch(const StringView &key);

    template <typename Input>
    void watch(Input first, Input last);

    template <typename T>
    void watch(std::initializer_list<T> il) {
        watch(il.begin(), il.end());
    }

    // Stream commands.

    long long xack(const StringView &key, const StringView &group, const StringView &id);

    template <typename Input>
    long long xack(const StringView &key, const StringView &group, Input first, Input last);

    template <typename T>
    long long xack(const StringView &key, const StringView &group, std::initializer_list<T> il) {
        return xack(key, group, il.begin(), il.end());
    }

    template <typename Input>
    std::string xadd(const StringView &key, const StringView &id, Input first, Input last);

    template <typename T>
    std::string xadd(const StringView &key, const StringView &id, std::initializer_list<T> il) {
        return xadd(key, id, il.begin(), il.end());
    }

    template <typename Input>
    std::string xadd(const StringView &key,
                        const StringView &id,
                        Input first,
                        Input last,
                        long long count,
                        bool approx = true);

    template <typename T>
    std::string xadd(const StringView &key,
                        const StringView &id,
                        std::initializer_list<T> il,
                        long long count,
                        bool approx = true) {
        return xadd(key, id, il.begin(), il.end(), count, approx);
    }

    template <typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                const StringView &id,
                Output output);

    template <typename Input, typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                Input first,
                Input last,
                Output output);

    template <typename T, typename Output>
    void xclaim(const StringView &key,
                const StringView &group,
                const StringView &consumer,
                const std::chrono::milliseconds &min_idle_time,
                std::initializer_list<T> il,
                Output output) {
        xclaim(key, group, consumer, min_idle_time, il.begin(), il.end(), output);
    }

    long long xdel(const StringView &key, const StringView &id);

    template <typename Input>
    long long xdel(const StringView &key, Input first, Input last);

    template <typename T>
    long long xdel(const StringView &key, std::initializer_list<T> il) {
        return xdel(key, il.begin(), il.end());
    }

    void xgroup_create(const StringView &key,
                        const StringView &group,
                        const StringView &id,
                        bool mkstream = false);

    void xgroup_setid(const StringView &key, const StringView &group, const StringView &id);

    long long xgroup_destroy(const StringView &key, const StringView &group);

    long long xgroup_delconsumer(const StringView &key,
                                    const StringView &group,
                                    const StringView &consumer);

    long long xlen(const StringView &key);

    template <typename Output>
    auto xpending(const StringView &key, const StringView &group, Output output)
        -> std::tuple<long long, OptionalString, OptionalString>;

    template <typename Output>
    void xpending(const StringView &key,
                    const StringView &group,
                    const StringView &start,
                    const StringView &end,
                    long long count,
                    Output output);

    template <typename Output>
    void xpending(const StringView &key,
                    const StringView &group,
                    const StringView &start,
                    const StringView &end,
                    long long count,
                    const StringView &consumer,
                    Output output);

    template <typename Output>
    void xrange(const StringView &key,
                const StringView &start,
                const StringView &end,
                Output output);

    template <typename Output>
    void xrange(const StringView &key,
                const StringView &start,
                const StringView &end,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                Output output) {
        xread(key, id, 0, output);
    }

    template <typename Input, typename Output>
    auto xread(Input first, Input last, long long count, Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xread(Input first, Input last, Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xread(first ,last, 0, output);
    }

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                const std::chrono::milliseconds &timeout,
                long long count,
                Output output);

    template <typename Output>
    void xread(const StringView &key,
                const StringView &id,
                const std::chrono::milliseconds &timeout,
                Output output) {
        xread(key, id, timeout, 0, output);
    }

    template <typename Input, typename Output>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout,
                long long count,
                Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xread(Input first,
                Input last,
                const std::chrono::milliseconds &timeout,
                Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xread(first, last, timeout, 0, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    long long count,
                    bool noack,
                    Output output);

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    long long count,
                    Output output) {
        xreadgroup(group, consumer, key, id, count, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    Output output) {
        xreadgroup(group, consumer, key, id, 0, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count,
                    bool noack,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    long long count,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first ,last, count, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first ,last, 0, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    bool noack,
                    Output output);

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    Output output) {
        xreadgroup(group, consumer, key, id, timeout, count, false, output);
    }

    template <typename Output>
    void xreadgroup(const StringView &group,
                    const StringView &consumer,
                    const StringView &key,
                    const StringView &id,
                    const std::chrono::milliseconds &timeout,
                    Output output) {
        xreadgroup(group, consumer, key, id, timeout, 0, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    bool noack,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type;

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    long long count,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first, last, timeout, count, false, output);
    }

    template <typename Input, typename Output>
    auto xreadgroup(const StringView &group,
                    const StringView &consumer,
                    Input first,
                    Input last,
                    const std::chrono::milliseconds &timeout,
                    Output output)
        -> typename std::enable_if<!std::is_convertible<Input, StringView>::value>::type {
        xreadgroup(group, consumer, first, last, timeout, 0, false, output);
    }

    template <typename Output>
    void xrevrange(const StringView &key,
                    const StringView &end,
                    const StringView &start,
                    Output output);

    template <typename Output>
    void xrevrange(const StringView &key,
                    const StringView &end,
                    const StringView &start,
                    long long count,
                    Output output);

    long long xtrim(const StringView &key, long long count, bool approx = true);

private:
    class ConnectionPoolGuard {
    public:
        ConnectionPoolGuard(ConnectionPool &pool,
                            Connection &connection) : _pool(pool), _connection(connection) {}

        ~ConnectionPoolGuard() {
            _pool.release(std::move(_connection));
        }

    private:
        ConnectionPool &_pool;
        Connection &_connection;
    };

    template <typename Impl>
    friend class QueuedRedis;

    friend class RedisCluster;

    // For internal use.
    explicit Redis(const ConnectionSPtr &connection);

    template <std::size_t ...Is, typename ...Args>
    ReplyUPtr _command(const StringView &cmd_name, const IndexSequence<Is...> &, Args &&...args) {
        return command(cmd_name, NthValue<Is>(std::forward<Args>(args)...)...);
    }

    template <typename Cmd, typename ...Args>
    ReplyUPtr _command(Connection &connection, Cmd cmd, Args &&...args);

    template <typename Cmd, typename ...Args>
    ReplyUPtr _score_command(std::true_type, Cmd cmd, Args &&... args);

    template <typename Cmd, typename ...Args>
    ReplyUPtr _score_command(std::false_type, Cmd cmd, Args &&... args);

    template <typename Output, typename Cmd, typename ...Args>
    ReplyUPtr _score_command(Cmd cmd, Args &&... args);

    // Pool Mode.
    // Public constructors create a *Redis* instance with a pool.
    // In this case, *_connection* is a null pointer, and is never used.
    ConnectionPool _pool;

    // Single Connection Mode.
    // Private constructor creats a *Redis* instance with a single connection.
    // This is used when we create Transaction, Pipeline and Subscriber.
    // In this case, *_pool* is empty, and is never used.
    ConnectionSPtr _connection;
};

}

}

#include "redis.hpp"

#endif // end SEWENEW_REDISPLUSPLUS_REDIS_H
