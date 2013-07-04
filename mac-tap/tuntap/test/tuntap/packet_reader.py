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
import Queue
import select
import signal
import socket
import pickle
import threading

MAX_PACKET_SIZE = 4096

def handleEAgain(fn, *args, **kwargs):
    """
    Wraps a function call in loop, restarting on EAGAIN.
    """
    while True:
        try:
            return fn(*args, **kwargs)
        except EnvironmentError as e:
            if e.errno != errno.EAGAIN:
              raise
        except:
            raise


class BlockingPacketSource(object):
    """
    In order to be able to test blocking reads and not hang forever if the expected data never
    arrives, we do the blocking read call in a forked subprocess that forwards the data read from
    the fd over a domain socket.
    """

    def __init__(self, fd):
        (self._rsock, wsock) = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        child = os.fork()
        if child != 0:
            wsock.close()
            self._child = child
            return

        self._rsock.close()

        # This is the read loop in the forked process and it won't quit until either the process
        # gets killed or there is a read error.
        try:
            while True:
                packet = handleEAgain(os.read, fd, MAX_PACKET_SIZE)
                handleEAgain(wsock.send, pickle.dumps((0, packet)))
                if len(packet) == 0:
                    break
        except KeyboardInterrupt:
            pass
        except EnvironmentError as e:
            handleEAgain(wsock.send, pickle.dumps((e.errno, '')))
        finally:
            os.close(fd)
            wsock.close()
            os._exit(os.EX_OK)

    def read(self, killpipe):
        (r, w, x) = select.select([self._rsock, killpipe], [], [])
        if killpipe in r:
            return None
        if self._rsock in r:
            try:
                return handleEAgain(self._rsock.recv, MAX_PACKET_SIZE)
            except EnvironmentError as e:
                # If there's a read error on the subprocess, it'll close the socket.
                if e.errno != errno.ECONNRESET:
                    raise e
        return None

    def stop(self):
        os.kill(self._child, signal.SIGINT)
        os.waitpid(self._child, 0)
        self._rsock.close()


class SelectPacketSource(object):
    """
    Reads data from a file descriptor, waiting for input using select().
    """

    def __init__(self, fd):
        self._fd = fd

    def read(self, killpipe):
        (r, w, x) = select.select([self._fd, killpipe], [], [])
        if killpipe in r:
            return None
        if self._fd in r:
            packet = handleEAgain(os.read, self._fd, MAX_PACKET_SIZE)
            return pickle.dumps((0, packet))
        return None

    def stop(self):
        pass

class Expectation(object):
    """
    Describes an expectation. Expectations are specified as dictionaries to match the packet
    against. Entries may specify nested dictionaries for recursive matching and callables can be
    used as predicates. Any other entry will be compared to the corresponding value in the packet.
    """

    def __init__(self, expectation, times, action):
        self._expectation = expectation
        self._times = times
        self._action = action

    @property
    def active(self):
        return self._times == None or self.pending

    @property
    def pending(self):
        return self._times != None and self._times > 0

    def check(self, packet):
        #print 'Matching %s against %s' % (packet, self._expectation)
        if self.active and Expectation._matches(packet, self._expectation):
            if self._times:
                self._times -= 1
            if callable(self._action):
                self._action(packet)
            return True
        return False

    @staticmethod
    def _matches(packet, expectation):
        if isinstance(expectation, dict):
            for (name, entry) in expectation.iteritems():
                try:
                    val = getattr(packet, name)
                except AttributeError:
                    return False
                if not Expectation._matches(val, entry):
                    return False
            return True
        elif callable(expectation):
            return expectation(packet)
        else:
            return packet == expectation


class PacketReader(object):
    """
    Takes care of reading packets and matching them against expectations.
    """

    def __init__(self, source, decode = str, skip = False):
        """
        Initializes a new reader.

        Args:
            source: packet source to read packets from.
            decode: packet decoding function.
            skip: whether non-matching packets are to be skipped.
        """
        self._source = source
        self._decode = decode
        self._skip = skip
        self._expectations = []
        self._packets = Queue.Queue()
        self._shutdownPipe = os.pipe()
        self._stop = threading.Event()

    def start(self):
        self._readThread = threading.Thread(target = self)
        self._readThread.start()

    def stop(self):
        self._stop.set()
        handleEAgain(os.write, self._shutdownPipe[1], 'stop')
        self._readThread.join()
        self._source.stop()
        os.close(self._shutdownPipe[0])
        os.close(self._shutdownPipe[1])

    def __call__(self):
        """
        Reading service function, runs in a separate thread.
        """
        try:
            while True:
                packet = handleEAgain(self._source.read, self._shutdownPipe[0])
                if not packet:
                    self._packets.put((0, ''))
                    break
                self._packets.put(pickle.loads(packet))
        except EnvironmentError as e:
            # The read() is racing against stop(), ignore these situations.
            if e.errno == EIO and self._stop.isSet():
                self._packets.put((0, ''))
            self._packets.put((e.errno, ''))
        
    def expect(self, expectation, times = 1, action = None):
        """
        Adds an expectation for a packet to be received.

        Args:
            expectation: Dictionary describing the expected packet.
            times: Number of packets expected. None for unlimited.
            action: A callback to run after the packet has been received.
        """
        assert times != 0
        self._expectations.append(Expectation(expectation, times, action))

    @property
    def expectationsPending(self):
        for e in self._expectations:
            if e.pending:
                return True
        return False

    def run(self, timeout = 1):
        """
        Runs the packet reader, waiting for all limited expectations to be met.

        Args:
            timeout: Wait timeout in seconds.
        """
        while self.expectationsPending:
            try: 
                (code, payload) = self._packets.get(True, timeout)
            except Queue.Empty:
                # No packet received.
                break

            if code != 0:
                # read error, re-raise.
                raise OSError((code, os.strerror(code)))

            if len(payload) == 0:
                # EOF on read.
                break

            # decode the packet and match it against expectation.
            matches = False
            for e in self._expectations:
                if e.check(self._decode(payload)):
                    matches = True
                    break
            if not matches and not self._skip:
                return False

        return not self.expectationsPending

