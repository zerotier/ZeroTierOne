/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_HASHTABLE_HPP
#define ZT_HASHTABLE_HPP

#include "Constants.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <vector>

namespace ZeroTier {

/**
 * A minimal hash table implementation for the ZeroTier core
 *
 * This is optimized for smaller data sets.
 */
template<typename K,typename V>
class Hashtable
{
private:
	struct _Bucket
	{
		ZT_ALWAYS_INLINE _Bucket(const K &k,const V &v) : k(k),v(v) {}
		ZT_ALWAYS_INLINE _Bucket(const K &k) : k(k),v() {}
		ZT_ALWAYS_INLINE _Bucket(const _Bucket &b) : k(b.k),v(b.v) {}
		ZT_ALWAYS_INLINE _Bucket &operator=(const _Bucket &b) { k = b.k; v = b.v; return *this; }
		_Bucket *next; // must be set manually for each _Bucket
		const K k;
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
		ZT_ALWAYS_INLINE Iterator(Hashtable &ht) :
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
		ZT_ALWAYS_INLINE bool next(K *&kptr,V *&vptr)
		{
			for(;;) {
				if (_b) {
					kptr = (K *)(&(_b->k));
					vptr = (V *)(&(_b->v));
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
		_Bucket *_b;
	};
	//friend class Hashtable<K,V>::Iterator;

	/**
	 * @param bc Initial capacity in buckets (default: 32, must be nonzero)
	 */
	ZT_ALWAYS_INLINE Hashtable(unsigned long bc = 32) :
		_t(reinterpret_cast<_Bucket **>(::malloc(sizeof(_Bucket *) * bc))),
		_bc(bc),
		_s(0)
	{
		if (!_t)
			throw ZT_EXCEPTION_OUT_OF_MEMORY;
		for(unsigned long i=0;i<bc;++i)
			_t[i] = (_Bucket *)0;
	}

	ZT_ALWAYS_INLINE Hashtable(const Hashtable<K,V> &ht) :
		_t(reinterpret_cast<_Bucket **>(::malloc(sizeof(_Bucket *) * ht._bc))),
		_bc(ht._bc),
		_s(ht._s)
	{
		if (!_t)
			throw ZT_EXCEPTION_OUT_OF_MEMORY;
		for(unsigned long i=0;i<_bc;++i)
			_t[i] = (_Bucket *)0;
		for(unsigned long i=0;i<_bc;++i) {
			const _Bucket *b = ht._t[i];
			while (b) {
				_Bucket *nb = new _Bucket(*b);
				nb->next = _t[i];
				_t[i] = nb;
				b = b->next;
			}
		}
	}

	ZT_ALWAYS_INLINE ~Hashtable()
	{
		this->clear();
		::free(_t);
	}

	ZT_ALWAYS_INLINE Hashtable &operator=(const Hashtable<K,V> &ht)
	{
		this->clear();
		if (ht._s) {
			for(unsigned long i=0;i<ht._bc;++i) {
				const _Bucket *b = ht._t[i];
				while (b) {
					this->set(b->k,b->v);
					b = b->next;
				}
			}
		}
		return *this;
	}

	/**
	 * Erase all entries
	 */
	ZT_ALWAYS_INLINE void clear()
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
	 * @return Vector of all keys
	 */
	ZT_ALWAYS_INLINE typename std::vector<K> keys() const
	{
		typename std::vector<K> k;
		if (_s) {
			k.reserve(_s);
			for(unsigned long i=0;i<_bc;++i) {
				_Bucket *b = _t[i];
				while (b) {
					k.push_back(b->k);
					b = b->next;
				}
			}
		}
		return k;
	}

	/**
	 * Append all keys (in unspecified order) to the supplied vector or list
	 *
	 * @param v Vector, list, or other compliant container
	 * @tparam Type of V (generally inferred)
	 */
	template<typename C>
	ZT_ALWAYS_INLINE void appendKeys(C &v) const
	{
		if (_s) {
			for(unsigned long i=0;i<_bc;++i) {
				_Bucket *b = _t[i];
				while (b) {
					v.push_back(b->k);
					b = b->next;
				}
			}
		}
	}

	/**
	 * @return Vector of all entries (pairs of K,V)
	 */
	ZT_ALWAYS_INLINE typename std::vector< std::pair<K,V> > entries() const
	{
		typename std::vector< std::pair<K,V> > k;
		if (_s) {
			k.reserve(_s);
			for(unsigned long i=0;i<_bc;++i) {
				_Bucket *b = _t[i];
				while (b) {
					k.push_back(std::pair<K,V>(b->k,b->v));
					b = b->next;
				}
			}
		}
		return k;
	}

	/**
	 * @param k Key
	 * @return Pointer to value or NULL if not found
	 */
	ZT_ALWAYS_INLINE V *get(const K k)
	{
		_Bucket *b = _t[_hc(k) % _bc];
		while (b) {
			if (b->k == k)
				return &(b->v);
			b = b->next;
		}
		return (V *)0;
	}
	ZT_ALWAYS_INLINE const V *get(const K k) const { return const_cast<Hashtable *>(this)->get(k); }

	/**
	 * @param k Key
	 * @param v Value to fill with result
	 * @return True if value was found and set (if false, v is not modified)
	 */
	ZT_ALWAYS_INLINE bool get(const K &k,V &v) const
	{
		_Bucket *b = _t[_hc(k) % _bc];
		while (b) {
			if (b->k == k) {
				v = b->v;
				return true;
			}
			b = b->next;
		}
		return false;
	}

	/**
	 * @param k Key to check
	 * @return True if key is present
	 */
	ZT_ALWAYS_INLINE bool contains(const K &k) const
	{
		_Bucket *b = _t[_hc(k) % _bc];
		while (b) {
			if (b->k == k)
				return true;
			b = b->next;
		}
		return false;
	}

	/**
	 * @param k Key
	 * @return True if value was present
	 */
	ZT_ALWAYS_INLINE bool erase(const K &k)
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
	 * @return Reference to value in table
	 */
	ZT_ALWAYS_INLINE V &set(const K &k,const V &v)
	{
		const unsigned long h = _hc(k);
		unsigned long bidx = h % _bc;

		_Bucket *b = _t[bidx];
		while (b) {
			if (b->k == k) {
				b->v = v;
				return b->v;
			}
			b = b->next;
		}

		if (_s >= _bc) {
			_grow();
			bidx = h % _bc;
		}

		b = new _Bucket(k,v);
		b->next = _t[bidx];
		_t[bidx] = b;
		++_s;
		return b->v;
	}

	/**
	 * @param k Key
	 * @return Value, possibly newly created
	 */
	ZT_ALWAYS_INLINE V &operator[](const K k)
	{
		const unsigned long h = _hc(k);
		unsigned long bidx = h % _bc;

		_Bucket *b = _t[bidx];
		while (b) {
			if (b->k == k)
				return b->v;
			b = b->next;
		}

		if (_s >= _bc) {
			_grow();
			bidx = h % _bc;
		}

		b = new _Bucket(k);
		b->next = _t[bidx];
		_t[bidx] = b;
		++_s;
		return b->v;
	}

	/**
	 * @return Number of entries
	 */
	ZT_ALWAYS_INLINE unsigned long size() const { return _s; }

	/**
	 * @return True if table is empty
	 */
	ZT_ALWAYS_INLINE bool empty() const { return (_s == 0); }

private:
	template<typename O>
	static ZT_ALWAYS_INLINE unsigned long _hc(const O &obj) { return (unsigned long)obj.hashCode(); }

	static ZT_ALWAYS_INLINE unsigned long _hc(const uint64_t i) { return (unsigned long)(i ^ (i >> 32)); }
	static ZT_ALWAYS_INLINE unsigned long _hc(const uint32_t i) { return ((unsigned long)i * (unsigned long)0x9e3779b1); }
	static ZT_ALWAYS_INLINE unsigned long _hc(const uint16_t i) { return ((unsigned long)i * (unsigned long)0x9e3779b1); }
	static ZT_ALWAYS_INLINE unsigned long _hc(const int i) { return ((unsigned long)i * (unsigned long)0x9e3379b1); }
	static ZT_ALWAYS_INLINE unsigned long _hc(void *p) { return ((unsigned long)((uintptr_t)p) * (unsigned long)0x9e3779b1); }
	static ZT_ALWAYS_INLINE unsigned long _hc(const void *p) { return ((unsigned long)((uintptr_t)p) * (unsigned long)0x9e3779b1); }

	ZT_ALWAYS_INLINE void _grow()
	{
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

	_Bucket **_t;
	unsigned long _bc;
	unsigned long _s;
};

} // namespace ZeroTier

#endif
