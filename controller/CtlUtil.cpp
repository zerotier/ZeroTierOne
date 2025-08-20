#include "CtlUtil.hpp"

#ifdef ZT_CONTROLLER_USE_LIBPQ

#include <iomanip>
#include <sstream>

namespace ZeroTier {

const char* _timestr()
{
	time_t t = time(0);
	char* ts = ctime(&t);
	char* p = ts;
	if (! p)
		return "";
	while (*p) {
		if (*p == '\n') {
			*p = (char)0;
			break;
		}
		++p;
	}
	return ts;
}

std::vector<std::string> split(std::string str, char delim)
{
	std::istringstream iss(str);
	std::vector<std::string> tokens;
	std::string item;
	while (std::getline(iss, item, delim)) {
		tokens.push_back(item);
	}
	return tokens;
}

std::string url_encode(const std::string& value)
{
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;

	for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		std::string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << std::uppercase;
		escaped << '%' << std::setw(2) << int((unsigned char)c);
		escaped << std::nouppercase;
	}

	return escaped.str();
}

}	// namespace ZeroTier

#endif