#! /usr/bin/env python3
"""Brute-force test script: test libpqxx against many compilers etc.

This script makes no changes in the source tree; all builds happen in
temporary directories.

To make this possible, you may need to run "make distclean" in the
source tree.  The configure script will refuse to configure otherwise.
"""

# Without this, pocketlint does not yet understand the print function.
from __future__ import print_function

from abc import (
    ABCMeta,
    abstractmethod,
    )
from argparse import ArgumentParser
from contextlib import contextmanager
from datetime import datetime
from functools import partial
import json
from multiprocessing import (
    JoinableQueue,
    Process,
    Queue,
    )
from multiprocessing.pool import (
    Pool,
    )
from os import (
    cpu_count,
    getcwd,
    )
import os.path
from queue import Empty
from shutil import rmtree
from subprocess import (
    CalledProcessError,
    check_call,
    check_output,
    DEVNULL,
    )
from sys import (
    stderr,
    stdout,
    )
from tempfile import mkdtemp
from textwrap import dedent


CPUS = cpu_count()

GCC_VERSIONS = list(range(8, 14))
GCC = ['g++-%d' % ver for ver in GCC_VERSIONS]
CLANG_VERSIONS = list(range(7, 15))
CLANG = ['clang++-6.0'] + ['clang++-%d' % ver for ver in CLANG_VERSIONS]
CXX = GCC + CLANG

STDLIB = (
    '',
    '-stdlib=libc++',
    )

OPT = ('-O0', '-O3')

LINK = {
    'static': ['--enable-static', '--disable-shared'],
    'dynamic': ['--disable-static', '--enable-shared'],
}

DEBUG = {
    'plain': [],
    'audit': ['--enable-audit'],
    'maintainer': ['--enable-maintainer-mode'],
    'full': ['--enable-audit', '--enable-maintainer-mode'],
}


# CMake "generators."  Maps a value for cmake's -G option to a command line to
# run.
#
# I prefer Ninja if available, because it's fast.  But hey, the default will
# work.
#
# Maps the name of the generator (as used with cmake's -G option) to the
# actual command line needed to do the build.
CMAKE_GENERATORS = {
    'Ninja': ['ninja'],
    'Unix Makefiles': ['make', '-j%d' % CPUS],
}


class Fail(Exception):
    """A known, well-handled exception.  Doesn't need a traceback."""


class Skip(Exception):
    """"We're not doing this build.  It's not an error though."""


def run(cmd, output, cwd=None):
    """Run a command, write output to file-like object."""
    command_line = ' '.join(cmd)
    output.write("%s\n\n" % command_line)
    check_call(cmd, stdout=output, stderr=output, cwd=cwd)


def report(output, message):
    """Report a message to output, and standard output."""
    print(message, flush=True)
    output.write('\n\n')
    output.write(message)
    output.write('\n')


def file_contains(path, text):
    """Does the file at path contain text?"""
    with open(path) as stream:
        for line in stream:
            if text in line:
                return True
    return False


@contextmanager
def tmp_dir():
    """Create a temporary directory, and clean it up again."""
    tmp = mkdtemp()
    try:
        yield tmp
    finally:
        rmtree(tmp)


def write_check_code(work_dir):
    """Write a simple C++ program so we can tesst whether we can compile it.

    Returns the file's full path.
    """
    path = os.path.join(work_dir, "check.cxx")
    with open(path, 'w') as source:
        source.write(dedent("""\
            #include <iostream>
            int main()
            {
                std::cout << "Hello world." << std::endl;
            }
            """))

    return path


def check_compiler(work_dir, cxx, stdlib, check, verbose=False):
    """Is the given compiler combo available?"""
    err_file = os.path.join(work_dir, 'stderr.log')
    if verbose:
        err_output = open(err_file, 'w')
    else:
        err_output = DEVNULL
    try:
        command = [cxx, check]
        if stdlib != '':
            command.append(stdlib)
        check_call(command, cwd=work_dir, stderr=err_output)
    except (OSError, CalledProcessError):
        if verbose:
            with open(err_file) as errors:
                stdout.write(errors.read())
            print("Can't build with '%s %s'.  Skipping." % (cxx, stdlib))
        return False
    else:
        return True


