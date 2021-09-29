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

#include "command.h"
#include <cassert>

namespace sw {

namespace redis {

namespace cmd {

// KEY commands.

void restore(Connection &connection,
                const StringView &key,
                const StringView &val,
                long long ttl,
                bool replace) {
    CmdArgs args;
    args << "RESTORE" << key << ttl << val;

    if (replace) {
        args << "REPLACE";
    }

    connection.send(args);
}

// STRING commands.

void bitop(Connection &connection,
            BitOp op,
            const StringView &destination,
            const StringView &key) {
    CmdArgs args;

    detail::set_bitop(args, op);

    args << destination << key;

    connection.send(args);
}

void set(Connection &connection,
            const StringView &key,
            const StringView &val,
            long long ttl,
            UpdateType type) {
    CmdArgs args;
    args << "SET" << key << val;

    if (ttl > 0) {
        args << "PX" << ttl;
    }

    detail::set_update_type(args, type);

    connection.send(args);
}

// LIST commands.

void linsert(Connection &connection,
                const StringView &key,
                InsertPosition position,
                const StringView &pivot,
                const StringView &val) {
    std::string pos;
    switch (position) {
    case InsertPosition::BEFORE:
        pos = "BEFORE";
        break;

    case InsertPosition::AFTER:
        pos = "AFTER";
        break;

    default:
        assert(false);
    }

    connection.send("LINSERT %b %s %b %b",
                    key.data(), key.size(),
                    pos.c_str(),
                    pivot.data(), pivot.size(),
                    val.data(), val.size());
}

// GEO commands.

void geodist(Connection &connection,
                const StringView &key,
                const StringView &member1,
                const StringView &member2,
                GeoUnit unit) {
    CmdArgs args;
    args << "GEODIST" << key << member1 << member2;

    detail::set_geo_unit(args, unit);

    connection.send(args);
}

void georadius_store(Connection &connection,
                        const StringView &key,
                        const std::pair<double, double> &loc,
                        double radius,
                        GeoUnit unit,
                        const StringView &destination,
                        bool store_dist,
                        long long count) {
    CmdArgs args;
    args << "GEORADIUS" << key << loc.first << loc.second;

    detail::set_georadius_store_parameters(args,
                                            radius,
                                            unit,
                                            destination,
                                            store_dist,
                                            count);

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
                bool with_hash) {
    CmdArgs args;
    args << "GEORADIUS" << key << loc.first << loc.second;

    detail::set_georadius_parameters(args,
                                        radius,
                                        unit,
                                        count,
                                        asc,
                                        with_coord,
                                        with_dist,
                                        with_hash);

    connection.send(args);
}

void georadiusbymember(Connection &connection,
                        const StringView &key,
                        const StringView &member,
                        double radius,
                        GeoUnit unit,
                        long long count,
                        bool asc,
                        bool with_coord,
                        bool with_dist,
                        bool with_hash) {
    CmdArgs args;
    args << "GEORADIUSBYMEMBER" << key << member;

    detail::set_georadius_parameters(args,
                                        radius,
                                        unit,
                                        count,
                                        asc,
                                        with_coord,
                                        with_dist,
                                        with_hash);

    connection.send(args);
}

void georadiusbymember_store(Connection &connection,
                                const StringView &key,
                                const StringView &member,
                                double radius,
                                GeoUnit unit,
                                const StringView &destination,
                                bool store_dist,
                                long long count) {
    CmdArgs args;
    args << "GEORADIUSBYMEMBER" << key << member;

    detail::set_georadius_store_parameters(args,
                                            radius,
                                            unit,
                                            destination,
                                            store_dist,
                                            count);

    connection.send(args);
}

// Stream commands.

void xtrim(Connection &connection, const StringView &key, long long count, bool approx) {
    CmdArgs args;
    args << "XTRIM" << key << "MAXLEN";

    if (approx) {
        args << "~";
    }

    args << count;

    connection.send(args);
}

namespace detail {

void set_bitop(CmdArgs &args, BitOp op) {
    args << "BITOP";

    switch (op) {
    case BitOp::AND:
        args << "AND";
        break;

    case BitOp::OR:
        args << "OR";
        break;

    case BitOp::XOR:
        args << "XOR";
        break;

    case BitOp::NOT:
        args << "NOT";
        break;

    default:
        throw Error("Unknown bit operations");
    }
}

void set_update_type(CmdArgs &args, UpdateType type) {
    switch (type) {
    case UpdateType::EXIST:
        args << "XX";
        break;

    case UpdateType::NOT_EXIST:
        args << "NX";
        break;

    case UpdateType::ALWAYS:
        // Do nothing.
        break;

    default:
        throw Error("Unknown update type");
    }
}

void set_aggregation_type(CmdArgs &args, Aggregation aggr) {
    args << "AGGREGATE";

    switch (aggr) {
    case Aggregation::SUM:
        args << "SUM";
        break;

    case Aggregation::MIN:
        args << "MIN";
        break;

    case Aggregation::MAX:
        args << "MAX";
        break;

    default:
        throw Error("Unknown aggregation type");
    }
}

void set_geo_unit(CmdArgs &args, GeoUnit unit) {
    switch (unit) {
    case GeoUnit::M:
        args << "m";
        break;

    case GeoUnit::KM:
        args << "km";
        break;

    case GeoUnit::MI:
        args << "mi";
        break;

    case GeoUnit::FT:
        args << "ft";
        break;

    default:
        throw Error("Unknown geo unit type");
        break;
    }
}

void set_georadius_store_parameters(CmdArgs &args,
                                    double radius,
                                    GeoUnit unit,
                                    const StringView &destination,
                                    bool store_dist,
                                    long long count) {
    args << radius;

    detail::set_geo_unit(args, unit);

    args << "COUNT" << count;

    if (store_dist) {
        args << "STOREDIST";
    } else {
        args << "STORE";
    }

    args << destination;
}

void set_georadius_parameters(CmdArgs &args,
                                double radius,
                                GeoUnit unit,
                                long long count,
                                bool asc,
                                bool with_coord,
                                bool with_dist,
                                bool with_hash) {
    args << radius;

    detail::set_geo_unit(args, unit);

    if (with_coord) {
        args << "WITHCOORD";
    }

    if (with_dist) {
        args << "WITHDIST";
    }

    if (with_hash) {
        args << "WITHHASH";
    }

    args << "COUNT" << count;

    if (asc) {
        args << "ASC";
    } else {
        args << "DESC";
    }
}

}

}

}

}
