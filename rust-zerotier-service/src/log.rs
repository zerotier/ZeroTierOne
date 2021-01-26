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

use std::fs::{File, OpenOptions};
use std::sync::Mutex;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::io::{Write, Seek, SeekFrom};
use std::cell::Cell;
use zerotier_core::PortableAtomicI64;
use chrono::Datelike;
use std::fmt::Display;

pub struct Log {
    prefix: String,
    path: String,
    file: Mutex<Cell<Option<File>>>,
    cur_size: PortableAtomicI64,
    max_size: AtomicUsize,
}

impl Log {
    const MIN_MAX_SIZE: usize = 4096;

    pub fn new(path: &str, max_size: usize, prefix: &str) -> Log {
        let mut p = String::from(prefix);
        if !p.is_empty() {
            p.push(' ');
        }
        Log{
            prefix: p,
            path: String::from(path),
            file: Mutex::new(Cell::new(None)),
            cur_size: PortableAtomicI64::new(0),
            max_size: AtomicUsize::new(if max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { max_size }),
        }
    }

    pub fn set_max_size(&self, new_max_size: usize) {
        self.max_size.store(if new_max_size < Log::MIN_MAX_SIZE { Log::MIN_MAX_SIZE } else { new_max_size },Ordering::Relaxed);
    }

    pub fn log<S: Into<String>>(&self, s: S) {
        let mut fc = self.file.lock().unwrap();

        let max_size = self.max_size.load(Ordering::Relaxed);
        if max_size > 0 && fc.get_mut().is_some() {
            if self.cur_size.get() >= max_size as i64 {
                fc.replace(None); // close and dispose of old File
                let mut old_path = self.path.clone();
                old_path.push_str(".old");
                let _ = std::fs::remove_file(old_path.as_str());
                let _ = std::fs::rename(self.path.as_str(), old_path.as_str());
                let _ = std::fs::remove_file(self.path.as_str()); // should fail
                self.cur_size.set(0);
            }
        }

        if fc.get_mut().is_none() {
            let mut f = OpenOptions::new().read(true).write(true).create(true).open(self.path.as_str());
            if f.is_err() {
                return;
            }
            let mut f = f.unwrap();
            let eof = f.seek(SeekFrom::End(0));
            if eof.is_err() {
                return;
            }
            self.cur_size.set(eof.unwrap() as i64);
            fc.replace(Some(f));
        }

        let mut f = fc.get_mut().as_mut().unwrap();
        let now_str = chrono::Local::now().format("%Y-%m-%d %H:%M:%S").to_string();
        let log_line = format!("{}[{}] {}\n", self.prefix.as_str(), now_str.as_str(), s.into());
        let _ = f.write_all(log_line.as_bytes());
        let _ = f.flush();
        self.cur_size.fetch_add(log_line.len() as i64);
    }
}

unsafe impl Sync for Log {}
