#pragma once

#include <vector>
#include <map>
#include <ctime>
#include <string>

namespace RethinkDB {

class Datum;

// Represents a null datum
struct Nil { };

using Array = std::vector<Datum>;
using Object = std::map<std::string, Datum>;

// Represents a string of bytes. Plain std::strings are passed on to the server as utf-8 strings
struct Binary {
    bool operator== (const Binary& other) const {
        return data == other.data;
    }

    Binary(const std::string& data_) : data(data_) { }
    Binary(std::string&& data_) : data(std::move(data_)) { }
    std::string data;
};

// Represents a point in time as
//  * A floating amount of seconds since the UNIX epoch
//  * And a timezone offset represented as seconds relative to UTC
struct Time {
    Time(double epoch_time_, double utc_offset_ = 0) :
        epoch_time(epoch_time_), utc_offset(utc_offset_) { }

    static Time now() {
        return Time(time(NULL));
    }

    static bool parse_utc_offset(const std::string&, double*);
    static double parse_utc_offset(const std::string&);
    static std::string utc_offset_string(double);

    double epoch_time;
    double utc_offset;
};

// Not implemented
class Point;
class Line;
class Polygon;

}
