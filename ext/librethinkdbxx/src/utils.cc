#include "utils.h"
#include "error.h"

namespace RethinkDB {

size_t utf8_encode(unsigned int code, char* buf) {
    if (!(code & ~0x7F)) {
        buf[0] = code;
        return 1;
    } else if (!(code & ~0x7FF)) {
        buf[0] = 0xC0 | (code >> 6);
        buf[1] = 0x80 | (code & 0x3F);
        return 2;
    } else if (!(code & ~0xFFFF)) {
        buf[0] = 0xE0 | (code >> 12);
        buf[1] = 0x80 | ((code >> 6) & 0x3F);
        buf[2] = 0x80 | (code & 0x3F);
        return 3;
    } else if (!(code & ~0x1FFFFF)) {
        buf[0] = 0xF0 | (code >> 18);
        buf[1] = 0x80 | ((code >> 12) & 0x3F);
        buf[2] = 0x80 | ((code >> 6) & 0x3F);
        buf[3] = 0x80 | (code & 0x3F);
        return 4;
    } else if (!(code & ~0x3FFFFFF)) {
        buf[0] = 0xF8 | (code >> 24);
        buf[1] = 0x80 | ((code >> 18) & 0x3F);
        buf[2] = 0x80 | ((code >> 12) & 0x3F);
        buf[3] = 0x80 | ((code >> 6) & 0x3F);
        buf[4] = 0x80 | (code & 0x3F);
        return 5;
    } else if (!(code & ~0x7FFFFFFF)) {
        buf[0] = 0xFC | (code >> 30);
        buf[1] = 0x80 | ((code >> 24) & 0x3F);
        buf[2] = 0x80 | ((code >> 18) & 0x3F);
        buf[3] = 0x80 | ((code >> 12) & 0x3F);
        buf[4] = 0x80 | ((code >> 6) & 0x3F);
        buf[5] = 0x80 | (code & 0x3F);
        return 6;
    } else {
        throw Error("Invalid unicode codepoint %ud", code);
    }
}

bool base64_decode(char c, int* out) {
    if (c >= 'A' && c <= 'Z') {
        *out = c - 'A';
    } else if (c >= 'a' && c <= 'z') {
        *out = c - ('a' - 26);
    } else if (c >= '0' && c <= '9') {
        *out = c - ('0' - 52);
    } else if (c == '+') {
        *out = 62;
    } else if (c == '/') {
        *out = 63;
    } else {
        return false;
    }
    return true;
}

bool base64_decode(const std::string& in, std::string& out) {
    out.clear();
    out.reserve(in.size() * 3 / 4);
    auto read = in.begin();
    while (true) {
        int c[4];
        int end = 4;
        for (int i = 0; i < 4; i++) {
            while (true) {
                if (read == in.end()) {
                    c[i] = 0;
                    end = i;
                    i = 3;
                    break;
                } else if (base64_decode(*read, &c[i])) {
                    ++read;
                    break;
                } else {
                    ++read;
                }
            }
        }
        if (end == 1) return false;
        int val = c[0] << 18 | c[1] << 12 | c[2] << 6 | c[3];
        if (end > 1) out.append(1, val >> 16);
        if (end > 2) out.append(1, val >> 8 & 0xFF);
        if (end > 3) out.append(1, val & 0xFF);
        if (end != 4) break;
    }
    return true;
}

char base64_encode(unsigned int c) {
    if (c < 26) {
        return 'A' + c;
    } else if (c < 52) {
        return 'a' + c - 26;
    } else if (c < 62) {
        return '0' + c - 52;
    } else if (c == 62) {
        return '+';
    } else if (c == 63) {
        return '/';
    } else {
        throw Error("unreachable: base64 encoding %d", c);
    }
}

void base64_encode(unsigned int* c, int n, std::string& out) {
    if (n == 0) {
        return;
    }
    out.append(1, base64_encode(c[0] >> 2));
    out.append(1, base64_encode((c[0] & 0x3) << 4 | c[1] >> 4));
    if (n == 1) {
        out.append("==");
        return;
    }
    out.append(1, base64_encode((c[1] & 0xF) << 2 | c[2] >> 6));
    if (n == 2) {
        out.append("=");
        return;
    }
    out.append(1, base64_encode(c[2] & 0x3F));
}

std::string base64_encode(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 4 / 3 + in.size() / 48 + 3);
    auto read = in.begin();
    while (true) {
        for (int group = 0; group < 16; ++group) {
            unsigned int c[3];
            int i = 0;
            for (; i < 3; ++i) {
                if (read == in.end()) {
                    c[i] = 0;
                    break;
                } else {
                    c[i] = static_cast<unsigned char>(*read++);
                }
            }
            base64_encode(c, i, out);
            if (i != 3) {
                return out;
            }
        }
        out.append("\n");
    }
}

}
