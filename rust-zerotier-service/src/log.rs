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
use std::io::{Seek, SeekFrom, Write};
use std::sync::atomic::{AtomicUsize, Ordering, AtomicBool};
use std::sync::Mutex;

use chrono::Datelike;

use zerotier_core::PortableAtomicI64;

struct LogIntl {
    file: Option<File>,
    cur_size: u64,
    max_size: usize,
    enabled: bool
}

pub(crate) struct Log {
    prefix: String,
    path: String,
    intl: Mutex<LogIntl>
}

impl Log {
    const MIN_MAX_SIZE: usize = 1024;

    pub fn new(path: &str, max_size: usize, prefix: &str) -> Log {
        let mut p = String::from(prefix);
        if !p.is_empty() {
            p.push(' ');
        }
        Log{
            prefix: p,
            path: String::from(path),
            intl: Mutex::new(LogIntl {
                file: None,
                cur_size: 0,
                max_size: if max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { max_size },
                enabled: true,
            }),
        }
    }

    pub fn set_max_size(&self, new_max_size: usize) {
        self.intl.lock().unwrap().max_size = if new_max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { new_max_size };
    }

    pub fn set_enabled(&self, enabled: bool) {
        self.intl.lock().unwrap().enabled = enabled;
    }

    pub fn log<S: AsRef<str>>(&self, s: S) {
        let mut l = self.intl.lock().unwrap();
        if l.enabled {
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

            let f = l.file.as_mut().unwrap();
            let now_str = chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string();
            let ss: &str = s.as_ref();
            let log_line = format!("{}[{}] {}\n", self.prefix.as_str(), now_str.as_str(), ss);
            let _ = f.write_all(log_line.as_bytes());
            let _ = f.flush();
            l.cur_size += log_line.len() as u64;
        }
    }
}

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
