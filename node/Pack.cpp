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

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include "Pack.hpp"
#include "BlobArray.hpp"
#include "Utils.hpp"

#include <openssl/sha.h>

namespace ZeroTier {

std::vector<const Pack::Entry *> Pack::getAll() const
{
	std::vector<const Entry *> v;
	for(std::map<std::string,Entry>::const_iterator e=_entries.begin();e!=_entries.end();++e)
		v.push_back(&(e->second));
	return v;
}

const Pack::Entry *Pack::get(const std::string &name) const
{
	std::map<std::string,Entry>::const_iterator e(_entries.find(name));
	return ((e == _entries.end()) ? (const Entry *)0 : &(e->second));
}

const Pack::Entry *Pack::put(const std::string &name,const std::string &content)
{
	SHA256_CTX sha;

	Pack::Entry &e = _entries[name];
	e.name = name;
	e.content = content;

	SHA256_Init(&sha);
	SHA256_Update(&sha,content.data(),content.length());
	SHA256_Final(e.sha256,&sha);

	e.signedBy = 0;
	e.signature.assign((const char *)0,0);

	return &e;
}

void Pack::clear()
{
	_entries.clear();
}

std::string Pack::serialize() const
{
	BlobArray archive;
	for(std::map<std::string,Entry>::const_iterator e=_entries.begin();e!=_entries.end();++e) {
		BlobArray entry;
		entry.push_back(e->second.name);
		entry.push_back(e->second.content);
		entry.push_back(std::string((const char *)e->second.sha256,sizeof(e->second.sha256)));
		entry.push_back(e->second.signedBy.toBinaryString());
		entry.push_back(e->second.signature);
		archive.push_back(entry.serialize());
	}

	std::string ser(archive.serialize());
	std::string comp;
	Utils::compress(ser.begin(),ser.end(),Utils::StringAppendOutput(comp));
	return comp;
}

bool Pack::deserialize(const void *sd,unsigned int sdlen)
{
	unsigned char dig[32];
	SHA256_CTX sha;

	std::string decomp;
	if (!Utils::decompress(((const char *)sd),((const char *)sd) + sdlen,Utils::StringAppendOutput(decomp)))
		return false;

	BlobArray archive;
	archive.deserialize(decomp.data(),decomp.length());
	clear();
	for(BlobArray::const_iterator i=archive.begin();i!=archive.end();++i) {
		BlobArray entry;
		entry.deserialize(i->data(),i->length());

		if (entry.size() != 5) return false;
		if (entry[2].length() != 32) return false; // SHA-256
		if (entry[3].length() != ZT_ADDRESS_LENGTH) return false; // Address

		Pack::Entry &e = _entries[entry[0]];
		e.name = entry[0];
		e.content = entry[1];

		SHA256_Init(&sha);
		SHA256_Update(&sha,e.content.data(),e.content.length());
		SHA256_Final(dig,&sha);
		if (memcmp(dig,entry[2].data(),32)) return false; // integrity check failed
		memcpy(e.sha256,dig,32);

		if (entry[3].length() == ZT_ADDRESS_LENGTH)
			e.signedBy.setTo(entry[3].data());
		else e.signedBy = 0;
		e.signature = entry[4];
	}
	return true;
}

bool Pack::signAll(const Identity &id)
{
	for(std::map<std::string,Entry>::iterator e=_entries.begin();e!=_entries.end();++e) {
		e->second.signedBy = id.address();
		e->second.signature = id.sign(e->second.sha256);
		if (!e->second.signature.length())
			return false;
	}
	return true;
}

std::vector<const Pack::Entry *> Pack::verifyAll(const Identity &id,bool mandatory) const
{
	std::vector<const Entry *> bad;
	for(std::map<std::string,Entry>::const_iterator e=_entries.begin();e!=_entries.end();++e) {
		if ((e->second.signedBy)&&(e->second.signature.length())) {
			if (id.address() != e->second.signedBy)
				bad.push_back(&(e->second));
			else if (!id.verifySignature(e->second.sha256,e->second.signature.data(),e->second.signature.length()))
				bad.push_back(&(e->second));
		} else if (mandatory)
			bad.push_back(&(e->second));
	}
	return bad;
}

} // namespace ZeroTier
