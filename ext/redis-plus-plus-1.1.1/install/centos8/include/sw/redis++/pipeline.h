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

#ifndef SEWENEW_REDISPLUSPLUS_PIPELINE_H
#define SEWENEW_REDISPLUSPLUS_PIPELINE_H

#include <cassert>
#include <vector>
#include "connection.h"

namespace sw {

namespace redis {

class PipelineImpl {
public:
    template <typename Cmd, typename ...Args>
    void command(Connection &connection, Cmd cmd, Args &&...args) {
        assert(!connection.broken());

        cmd(connection, std::forward<Args>(args)...);
    }

    std::vector<ReplyUPtr> exec(Connection &connection, std::size_t cmd_num);

    void discard(Connection &connection, std::size_t /*cmd_num*/) {
        // Reconnect to Redis to discard all commands.
        connection.reconnect();
    }
};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_PIPELINE_H
