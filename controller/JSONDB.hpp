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

#ifndef ZT_JSONDB_HPP
#define ZT_JSONDB_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <map>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include "../node/Constants.hpp"
#include "../node/Utils.hpp"
#include "../ext/json/json.hpp"
#include "../osdep/OSUtils.hpp"

namespace ZeroTier {

/**
 * Hierarchical JSON store that persists into the filesystem
 */
class JSONDB
{
public:
	JSONDB(const std::string &basePath) :
		_basePath(basePath)
	{
		_reload(_basePath,std::string());
	}

	inline void reload()
	{
		_db.clear();
		_reload(_basePath,std::string());
	}

	bool writeRaw(const std::string &n,const std::string &obj);

	bool put(const std::string &n,const nlohmann::json &obj);

	inline bool put(const std::string &n1,const std::string &n2,const nlohmann::json &obj) { return this->put((n1 + "/" + n2),obj); }
	inline bool put(const std::string &n1,const std::string &n2,const std::string &n3,const nlohmann::json &obj) { return this->put((n1 + "/" + n2 + "/" + n3),obj); }
	inline bool put(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,const nlohmann::json &obj) { return this->put((n1 + "/" + n2 + "/" + n3 + "/" + n4),obj); }
	inline bool put(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,const std::string &n5,const nlohmann::json &obj) { return this->put((n1 + "/" + n2 + "/" + n3 + "/" + n4 + "/" + n5),obj); }

	const nlohmann::json &get(const std::string &n);

	inline const nlohmann::json &get(const std::string &n1,const std::string &n2) { return this->get((n1 + "/" + n2)); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3) { return this->get((n1 + "/" + n2 + "/" + n3)); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4) { return this->get((n1 + "/" + n2 + "/" + n3 + "/" + n4)); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,const std::string &n5) { return this->get((n1 + "/" + n2 + "/" + n3 + "/" + n4 + "/" + n5)); }

	void erase(const std::string &n);

	inline void erase(const std::string &n1,const std::string &n2) { this->erase(n1 + "/" + n2); }
	inline void erase(const std::string &n1,const std::string &n2,const std::string &n3) { this->erase(n1 + "/" + n2 + "/" + n3); }
	inline void erase(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4) { this->erase(n1 + "/" + n2 + "/" + n3 + "/" + n4); }
	inline void erase(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,const std::string &n5) { this->erase(n1 + "/" + n2 + "/" + n3 + "/" + n4 + "/" + n5); }

	template<typename F>
	inline void filter(const std::string &prefix,F func)
	{
		for(std::map<std::string,_E>::iterator i(_db.lower_bound(prefix));i!=_db.end();) {
			if ((i->first.length() >= prefix.length())&&(!memcmp(i->first.data(),prefix.data(),prefix.length()))) {
				if (!func(i->first,get(i->first))) {
					std::map<std::string,_E>::iterator i2(i); ++i2;
					this->erase(i->first);
					i = i2;
				} else ++i;
			} else break;
		}
	}

	inline bool operator==(const JSONDB &db) const { return ((_basePath == db._basePath)&&(_db == db._db)); }
	inline bool operator!=(const JSONDB &db) const { return (!(*this == db)); }

private:
	void _reload(const std::string &p,const std::string &b);
	bool _isValidObjectName(const std::string &n);
	std::string _genPath(const std::string &n,bool create);

	struct _E
	{
		nlohmann::json obj;
		inline bool operator==(const _E &e) const { return (obj == e.obj); }
		inline bool operator!=(const _E &e) const { return (obj != e.obj); }
	};

	std::string _basePath;
	std::map<std::string,_E> _db;
};

} // namespace ZeroTier

#endif
