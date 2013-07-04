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
import fcntl
import io
import os
import struct
from tuntap import ioctl

class CharDevHarness(object):
    """
    Base class for the tun and tap character device harnesses. Manages a single character
    interface, keeps the file descriptor and handles I/O.
    """

    _MAX_CHAR_DEV = 16
    _MAX_PACKET_SIZE = 4096

    def __init__(self, class_name, unit = None):
        """
        Initializes the harness.

        Args:
            class_name: Path name pattern.
            unit: The character device number.
        """
        self._class_name = class_name
        self._unit = unit
        self._dev = None

    def _openCharDev(self, unit):
        """
        Opens the character device.

        Args:
            unit: The character device number.
        """
        assert not self._dev

        name = self._class_name % unit
        self._dev = os.open(name, os.O_RDWR)

    def open(self):
        """
        Opens the character device.
        """
        if self._unit != None:
            self._openCharDev(self._unit)
            return

        # Try to open character devices in turn.
        for i in xrange(0, self._MAX_CHAR_DEV):
            try:
                self._openCharDev(i)
                self._unit = i
                return
            except OSError as e:
                if e.errno != errno.EBUSY:
                    raise e

        # All devices busy.
        raise OSError(errno.EBUSY)

    def close(self):
        """
        Closes the character device.
        """
        assert self._dev
        os.close(self._dev)
        self._dev = None

    def fileno(self):
        assert self._dev
        return self._dev

    def send(self, packet):
        assert self._dev
        os.write(self._dev, packet)

    def ioctl(self, cmd, format, arg):
        """
        Performs an ioctl on the character device.

        Args:
            cmd: the ioctl cmd identifier.
            format: argument format.
            arg: argument data tuple.

        Returns:
            Output argument tuple.
        """
        assert self._dev
        return struct.unpack(format, fcntl.ioctl(self._dev, cmd, struct.pack(format, arg)))

    @property
    def unit(self):
        """
        Returns the interface unit, if known.
        """
        return self._unit


class TunCharDevHarness(CharDevHarness):
    """
    Character device harness for tun devices.
    """

    TUNSIFHEAD = ioctl.IOC(ioctl.OUT, 't', 96, 'i')
    TUNGIFHEAD = ioctl.IOC(ioctl.IN, 't', 97, 'i')

    def __init__(self, unit = None):
        """
        Initializes the harness.

        Args:
            unit: Character device index
        """
        super(TunCharDevHarness, self).__init__('/dev/tun%d', unit = unit)

    @property
    def prependAF(self):
        """
        Gets the AF prepending flag.

        Returns:
            A flag indicating whether packets on the char dev are prefixed with the AF number.
        """
        return self.ioctl(self.TUNGIFHEAD, 'i', (0))[0]

    @prependAF.setter
    def prependAF(self, prependAF):
        """
        Sets the AF prepending flag.

        Args:
            prependAF: whether the packets on the char dev are prefixed with the AF number.
        """
        self.ioctl(self.TUNSIFHEAD, 'i', (prependAF))


class TapCharDevHarness(CharDevHarness):
    """
    Character device harness for tap devices.
    """

    def __init__(self, unit = None):
        """
        Initializes the harness.

        Args:
            unit: Character device index
        """
        super(TapCharDevHarness, self).__init__('/dev/tap%d', unit = unit)
