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

#ifndef SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_HPP
#define SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_HPP

namespace sw {

namespace redis {

template <typename Impl>
template <typename ...Args>
QueuedRedis<Impl>::QueuedRedis(const ConnectionSPtr &connection, Args &&...args) :
            _connection(connection),
            _impl(std::forward<Args>(args)...) {
    assert(_connection);
}

template <typename Impl>
Redis QueuedRedis<Impl>::redis() {
    return Redis(_connection);
}

template <typename Impl>
template <typename Cmd, typename ...Args>
auto QueuedRedis<Impl>::command(Cmd cmd, Args &&...args)
    -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value,
                                QueuedRedis<Impl>&>::type {
    try {
        _sanity_check();

        _impl.command(*_connection, cmd, std::forward<Args>(args)...);

        ++_cmd_num;
    } catch (const Error &e) {
        _invalidate();
        throw;
    }

    return *this;
}

template <typename Impl>
template <typename ...Args>
QueuedRedis<Impl>& QueuedRedis<Impl>::command(const StringView &cmd_name, Args &&...args) {
    auto cmd = [](Connection &connection, const StringView &cmd_name, Args &&...args) {
                    CmdArgs cmd_args;
                    cmd_args.append(cmd_name, std::forward<Args>(args)...);
                    connection.send(cmd_args);
    };

    return command(cmd, cmd_name, std::forward<Args>(args)...);
}

template <typename Impl>
template <typename Input>
auto QueuedRedis<Impl>::command(Input first, Input last)
    -> typename std::enable_if<IsIter<Input>::value, QueuedRedis<Impl>&>::type {
    if (first == last) {
        throw Error("command: empty range");
    }

    auto cmd = [](Connection &connection, Input first, Input last) {
                    CmdArgs cmd_args;
                    while (first != last) {
                        cmd_args.append(*first);
                        ++first;
                    }
                    connection.send(cmd_args);
    };

    return command(cmd, first, last);
}

template <typename Impl>
QueuedReplies QueuedRedis<Impl>::exec() {
    try {
        _sanity_check();

        auto replies = _impl.exec(*_connection, _cmd_num);

        _rewrite_replies(replies);

        _reset();

        return QueuedReplies(std::move(replies));
    } catch (const Error &e) {
        _invalidate();
        throw;
    }
}

template <typename Impl>
void QueuedRedis<Impl>::discard() {
    try {
        _sanity_check();

        _impl.discard(*_connection, _cmd_num);

        _reset();
    } catch (const Error &e) {
        _invalidate();
        throw;
    }
}

template <typename Impl>
void QueuedRedis<Impl>::_sanity_check() const {
    if (!_valid) {
        throw Error("Not in valid state");
    }

    if (_connection->broken()) {
        throw Error("Connection is broken");
    }
}

template <typename Impl>
inline void QueuedRedis<Impl>::_reset() {
    _cmd_num = 0;

    _set_cmd_indexes.clear();

    _georadius_cmd_indexes.clear();
}

template <typename Impl>
void QueuedRedis<Impl>::_invalidate() {
    _valid = false;

    _reset();
}

template <typename Impl>
void QueuedRedis<Impl>::_rewrite_replies(std::vector<ReplyUPtr> &replies) const {
    _rewrite_replies(_set_cmd_indexes, reply::rewrite_set_reply, replies);

    _rewrite_replies(_georadius_cmd_indexes, reply::rewrite_georadius_reply, replies);
}

template <typename Impl>
template <typename Func>
void QueuedRedis<Impl>::_rewrite_replies(const std::vector<std::size_t> &indexes,
                                            Func rewriter,
                                            std::vector<ReplyUPtr> &replies) const {
    for (auto idx : indexes) {
        assert(idx < replies.size());

        auto &reply = replies[idx];

        assert(reply);

        rewriter(*reply);
    }
}

inline std::size_t QueuedReplies::size() const {
    return _replies.size();
}

inline redisReply& QueuedReplies::get(std::size_t idx) {
    _index_check(idx);

    auto &reply = _replies[idx];

    assert(reply);

    return *reply;
}

template <typename Result>
inline Result QueuedReplies::get(std::size_t idx) {
    auto &reply = get(idx);

    return reply::parse<Result>(reply);
}

template <typename Output>
inline void QueuedReplies::get(std::size_t idx, Output output) {
    auto &reply = get(idx);

    reply::to_array(reply, output);
}

inline void QueuedReplies::_index_check(std::size_t idx) const {
    if (idx >= size()) {
        throw Error("Out of range");
    }
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_QUEUED_REDIS_HPP
