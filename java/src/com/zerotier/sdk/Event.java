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
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

package com.zerotier.sdk;

public enum Event {

    /**
     * Node has been initialized
     *
     * This is the first event generated, and is always sent. It may occur
     * before Node's constructor returns.
     */
	EVENT_UP,

    /**
     * Node is offline -- network does not seem to be reachable by any available strategy
     */
	EVENT_OFFLINE,

    /**
     * Node is online -- at least one upstream node appears reachable
     *
     * Meta-data: none
     */
    EVENT_ONLINE,

    /**
     * Node is shutting down
     * 
     * <p>This is generated within Node's destructor when it is being shut down.
     * It's done for convenience, since cleaning up other state in the event
     * handler may appear more idiomatic.</p>
     */
	EVENT_DOWN,

    /**
     * Your identity has collided with another node's ZeroTier address
     * 
     * <p>This happens if two different public keys both hash (via the algorithm
     * in Identity::generate()) to the same 40-bit ZeroTier address.</p>
     * 
     * <p>This is something you should "never" see, where "never" is defined as
     * once per 2^39 new node initializations / identity creations. If you do
     * see it, you're going to see it very soon after a node is first
     * initialized.</p>
     * 
     * <p>This is reported as an event rather than a return code since it's
     * detected asynchronously via error messages from authoritative nodes.</p>
     * 
     * <p>If this occurs, you must shut down and delete the node, delete the
     * identity.secret record/file from the data store, and restart to generate
     * a new identity. If you don't do this, you will not be able to communicate
     * with other nodes.</p>
     * 
     * <p>We'd automate this process, but we don't think silently deleting
     * private keys or changing our address without telling the calling code
     * is good form. It violates the principle of least surprise.</p>
     * 
     * <p>You can technically get away with not handling this, but we recommend
     * doing so in a mature reliable application. Besides, handling this
     * condition is a good way to make sure it never arises. It's like how
     * umbrellas prevent rain and smoke detectors prevent fires. They do, right?</p>
     */
	EVENT_FATAL_ERROR_IDENTITY_COLLISION,

    /**
     * Trace (debugging) message
     *
     * <p>These events are only generated if this is a TRACE-enabled build.</p>
     *
     * <p>Meta-data: {@link String}, TRACE message</p>
     */
    EVENT_TRACE,

    /**
     * VERB_USER_MESSAGE received
     *
     * These are generated when a VERB_USER_MESSAGE packet is received via
     * ZeroTier VL1.
     */
    EVENT_USER_MESSAGE,

    /**
     * Remote trace received
     *
     * These are generated when a VERB_REMOTE_TRACE is received. Note
     * that any node can fling one of these at us. It is your responsibility
     * to filter and determine if it's worth paying attention to. If it's
     * not just drop it. Most nodes that are not active controllers ignore
     * these, and controllers only save them if they pertain to networks
     * with remote tracing enabled.
     */
    EVENT_REMOTE_TRACE;
}