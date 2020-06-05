/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

package zerotier

import (
	"io"
	"os"
	"sync"
)

type sizeLimitWriter struct {
	f *os.File
	l sync.Mutex
}

func sizeLimitWriterOpen(p string) (*sizeLimitWriter, error) {
	f, err := os.OpenFile(p, os.O_CREATE|os.O_RDWR, 0644)
	if err != nil {
		return nil, err
	}
	_, _ = f.Seek(0, io.SeekEnd)
	return &sizeLimitWriter{f: f}, nil
}

// Write implements io.Writer
func (w *sizeLimitWriter) Write(b []byte) (int, error) {
	w.l.Lock()
	defer w.l.Unlock()
	return w.f.Write(b)
}

// Close closes the underlying file
func (w *sizeLimitWriter) Close() error {
	w.l.Lock()
	defer w.l.Unlock()
	return w.f.Close()
}

func (w *sizeLimitWriter) trim(maxSize int, trimFactor float64, trimAtCR bool) error {
	w.l.Lock()
	defer w.l.Unlock()

	flen, err := w.f.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}

	if flen > int64(maxSize) {
		var buf [131072]byte
		trimAt := int64(float64(maxSize) * trimFactor)
		if trimAt >= flen { // sanity check
			return nil
		}

		if trimAtCR {
		lookForCR:
			for {
				nr, err := w.f.ReadAt(buf[0:1024], trimAt)
				if err != nil {
					return err
				}
				for i := 0; i < nr; i++ {
					trimAt++
					if buf[i] == byte('\n') {
						break lookForCR
					}
				}
				if trimAt >= flen {
					return nil
				}
			}
		}

		copyTo := int64(0)
		for trimAt < flen {
			nr, _ := w.f.ReadAt(buf[:], trimAt)
			if nr > 0 {
				wr, _ := w.f.WriteAt(buf[0:nr], copyTo)
				if wr > 0 {
					copyTo += int64(wr)
				} else {
					break
				}
			} else {
				break
			}
		}

		err = w.f.Truncate(copyTo)
		if err != nil {
			return err
		}
		_, err = w.f.Seek(0, io.SeekEnd)
		return err
	}

	return nil
}
