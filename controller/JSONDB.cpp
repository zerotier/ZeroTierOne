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
	e.lastModifiedOnDisk = OSUtils::getLastModified(path.c_str());
	e.lastCheck = OSUtils::now();

	return true;
}

const nlohmann::json &JSONDB::get(const std::string &n,unsigned long maxSinceCheck)
{
	if (!_isValidObjectName(n))
		return _EMPTY_JSON;

	const uint64_t now = OSUtils::now();
	std::string buf;
	std::map<std::string,_E>::iterator e(_db.find(n));

	if (e != _db.end()) {
		if ((now - e->second.lastCheck) <= (uint64_t)maxSinceCheck)
			return e->second.obj;

		const std::string path(_genPath(n,false));
		if (!path.length()) // sanity check
			return _EMPTY_JSON;

		// We are somewhat tolerant to momentary disk failures here. This may
		// occur over e.g. EC2's elastic filesystem (NFS).
		const uint64_t lm = OSUtils::getLastModified(path.c_str());
		if (e->second.lastModifiedOnDisk != lm) {
			if (OSUtils::readFile(path.c_str(),buf)) {
				try {
					e->second.obj = OSUtils::jsonParse(buf);
					e->second.lastModifiedOnDisk = lm; // don't update these if there is a parse error -- try again and again ASAP
					e->second.lastCheck = now;
				} catch ( ... ) {} // parse errors result in "holding pattern" behavior
			}
		}

		return e->second.obj;
	} else {
		const std::string path(_genPath(n,false));
		if (!path.length())
			return _EMPTY_JSON;

		if (!OSUtils::readFile(path.c_str(),buf))
			return _EMPTY_JSON;

		const uint64_t lm = OSUtils::getLastModified(path.c_str());
		_E &e2 = _db[n];
		try {
			e2.obj = OSUtils::jsonParse(buf);
		} catch ( ... ) {
			e2.obj = _EMPTY_JSON;
			buf = "{}";
		}
		e2.lastModifiedOnDisk = lm;
		e2.lastCheck = now;

		return e2.obj;
	}
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

void JSONDB::_reload(const std::string &p)
{
	std::map<std::string,char> l(OSUtils::listDirectoryFull(p.c_str()));
	for(std::map<std::string,char>::iterator li(l.begin());li!=l.end();++li) {
		if (li->second == 'f') {
			// assume p starts with _basePath, which it always does -- will throw otherwise
			std::string n(p.substr(_basePath.length()));
			while ((n.length() > 0)&&(n[0] == ZT_PATH_SEPARATOR)) n = n.substr(1);
			if (ZT_PATH_SEPARATOR != '/') std::replace(n.begin(),n.end(),ZT_PATH_SEPARATOR,'/');
			if ((n.length() > 0)&&(n[n.length() - 1] != '/')) n.push_back('/');
			n.append(li->first);
			if ((n.length() > 5)&&(n.substr(n.length() - 5) == ".json")) {
				this->get(n.substr(0,n.length() - 5),0); // causes load and cache or update
			}
		} else if (li->second == 'd') {
			this->_reload(p + ZT_PATH_SEPARATOR + li->first);
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
