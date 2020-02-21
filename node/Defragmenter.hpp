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
#include "SharedPtr.hpp"
#include "Hashtable.hpp"
#include "Mutex.hpp"
#include "Path.hpp"
#include "FCV.hpp"

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
 * This class is thread-safe and handles locking internally.
 *
 * @tparam MF Maximum number of fragments that each message can possess
 * @tparam GCS Garbage collection target size for the incoming message queue
 * @tparam GCT Garbage collection trigger threshold, usually 2X GCS
 */
template<unsigned int MF,unsigned int GCS = 32,unsigned int GCT = 64>
class Defragmenter
{
public:
	/**
	 * Return values from assemble()
	 */
	enum ResultCode
	{
		/**
		 * No error occurred, fragment accepted
		 */
		OK,

		/**
		 * Message fully assembled and placed in message vector
		 */
		COMPLETE,

		/**
		 * We already have this fragment number or the message is complete
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
	 * The 'via' parameter causes this fragment to be registered with a path and
	 * unregistered when done or abandoned. It's only used the first time it's
	 * supplied (the first non-NULL) for a given message ID. This is a mitigation
	 * against memory exhausting DOS attacks.
	 *
	 * @tparam X Template parameter type for Buf<> containing fragment (inferred)
	 * @param messageId Message ID (a unique ID identifying this message)
	 * @param message Fixed capacity vector that will be filled with the result if result code is DONE
	 * @param fragment Buffer containing fragment that will be filed under this message's ID
	 * @param fragmentDataIndex Index of data in fragment's data.bytes (fragment's data.fields type is ignored)
	 * @param fragmentDataSize Length of data in fragment's data.bytes (fragment's data.fields type is ignored)
	 * @param fragmentNo Number of fragment (0..totalFragmentsExpected, non-inclusive)
	 * @param totalFragmentsExpected Total number of expected fragments in this message or 0 to use cached value
	 * @param now Current time
	 * @param via If non-NULL this is the path on which this message fragment was received
	 * @param maxIncomingFragmentsPerPath If via is non-NULL this is a cutoff for maximum fragments in flight via this path
	 * @return Result code
	 */
	ZT_ALWAYS_INLINE ResultCode assemble(
		const uint64_t messageId,
		FCV< Buf::Slice,MF > &message,
		SharedPtr<Buf> &fragment,
		const unsigned int fragmentDataIndex,
		const unsigned int fragmentDataSize,
		const unsigned int fragmentNo,
		const unsigned int totalFragmentsExpected,
		const int64_t now,
		const SharedPtr< Path > &via,
		const unsigned int maxIncomingFragmentsPerPath)
	{
		// Sanity checks for malformed fragments or invalid input parameters.
		if ((fragmentNo >= totalFragmentsExpected)||(totalFragmentsExpected > MF)||(totalFragmentsExpected == 0))
			return ERR_INVALID_FRAGMENT;

		// We hold the read lock on _messages unless we need to add a new entry or do GC.
		RWMutex::RMaybeWLock ml(_messages_l);

		// Check message hash table size and perform GC if necessary.
		if (_messages.size() >= GCT) {
			try {
				// Scan messages with read lock still locked first and make a sorted list of
				// message entries by last modified time. Then lock for writing and delete
				// the oldest entries to bring the size of the messages hash table down to
				// under the target size. This tries to minimize the amount of time the write
				// lock is held since many threads can hold the read lock but all threads must
				// wait if someone holds the write lock.
				std::vector<std::pair<int64_t,uint64_t> > messagesByLastUsedTime;
				messagesByLastUsedTime.reserve(_messages.size());

				typename Hashtable<uint64_t,_E>::Iterator i(_messages);
				uint64_t *mk = nullptr;
				_E *mv = nullptr;
				while (i.next(mk,mv))
					messagesByLastUsedTime.push_back(std::pair<int64_t,uint64_t>(mv->lastUsed,*mk));

				std::sort(messagesByLastUsedTime.begin(),messagesByLastUsedTime.end());

				ml.writing(); // acquire write lock on _messages
				for (unsigned long x = 0,y = (messagesByLastUsedTime.size() - GCS); x <= y; ++x)
					_messages.erase(messagesByLastUsedTime[x].second);
			} catch (...) {
				return ERR_OUT_OF_MEMORY;
			}
		}

		// Get or create message fragment.
		_E *e = _messages.get(messageId);
		if (!e) {
			ml.writing(); // acquire write lock on _messages if not already
			try {
				e = &(_messages[messageId]);
			} catch (...) {
				return ERR_OUT_OF_MEMORY;
			}
			e->id = messageId;
		}

		// Switch back to holding only the read lock on _messages if we have locked for write
		ml.reading();

		// Acquire lock on entry itself
		Mutex::Lock el(e->lock);

		// This magic value means this message has already been assembled and is done.
		if (e->lastUsed < 0)
			return ERR_DUPLICATE_FRAGMENT;

		// Update last-activity timestamp for this entry, delaying GC.
		e->lastUsed = now;

		// Learn total fragments expected if a value is given. Otherwise the cached
		// value gets used. This is to support the implementation of fragmentation
		// in the ZT protocol where only fragments carry the total.
		if (totalFragmentsExpected > 0)
			e->totalFragmentsExpected = totalFragmentsExpected;

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
			if (tooManyPerPath)
				return ERR_TOO_MANY_FRAGMENTS_FOR_PATH;
		}

		// If we already have fragment number X, abort. Note that we do not
		// actually compare data here. Two same-numbered fragments with different
		// data would just mean the transfer is corrupt and would be detected
		// later e.g. by packet MAC check. Other use cases of this code like
		// network configs check each fragment so this basically can't happen.
		Buf::Slice &s = e->message.at(fragmentNo);
		if (s.b)
			return ERR_DUPLICATE_FRAGMENT;

		// Take ownership of fragment, setting 'fragment' pointer to NULL. The simple
		// transfer of the pointer avoids a synchronized increment/decrement of the object's
		// reference count.
		s.b.move(fragment);
		s.s = fragmentDataIndex;
		s.e = fragmentDataIndex + fragmentDataSize;

		// If we now have all fragments then assemble them.
		if ((e->message.size() >= e->totalFragmentsExpected)&&(e->totalFragmentsExpected > 0)) {
			// This message is done so de-register it with its path if one is associated.
			if (e->via) {
				e->via->_inboundFragmentedMessages_l.lock();
				e->via->_inboundFragmentedMessages.erase(messageId);
				e->via->_inboundFragmentedMessages_l.unlock();
				e->via.zero();
			}

			// Slices are TriviallyCopyable and so may be memcpy'd from e->message to
			// the result parameter. This is fast.
			e->message.unsafeMoveTo(message);
			e->lastUsed = -1; // mark as "done" and force GC to collect

			return COMPLETE;
		}

		return OK;
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
		ZT_ALWAYS_INLINE _E() : id(0),lastUsed(0),totalFragmentsExpected(0),via(),message(),lock() {}
		ZT_ALWAYS_INLINE ~_E()
		{
			via->_inboundFragmentedMessages_l.lock();
			via->_inboundFragmentedMessages.erase(id);
			via->_inboundFragmentedMessages_l.unlock();
		}
		uint64_t id;
		volatile int64_t lastUsed;
		unsigned int totalFragmentsExpected;
		SharedPtr<Path> via;
		FCV< Buf::Slice,MF > message;
		Mutex lock;
	};

	Hashtable< uint64_t,_E > _messages;
	RWMutex _messages_l;
};

} // namespace ZeroTier

#endif
