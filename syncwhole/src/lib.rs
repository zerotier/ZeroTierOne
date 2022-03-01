pub(crate) mod varint;
pub(crate) mod protocol;
pub(crate) mod iblt;
pub mod database;
pub mod node;
pub mod host;

pub(crate) fn ms_since_epoch() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap().as_millis() as i64
}

pub(crate) fn ms_monotonic() -> i64 {
    std::time::Instant::now().elapsed().as_millis() as i64
}