# TODO: Use Pool.
def check_compilers(compilers, stdlibs, verbose=False):
    """Check which compiler configurations are viable."""
    with tmp_dir() as work_dir:
        check = write_check_code(work_dir)
        return [
            (cxx, stdlib)
            for stdlib in stdlibs
            for cxx in compilers
            if check_compiler(
                work_dir, cxx, stdlib, check=check, verbose=verbose)
        ]


def find_cmake_command():
    """Figure out a CMake generator we can use, or None."""
    try:
        caps = check_output(['cmake', '-E', 'capabilities'])
    except FileNotFoundError:
        return None

    names = {generator['name'] for generator in json.loads(caps)['generators']}
    for gen in CMAKE_GENERATORS.keys():
        if gen in names:
            return gen
    return None


class Config:
    """Configuration for a build.

    These classes must be suitable for pickling, so we can send its objects to
    worker processes.
    """
    __metaclass__ = ABCMeta

    @abstractmethod
    def name(self):
        """Return an identifier for this build configuration."""

    def make_log_name(self):
        """Compose log file name for this build."""
        return "build-%s.out" % self.name()


class Build:
    """A pending or ondoing build, in its own directory.

    Each step returns True for Success, or False for failure.

    These classes must be suitable for pickling, so we can send its objects to
    worker processes.
    """
    def __init__(self, logs_dir, config=None):
        self.config = config
        self.log = os.path.join(logs_dir, config.make_log_name())
        # Start a fresh log file.
        with open(self.log, 'w') as log:
            log.write("Starting %s.\n" % datetime.utcnow())
        self.work_dir = mkdtemp()

    def clean_up(self):
        """Delete the build tree."""
        rmtree(self.work_dir)

    @abstractmethod
    def configure(self, log):
        """Prepare for a build."""

    @abstractmethod
    def build(self, log):
        """Build the code, including the tests.  Don't run tests though."""

    def test(self, log):
        """Run tests."""
        run(
            [os.path.join(os.path.curdir, 'test', 'runner')], log,
            cwd=self.work_dir)

    def logging(self, function):
        """Call function, pass open write handle for `self.log`."""
# TODO: Should probably be a decorator.
        with open(self.log, 'a') as log:
            try:
                function(log)
            except Exception as error:
                log.write("%s\n" % error)
                raise

    def do_configure(self):
        """Call `configure`, writing output to `self.log`."""
        self.logging(self.configure)

    def do_build(self):
        """Call `build`, writing output to `self.log`."""
        self.logging(self.build)

    def do_test(self):
        """Call `test`, writing output to `self.log`."""
        self.logging(self.test)


class AutotoolsConfig(Config):
    """A combination of build options for the "configure" script."""
    def __init__(self, cxx, opt, stdlib, link, link_opts, debug, debug_opts):
        self.cxx = cxx
        self.opt = opt
        self.stdlib = stdlib
        self.link = link
        self.link_opts = link_opts
        self.debug = debug
        self.debug_opts = debug_opts

    def name(self):
        return '_'.join([
            self.cxx, self.opt, self.stdlib, self.link, self.debug])


class AutotoolsBuild(Build):
    """Build using the "configure" script."""
    __metaclass__ = ABCMeta

    def configure(self, log):
        configure = [
            os.path.join(getcwd(), "configure"),
            "CXX=%s" % self.config.cxx,
            ]

        if self.config.stdlib == '':
            configure += [
                "CXXFLAGS=%s" % self.config.opt,
            ]
        else:
            configure += [
                "CXXFLAGS=%s %s" % (self.config.opt, self.config.stdlib),
                "LDFLAGS=%s" % self.config.stdlib,
                ]

        configure += [
            "--disable-documentation",
            ] + self.config.link_opts + self.config.debug_opts

        run(configure, log, cwd=self.work_dir)

    def build(self, log):
        run(['make', '-j%d' % CPUS], log, cwd=self.work_dir)
        # Passing "TESTS=" like this will suppress the actual running of
        # the tests.  We run them in the "test" stage.
        run(['make', '-j%d' % CPUS, 'check', 'TESTS='], log, cwd=self.work_dir)


class CMakeConfig(Config):
    """Configuration for a CMake build."""
    def __init__(self, generator):
        self.generator = generator
        self.builder = CMAKE_GENERATORS[generator]

    def name(self):
        return "cmake"


