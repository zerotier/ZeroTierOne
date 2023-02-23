/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c) ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::collections::VecDeque;
use std::sync::Arc;

use tokio::sync::Notify;
use tokio::task::JoinHandle;
use tokio::time::Instant;

/// Watches tokio jobs and times them out if they run past a deadline or aborts them all if the reaper is dropped.
pub struct Reaper {
    q: Arc<(std::sync::Mutex<VecDeque<(JoinHandle<()>, Instant)>>, Notify)>,
    finisher: JoinHandle<()>,
}

impl Reaper {
    pub fn new(runtime: &tokio::runtime::Handle) -> Self {
        let q = Arc::new((std::sync::Mutex::new(VecDeque::with_capacity(16)), Notify::new()));
        Self {
            q: q.clone(),
            finisher: runtime.spawn(async move {
                loop {
                    q.1.notified().await;
                    loop {
                        let j = q.0.lock().unwrap().pop_front();
                        if let Some(j) = j {
                            let _ = tokio::time::timeout_at(j.1, j.0).await;
                        } else {
                            break;
                        }
                    }
                }
            }),
        }
    }

    /// Add a job to be executed with timeout at a given instant.
    #[inline]
    pub fn add(&self, job: JoinHandle<()>, deadline: Instant) {
        self.q.0.lock().unwrap().push_back((job, deadline));
        self.q.1.notify_waiters();
    }
}

impl Drop for Reaper {
    #[inline]
    fn drop(&mut self) {
        self.finisher.abort();
        self.q.0.lock().unwrap().drain(..).for_each(|j| j.0.abort());
    }
}
