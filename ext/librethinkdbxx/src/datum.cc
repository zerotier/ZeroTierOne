#include <float.h>
#include <cmath>

#include "datum.h"
#include "json_p.h"
#include "utils.h"
#include "cursor.h"

#include "rapidjson-config.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace RethinkDB {

using TT = Protocol::Term::TermType;

bool Datum::is_nil() const {
    return type == Type::NIL;
}

bool Datum::is_boolean() const {
    return type == Type::BOOLEAN;
}

bool Datum::is_number() const {
    return type == Type::NUMBER;
}

bool Datum::is_string() const {
    return type == Type::STRING;
}

bool Datum::is_object() const {
    return type == Type::OBJECT;
}

bool Datum::is_array() const {
    return type == Type::ARRAY;
}

bool Datum::is_binary() const {
    return type == Type::BINARY;
}

bool Datum::is_time() const {
    return type == Type::TIME;
}

bool* Datum::get_boolean() {
    if (type == Type::BOOLEAN) {
        return &value.boolean;
    } else {
        return NULL;
    }
}

const bool* Datum::get_boolean() const {
    if (type == Type::BOOLEAN) {
        return &value.boolean;
    } else {
        return NULL;
    }
}

double* Datum::get_number() {
    if (type == Type::NUMBER) {
        return &value.number;
    } else {
        return NULL;
    }
}

const double* Datum::get_number() const {
    if (type == Type::NUMBER) {
        return &value.number;
    } else {
        return NULL;
    }
}

std::string* Datum::get_string() {
    if (type == Type::STRING) {
        return &value.string;
    } else {
        return NULL;
    }
}

const std::string* Datum::get_string() const {
    if (type == Type::STRING) {
        return &value.string;
    } else {
        return NULL;
    }
}

Datum* Datum::get_field(std::string key) {
    if (type != Type::OBJECT) {
        return NULL;
    }
    auto it = value.object.find(key);
    if (it == value.object.end()) {
        return NULL;
    }
    return &it->second;
}

const Datum* Datum::get_field(std::string key) const {
    if (type != Type::OBJECT) {
        return NULL;
    }
    auto it = value.object.find(key);
    if (it == value.object.end()) {
        return NULL;
    }
    return &it->second;
}

Datum* Datum::get_nth(size_t i) {
    if (type != Type::ARRAY) {
        return NULL;
    }
    if (i >= value.array.size()) {
        return NULL;
    }
    return &value.array[i];
}

const Datum* Datum::get_nth(size_t i) const {
    if (type != Type::ARRAY) {
        return NULL;
    }
    if (i >= value.array.size()) {
        return NULL;
    }
    return &value.array[i];
}

Object* Datum::get_object() {
    if (type == Type::OBJECT) {
        return &value.object;
    } else {
        return NULL;
    }
}

const Object* Datum::get_object() const {
    if (type == Type::OBJECT) {
        return &value.object;
    } else {
        return NULL;
    }
}

Array* Datum::get_array() {
    if (type == Type::ARRAY) {
        return &value.array;
    } else {
        return NULL;
    }
}

const Array* Datum::get_array() const {
    if (type == Type::ARRAY) {
        return &value.array;
    } else {
        return NULL;
    }
}

Binary* Datum::get_binary() {
    if (type == Type::BINARY) {
        return &value.binary;
    } else {
        return NULL;
    }
}

const Binary* Datum::get_binary() const {
    if (type == Type::BINARY) {
        return &value.binary;
    } else {
        return NULL;
    }
}

Time* Datum::get_time() {
    if (type == Type::TIME) {
        return &value.time;
    } else {
        return NULL;
    }
}

const Time* Datum::get_time() const {
    if (type == Type::TIME) {
        return &value.time;
    } else {
        return NULL;
    }
}

bool& Datum::extract_boolean() {
    if (type != Type::BOOLEAN) {
        throw Error("extract_bool: Not a boolean");
    }
    return value.boolean;
}

double& Datum::extract_number() {
    if (type != Type::NUMBER) {
        throw Error("extract_number: Not a number: %s", write_datum(*this).c_str());
    }
    return value.number;
}

std::string& Datum::extract_string() {
    if (type != Type::STRING) {
        throw Error("extract_string: Not a string");
    }
    return value.string;
}

Object& Datum::extract_object() {
    if (type != Type::OBJECT) {
        throw Error("extract_object: Not an object");
    }
    return value.object;
}

Datum& Datum::extract_field(std::string key) {
    if (type != Type::OBJECT) {
        throw Error("extract_field: Not an object");
    }
    auto it = value.object.find(key);
    if (it == value.object.end()) {
        throw Error("extract_field: No such key in object");
    }
    return it->second;
}

Datum& Datum::extract_nth(size_t i) {
    if (type != Type::ARRAY) {
        throw Error("extract_nth: Not an array");
    }
    if (i >= value.array.size()) {
        throw Error("extract_nth: index too large");
    }
    return value.array[i];
}

Array& Datum::extract_array() {
    if (type != Type::ARRAY) {
        throw Error("get_array: Not an array");
    }
    return value.array;
}

Binary& Datum::extract_binary() {
    if (type != Type::BINARY) {
        throw Error("get_binary: Not a binary");
    }
    return value.binary;
}

Time& Datum::extract_time() {
    if (type != Type::TIME) {
        throw Error("get_time: Not a time");
    }
    return value.time;
}

