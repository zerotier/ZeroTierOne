/* (c) ZeroTier, Inc.
 * See LICENSE.txt in nonfree/
 */

#ifndef NOTIFICATION_LISTENER_HPP
#define NOTIFICATION_LISTENER_HPP

#include <memory>
#include <string>

namespace ZeroTier {

/**
 * Outcome of handling a single notification.
 *
 * Drives the ack/nack decision for delivery mechanisms that support it (GCP PubSub).
 * Mechanisms without redelivery semantics (Postgres LISTEN/NOTIFY, Redis) ignore it.
 */
enum class NotificationResult {
	Ok,					// processed successfully -> ack
	PermanentFailure,	// message is unprocessable (parse/validation) -> ack to drop, avoid poison redelivery
	TransientFailure,	// retryable error (DB unavailable, connection pool exhausted) -> nack to redeliver
};

/**
 * Completes the ack/nack for a single notification once its effect has been durably
 * committed (Ok), determined unprocessable/not-applicable (PermanentFailure -> ack/drop),
 * or has failed transiently (TransientFailure -> nack/redeliver).
 *
 * Lets a delivery mechanism that acks (GCP PubSub) defer the ack until *after* the async
 * DB commit, so an un-committed change is redelivered rather than lost. Delivery
 * mechanisms without redelivery semantics (Redis, Postgres LISTEN/NOTIFY) don't create one.
 * Implementations must be safe to complete exactly once from any thread.
 */
class NotificationCompletion {
  public:
	virtual ~NotificationCompletion()
	{
	}
	virtual void complete(NotificationResult result) = 0;
};

/**
 * Per-thread hand-off used to defer a notification's ack until its asynchronous DB commit
 * finishes, without threading the completion through every DB/listener signature.
 *
 * The delivery callback (e.g. the PubSub subscriber callback) sets the pending completion
 * around its synchronous onNotification() call. The DB's enqueue path
 * (CentralDB::save/eraseNetwork/eraseMember) *takes* it -- moving ownership into the
 * commit-queue item -- when it enqueues async work, and the commit thread completes it after
 * the write. If the write is a no-op or never reaches the commit queue, the completion is
 * left in place and the delivery callback completes it synchronously. DB backends that don't
 * participate (Redis/Postgres, or writes originating off the delivery thread) simply never
 * find a pending completion and behave as before.
 */
void setPendingCompletion(std::shared_ptr<NotificationCompletion> completion);
std::shared_ptr<NotificationCompletion> takePendingCompletion();

/**
 * Base class for notification listeners
 *
 * This class is used to receive notifications from various sources such as Redis, PostgreSQL, etc.
 */
class NotificationListener {
  public:
	NotificationListener() = default;
	virtual ~NotificationListener()
	{
	}

	/**
	 * Called when a notification is received.
	 *
	 * Payload should be parsed and passed to the database handler's save method.
	 *
	 * @param payload The payload of the notification.
	 * @return how the message should be handled (ack/drop vs. nack/redeliver).
	 */
	virtual NotificationResult onNotification(const std::string& payload) = 0;

	/**
	 * Stop and join any background thread so no further onNotification callbacks fire.
	 *
	 * Listeners hold a raw DB* and call back into it from their worker thread; the owner
	 * must be able to quiesce them before tearing down the DB. Idempotent. Default no-op
	 * for listeners that have no background thread.
	 */
	virtual void stop()
	{
	}
};

}	// namespace ZeroTier

#endif	 // NOTIFICATION_LISTENER_HPP