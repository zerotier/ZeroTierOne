#include <cstdlib>

#include "types.h"
#include "error.h"

namespace RethinkDB {

bool Time::parse_utc_offset(const std::string& string, double* offset) {
    const char *s = string.c_str();
    double sign = 1;
    switch (s[0]) {
    case '-':
        sign = -1;
    case '+':
        ++s;
        break;
    case 0:
        return false;
    }
    for (int i = 0; i < 5; ++i) {
        if (s[i] == 0) return false;
        if (i == 2) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    if (s[2] != ':') return false;
    *offset = sign * ((s[0] - '0') * 36000 + (s[1] - '0') * 3600 + (s[3] - '0') * 600 + (s[4] - '0') * 60);
    return true;
}

double Time::parse_utc_offset(const std::string& string) {
    double out;
    if (!parse_utc_offset(string, &out)) {
        throw Error("invalid utc offset `%s'", string.c_str());
    }
    return out;
}

std::string Time::utc_offset_string(double offset) {
    char buf[8];
    int hour = offset / 3600;
    int minutes = std::abs(static_cast<int>(offset / 60)) % 60;
    int n = snprintf(buf, 7, "%+03d:%02d", hour, minutes);
    buf[n] = 0;
    return std::string(buf);
}

}
