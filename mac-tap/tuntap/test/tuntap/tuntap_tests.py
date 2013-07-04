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

import argparse
import itertools
import re
import sys
import unittest

from tuntap.packet_codec import TunPacketCodec, TunAFPacketCodec, TapPacketCodec
from tuntap.packet_reader import BlockingPacketSource, SelectPacketSource

from tuntap.test_char_dev import TestTunCharDev, TestTapCharDev
from tuntap.test_interface import TestTunInterface, TestTapInterface
from tuntap.test_ip import TestIp, TestIp6, TestMulticast, TestMulticast6, TestTapLladdr

class FilteringTestSuite(unittest.TestSuite):

    def __init__(self, filter):
        super(FilteringTestSuite, self).__init__()
        self._matcher = re.compile(filter or '.*')

    def __iter__(self):
        return itertools.ifilter(lambda test : self._matcher.search(str(test)),
                                 super(FilteringTestSuite, self).__iter__())

def loadTestsFromTestCase(testCaseClass, *args, **kwargs):
    testCaseNames = unittest.getTestCaseNames(testCaseClass, 'test_')
    return unittest.TestSuite(map(lambda n : testCaseClass(n, *args, **kwargs), testCaseNames))

def main(argv):
    # Parse the command line.
    parser = argparse.ArgumentParser(description = 'Run tuntap unit tests.')
    parser.add_argument('--tests', type = str, nargs = '?', default = None,
                        help = 'tests to run')
    parser.add_argument('--verbosity', type = int, nargs = '?', default = 2,
                        help = 'verbosity level')
    options = parser.parse_args(argv[1:])

    # Gather tests and run them.
    loader = unittest.TestLoader()
    suite = FilteringTestSuite(options.tests)
    suite.addTests(loadTestsFromTestCase(TestTunCharDev))
    suite.addTests(loadTestsFromTestCase(TestTapCharDev))
    suite.addTests(loadTestsFromTestCase(TestTunInterface))
    suite.addTests(loadTestsFromTestCase(TestTapInterface))

    codecs = (TunPacketCodec, TunAFPacketCodec, TapPacketCodec)
    sources = (SelectPacketSource, BlockingPacketSource)
    tests = (TestIp, TestIp6, TestMulticast, TestMulticast6)
    for (test, codec, source) in [ (test, codec, source) for test in tests
                                                         for codec in codecs
                                                         for source in sources ]:
        suite.addTests(loadTestsFromTestCase(test, lambda af, addr: codec(af, addr, source)))

    suite.addTests(loadTestsFromTestCase(TestTapLladdr))

    runner = unittest.TextTestRunner(stream = sys.stderr,
                                     descriptions = True,
                                     verbosity = options.verbosity)
    runner.run(suite)

if __name__ == '__main__':
    main(sys.argv)
