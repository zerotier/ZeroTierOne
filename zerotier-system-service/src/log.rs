/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * (c)2021 ZeroTier, Inc.
 * https://www.zerotier.com/
 */

use std::fs::{File, OpenOptions};
use std::io::{Seek, SeekFrom, stderr, Write};
use std::sync::Arc;

use parking_lot::Mutex;

/// It's big it's heavy it's wood.
pub struct Log {
    prefix: String,
    path: String,
    file: Option<File>,
    cur_size: u64,
    max_size: usize,
    log_to_stderr: bool,
    debug: bool,
}

impl Log {
    /// Minimum "maximum size" parameter.
    const MIN_MAX_SIZE: usize = 1024;

    /// Construct a new logger.
    ///
    /// If path is empty logs will not be written to files. If log_to_stderr is also
    /// false then no logs will be output at all.
    ///
    /// This returns an Arc<Mutex<Log>> suitable for use with the l! and d! macros, which
    /// expect a mutex guarded instance.
    pub fn new(path: &str, max_size: usize, log_to_stderr: bool, debug: bool, prefix: &str) -> Arc<Mutex<Log>> {
        let mut p = String::from(prefix);
        if !p.is_empty() {
            p.push(' ');
        }
        Arc::new(Mutex::new(Log{
            prefix: p,
            path: String::from(path),
            file: None,
            cur_size: 0,
            max_size: max_size.max(Self::MIN_MAX_SIZE),
            log_to_stderr,
            debug,
        }))
    }

    pub fn set_max_size(&mut self, new_max_size: usize) {
        self.max_size = if new_max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { new_max_size };
    }

    pub fn set_log_to_stderr(&mut self, log_to_stderr: bool) {
        self.log_to_stderr = log_to_stderr;
    }

    pub fn set_debug(&mut self, debug: bool) {
        self.debug = debug;
    }

    fn log_internal(&mut self, pfx: &str, s: &str) {
        if !s.is_empty() {
            let log_line = format!("{}[{}] {}{}\n", l.prefix.as_str(), chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string(), pfx, s);
            if !self.path.is_empty() {
                if self.file.is_none() {
                    let f = OpenOptions::new().read(true).write(true).create(true).open(self.path.as_str());
                    if f.is_err() {
                        return;
                    }
                    let mut f = f.unwrap();
                    let eof = f.seek(SeekFrom::End(0));
                    if eof.is_err() {
                        return;
                    }
                    self.cur_size = eof.unwrap();
                    self.file = Some(f);
                }

                if self.max_size > 0 && self.cur_size > self.max_size as u64 {
                    self.file = None;
                    self.cur_size = 0;

                    let mut old_path = self.path.clone();
                    old_path.push_str(".old");
                    let _ = std::fs::remove_file(old_path.as_str());
                    let _ = std::fs::rename(self.path.as_str(), old_path.as_str());
                    let _ = std::fs::remove_file(self.path.as_str()); // should fail

                    let f = OpenOptions::new().read(true).write(true).create(true).open(self.path.as_str());
                    if f.is_err() {
                        return;
                    }
                    self.file = Some(f.unwrap());
                }

                let f = self.file.as_mut().unwrap();
                let e = f.write_all(log_line.as_bytes());
                if e.is_err() {
                    eprintln!("ERROR: I/O error writing to log: {}", e.err().unwrap().to_string());
                    self.file = None;
                } else {
                    let _ = f.flush();
                    self.cur_size += log_line.len() as u64;
                }
            }

            if self.log_to_stderr {
                let _ = stderr().write_all(log_line.as_bytes());
            }
        }
    }

    pub fn log<S: AsRef<str>>(&mut self, s: S) {
        self.log_internal("", s.as_ref());
    }

    pub fn debug<S: AsRef<str>>(&mut self, s: S) {
        if self.debug {
            self.log_internal("DEBUG: ", s.as_ref());
        }
    }

    pub fn fatal<S: AsRef<str>>(&mut self, s: S) {
        let ss = s.as_ref();
        self.log_internal("FATAL: ", ss);
        eprintln!("FATAL: {}", ss);
    }
}

#[macro_export]
macro_rules! l(
    ($logger:expr, $($arg:tt)*) => {
        $logger.lock().log(format!($($arg)*))
    }
);

#[macro_export]
macro_rules! d(
    ($logger:expr, $($arg:tt)*) => {
        $logger.lock().debug(format!($($arg)*))
    }
);

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