int Datum::compare(const Datum& other) const {
#define COMPARE(a, b) do {          \
    if (a < b) { return -1; }       \
    if (a > b) { return 1; } } while(0)
#define COMPARE_OTHER(x) COMPARE(x, other.x)

    COMPARE_OTHER(type);
    int c;
    switch (type) {
    case Type::NIL: case Type::INVALID: break;
    case Type::BOOLEAN: COMPARE_OTHER(value.boolean); break;
    case Type::NUMBER: COMPARE_OTHER(value.number); break;
    case Type::STRING:
        c = value.string.compare(other.value.string);
        COMPARE(c, 0);
        break;
    case Type::BINARY:
        c = value.binary.data.compare(other.value.binary.data);
        COMPARE(c, 0);
        break;
    case Type::TIME:
        COMPARE(value.time.epoch_time, other.value.time.epoch_time);
        COMPARE(value.time.utc_offset, other.value.time.utc_offset);
        break;
    case Type::ARRAY:
        COMPARE_OTHER(value.array.size());
        for (size_t i = 0; i < value.array.size(); i++) {
            c = value.array[i].compare(other.value.array[i]);
            COMPARE(c, 0);
        }
        break;
    case Type::OBJECT:
        COMPARE_OTHER(value.object.size());
        for (Object::const_iterator l = value.object.begin(),
                 r = other.value.object.begin();
             l != value.object.end();
             ++l, ++r) {
            COMPARE(l->first, r->first);
            c = l->second.compare(r->second);
            COMPARE(c, 0);
        }
        break;
    default:
        throw Error("cannot compare invalid datum");
    }
    return 0;
#undef COMPARE_OTHER
#undef COMPARE
}

bool Datum::operator== (const Datum& other) const {
    return compare(other) == 0;
}

Datum Datum::from_raw() const {
    do {
        const Datum* type_field = get_field("$reql_type$");
        if (!type_field) break;
        const std::string* type = type_field->get_string();
        if (!type) break;;
        if (!strcmp(type->c_str(), "BINARY")) {
            const Datum* data_field = get_field("data");
            if (!data_field) break;
            const std::string* encoded_data = data_field->get_string();
            if (!encoded_data) break;
            Binary binary("");
            if (base64_decode(*encoded_data, binary.data)) {
                return binary;
            }
        } else if (!strcmp(type->c_str(), "TIME")) {
            const Datum* epoch_field = get_field("epoch_time");
            if (!epoch_field) break;
            const Datum* tz_field = get_field("timezone");
            if (!tz_field) break;
            const double* epoch_time = epoch_field->get_number();
            if (!epoch_time) break;
            const std::string* tz  = tz_field->get_string();
            if (!tz) break;
            double offset;
            if (!Time::parse_utc_offset(*tz, &offset)) break;
            return Time(*epoch_time, offset);
        }
    } while (0);
    return *this;
}

Datum Datum::to_raw() const {
    if (type == Type::BINARY) {
        return Object{
            {"$reql_type$", "BINARY"},
            {"data", base64_encode(value.binary.data)}};
    } else if (type == Type::TIME) {
        return Object{
            {"$reql_type$", "TIME"},
            {"epoch_time", value.time.epoch_time},
            {"timezone", Time::utc_offset_string(value.time.utc_offset)}};
    }
    return *this;
}

Datum::Datum(Cursor&& cursor) : Datum(cursor.to_datum()) { }
Datum::Datum(const Cursor& cursor) : Datum(cursor.to_datum()) { }

static const double max_dbl_int = 0x1LL << DBL_MANT_DIG;
static const double min_dbl_int = max_dbl_int * -1;
bool number_as_integer(double d, int64_t *i_out) {
    static_assert(DBL_MANT_DIG == 53, "Doubles are wrong size.");

    if (min_dbl_int <= d && d <= max_dbl_int) {
        int64_t i = d;
        if (static_cast<double>(i) == d) {
            *i_out = i;
            return true;
        }
    }
    return false;
}

template void Datum::write_json(
    rapidjson::Writer<rapidjson::StringBuffer> *writer) const;
template void Datum::write_json(
    rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer) const;

template <class json_writer_t>
void Datum::write_json(json_writer_t *writer) const {
    switch (type) {
    case Type::NIL: writer->Null(); break;
    case Type::BOOLEAN: writer->Bool(value.boolean); break;
    case Type::NUMBER: {
        const double d = value.number;
        // Always print -0.0 as a double since integers cannot represent -0.
        // Otherwise check if the number is an integer and print it as such.
        int64_t i;
        if (!(d == 0.0 && std::signbit(d)) && number_as_integer(d, &i)) {
            writer->Int64(i);
        } else {
            writer->Double(d);
        }
    } break;
    case Type::STRING: writer->String(value.string.data(), value.string.size()); break;
    case Type::ARRAY: {
        writer->StartArray();
        for (auto it : value.array) {
            it.write_json(writer);
        }
        writer->EndArray();
    } break;
    case Type::OBJECT: {
        writer->StartObject();
        for (auto it : value.object) {
            writer->Key(it.first.data(), it.first.size());
            it.second.write_json(writer);
        }
        writer->EndObject();
    } break;

    case Type::BINARY:
    case Type::TIME:
        to_raw().write_json(writer);
        break;
    default:
        throw Error("cannot write invalid datum");
    }
}

std::string Datum::as_json() const {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    write_json(&writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

Datum Datum::from_json(const std::string& json) {
    return read_datum(json);
}

}   // namespace RethinkDB
