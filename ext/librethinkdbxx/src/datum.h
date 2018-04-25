#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

#include "protocol_defs.h"
#include "error.h"
#include "types.h"

namespace RethinkDB {

class Cursor;

// The type of data stored in a RethinkDB database.
// The following JSON types are represented in a Datum as
//  * null -> Nil
//  * boolean -> bool
//  * number -> double
//  * unicode strings -> std::string
//  * array -> Array (aka std::vector<Datum>
//  * object -> Object (aka std::map<std::string, Datum>>
// Datums can also contain one of the following extra types
//  * binary strings -> Binary
//  * timestamps -> Time
//  * points. lines and polygons -> not implemented
class Datum {
public:
    Datum() : type(Type::INVALID), value() {}
    Datum(Nil) : type(Type::NIL), value() { }
    Datum(bool boolean_) : type(Type::BOOLEAN), value(boolean_) { }
    Datum(double number_) : type(Type::NUMBER), value(number_) { }
    Datum(const std::string& string_) : type(Type::STRING), value(string_) { }
    Datum(std::string&& string_) : type(Type::STRING), value(std::move(string_)) { }
    Datum(const Array& array_) : type(Type::ARRAY), value(array_) { }
    Datum(Array&& array_) : type(Type::ARRAY), value(std::move(array_)) { }
    Datum(const Binary& binary) : type(Type::BINARY), value(binary) { }
    Datum(Binary&& binary) : type(Type::BINARY), value(std::move(binary)) { }
    Datum(const Time time) : type(Type::TIME), value(time) { }
    Datum(const Object& object_) : type(Type::OBJECT), value(object_) { }
    Datum(Object&& object_) : type(Type::OBJECT), value(std::move(object_)) { }
    Datum(const Datum& other) : type(other.type), value(other.type, other.value) { }
    Datum(Datum&& other) : type(other.type), value(other.type, std::move(other.value)) { }

    Datum& operator=(const Datum& other) {
        value.destroy(type);
        type = other.type;
        value.set(type, other.value);
        return *this;
    }

    Datum& operator=(Datum&& other) {
        value.destroy(type);
        type = other.type;
        value.set(type, std::move(other.value));
        return *this;
    }

    Datum(unsigned short number_) : Datum(static_cast<double>(number_)) { }
    Datum(signed short number_) : Datum(static_cast<double>(number_)) { }
    Datum(unsigned int number_) : Datum(static_cast<double>(number_)) { }
    Datum(signed int number_) : Datum(static_cast<double>(number_)) { }
    Datum(unsigned long number_) : Datum(static_cast<double>(number_)) { }
    Datum(signed long number_) : Datum(static_cast<double>(number_)) { }
    Datum(unsigned long long number_) : Datum(static_cast<double>(number_)) { }
    Datum(signed long long number_) : Datum(static_cast<double>(number_)) { }

    Datum(Protocol::Term::TermType type) : Datum(static_cast<double>(type)) { }
    Datum(const char* string) : Datum(static_cast<std::string>(string)) { }

    // Cursors are implicitly converted into datums
    Datum(Cursor&&);
    Datum(const Cursor&);

    template <class T>
    Datum(const std::map<std::string, T>& map) : type(Type::OBJECT), value(Object()) {
        for (const auto& it : map) {
            value.object.emplace(it.left, Datum(it.right));
        }
    }

    template <class T>
    Datum(std::map<std::string, T>&& map) : type(Type::OBJECT), value(Object()) {
        for (auto& it : map) {
            value.object.emplace(it.first, Datum(std::move(it.second)));
        }
    }

    template <class T>
    Datum(const std::vector<T>& vec) : type(Type::ARRAY), value(Array()) {
        for (const auto& it : vec) {
            value.array.emplace_back(it);
        }
    }

    template <class T>
    Datum(std::vector<T>&& vec) : type(Type::ARRAY), value(Array()) {
        for (auto& it : vec) {
            value.array.emplace_back(std::move(it));
        }
    }

    ~Datum() {
        value.destroy(type);
    }

    // Apply a visitor
    template <class R, class F, class ...A>
    R apply(F f, A&& ...args) const & {
        switch (type) {
        case Type::NIL: return f(Nil(), std::forward<A>(args)...); break;
        case Type::BOOLEAN: return f(value.boolean, std::forward<A>(args)...); break;
        case Type::NUMBER: return f(value.number, std::forward<A>(args)...); break;
        case Type::STRING: return f(value.string, std::forward<A>(args)...); break;
        case Type::OBJECT: return f(value.object, std::forward<A>(args)...); break;
        case Type::ARRAY: return f(value.array, std::forward<A>(args)...); break;
        case Type::BINARY: return f(value.binary, std::forward<A>(args)...); break;
        case Type::TIME: return f(value.time, std::forward<A>(args)...); break;
        default:
            throw Error("internal error: no such datum type %d", static_cast<int>(type));
        }
    }

