mod store;
mod replicator;
mod protocol;
mod varint;

pub(crate) fn ms_since_epoch() -> u64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as u64
}

pub(crate) fn ms_monotonic() -> u64 {
    std::time::Instant::now().elapsed().as_millis() as u64
}
