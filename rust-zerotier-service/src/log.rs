/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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
    file: Option<File>,
    cur_size: u64,
    max_size: usize,
    log_to_stderr: bool,
}

pub(crate) struct Log {
    prefix: String,
    path: String,
    inner: Mutex<LogIntl>,
}

impl Log {
    const MIN_MAX_SIZE: usize = 1024;

    pub fn new(path: &str, max_size: usize, log_to_stderr: bool, prefix: &str) -> Log {
        let mut p = String::from(prefix);
        if !p.is_empty() {
            p.push(' ');
        }
        Log{
            prefix: p,
            path: String::from(path),
            inner: Mutex::new(LogIntl {
                file: None,
                cur_size: 0,
                max_size: if max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { max_size },
                log_to_stderr: log_to_stderr,
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
        // Output FATAL errors to stderr.
        let ss: &str = s.as_ref();
        if ss.starts_with("FATAL") {
            eprintln!("{}", ss);
        }

        let mut l = self.inner.lock().unwrap();

        // If the file isn't open, open or create and seek to end.
        if l.file.is_none() {
            let mut f = OpenOptions::new().read(true).write(true).create(true).open(self.path.as_str());
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

        // If there is a maximum size limit configured, rotate if exceeded.
        if l.max_size > 0 && l.cur_size > l.max_size as u64 {
            l.file = None;
            l.cur_size = 0;

            let mut old_path = self.path.clone();
            old_path.push_str(".old");
            let _ = std::fs::remove_file(old_path.as_str());
            let _ = std::fs::rename(self.path.as_str(), old_path.as_str());
            let _ = std::fs::remove_file(self.path.as_str()); // should fail

            let mut f = OpenOptions::new().read(true).write(true).create(true).open(self.path.as_str());
            if f.is_err() {
                return;
            }
            l.file = Some(f.unwrap());
        }

        let log_line = format!("{}[{}] {}\n", self.prefix.as_str(), chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string(), ss);
        if l.log_to_stderr {
            stderr().write_all(log_line.as_bytes());
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
