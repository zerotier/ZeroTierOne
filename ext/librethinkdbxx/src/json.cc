#include "json_p.h"
#include "error.h"
#include "utils.h"

#include "rapidjson-config.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

namespace RethinkDB {

Datum read_datum(const std::string& json) {
    rapidjson::Document document;
    document.Parse(json);
    return read_datum(document);
}

Datum read_datum(const rapidjson::Value &json) {
    switch(json.GetType()) {
    case rapidjson::kNullType: return Nil();
    case rapidjson::kFalseType: return false;
    case rapidjson::kTrueType: return true;
    case rapidjson::kNumberType: return json.GetDouble();
    case rapidjson::kStringType:
        return std::string(json.GetString(), json.GetStringLength());

    case rapidjson::kObjectType: {
        Object result;
        for (rapidjson::Value::ConstMemberIterator it = json.MemberBegin();
             it != json.MemberEnd(); ++it) {
            result.insert(std::make_pair(std::string(it->name.GetString(),
                                         it->name.GetStringLength()),
                                         read_datum(it->value)));
        }

        if (result.count("$reql_type$"))
            return Datum(std::move(result)).from_raw();
        return std::move(result);
    } break;
    case rapidjson::kArrayType: {
        Array result;
        result.reserve(json.Size());
        for (rapidjson::Value::ConstValueIterator it = json.Begin();
             it != json.End(); ++it) {
            result.push_back(read_datum(*it));
        }
        return std::move(result);
    } break;
    default:
        throw Error("invalid rapidjson value");
    }
}

std::string write_datum(const Datum& datum) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    datum.write_json(&writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

}