class CMakeBuild(Build):
    """Build using CMake.

    Ignores the config for now.
    """
    __metaclass__ = ABCMeta

    def configure(self, log):
        source_dir = getcwd()
        generator = self.config.generator
        run(
            ['cmake', '-G', generator, source_dir], output=log,
            cwd=self.work_dir)

    def build(self, log):
        run(self.config.builder, log, cwd=self.work_dir)


def parse_args():
    """Parse command-line arguments."""
    parser = ArgumentParser(description=__doc__)
    parser.add_argument('--verbose', '-v', action='store_true')
    parser.add_argument(
        '--compilers', '-c', default=','.join(CXX),
        help="Compilers, separated by commas.  Default is %(default)s.")
    parser.add_argument(
        '--optimize', '-O', default=','.join(OPT),
        help=(
            "Alternative optimisation options, separated by commas.  "
            "Default is %(default)s."))
    parser.add_argument(
        '--stdlibs', '-L', default=','.join(STDLIB),
        help=(
            "Comma-separated options for choosing standard library.  "
            "Defaults to %(default)s."))
    parser.add_argument(
        '--logs', '-l', default='.', metavar='DIRECTORY',
        help="Write build logs to DIRECTORY.")
    parser.add_argument(
        '--jobs', '-j', default=CPUS, metavar='CPUS',
        help=(
            "When running 'make', run up to CPUS concurrent processes.  "
            "Defaults to %(default)s."))
    parser.add_argument(
        '--minimal', '-m', action='store_true',
        help="Make it as short a run as possible.  For testing this script.")
    return parser.parse_args()


def soft_get(queue, block=True):
    """Get an item off `queue`, or `None` if the queue is empty."""
    try:
        return queue.get(block)
    except Empty:
        return None


def read_queue(queue, block=True):
    """Read entries off `queue`, terminating when it gets a `None`.

    Also terminates when the queue is empty.
    """
    entry = soft_get(queue, block)
    while entry is not None:
        yield entry
        entry = soft_get(queue, block)


def service_builds(in_queue, fail_queue, out_queue):
    """Worker process for "build" stage: process one job at a time.

    Sends successful builds to `out_queue`, and failed builds to `fail_queue`.

    Terminates when it receives a `None`, at which point it will send a `None`
    into `out_queue` in turn.
    """
    for build in read_queue(in_queue):
        try:
            build.do_build()
        except Exception as error:
            fail_queue.put((build, "%s" % error))
        else:
            out_queue.put(build)
        in_queue.task_done()

    # Mark the end of the queue.
    out_queue.put(None)


def service_tests(in_queue, fail_queue, out_queue):
    """Worker process for "test" stage: test one build at a time.

    Sends successful builds to `out_queue`, and failed builds to `fail_queue`.

    Terminates when it receives a final `None`.  Does not send out a final
    `None` of its own.
    """
    for build in read_queue(in_queue):
        try:
            build.do_test()
        except Exception as error:
            fail_queue.put((build, "%s" % error))
        else:
            out_queue.put(build)
        in_queue.task_done()


def report_failures(queue, message):
    """Report failures from a failure queue.  Return total number."""
    failures = 0
    for build, error in read_queue(queue, block=False):
        print("%s: %s - %s" % (message, build.config.name(), error))
        failures += 1
    return failures


def count_entries(queue):
    """Get and discard all entries from `queue`, return the total count."""
    total = 0
    for _ in read_queue(queue, block=False):
        total += 1
    return total


def gather_builds(args):
    """Produce the list of builds we want to perform."""
    if args.verbose:
        print("\nChecking available compilers.")

    compiler_candidates = args.compilers.split(',')
    compilers = check_compilers(
        compiler_candidates, args.stdlibs.split(','),
        verbose=args.verbose)
    if list(compilers) == []:
        raise Fail(
            "Did not find any viable compilers.  Tried: %s."
            % ', '.join(compiler_candidates))

    opt_levels = args.optimize.split(',')
    link_types = LINK.items()
    debug_mixes = DEBUG.items()

    if args.minimal:
        compilers = compilers[:1]
        opt_levels = opt_levels[:1]
        link_types = list(link_types)[:1]
        debug_mixes = list(debug_mixes)[:1]

    builds = [
        AutotoolsBuild(
            args.logs,
            AutotoolsConfig(
                opt=opt, link=link, link_opts=link_opts, debug=debug,
                debug_opts=debug_opts, cxx=cxx, stdlib=stdlib))
        for opt in sorted(opt_levels)
        for link, link_opts in sorted(link_types)
        for debug, debug_opts in sorted(debug_mixes)
        for cxx, stdlib in compilers
    ]

    cmake = find_cmake_command()
    if cmake is not None:
        builds.append(CMakeBuild(args.logs, CMakeConfig(cmake)))
    return builds


