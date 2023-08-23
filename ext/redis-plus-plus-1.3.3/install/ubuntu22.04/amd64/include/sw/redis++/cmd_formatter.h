/**************************************************************************
   Copyright (c) 2021 sewenew

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

#ifndef SEWENEW_REDISPLUSPLUS_CMD_FORMATTER_H
#define SEWENEW_REDISPLUSPLUS_CMD_FORMATTER_H

#include <hiredis/hiredis.h>
#include "command_options.h"
#include "command_args.h"
#include "command.h"
#include "errors.h"

namespace sw {

namespace redis {

class FormattedCommand {
public:
    FormattedCommand(char *data, int len) : _data(data), _size(len) {
        if (data == nullptr || len < 0) {
            throw Error("failed to format command");
        }
    }

    FormattedCommand(const FormattedCommand &) = delete;
    FormattedCommand& operator=(const FormattedCommand &) = delete;

    FormattedCommand(FormattedCommand &&that) noexcept {
        _move(std::move(that));
    }

    FormattedCommand& operator=(FormattedCommand &&that) noexcept {
        if (this != &that) {
            _move(std::move(that));
        }

        return *this;
    }

    ~FormattedCommand() noexcept {
        if (_data != nullptr) {
            redisFreeCommand(_data);
        }
    }

    const char* data() const noexcept {
        return _data;
    }

    int size() const noexcept {
        return _size;
    }

private:
    void _move(FormattedCommand &&that) noexcept {
        _data = that._data;
        _size = that._size;
        that._data = nullptr;
        that._size = 0;
    }

    char *_data = nullptr;
    int _size = 0;
};

namespace fmt {

template <typename ...Args>
FormattedCommand format_cmd(const char *format, Args &&...args) {
    char *data = nullptr;
    auto len = redisFormatCommand(&data, format, std::forward<Args>(args)...);

    return FormattedCommand(data, len);
}

inline FormattedCommand format_cmd(int argc, const char **argv, const std::size_t *argv_len) {
    char *data = nullptr;
    auto len = redisFormatCommandArgv(&data, argc, argv, argv_len);

    return FormattedCommand(data, len);
}

inline FormattedCommand format_cmd(CmdArgs &args) {
    char *data = nullptr;
    auto len = redisFormatCommandArgv(&data, args.size(), args.argv(), args.argv_len());

    return FormattedCommand(data, len);
}

struct SetResultParser {
    bool operator()(redisReply &reply) const {
        sw::redis::reply::rewrite_set_reply(reply);
        return sw::redis::reply::parse<bool>(reply);
    }
};

// CONNECTION commands.

inline FormattedCommand echo(const StringView &msg) {
    return format_cmd("ECHO %b", msg.data(), msg.size());
}

inline FormattedCommand ping() {
    return format_cmd("PING");
}

inline FormattedCommand ping(const StringView &msg) {
    return format_cmd("PING %b", msg.data(), msg.size());
}

inline FormattedCommand del(const StringView &key) {
    return format_cmd("DEL %b", key.data(), key.size());
}

template <typename Input>
FormattedCommand del_range(Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "DEL" << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand exists(const StringView &key) {
    return format_cmd("EXISTS %b", key.data(), key.size());
}

template <typename Input>
FormattedCommand exists_range(Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "EXISTS" << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand expire(const StringView &key, const std::chrono::seconds &timeout) {
    return format_cmd("EXPIRE %b %lld", key.data(), key.size(), timeout.count());
}

inline FormattedCommand expireat(const StringView &key,
        const std::chrono::time_point<std::chrono::system_clock,
                                        std::chrono::seconds> &tp) {
    return format_cmd("EXPIREAT %b %lld", key.data(), key.size(), tp.time_since_epoch().count());
}

inline FormattedCommand pexpire(const StringView &key,
        const std::chrono::milliseconds &timeout) {
    return format_cmd("PEXPIRE %b %lld", key.data(), key.size(), timeout.count());
}

inline FormattedCommand pexpireat(const StringView &key,
        const std::chrono::time_point<std::chrono::system_clock,
                                            std::chrono::milliseconds> &tp) {
    return format_cmd("PEXPIREAT %b %lld", key.data(), key.size(), tp.time_since_epoch().count());
}

inline FormattedCommand pttl(const StringView &key) {
    return format_cmd("PTTL %b", key.data(), key.size());
}

inline FormattedCommand rename(const StringView &key, const StringView &newkey) {
    return format_cmd("RENAME %b %b", key.data(), key.size(), newkey.data(), newkey.size());
}

inline FormattedCommand renamenx(const StringView &key, const StringView &newkey) {
    return format_cmd("RENAMENX %b %b", key.data(), key.size(), newkey.data(), newkey.size());
}

inline FormattedCommand ttl(const StringView &key) {
    return format_cmd("TTL %b", key.data(), key.size());
}

inline FormattedCommand unlink(const StringView &key) {
    return format_cmd("UNLINK %b", key.data(), key.size());
}

template <typename Input>
FormattedCommand unlink_range(Input first, Input last) {
    CmdArgs args;
    args << "UNLINK" << std::make_pair(first, last);

    return format_cmd(args);
}

// STRING commands.

inline FormattedCommand get(const StringView &key) {
    return format_cmd("GET %b", key.data(), key.size());
}

inline FormattedCommand incr(const StringView &key) {
    return format_cmd("INCR %b", key.data(), key.size());
}

inline FormattedCommand incrby(const StringView &key, long long increment) {
    return format_cmd("INCRBY %b %lld", key.data(), key.size(), increment);
}

inline FormattedCommand incrbyfloat(const StringView &key, double increment) {
    return format_cmd("INCRBYFLOAT %b %f", key.data(), key.size(), increment);
}

template <typename Input>
FormattedCommand mget(Input first, Input last) {
    CmdArgs args;
    args << "MGET" << std::make_pair(first, last);

    return format_cmd(args);
}

template <typename Input>
FormattedCommand mset(Input first, Input last) {
    CmdArgs args;
    args << "MSET" << std::make_pair(first, last);

    return format_cmd(args);
}

template <typename Input>
FormattedCommand msetnx(Input first, Input last) {
    CmdArgs args;
    args << "MSETNX" << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand set(const StringView &key,
        const StringView &val,
        const std::chrono::milliseconds &ttl,
        UpdateType type) {
    CmdArgs args;
    args << "SET" << key << val;

    if (ttl > std::chrono::milliseconds(0)) {
        args << "PX" << ttl.count();
    }

    cmd::detail::set_update_type(args, type);

    return format_cmd(args);
}

inline FormattedCommand strlen(const StringView &key) {
    return format_cmd("STRLEN %b", key.data(), key.size());
}

inline FormattedCommand blpop(const StringView &key, const std::chrono::seconds &timeout) {
    return format_cmd("BLPOP %b %lld", key.data(), key.size(), timeout.count());
}

template <typename Input>
FormattedCommand blpop_range(Input first, Input last, const std::chrono::seconds &timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BLPOP" << std::make_pair(first, last) << timeout.count();

    return format_cmd(args);
}

inline FormattedCommand brpop(const StringView &key, const std::chrono::seconds &timeout) {
    return format_cmd("BRPOP %b %lld", key.data(), key.size(), timeout.count());
}


template <typename Input>
FormattedCommand brpop_range(Input first, Input last, const std::chrono::seconds &timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BRPOP" << std::make_pair(first, last) << timeout.count();

    return format_cmd(args);
}

inline FormattedCommand brpoplpush(const StringView &source,
        const StringView &destination,
        const std::chrono::seconds &timeout) {
    return format_cmd("BRPOPLPUSH %b %b %lld",
            source.data(), source.size(),
            destination.data(), destination.size(),
            timeout.count());
}

inline FormattedCommand llen(const StringView &key) {
    return format_cmd("LLEN %b", key.data(), key.size());
}

inline FormattedCommand lpop(const StringView &key) {
    return format_cmd("LPOP %b", key.data(), key.size());
}

inline FormattedCommand lpush(const StringView &key, const StringView &val) {
    return format_cmd("LPUSH %b %b", key.data(), key.size(), val.data(), val.size());
}

template <typename Input>
FormattedCommand lpush_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "LPUSH" << key << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand lrange(const StringView &key, long long start, long long stop) {
    return format_cmd("LRANGE %b %lld %lld", key.data(), key.size(), start, stop);
}

inline FormattedCommand lrem(const StringView &key, long long count, const StringView &val) {
    return format_cmd("LREM %b %lld %b", key.data(), key.size(), count, val.data(), val.size());
}

inline FormattedCommand ltrim(const StringView &key, long long start, long long stop) {
    return format_cmd("LTRIM %b %lld %lld", key.data(), key.size(), start, stop);
}

inline FormattedCommand rpop(const StringView &key) {
    return format_cmd("RPOP %b", key.data(), key.size());
}

inline FormattedCommand rpoplpush(const StringView &source, const StringView &destination) {
    return format_cmd("RPOPLPUSH %b %b",
            source.data(), source.size(),
            destination.data(), destination.size());
}

inline FormattedCommand rpush(const StringView &key, const StringView &val) {
    return format_cmd("RPUSH %b %b", key.data(), key.size(), val.data(), val.size());
}

template <typename Input>
FormattedCommand rpush_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "RPUSH" << key << std::make_pair(first, last);

    return format_cmd(args);
}

// HASH commands.

inline FormattedCommand hdel(const StringView &key, const StringView &field) {
    return format_cmd("HDEL %b %b", key.data(), key.size(), field.data(), field.size());
}

template <typename Input>
FormattedCommand hdel_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HDEL" << key << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand hexists(const StringView &key, const StringView &field) {
    return format_cmd("HEXISTS %b %b", key.data(), key.size(), field.data(), field.size());
}

inline FormattedCommand hget(const StringView &key, const StringView &field) {
    return format_cmd("HGET %b %b", key.data(), key.size(), field.data(), field.size());
}

inline FormattedCommand hgetall(const StringView &key) {
    return format_cmd("HGETALL %b", key.data(), key.size());
}

inline FormattedCommand hincrby(const StringView &key,
        const StringView &field,
        long long increment) {
    return format_cmd("HINCRBY %b %b %lld",
            key.data(), key.size(),
            field.data(), field.size(),
            increment);
}

inline FormattedCommand hincrbyfloat(const StringView &key,
        const StringView &field,
        double increment) {
    return format_cmd("HINCRBYFLOAT %b %b %f",
            key.data(), key.size(),
            field.data(), field.size(),
            increment);
}

inline FormattedCommand hkeys(const StringView &key) {
    return format_cmd("HKEYS %b", key.data(), key.size());
}

inline FormattedCommand hlen(const StringView &key) {
    return format_cmd("HLEN %b", key.data(), key.size());
}

template <typename Input>
FormattedCommand hmget(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HMGET" << key << std::make_pair(first, last);

    return format_cmd(args);
}

template <typename Input>
FormattedCommand hmset(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "HMSET" << key << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand hset(const StringView &key,
        const StringView &field,
        const StringView &val) {
    return format_cmd("HSET %b %b %b",
            key.data(), key.size(),
            field.data(), field.size(),
            val.data(), val.size());
}

template <typename Input>
auto hset_range(const StringView &key,
        Input first,
        Input last)
    -> typename std::enable_if<!std::is_convertible<Input, StringView>::value,
                                FormattedCommand>::type {
    assert(first != last);

    CmdArgs args;
    args << "HSET" << key << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand hvals(const StringView &key) {
    return format_cmd("HVALS %b", key.data(), key.size());
}

// SET commands.

inline FormattedCommand sadd(const StringView &key, const StringView &member) {
    return format_cmd("SADD %b %b", key.data(), key.size(), member.data(), member.size());
}

template <typename Input>
FormattedCommand sadd_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SADD" << key << std::make_pair(first, last);

    return format_cmd(args);
}

inline FormattedCommand scard(const StringView &key) {
    return format_cmd("SCARD %b", key.data(), key.size());
}

inline FormattedCommand sismember(const StringView &key, const StringView &member) {
    return format_cmd("SISMEMBER %b %b", key.data(), key.size(), member.data(), member.size());
}

inline FormattedCommand smembers(const StringView &key) {
    return format_cmd("SMEMBERS %b", key.data(), key.size());
}

inline FormattedCommand spop(const StringView &key) {
    return format_cmd("SPOP %b", key.data(), key.size());
}

inline FormattedCommand spop(const StringView &key, long long count) {
    return format_cmd("SPOP %b %lld", key.data(), key.size(), count);
}

inline FormattedCommand srem(const StringView &key, const StringView &member) {
    return format_cmd("SREM %b %b", key.data(), key.size(), member.data(), member.size());
}

template <typename Input>
FormattedCommand srem_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "SREM" << key << std::make_pair(first, last);

    return format_cmd(args);
}

// SORTED SET commands.

inline FormattedCommand bzpopmax(const StringView &key, const std::chrono::seconds &timeout) {
    return format_cmd("BZPOPMAX %b %lld", key.data(), key.size(), timeout.count());
}

template <typename Input>
FormattedCommand bzpopmax_range(Input first,
        Input last,
        const std::chrono::seconds &timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BZPOPMAX" << std::make_pair(first, last) << timeout.count();

    return format_cmd(args);
}

inline FormattedCommand bzpopmin(const StringView &key, const std::chrono::seconds &timeout) {
    return format_cmd("BZPOPMIN %b %lld", key.data(), key.size(), timeout.count());
}

template <typename Input>
FormattedCommand bzpopmin_range(Input first, Input last, const std::chrono::seconds &timeout) {
    assert(first != last);

    CmdArgs args;
    args << "BZPOPMIN" << std::make_pair(first, last) << timeout.count();

    return format_cmd(args);
}

inline FormattedCommand zadd(const StringView &key,
        const StringView &member,
        double score,
        UpdateType type,
        bool changed) {
    CmdArgs args;
    args << "ZADD" << key;

    cmd::detail::set_update_type(args, type);

    if (changed) {
        args << "CH";
    }

    args << score << member;

    return format_cmd(args);
}

template <typename Input>
FormattedCommand zadd_range(const StringView &key,
        Input first,
        Input last,
        UpdateType type,
        bool changed) {
    CmdArgs args;
    args << "ZADD" << key;

    cmd::detail::set_update_type(args, type);

    if (changed) {
        args << "CH";
    }

    while (first != last) {
        // Swap the <member, score> pair to <score, member> pair.
        args << first->second << first->first;
        ++first;
    }

    return format_cmd(args);
}

inline FormattedCommand zcard(const StringView &key) {
    return format_cmd("ZCARD %b", key.data(), key.size());
}

template <typename Interval>
FormattedCommand zcount(const StringView &key, const Interval &interval) {
    return format_cmd("ZCOUNT %b %s %s",
            key.data(), key.size(),
            interval.min().c_str(),
            interval.max().c_str());
}

inline FormattedCommand zincrby(const StringView &key,
        double increment,
        const StringView &member) {
    return format_cmd("ZINCRBY %b %f %b",
            key.data(), key.size(),
            increment,
            member.data(), member.size());
}

template <typename Interval>
FormattedCommand zlexcount(const StringView &key,
        const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZLEXCOUNT %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

inline FormattedCommand zpopmax(const StringView &key) {
    return format_cmd("ZPOPMAX %b", key.data(), key.size());
}

inline FormattedCommand zpopmax(const StringView &key, long long count) {
    return format_cmd("ZPOPMAX %b %lld", key.data(), key.size(), count);
}

inline FormattedCommand zpopmin(const StringView &key) {
    return format_cmd("ZPOPMIN %b", key.data(), key.size());
}

inline FormattedCommand zpopmin_count(const StringView &key, long long count) {
    return format_cmd("ZPOPMIN %b %lld", key.data(), key.size(), count);
}

inline FormattedCommand zrange(const StringView &key, long long start, long long stop) {
    return format_cmd("ZRANGE %b %lld %lld", key.data(), key.size(), start, stop);
}

template <typename Interval>
FormattedCommand zrangebylex(const StringView &key,
        const Interval &interval,
        const LimitOptions &opts) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZRANGEBYLEX %b %b %b LIMIT %lld %lld",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size(),
                    opts.offset,
                    opts.count);
}

template <typename Interval>
FormattedCommand zrangebyscore(const StringView &key,
        const Interval &interval,
        const LimitOptions &opts) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZRANGEBYSCORE %b %b %b LIMIT %lld %lld",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size(),
                    opts.offset,
                    opts.count);
}

inline FormattedCommand zrank(const StringView &key, const StringView &member) {
    return format_cmd("ZRANK %b %b", key.data(), key.size(), member.data(), member.size());
}

inline FormattedCommand zrem(const StringView &key, const StringView &member) {
    return format_cmd("ZREM %b %b", key.data(), key.size(), member.data(), member.size());
}

template <typename Input>
FormattedCommand zrem_range(const StringView &key, Input first, Input last) {
    assert(first != last);

    CmdArgs args;
    args << "ZREM" << key << std::make_pair(first, last);

    return format_cmd(args);
}

template <typename Interval>
FormattedCommand zremrangebylex(const StringView &key, const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZREMRANGEBYLEX %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

inline FormattedCommand zremrangebyrank(const StringView &key, long long start, long long stop) {
    return format_cmd("ZREMRANGEBYRANK %b %lld %lld", key.data(), key.size(), start, stop);
}

template <typename Interval>
FormattedCommand zremrangebyscore(const StringView &key,
        const Interval &interval) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZREMRANGEBYSCORE %b %b %b",
                    key.data(), key.size(),
                    min.data(), min.size(),
                    max.data(), max.size());
}

template <typename Interval>
FormattedCommand zrevrangebylex(const StringView &key,
        const Interval &interval,
        const LimitOptions &opts) {
    const auto &min = interval.min();
    const auto &max = interval.max();

    return format_cmd("ZREVRANGEBYLEX %b %b %b LIMIT %lld %lld",
                    key.data(), key.size(),
                    max.data(), max.size(),
                    min.data(), min.size(),
                    opts.offset,
                    opts.count);
}

inline FormattedCommand zrevrank(const StringView &key, const StringView &member) {
    return format_cmd("ZREVRANK %b %b", key.data(), key.size(), member.data(), member.size());
}

inline FormattedCommand zscore(const StringView &key, const StringView &member) {
    return format_cmd("ZSCORE %b %b", key.data(), key.size(), member.data(), member.size());
}

// SCRIPTING commands.
template <typename Keys, typename Args>
FormattedCommand eval(const StringView &script,
        Keys keys_first,
        Keys keys_last,
        Args args_first,
        Args args_last) {
    CmdArgs args;
    auto keys_num = std::distance(keys_first, keys_last);

    args << "EVAL" << script << keys_num
            << std::make_pair(keys_first, keys_last)
            << std::make_pair(args_first, args_last);

    return format_cmd(args);
}

template <typename Keys, typename Args>
FormattedCommand evalsha(const StringView &script,
        Keys keys_first,
        Keys keys_last,
        Args args_first,
        Args args_last) {
    CmdArgs args;
    auto keys_num = std::distance(keys_first, keys_last);

    args << "EVALSHA" << script << keys_num
            << std::make_pair(keys_first, keys_last)
            << std::make_pair(args_first, args_last);

    return format_cmd(args);
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_CMD_FORMATTER_H
