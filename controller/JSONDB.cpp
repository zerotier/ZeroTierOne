/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "JSONDB.hpp"

#define ZT_JSONDB_HTTP_TIMEOUT 60000

namespace ZeroTier {

static const nlohmann::json _EMPTY_JSON(nlohmann::json::object());
static const std::map<std::string,std::string> _ZT_JSONDB_GET_HEADERS;

JSONDB::JSONDB(const std::string &basePath) :
	_basePath(basePath),
	_ready(false)
{
	if ((_basePath.length() > 7)&&(_basePath.substr(0,7) == "http://")) {
		// TODO: this doesn't yet support IPv6 since bracketed address notiation isn't supported.
		// Typically it's used with 127.0.0.1 anyway.
		std::string hn = _basePath.substr(7);
		std::size_t hnend = hn.find_first_of('/');
		if (hnend != std::string::npos)
			hn = hn.substr(0,hnend);
		std::size_t hnsep = hn.find_last_of(':');
		if (hnsep != std::string::npos)
			hn[hnsep] = '/';
		_httpAddr.fromString(hn);
		if (hnend != std::string::npos)
			_basePath = _basePath.substr(7 + hnend);
		if (_basePath.length() == 0)
			_basePath = "/";
		if (_basePath[0] != '/')
			_basePath = std::string("/") + _basePath;
	} else {
		OSUtils::mkdir(_basePath.c_str());
		OSUtils::lockDownFile(_basePath.c_str(),true); // networks might contain auth tokens, etc., so restrict directory permissions
	}
	_ready = _reload(_basePath,std::string());
}

bool JSONDB::writeRaw(const std::string &n,const std::string &obj)
{
	if (!_isValidObjectName(n))
		return false;
	if (_httpAddr) {
		std::map<std::string,std::string> headers;
		std::string body;
		std::map<std::string,std::string> reqHeaders;
		char tmp[64];
		Utils::snprintf(tmp,sizeof(tmp),"%lu",(unsigned long)obj.length());
		reqHeaders["Content-Length"] = tmp;
		reqHeaders["Content-Type"] = "application/json";
		const unsigned int sc = Http::PUT(1048576,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),reqHeaders,obj.data(),(unsigned long)obj.length(),headers,body);
		return (sc == 200);
	} else {
		const std::string path(_genPath(n,true));
		if (!path.length())
			return false;
		return OSUtils::writeFile(path.c_str(),obj);
	}
}

bool JSONDB::put(const std::string &n,const nlohmann::json &obj)
{
	const bool r = writeRaw(n,OSUtils::jsonDump(obj));
	_db[n].obj = obj;
	return r;
}

const nlohmann::json &JSONDB::get(const std::string &n)
{
	while (!_ready) {
		Thread::sleep(250);
		_ready = _reload(_basePath,std::string());
	}

	if (!_isValidObjectName(n))
		return _EMPTY_JSON;
	std::map<std::string,_E>::iterator e(_db.find(n));
	if (e != _db.end())
		return e->second.obj;

	std::string buf;
	if (_httpAddr) {
		std::map<std::string,std::string> headers;
		const unsigned int sc = Http::GET(1048576,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),_ZT_JSONDB_GET_HEADERS,headers,buf);
		if (sc != 200)
			return _EMPTY_JSON;
	} else {
		const std::string path(_genPath(n,false));
		if (!path.length())
			return _EMPTY_JSON;
		if (!OSUtils::readFile(path.c_str(),buf))
			return _EMPTY_JSON;
	}

	try {
		_E &e2 = _db[n];
		e2.obj = OSUtils::jsonParse(buf);
		return e2.obj;
	} catch ( ... ) {
		_db.erase(n);
		return _EMPTY_JSON;
	}
}

void JSONDB::erase(const std::string &n)
{
	if (!_isValidObjectName(n))
		return;

	if (_httpAddr) {
		std::string body;
		std::map<std::string,std::string> headers;
		Http::DEL(1048576,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),(_basePath+"/"+n).c_str(),_ZT_JSONDB_GET_HEADERS,headers,body);
	} else {
		std::string path(_genPath(n,true));
		if (!path.length())
			return;
		OSUtils::rm(path.c_str());
	}

	_db.erase(n);
}

bool JSONDB::_reload(const std::string &p,const std::string &b)
{
	if (_httpAddr) {
		std::string body;
		std::map<std::string,std::string> headers;
		const unsigned int sc = Http::GET(2147483647,ZT_JSONDB_HTTP_TIMEOUT,reinterpret_cast<const struct sockaddr *>(&_httpAddr),_basePath.c_str(),_ZT_JSONDB_GET_HEADERS,headers,body);
		if (sc == 200) {
			try {
				nlohmann::json dbImg(OSUtils::jsonParse(body));
				std::string tmp;
				if (dbImg.is_object()) {
					for(nlohmann::json::iterator i(dbImg.begin());i!=dbImg.end();++i) {
						if (i.value().is_object()) {
							tmp = i.key();
							_db[tmp].obj = i.value();
						}
					}
					return true;
				}
			} catch ( ... ) {} // invalid JSON, so maybe incomplete request
		}
		return false;
	} else {
		std::vector<std::string> dl(OSUtils::listDirectory(p.c_str(),true));
		for(std::vector<std::string>::const_iterator di(dl.begin());di!=dl.end();++di) {
			if ((di->length() > 5)&&(di->substr(di->length() - 5) == ".json")) {
				this->get(b + di->substr(0,di->length() - 5));
			} else {
				this->_reload((p + ZT_PATH_SEPARATOR + *di),(b + *di + ZT_PATH_SEPARATOR));
			}
		}
		return true;
	}
}

bool JSONDB::_isValidObjectName(const std::string &n)
{
	if (n.length() == 0)
		return false;
	const char *p = n.c_str();
	char c;
	// For security reasons we should not allow dots, backslashes, or other path characters or potential path characters.
	while ((c = *(p++))) {
		if (!( ((c >= 'a')&&(c <= 'z')) || ((c >= 'A')&&(c <= 'Z')) || ((c >= '0')&&(c <= '9')) || (c == '/') || (c == '_') || (c == '~') || (c == '-') ))
			return false;
	}
	return true;
}

std::string JSONDB::_genPath(const std::string &n,bool create)
{
	std::vector<std::string> pt(OSUtils::split(n.c_str(),"/","",""));
	if (pt.size() == 0)
		return std::string();

	char sep;
	if (_httpAddr) {
		sep = '/';
		create = false;
	} else {
		sep = ZT_PATH_SEPARATOR;
	}

	std::string p(_basePath);
	if (create) OSUtils::mkdir(p.c_str());
	for(unsigned long i=0,j=(unsigned long)(pt.size()-1);i<j;++i) {
		p.push_back(sep);
		p.append(pt[i]);
		if (create) OSUtils::mkdir(p.c_str());
	}

	p.push_back(sep);
	p.append(pt[pt.size()-1]);
	p.append(".json");

	return p;
}

} // namespace ZeroTier
