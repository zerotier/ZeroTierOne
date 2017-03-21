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

namespace ZeroTier {

static const nlohmann::json _EMPTY_JSON(nlohmann::json::object());

bool JSONDB::writeRaw(const std::string &n,const std::string &obj)
{
	if (!_isValidObjectName(n))
		return false;

	const std::string path(_genPath(n,true));
	if (!path.length())
		return false;

	const std::string buf(obj);
	if (!OSUtils::writeFile(path.c_str(),buf))
		return false;

	return true;
}

bool JSONDB::put(const std::string &n,const nlohmann::json &obj)
{
	if (!_isValidObjectName(n))
		return false;

	const std::string path(_genPath(n,true));
	if (!path.length())
		return false;

	const std::string buf(OSUtils::jsonDump(obj));
	if (!OSUtils::writeFile(path.c_str(),buf))
		return false;

	_E &e = _db[n];
	e.obj = obj;

	return true;
}

const nlohmann::json &JSONDB::get(const std::string &n)
{
	if (!_isValidObjectName(n))
		return _EMPTY_JSON;

	std::map<std::string,_E>::iterator e(_db.find(n));
	if (e != _db.end())
		return e->second.obj;

	const std::string path(_genPath(n,false));
	if (!path.length())
		return _EMPTY_JSON;
	std::string buf;
	if (!OSUtils::readFile(path.c_str(),buf))
		return _EMPTY_JSON;

	_E &e2 = _db[n];
	try {
		e2.obj = OSUtils::jsonParse(buf);
	} catch ( ... ) {
		e2.obj = _EMPTY_JSON;
		buf = "{}";
	}

	return e2.obj;
}

void JSONDB::erase(const std::string &n)
{
	if (!_isValidObjectName(n))
		return;

	std::string path(_genPath(n,true));
	if (!path.length())
		return;

	OSUtils::rm(path.c_str());
	_db.erase(n);
}

void JSONDB::_reload(const std::string &p,const std::string &b)
{
	std::vector<std::string> dl(OSUtils::listDirectory(p.c_str()));
	for(std::vector<std::string>::const_iterator di(dl.begin());di!=dl.end();++di) {
		if ((di->length() > 5)&&(di->substr(di->length() - 5) == ".json")) {
			this->get(b + di->substr(0,di->length() - 5));
		} else {
			this->_reload((p + ZT_PATH_SEPARATOR + *di),(b + *di + ZT_PATH_SEPARATOR));
		}
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

	std::string p(_basePath);
	if (create) OSUtils::mkdir(p.c_str());
	for(unsigned long i=0,j=(unsigned long)(pt.size()-1);i<j;++i) {
		p.push_back(ZT_PATH_SEPARATOR);
		p.append(pt[i]);
		if (create) OSUtils::mkdir(p.c_str());
	}

	p.push_back(ZT_PATH_SEPARATOR);
	p.append(pt[pt.size()-1]);
	p.append(".json");

	return p;
}

} // namespace ZeroTier
