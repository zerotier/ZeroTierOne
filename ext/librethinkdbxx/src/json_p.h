#pragma once

#include "datum.h"

namespace rapidjson {
  class CrtAllocator;
  template<typename> struct UTF8;
  template <typename, typename> class GenericValue;
  template <typename> class MemoryPoolAllocator;
  typedef GenericValue<UTF8<char>, MemoryPoolAllocator<CrtAllocator> > Value;
}

namespace RethinkDB {

Datum read_datum(const std::string&);
Datum read_datum(const rapidjson::Value &json);
std::string write_datum(const Datum&);

}
