/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_DEFRAGMENTER_HPP
#define ZT_DEFRAGMENTER_HPP

#include "Constants.hpp"
#include "Buf.hpp"
#include "AtomicCounter.hpp"
#include "SharedPtr.hpp"
#include "Hashtable.hpp"
#include "Mutex.hpp"
#include "Path.hpp"

#include <cstring>
#include <cstdlib>
#include <vector>

namespace ZeroTier {

/**
 * Generalized putter back together-er for fragmented messages
 *
 * This is used both for packet fragment assembly and multi-chunk network config
 * assembly. This is abstracted out of the code that uses it because it's a bit of
 * a hairy and difficult thing to get both correct and fast, and because its
 * hairiness makes it very desirable to be able to test and fuzz this code
 * independently.
 *
 * Here be dragons!
 *
 * @tparam MF Maximum number of fragments that each message can possess
 */
template<unsigned int MF>
class Defragmenter
{
public:
	/**
	 * Error codes for assemble()
	 */
	enum ErrorCode
	{
		/**
		 * No error occurred
		 */
		ERR_NONE,

		/**
		 * This fragment duplicates another with the same fragment number for this message
		 */
		ERR_DUPLICATE_FRAGMENT,

		/**
		 * The fragment is invalid, such as e.g. having a fragment number beyond the expected count.
		 */
		ERR_INVALID_FRAGMENT,

		/**
		 * Too many fragments are in flight for this path
		 *
		 * The message will be marked as if it's done (all fragments received) but will
		 * be abandoned. Subsequent fragments will generate a DUPLICATE_FRAGMENT error.
		 *
		 * This is an anti-denial-of-service feature to limit the number of inbound
		 * fragments that can be in flight over a given physical network path.
		 */
		ERR_TOO_MANY_FRAGMENTS_FOR_PATH,

		/**
		 * Memory (or some other limit) exhausted
		 */
		ERR_OUT_OF_MEMORY
	};

	/**
	 * Return tuple for assemble()
	 */
	struct Result
	{
		ZT_ALWAYS_INLINE Result() : message(),messageFragmentCount(0),error(Defragmenter::ERR_NONE) {}

		/**
		 * Fully assembled message as a series of slices of fragments
		 */
		Buf<>::Slice message[MF];

		/**
		 * Fully assembled message fragment count (number of slices)
		 *
		 * This will be nonzero if the message is fully assembled.
		 */
		unsigned int messageFragmentCount;

		/**
		 * Error code or ERR_NONE if none
		 */
		Defragmenter::ErrorCode error;
	};

