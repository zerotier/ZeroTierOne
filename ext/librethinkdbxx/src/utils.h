#pragma once

#include <cstddef>
#include <string>

namespace RethinkDB {

// The size of the longest UTF-8 encoded unicode codepoint
const size_t max_utf8_encoded_size = 6;

// Decode a base64 string. Returns false on failure.
bool base64_decode(const std::string& in, std::string& out);
std::string base64_encode(const std::string&);

// Encodes a single unicode codepoint into UTF-8. Returns the number of bytes written.
// Does not add a trailing null byte
size_t utf8_encode(unsigned int, char*);

}
