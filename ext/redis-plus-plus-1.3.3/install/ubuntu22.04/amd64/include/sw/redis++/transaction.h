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

#ifndef SEWENEW_REDISPLUSPLUS_TRANSACTION_H
#define SEWENEW_REDISPLUSPLUS_TRANSACTION_H

#include <cassert>
#include <vector>
#include "connection.h"
#include "errors.h"

namespace sw {

namespace redis {

class TransactionImpl {
public:
    explicit TransactionImpl(bool piped) : _piped(piped) {}

    template <typename Cmd, typename ...Args>
    void command(Connection &connection, Cmd cmd, Args &&...args);

    std::vector<ReplyUPtr> exec(Connection &connection, std::size_t cmd_num);

    void discard(Connection &connection, std::size_t cmd_num);

private:
    void _open_transaction(Connection &connection);

    void _close_transaction();

    void _get_queued_reply(Connection &connection);

    void _get_queued_replies(Connection &connection, std::size_t cmd_num);

    std::vector<ReplyUPtr> _exec(Connection &connection);

    void _discard(Connection &connection);

    bool _in_transaction = false;

    bool _piped;
};

template <typename Cmd, typename ...Args>
void TransactionImpl::command(Connection &connection, Cmd cmd, Args &&...args) {
    assert(!connection.broken());

    if (!_in_transaction) {
        _open_transaction(connection);
    }

    cmd(connection, std::forward<Args>(args)...);

    if (!_piped) {
        _get_queued_reply(connection);
    }
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TRANSACTION_H
