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

static const nlohmann::json _EMPTY_JSON({{}});

bool JSONDB::put(const std::string &n,const nlohmann::json &obj)
{
	if (!_isValidObjectName(n))
		return false;

	std::string path(_genPath(n,false));
	if (!path.length())
		return false;

	std::string buf(obj.dump(2));
	if (!OSUtils::writeFile(path.c_str(),buf))
		return false;

	_E &e = _db[n];

	e.lastModifiedOnDisk = OSUtils::getLastModified(path.c_str());
	e.lastCheck = OSUtils::now();
	e.obj = obj;

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

		std::string path(_genPath(n,false));
		if (!path.length()) // sanity check
			return _EMPTY_JSON;

		// We are somewhat tolerant to momentary disk failures here. This may
		// occur over e.g. EC2's elastic filesystem (NFS).
		const uint64_t lm = OSUtils::getLastModified(path.c_str());
		if ((lm)&&(e->second.lastModifiedOnDisk != lm)) {
			if (OSUtils::readFile(path.c_str(),buf)) {
				try {
					e->second.lastModifiedOnDisk = lm;
					e->second.lastCheck = now;
					e->second.obj = nlohmann::json::parse(buf);
				} catch ( ... ) {
					e->second.obj = _EMPTY_JSON;
				}
			}
		}

		return e->second.obj;
	} else {
		std::string path(_genPath(n,false));
		if (!path.length())
			return _EMPTY_JSON;

		if (!OSUtils::readFile(path.c_str(),buf))
			return _EMPTY_JSON;

		const uint64_t lm = OSUtils::getLastModified(path.c_str());
		if (!lm)
			return _EMPTY_JSON;

		_E &e2 = _db[n];
		e2.lastModifiedOnDisk = lm;
		e2.lastCheck = now;
		try {
			e2.obj = nlohmann::json::parse(buf);
		} catch ( ... ) {
			e2.obj = _EMPTY_JSON;
		}

		return e2.obj;
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
	std::vector<std::string> pt(Utils::split(n.c_str(),"/","",""));
	if (pt.size() == 0)
		return std::string();
	if (pt.size() == 1)
		return pt[0];

	std::string p(_basePath);
	if (create) OSUtils::mkdir(p.c_str());
	for(unsigned long i=0,j=pt.size()-1;i<j;++i) {
		p.push_back(ZT_PATH_SEPARATOR);
		p.append(pt[i]);
		if (create) OSUtils::mkdir(p.c_str());
	}

	p.push_back(ZT_PATH_SEPARATOR);
	p.append(pt[pt.size()-1]);
	p.append(".json");

	return p;
}

void JSONDB::_reloadAll(const std::string &path)
{
}

} // namespace ZeroTier
