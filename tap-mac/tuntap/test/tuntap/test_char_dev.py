# Copyright (c) 2011 Mattias Nissler <mattias.nissler@gmx.de>
#
# Redistribution and use in source and binary forms, with or without modification, are permitted
# provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice, this list of
#      conditions and the following disclaimer.
#   2. Redistributions in binary form must reproduce the above copyright notice, this list of
#      conditions and the following disclaimer in the documentation and/or other materials provided
#      with the distribution.
#   3. The name of the author may not be used to endorse or promote products derived from this
#      software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import os
from tuntap.char_dev_harness import TunCharDevHarness, TapCharDevHarness
from unittest import TestCase

class TestCharDev(TestCase):

    def __init__(self, name, newHarness):
        super(TestCharDev, self).__init__(name)
        self._newHarness = newHarness

    def setUp(self):
        self.char_dev = self._newHarness()
        self.char_dev.open()
    
    def tearDown(self):
        self.char_dev.close()

    def test_Open(self):
        pass

    def test_OpenTwiceBusy(self):
        second = self._newHarness(self.char_dev.unit)
        try:
            second.open()
            second.close()
            self.fail()
        except OSError as e:
            self.assertEqual(errno.EBUSY, e.errno)

    def test_ReadFails(self):
        try:
            os.read(self.char_dev.fileno(), 1)
            self.fail()
        except OSError as e:
            self.assertEqual(errno.EIO, e.errno)

    def test_WriteFails(self):
        try:
            os.write(self.char_dev.fileno(), '')
            self.fail()
        except OSError as e:
            self.assertEqual(errno.EIO, e.errno)


class TestTunCharDev(TestCharDev):

    def __init__(self, name):
        super(TestTunCharDev, self).__init__(name, TunCharDevHarness)

    def test_AFPrepend(self):
        self.assertFalse(self.char_dev.prependAF)

        self.char_dev.prependAF = 1
        self.assertTrue(self.char_dev.prependAF)

        self.char_dev.prependAF = 0
        self.assertFalse(self.char_dev.prependAF)


class TestTapCharDev(TestCharDev):

    def __init__(self, name):
        super(TestTapCharDev, self).__init__(name, TapCharDevHarness)