	/**
	 * Process a fragment of a multi-part message
	 *
	 * The message ID is arbitrary but must be something that can uniquely
	 * group fragments for a given final message. The total fragments expected
	 * value is expectded to be the same for all fragments in a message. Results
	 * are undefined and probably wrong if this value changes across a message.
	 * Fragment numbers must be sequential starting with 0 and going up to
	 * one minus total fragments expected (non-inclusive range).
	 *
	 * Fragments can arrive in any order. Duplicates are dropped and ignored.
	 *
	 * It's the responsibility of the caller to do whatever validation needs to
	 * be done before considering a fragment valid and to make sure the fragment
	 * data index and size parameters are valid.
	 *
	 * The fragment supplied to this function is kept and held under the supplied
	 * message ID until or unless (1) the message is fully assembled, (2) the
	 * message is orphaned and its entry is taken by a new message, or (3) the
	 * clear() function is called to forget all incoming messages. The pointer
	 * at the 'fragment' reference will be zeroed since this pointer is handed
	 * off, so the SharedPtr<> passed in as 'fragment' will be NULL after this
	 * function is called.
	 *
	 * The result returned by this function is a structure containing a series
	 * of assembled and complete fragments, a fragment count, and an error.
	 * If the message fragment count is non-zero then the message has been
	 * successfully assembled. If the fragment count is zero then an error may
	 * have occurred or the message may simply not yet be complete.
	 *
	 * The calling code must decide what to do with the assembled and ordered
	 * fragments, such as memcpy'ing them into a contiguous buffer or handling
	 * them as a vector of fragments.
	 *
	 * The 'via' parameter causes this fragment to be registered with a path and
	 * unregistered when done or abandoned. It's only used the first time it's
	 * supplied (the first non-NULL) for a given message ID. This is a mitigation
	 * against memory exhausting DOS attacks.
	 *
	 * Lastly the message queue size target and GC trigger parameters control
	 * garbage collection of defragmenter message queue entries. If the size
	 * target parameter is non-zero then the message queue is cleaned when its
	 * size reaches the GC trigger parameter, which MUST be larger than the size
	 * target. Cleaning is done by sorting all entries by their last modified
	 * timestamp and removing the oldest N entries so as to bring the size down
	 * to under the size target. The use of a trigger size that is larger than
	 * the size target reduces CPU-wasting thrashing. A good value for the trigger
	 * is 2X the size target, causing cleanups to happen only occasionally.
	 *
	 * If the GC parameters are set to zero then clear() must be called from time
	 * to time or memory use will grow without bound.
	 *
	 * @tparam X Template parameter type for Buf<> containing fragment (inferred)
	 * @param messageId Message ID (a unique ID identifying this message)
	 * @param fragment Buffer containing fragment that will be filed under this message's ID
	 * @param fragmentDataIndex Index of data in fragment's data.bytes (fragment's data.fields type is ignored)
	 * @param fragmentDataSize Length of data in fragment's data.bytes (fragment's data.fields type is ignored)
	 * @param fragmentNo Number of fragment (0..totalFragmentsExpected, non-inclusive)
	 * @param totalFragmentsExpected Total number of expected fragments in this message
	 * @param now Current time
	 * @param via If non-NULL this is the path on which this message fragment was received
	 * @param maxIncomingFragmentsPerPath If via is non-NULL this is a cutoff for maximum fragments in flight via this path
	 * @param messageQueueSizeTarget If non-zero periodically clean the message queue to bring it under this size
	 * @param messageQueueSizeGCTrigger A value larger than messageQueueSizeTarget that is when cleaning is performed
	 * @return Result buffer (pointer to 'result' or newly allocated buffer) or NULL if message not complete
	 */
	ZT_ALWAYS_INLINE Result assemble(
		const uint64_t messageId,
		SharedPtr< Buf<> > &fragment,
		const unsigned int fragmentDataIndex,
		const unsigned int fragmentDataSize,
		const unsigned int fragmentNo,
		const unsigned int totalFragmentsExpected,
		const int64_t now,
		const SharedPtr< Path > &via,
		const unsigned int maxIncomingFragmentsPerPath,
		const unsigned long messageQueueSizeTarget,
		const unsigned long messageQueueSizeGCTrigger)
	{
		Result r;

		// Sanity checks for malformed fragments or invalid input parameters.
		if ((fragmentNo >= totalFragmentsExpected)||(totalFragmentsExpected > MF)||(totalFragmentsExpected == 0)) {
			r.error = ERR_INVALID_FRAGMENT;
			return r;
		}

		// If there is only one fragment just return that fragment and we are done.
		if (totalFragmentsExpected < 2) {
			if (fragmentNo == 0) {
				r.message[0].b.move(fragment);
				r.message[0].s = fragmentDataIndex;
				r.message[0].e = fragmentDataSize;
				r.messageFragmentCount = 1;
				return r;
			} else {
				r.error = ERR_INVALID_FRAGMENT;
				return r;
			}
		}

		// Lock messages for read and look up current entry. Also check the
		// GC trigger and if we've exceeded that threshold then older message
		// entries are garbage collected.
		_messages_l.rlock();
		if (messageQueueSizeTarget > 0) {
			if (_messages.size() >= messageQueueSizeGCTrigger) {
				try {
					// Scan messages with read lock still locked first and make a sorted list of
					// message entries by last modified time. Then lock for writing and delete
					// the oldest entries to bring the size of the messages hash table down to
					// under the target size. This tries to minimize the amount of time the write
					// lock is held since many threads can hold the read lock but all threads must
					// wait if someone holds the write lock.
					std::vector< std::pair<int64_t,uint64_t> > messagesByLastUsedTime;
					messagesByLastUsedTime.reserve(_messages.size());

					typename Hashtable<uint64_t,_E>::Iterator i(_messages);
					uint64_t *mk = nullptr;
					_E *mv = nullptr;
					while (i.next(mk,mv))
						messagesByLastUsedTime.push_back(std::pair<int64_t,uint64_t>(mv->lastUsed,*mk));

					std::sort(messagesByLastUsedTime.begin(),messagesByLastUsedTime.end());

					_messages_l.runlock();
					_messages_l.lock();
					for (unsigned long x = 0,y = (messagesByLastUsedTime.size() - messageQueueSizeTarget); x <= y; ++x)
						_messages.erase(messagesByLastUsedTime[x].second);
					_messages_l.unlock();
					_messages_l.rlock();
				} catch (...) {
					// The only way something in that code can throw is if a bad_alloc occurs when
					// reserve() is called in the vector. In this case we flush the entire queue
					// and error out. This is very rare and on some platforms impossible.
					_messages_l.runlock();
					_messages_l.lock();
					_messages.clear();
					_messages_l.unlock();
					r.error = ERR_OUT_OF_MEMORY;
					return r;
				}
			}
		}
		_E *e = _messages.get(messageId);
		_messages_l.runlock();

		// If no entry exists we must briefly lock messages for write and create a new one.
		if (!e) {
			try {
				RWMutex::Lock ml(_messages_l);
				e = &(_messages[messageId]);
			} catch ( ... ) {
				r.error = ERR_OUT_OF_MEMORY;
				return r;
			}
			e->id = messageId;
		}

		// Now handle this fragment within this individual message entry.
		Mutex::Lock el(e->lock);

		// Note: it's important that _messages_l is not locked while the entry
		// is locked or a deadlock could occur due to GC or clear() being called
		// in another thread.

		// If there is a path associated with this fragment make sure we've registered
		// ourselves as in flight, check the limit, and abort if exceeded.
		if ((via)&&(!e->via)) {
			e->via = via;
			bool tooManyPerPath = false;
			via->_inboundFragmentedMessages_l.lock();
			try {
				if (via->_inboundFragmentedMessages.size() < maxIncomingFragmentsPerPath) {
					via->_inboundFragmentedMessages.insert(messageId);
				} else {
					tooManyPerPath = true;
				}
			} catch ( ... ) {
				// This would indicate something like bad_alloc thrown by the set. Treat
				// it as limit exceeded.
				tooManyPerPath = true;
			}
			via->_inboundFragmentedMessages_l.unlock();
			if (tooManyPerPath) {
				r.error = ERR_TOO_MANY_FRAGMENTS_FOR_PATH;
				return r;
			}
		}

		// Update last-activity timestamp for this entry.
		e->lastUsed = now;

		// If we already have fragment number X, abort. Note that we do not
		// actually compare data here. Two same-numbered fragments with different
		// data would just mean the transfer is corrupt and would be detected
		// later e.g. by packet MAC check. Other use cases of this code like
		// network configs check each fragment so this basically can't happen.
		Buf<>::Slice &s = e->fragment[fragmentNo];
		if (s.b) {
			r.error = ERR_DUPLICATE_FRAGMENT;
			return r;
		}

		// Take ownership of fragment, setting 'fragment' pointer to NULL. The simple
		// transfer of the pointer avoids a synchronized increment/decrement of the object's
		// reference count.
		s.b.move(fragment);
		s.s = fragmentDataIndex;
		s.e = fragmentDataIndex + fragmentDataSize;

		// If we now have all fragments then assemble them.
		if (++e->fragmentCount >= totalFragmentsExpected) {
			// This message is done so de-register it with its path if one is associated.
			if (e->via) {
				e->via->_inboundFragmentedMessages_l.lock();
				e->via->_inboundFragmentedMessages.erase(messageId);
				e->via->_inboundFragmentedMessages_l.unlock();
				e->via.zero();
			}

			// PERFORMANCE HACK: SharedPtr<> is introspective and only holds a pointer, so we
			// can 'move' the pointers it holds very quickly by bulk copying the source
			// slices and then zeroing the originals. This is only okay if the destination
			// currently holds no pointers, which should always be the case. Don't try this
			// at home kids.
			unsigned int msize = e->fragmentCount * sizeof(Buf<>::Slice);
			memcpy(reinterpret_cast<void *>(r.message),reinterpret_cast<const void *>(e->fragment),msize);
			memset(reinterpret_cast<void *>(e->fragment),0,msize);
			r.messageFragmentCount = e->fragmentCount;
		}

		return r;
	}

	/**
	 * Erase all message entries in the internal queue
	 */
	ZT_ALWAYS_INLINE void clear()
	{
		RWMutex::Lock ml(_messages_l);
		_messages.clear();
	}

private:
	struct _E
	{
		ZT_ALWAYS_INLINE _E() : id(0),lastUsed(0),via(),fragmentCount(0) {}
		ZT_ALWAYS_INLINE ~_E()
		{
			// Ensure that this entry is not in use while it is being deleted!
			lock.lock();
			if (via) {
				via->_inboundFragmentedMessages_l.lock();
				via->_inboundFragmentedMessages.erase(id);
				via->_inboundFragmentedMessages_l.unlock();
			}
			lock.unlock();
		}
		uint64_t id;
		volatile int64_t lastUsed;
		SharedPtr<Path> via;
		Buf<>::Slice fragment[MF];
		unsigned int fragmentCount;
		Mutex lock;
	};

	Hashtable< uint64_t,_E > _messages;
	RWMutex _messages_l;
};

} // namespace ZeroTier

#endif
