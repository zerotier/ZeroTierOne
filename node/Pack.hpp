/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_PACK_HPP
#define _ZT_PACK_HPP

#include <string>
#include <map>
#include <list>
#include <stdexcept>
#include "Address.hpp"
#include "Identity.hpp"

namespace ZeroTier {

/**
 * A very simple archive format for distributing packs of files or resources
 * 
 * This is used for things like the auto-updater. It's not suitable for huge
 * files, since at present it must work in memory. Packs support signing with
 * identities and signature verification.
 */
class Pack
{
public:
	/**
	 * Pack entry structure for looking up deserialized entries
	 */
	struct Entry
	{
		std::string name;
		std::string content;
		unsigned char sha256[32];
		Address signedBy;
		std::string signature;
	};

	Pack() {}
	~Pack() {}

	/**
	 * @return Vector of all entries
	 */
	std::vector<const Entry *> getAll() const;

	/**
	 * Look up an entry
	 * 
	 * @param name Name to look up
	 * @return Pointer to entry if it exists or NULL if not found
	 */
	const Entry *get(const std::string &name) const;

	/**
	 * Add an entry to this pack
	 * 
	 * @param name Entry to add
	 * @param content Entry's contents
	 * @return The new entry
	 */
	const Entry *put(const std::string &name,const std::string &content);

	/**
	 * Remove all entries
	 */
	void clear();

	/**
	 * @return Number of entries in pack
	 */
	inline unsigned int numEntries() const { return (unsigned int)_entries.size(); }

	/**
	 * Serialize this pack
	 * 
	 * @return Serialized form (compressed with LZ4)
	 */
	std::string serialize() const;

	/**
	 * Deserialize this pack
	 * 
	 * Any current contents are lost. This does not verify signatures,
	 * but does check SHA256 hashes for entry integrity. If the return
	 * value is false, the pack's contents are undefined.
	 * 
	 * @param sd Serialized data
	 * @param sdlen Length of serialized data
	 * @return True on success, false on deserialization error
	 */
	bool deserialize(const void *sd,unsigned int sdlen);
	inline bool deserialize(const std::string &sd) { return deserialize(sd.data(),sd.length()); }

	/**
	 * Sign all entries in this pack with a given identity
	 * 
	 * @param id Identity to sign with
	 * @return True on signature success, false if error
	 */
	bool signAll(const Identity &id);

	/**
	 * Verify all signed entries
	 * 
	 * @param id Identity to verify against
	 * @param mandatory If true, require that all entries be signed and fail if no signature
	 * @return Vector of entries that failed verification or empty vector if all passed
	 */
	std::vector<const Entry *> verifyAll(const Identity &id,bool mandatory) const;

private:
	std::map<std::string,Entry> _entries;
};

} // namespace ZeroTier

#endif
