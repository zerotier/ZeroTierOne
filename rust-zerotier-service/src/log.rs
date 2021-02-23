/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

use std::cell::Cell;
use std::fmt::Display;
use std::fs::{File, OpenOptions};
use std::io::{Seek, SeekFrom, Write, stderr};
use std::sync::Mutex;

use chrono::Datelike;

struct LogIntl {
    prefix: String,
    path: String,
    file: Option<File>,
    cur_size: u64,
    max_size: usize,
    log_to_stderr: bool,
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
    pub fn new(path: &str, max_size: usize, log_to_stderr: bool, prefix: &str) -> Log {
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
            }),
        }
    }

    pub fn set_max_size(&self, new_max_size: usize) {
        self.inner.lock().unwrap().max_size = if new_max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { new_max_size };
    }

    pub fn set_log_to_stderr(&self, log_to_stderr: bool) {
        self.inner.lock().unwrap().log_to_stderr = log_to_stderr;
    }

    pub fn log<S: AsRef<str>>(&self, s: S) {
        let mut l = self.inner.lock().unwrap();

        let ss: &str = s.as_ref();
        if ss.starts_with("FATAL") {
            eprintln!("{}", ss);
        }
        let log_line = format!("{}[{}] {}\n", l.prefix.as_str(), chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string(), ss);

        if l.path.len() > 0 {
            if l.file.is_none() {
                let mut f = OpenOptions::new().read(true).write(true).create(true).open(l.path.as_str());
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

                let mut f = OpenOptions::new().read(true).write(true).create(true).open(l.path.as_str());
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
            stderr().write_all(log_line.as_bytes());
        }
    }
}

#[macro_export]
macro_rules! l(
    ($logger:ident, $($arg:tt)*) => {
        $logger.log(format!($($arg)*))
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