def enqueue(queue, build, *args):
    """Put `build` on `queue`.

    Ignores additional arguments, so that it can be used as a clalback for
    `Pool`.

    We do this instead of a lambda in order to get the closure right.  We want
    the build for the current iteration, not the last one that was executed
    before the lambda runs.
    """
    queue.put(build)


def enqueue_error(queue, build, error):
    """Put the pair of `build` and `error` on `queue`."""
    queue.put((build, error))


def main(args):
    """Do it all."""
    if not os.path.isdir(args.logs):
        raise Fail("Logs location '%s' is not a directory." % args.logs)

    builds = gather_builds(args)
    if args.verbose:
        print("Lined up %d builds." % len(builds))

    # The "configure" step is single-threaded.  We can run many at the same
    # time, even when we're also running a "build" step at the same time.
    # This means we may run a lot more processes than we have CPUs, but there's
    # no law against that.  There's also I/O time to be covered.
    configure_pool = Pool()

    # Builds which have failed the "configure" stage, with their errors.  This
    # queue must never stall, so that we can let results pile up here while the
    # work continues.
    configure_fails = Queue(len(builds))

    # Waiting list for the "build" stage.  It contains Build objects,
    # terminated by a final None to signify that there are no more builds to be
    # done.
    build_queue = JoinableQueue(10)

    # Builds that have failed the "build" stage.
    build_fails = Queue(len(builds))

    # Waiting list for the "test" stage.  It contains Build objects, terminated
    # by a final None.
    test_queue = JoinableQueue(10)

    # The "build" step tries to utilise all CPUs, and it may use a fair bit of
    # memory.  Run only one of these at a time, in a single worker process.
    build_worker = Process(
        target=service_builds, args=(build_queue, build_fails, test_queue))
    build_worker.start()

    # Builds that have failed the "test" stage.
    test_fails = Queue(len(builds))

    # Completed builds.  This must never stall.
    done_queue = JoinableQueue(len(builds))

    # The "test" step can not run concurrently (yet).  So, run tests serially
    # in a single worker process.  It takes its jobs directly from the "build"
    # worker.
    test_worker = Process(
        target=service_tests, args=(test_queue, test_fails, done_queue))
    test_worker.start()

    # Feed all builds into the "configure" pool.  Each build which passes this
    # stage goes into the "build" queue.
    for build in builds:
        configure_pool.apply_async(
            build.do_configure, callback=partial(enqueue, build_queue, build),
            error_callback=partial(enqueue_error, configure_fails, build))
    if args.verbose:
        print("All jobs are underway.")
    configure_pool.close()
    configure_pool.join()

# TODO: Async reporting for faster feedback.
    configure_fail_count = report_failures(configure_fails, "CONFIGURE FAIL")
    if args.verbose:
        print("Configure stage done.")

    # Mark the end of the build queue for the build worker.
    build_queue.put(None)

    build_worker.join()
# TODO: Async reporting for faster feedback.
    build_fail_count = report_failures(build_fails, "BUILD FAIL")
    if args.verbose:
        print("Build step done.")

    # Mark the end of the test queue for the test worker.
    test_queue.put(None)

    test_worker.join()
# TODO: Async reporting for faster feedback.
# TODO: Collate failures into meaningful output, e.g. "shared library fails."
    test_fail_count = report_failures(test_fails, "TEST FAIL")
    if args.verbose:
        print("Test step done.")

    # All done.  Clean up.
    for build in builds:
        build.clean_up()

    ok_count = count_entries(done_queue)
    if ok_count == len(builds):
        print("All tests OK.")
    else:
        print(
            "Failures during configure: %d - build: %d - test: %d.  OK: %d."
            % (
                configure_fail_count,
                build_fail_count,
                test_fail_count,
                ok_count,
            ))


if __name__ == '__main__':
    try:
        exit(main(parse_args()))
    except Fail as failure:
        stderr.write("%s\n" % failure)
        exit(2)
