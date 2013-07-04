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

#ifndef _ZT_BLOBARRAY_HPP
#define _ZT_BLOBARRAY_HPP

#include <vector>
#include <string>
#include <algorithm>

namespace ZeroTier {

/**
 * A vector of binary strings serializable in a packed format
 *
 * The format uses variable-length integers to indicate the length of each
 * field. Each byte of the length has another byte with seven more significant
 * bits if its 8th bit is set. Fields can be up to 2^28 in length.
 */
class BlobArray : public std::vector<std::string>
{
public:
	inline std::string serialize() const
	{
		std::string r;
		for(BlobArray::const_iterator i=begin();i!=end();++i) {
			unsigned int flen = (unsigned int)i->length();
			do {
				unsigned char flenb = (unsigned char)(flen & 0x7f);
				flen >>= 7;
				flenb |= (flen) ? 0x80 : 0;
				r.push_back((char)flenb);
			} while (flen);
			r.append(*i);
		}
		return r;
	}

	/**
	 * Deserialize, replacing the current contents of this array
	 *
	 * @param data Serialized binary data
	 * @param len Length of serialized data
	 */
	inline void deserialize(const void *data,unsigned int len)
	{
		clear();
		for(unsigned int i=0;i<len;) {
			unsigned int flen = 0;
			unsigned int chunk = 0;
			while (i < len) {
				flen |= ((unsigned int)(((const unsigned char *)data)[i] & 0x7f)) << (7 * chunk++);
				if (!(((const unsigned char *)data)[i++] & 0x80))
					break;
			}
			flen = std::min(flen,len - i);
			push_back(std::string(((const char *)data) + i,flen));
			i += flen;
		}
	}
	inline void deserialize(const std::string &data)
	{
		deserialize(data.data(),(unsigned int)data.length());
	}
};

} // namespace ZeroTier

#endif

