use std::collections::VecDeque;
use std::sync::Arc;

use tokio::sync::Notify;
use tokio::task::JoinHandle;
use tokio::time::Instant;

/// Watches tokio jobs and times them out if they run past a deadline or aborts them all if the reaper is dropped.
pub struct Reaper {
    q: Arc<(parking_lot::Mutex<VecDeque<(JoinHandle<()>, Instant)>>, Notify)>,
    finisher: JoinHandle<()>,
}

impl Reaper {
    pub fn new() -> Self {
        let q = Arc::new((parking_lot::Mutex::new(VecDeque::with_capacity(16)), Notify::new()));
        Self {
            q: q.clone(),
            finisher: tokio::spawn(async move {
                loop {
                    q.1.notified().await;
                    loop {
                        let j = q.0.lock().pop_front();
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
        self.q.0.lock().push_back((job, deadline));
        self.q.1.notify_waiters();
    }
}

impl Drop for Reaper {
    #[inline]
    fn drop(&mut self) {
        self.finisher.abort();
        self.q.0.lock().drain(..).for_each(|j| j.0.abort());
    }
}
