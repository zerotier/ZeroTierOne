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
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 */

#ifndef ZT_HASHTABLE_HPP
#define ZT_HASHTABLE_HPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>

namespace ZeroTier {

/**
 * A minimal hash table implementation for the ZeroTier core
 *
 * This is not a drop-in replacement for STL containers, and has several
 * limitations. It's designed to be small and fast for use in the
 * ZeroTier core.
 *
 * Pairs of values can also be used as a key. In this case the first and
 * second element of the pair's hash codes are XORed.
 */
template<typename K,typename V>
class Hashtable
{
private:
	struct _Bucket
	{
		_Bucket(const K &k,const V &v) :
			k(k),
			v(v) {}
		_Bucket *next;
		K k;
		V v;
	};

public:
	/**
	 * A simple forward iterator (different from STL)
	 *
	 * It's safe to erase the last key, but not others. Don't use set() since that
	 * may rehash and invalidate the iterator. Note the erasing the key will destroy
	 * the targets of the pointers returned by next().
	 */
	class Iterator
	{
	public:
		/**
		 * @param ht Hash table to iterate over
		 */
		Iterator(Hashtable &ht) :
			_idx(0),
			_ht(&ht),
			_b(ht._t[0])
		{
		}

		/**
		 * @param kptr Pointer to set to point to next key
		 * @param vptr Pointer to set to point to next value
		 * @return True if kptr and vptr are set, false if no more entries
		 */
		inline bool next(K *&kptr,V *&vptr)
		{
			for(;;) {
				if (_b) {
					kptr = &(_b->k);
					vptr = &(_b->v);
					_b = _b->next;
					return true;
				}
				++_idx;
				if (_idx >= _ht->_bc)
					return false;
				_b = _ht->_t[_idx];
			}
		}

	private:
		unsigned long _idx;
		Hashtable *_ht;
		Hashtable::_Bucket *_b;
	};
	friend class Hashtable::Iterator;

	/**
	 * @param bc Initial capacity in buckets (default: 128, must be nonzero)
	 */
	Hashtable(unsigned long bc = 128) :
		_t(reinterpret_cast<_Bucket **>(::malloc(sizeof(_Bucket *) * bc))),
		_bc(bc),
		_s(0)
	{
		if (!_t)
			throw std::bad_alloc();
		for(unsigned long i=0;i<bc;++i)
			_t[i] = (_Bucket *)0;
	}

	~Hashtable()
	{
		clear();
		::free(_t);
	}

	/**
	 * Erase all entries
	 */
	inline void clear()
	{
		if (_s) {
			for(unsigned long i=0;i<_bc;++i) {
				_Bucket *b = _t[i];
				while (b) {
					_Bucket *const nb = b->next;
					delete b;
					b = nb;
				}
				_t[i] = (_Bucket *)0;
			}
			_s = 0;
		}
	}

	/**
	 * @param k Key
	 * @return Pointer to value or NULL if not found
	 */
	inline V *get(const K &k)
	{
		_Bucket *b = _t[_hc(k) % _bc];
		while (b) {
			if (b->k == k)
				return &(b->v);
			b = b->next;
		}
		return (V *)0;
	}
	inline const V *get(const K &k) const { return const_cast<Hashtable *>(this)->get(k); }

	/**
	 * @param k Key
	 * @return True if value was present
	 */
	inline bool erase(const K &k)
	{
		const unsigned long bidx = _hc(k) % _bc;
		_Bucket *lastb = (_Bucket *)0;
		_Bucket *b = _t[bidx];
		while (b) {
			if (b->k == k) {
				if (lastb)
					lastb->next = b->next;
				else _t[bidx] = b->next;
				delete b;
				--_s;
				return true;
			}
			lastb = b;
			b = b->next;
		}
		return false;
	}

	/**
	 * @param k Key
	 * @param v Value
	 */
	inline void set(const K &k,const V &v)
	{
		if (_s >= _bc) {
			const unsigned long nc = _bc * 2;
			_Bucket **nt = reinterpret_cast<_Bucket **>(::malloc(sizeof(_Bucket *) * nc));
			if (nt) {
				for(unsigned long i=0;i<nc;++i)
					nt[i] = (_Bucket *)0;
				for(unsigned long i=0;i<_bc;++i) {
					_Bucket *b = _t[i];
					while (b) {
						_Bucket *const nb = b->next;
						const unsigned long nidx = _hc(b->k) % nc;
						b->next = nt[nidx];
						nt[nidx] = b;
						b = nb;
					}
				}
				::free(_t);
				_t = nt;
				_bc = nc;
			}
		}
		const unsigned long bidx = _hc(k) % _bc;
		_Bucket *const b = new _Bucket(k,v);
		b->next = _t[bidx];
		_t[bidx] = b;
		++_s;
	}

	/**
	 * @return Number of entries
	 */
	inline unsigned long size() const throw() { return _s; }

	/**
	 * @return True if table is empty
	 */
	inline bool empty() const throw() { return (_s == 0); }

private:
	template<typename O>
	static inline unsigned long _hc(const O &obj)
	{
		return obj.hashCode();
	}
	static inline unsigned long _hc(const uint64_t i)
	{
		// NOTE: this is fine for network IDs, but might be bad for other kinds
		// of IDs if they are not evenly or randomly distributed.
		return (unsigned long)((i ^ (i >> 32)) * 2654435761ULL);
	}

	_Bucket **_t;
	unsigned long _bc;
	unsigned long _s;
};

} // namespace ZeroTier

#endif
