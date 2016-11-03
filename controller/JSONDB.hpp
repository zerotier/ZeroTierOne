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
		this->_reloadAll(_basePath);
	}

	bool put(const std::string &n,const nlohmann::json &obj);

	inline bool put(const std::string &n1,const std::string &n2,const nlohmann::json &obj) { return this->put((n1 + "/" + n2),obj); }
	inline bool put(const std::string &n1,const std::string &n2,const std::string &n3,const nlohmann::json &obj) { return this->put((n1 + "/" + n2 + "/" + n3),obj); }

	const nlohmann::json &get(const std::string &n,unsigned long maxSinceCheck = 0);

	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,unsigned long maxSinceCheck = 0) { return this->get((n1 + "/" + n2),maxSinceCheck); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3,unsigned long maxSinceCheck = 0) { return this->get((n1 + "/" + n2 + "/" + n3),maxSinceCheck); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,unsigned long maxSinceCheck = 0) { return this->get((n1 + "/" + n2 + "/" + n3 + "/" + n4),maxSinceCheck); }
	inline const nlohmann::json &get(const std::string &n1,const std::string &n2,const std::string &n3,const std::string &n4,const std::string &n5,unsigned long maxSinceCheck = 0) { return this->get((n1 + "/" + n2 + "/" + n3 + "/" + n4 + "/" + n5),maxSinceCheck); }

	template<typename F>
	inline void each(F func,unsigned long maxSinceCheck = 0)
	{
		const uint64_t now = OSUtils::now();
		for(std::map<std::string,_E>::const_iterator i(_db.begin());i!=_db.end();++i) {
			if ((now - i->second.lastCheck) > (uint64_t)maxSinceCheck)
				this->get(i->first);
			func(i->first,i->second.obj);
		}
	}

private:
	bool _isValidObjectName(const std::string &n);
	std::string _genPath(const std::string &n,bool create);
	void _reloadAll(const std::string &path);

	struct _E
	{
		uint64_t lastModifiedOnDisk;
		uint64_t lastCheck;
		nlohmann::json obj;
	};

	std::string _basePath;
	std::map<std::string,_E> _db;
};

} // namespace ZeroTier

#endif
