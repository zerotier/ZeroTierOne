/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::fs::{File, OpenOptions};
use std::io::{Seek, SeekFrom, Write, stderr};
use std::sync::Mutex;

struct LogIntl {
    prefix: String,
    path: String,
    file: Option<File>,
    cur_size: u64,
    max_size: usize,
    log_to_stderr: bool,
    debug: bool,
}

/// It's big it's heavy it's wood.
pub(crate) struct Log {
    inner: Mutex<LogIntl>,
}

impl Log {
    const MIN_MAX_SIZE: usize = 1024;

    /// Construct a new logger.
    /// If path is empty logs will not be written to files. If log_to_stderr is also
    /// false then no logs will be output at all.
    pub fn new(path: &str, max_size: usize, log_to_stderr: bool, debug: bool, prefix: &str) -> Log {
        let mut p = String::from(prefix);
        if !p.is_empty() {
            p.push(' ');
        }
        Log{
            inner: Mutex::new(LogIntl {
                prefix: p,
                path: String::from(path),
                file: None,
                cur_size: 0,
                max_size: if max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { max_size },
                log_to_stderr,
                debug,
            }),
        }
    }

    pub fn set_max_size(&self, new_max_size: usize) {
        self.inner.lock().unwrap().max_size = if new_max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { new_max_size };
    }

    pub fn set_log_to_stderr(&self, log_to_stderr: bool) {
        self.inner.lock().unwrap().log_to_stderr = log_to_stderr;
    }

    pub fn set_debug(&self, debug: bool) {
        self.inner.lock().unwrap().debug = debug;
    }

    fn log_internal(&self, l: &mut LogIntl, s: &str, pfx: &'static str) {
        if !s.is_empty() {
            let log_line = format!("{}[{}] {}{}\n", l.prefix.as_str(), chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string(), pfx, s);
            if !l.path.is_empty() {
                if l.file.is_none() {
                    let f = OpenOptions::new().read(true).write(true).create(true).open(l.path.as_str());
                    if f.is_err() {
                        return;
                    }
                    let mut f = f.unwrap();
                    let eof = f.seek(SeekFrom::End(0));
                    if eof.is_err() {
                        return;
                    }
                    l.cur_size = eof.unwrap();
                    l.file = Some(f);
                }

                if l.max_size > 0 && l.cur_size > l.max_size as u64 {
                    l.file = None;
                    l.cur_size = 0;

                    let mut old_path = l.path.clone();
                    old_path.push_str(".old");
                    let _ = std::fs::remove_file(old_path.as_str());
                    let _ = std::fs::rename(l.path.as_str(), old_path.as_str());
                    let _ = std::fs::remove_file(l.path.as_str()); // should fail

                    let f = OpenOptions::new().read(true).write(true).create(true).open(l.path.as_str());
                    if f.is_err() {
                        return;
                    }
                    l.file = Some(f.unwrap());
                }

                let f = l.file.as_mut().unwrap();
                let e = f.write_all(log_line.as_bytes());
                if e.is_err() {
                    eprintln!("ERROR: I/O error writing to log: {}", e.err().unwrap().to_string());
                    l.file = None;
                } else {
                    let _ = f.flush();
                    l.cur_size += log_line.len() as u64;
                }
            }

            if l.log_to_stderr {
                let _ = stderr().write_all(log_line.as_bytes());
            }
        }
    }

    pub fn log<S: AsRef<str>>(&self, s: S) {
        let mut l = self.inner.lock().unwrap();
        self.log_internal(&mut (*l), s.as_ref(), "");
    }

    pub fn debug<S: AsRef<str>>(&self, s: S) {
        let mut l = self.inner.lock().unwrap();
        if l.debug {
            self.log_internal(&mut (*l), s.as_ref(), "DEBUG: ");
        }
    }

    pub fn fatal<S: AsRef<str>>(&self, s: S) {
        let mut l = self.inner.lock().unwrap();
        let ss = s.as_ref();
        self.log_internal(&mut (*l), ss, "FATAL: ");
        eprintln!("FATAL: {}", ss);
    }
}

#[macro_export]
macro_rules! l(
    ($logger:expr, $($arg:tt)*) => {
        $logger.log(format!($($arg)*))
    }
);

#[macro_export]
macro_rules! d(
    ($logger:expr, $($arg:tt)*) => {
        $logger.debug(format!($($arg)*))
    }
);

unsafe impl Sync for Log {}

/*
#[cfg(test)]
mod tests {
    use crate::log::Log;

    #[test]
    fn test_log() {
        let l = Log::new("/tmp/ztlogtest.log", 65536, "");
        for i in 0..100000 {
            l.log(format!("line {}", i))
        }
    }
}
*/