    template <class R, class F, class ...A>
    R apply(F f, A&& ...args) && {
        switch (type) {
        case Type::NIL: return f(Nil(), std::forward<A>(args)...); break;
        case Type::BOOLEAN: return f(std::move(value.boolean), std::forward<A>(args)...); break;
        case Type::NUMBER: return f(std::move(value.number), std::forward<A>(args)...); break;
        case Type::STRING: return f(std::move(value.string), std::forward<A>(args)...); break;
        case Type::OBJECT: return f(std::move(value.object), std::forward<A>(args)...); break;
        case Type::ARRAY: return f(std::move(value.array), std::forward<A>(args)...); break;
        case Type::BINARY: return f(std::move(value.binary), std::forward<A>(args)...); break;
        case Type::TIME: return f(std::move(value.time), std::forward<A>(args)...); break;
        default:
            throw Error("internal error: no such datum type %d", static_cast<int>(type));
        }
    }

    bool is_nil() const;
    bool is_boolean() const;
    bool is_number() const;
    bool is_string() const;
    bool is_object() const;
    bool is_array() const;
    bool is_binary() const;
    bool is_time() const;

    // get_* returns nullptr if the datum has a different type

    bool* get_boolean();
    const bool* get_boolean() const;
    double* get_number();
    const double* get_number() const;
    std::string* get_string();
    const std::string* get_string() const;
    Object* get_object();
    const Object* get_object() const;
    Datum* get_field(std::string);
    const Datum* get_field(std::string) const;
    Array* get_array();
    const Array* get_array() const;
    Datum* get_nth(size_t);
    const Datum* get_nth(size_t) const;
    Binary* get_binary();
    const Binary* get_binary() const;
    Time* get_time();
    const Time* get_time() const;

    // extract_* throws an exception if the types don't match

    bool& extract_boolean();
    double& extract_number();
    std::string& extract_string();
    Object& extract_object();
    Datum& extract_field(std::string);
    Array& extract_array();
    Datum& extract_nth(size_t);
    Binary& extract_binary();
    Time& extract_time();

    // negative, zero or positive if this datum is smaller, identical or larger than the other one, respectively
    // This is meant to match the results of RethinkDB's comparison operators
    int compare(const Datum&) const;

    // Deep equality
    bool operator== (const Datum&) const;

    // Recusively replace non-JSON types into objects that represent them
    Datum to_raw() const;

    // Recursively replace objects with a $reql_type$ field into the datum they represent
    Datum from_raw() const;

    template <class json_writer_t> void write_json(json_writer_t *writer) const;

    std::string as_json() const;
    static Datum from_json(const std::string&);

    bool is_valid() const { return type != Type::INVALID; }

private:
    enum class Type {
        INVALID,    // default constructed
        ARRAY, BOOLEAN, NIL, NUMBER, OBJECT, BINARY, STRING, TIME
        // POINT, LINE, POLYGON
    };
    Type type;

    union datum_value {
        bool boolean;
        double number;
        std::string string;
        Object object;
        Array array;
        Binary binary;
        Time time;

        datum_value() { }
        datum_value(bool boolean_) : boolean(boolean_) { }
        datum_value(double number_) : number(number_) { }
        datum_value(const std::string& string_) : string(string_) { }
        datum_value(std::string&& string_) : string(std::move(string_)) { }
        datum_value(const Object& object_) : object(object_) { }
        datum_value(Object&& object_) : object(std::move(object_)) { }
        datum_value(const Array& array_) : array(array_) { }
        datum_value(Array&& array_) : array(std::move(array_)) { }
        datum_value(const Binary& binary_) : binary(binary_) { }
        datum_value(Binary&& binary_) : binary(std::move(binary_)) { }
        datum_value(Time time) : time(std::move(time)) { }

        datum_value(Type type, const datum_value& other){
            set(type, other);
        }

        datum_value(Type type, datum_value&& other){
            set(type, std::move(other));
        }

        void set(Type type, datum_value&& other) {
            switch(type){
            case Type::NIL: case Type::INVALID: break;
            case Type::BOOLEAN: new (this) bool(other.boolean); break;
            case Type::NUMBER: new (this) double(other.number); break;
            case Type::STRING: new (this) std::string(std::move(other.string)); break;
            case Type::OBJECT: new (this) Object(std::move(other.object)); break;
            case Type::ARRAY: new (this) Array(std::move(other.array)); break;
            case Type::BINARY: new (this) Binary(std::move(other.binary)); break;
            case Type::TIME: new (this) Time(std::move(other.time)); break;
            }
        }

        void set(Type type, const datum_value& other) {
            switch(type){
            case Type::NIL: case Type::INVALID: break;
            case Type::BOOLEAN: new (this) bool(other.boolean); break;
            case Type::NUMBER: new (this) double(other.number); break;
            case Type::STRING: new (this) std::string(other.string); break;
            case Type::OBJECT: new (this) Object(other.object); break;
            case Type::ARRAY: new (this) Array(other.array); break;
            case Type::BINARY: new (this) Binary(other.binary); break;
            case Type::TIME: new (this) Time(other.time); break;
            }
        }

        void destroy(Type type) {
            switch(type){
            case Type::INVALID: break;
            case Type::NIL: break;
            case Type::BOOLEAN: break;
            case Type::NUMBER: break;
            case Type::STRING: { typedef std::string str; string.~str(); } break;
            case Type::OBJECT: object.~Object(); break;
            case Type::ARRAY: array.~Array(); break;
            case Type::BINARY: binary.~Binary(); break;
            case Type::TIME: time.~Time(); break;
            }
        }

        ~datum_value() { }
    };

    datum_value value;
};

}
