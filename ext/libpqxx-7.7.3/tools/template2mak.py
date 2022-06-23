#! /usr/bin/env python3
"""Minimal macro processor.  Used for generating VC++ makefiles.

The available template commands are:

    Expand a template section for each file in a list of file patterns::
        ###MAKTEMPLATE:FOREACH my/path*/*.cxx,other*.cxx
        ...
        ###MAKTEMPLATE:ENDFOREACH

    In the template section, you can use `###BASENAME###` to get the base name
    of the file being processed (e.g. "base" for "../base.cxx"), and you can
    use `###FILENAME###` to get the full filename.


Copyright (c) 2000-2022, Bart Samwel and Jeroen T. Vermeulen.
"""

from __future__ import (
    absolute_import,
    print_function,
    unicode_literals,
    )

from argparse import (
    ArgumentError,
    ArgumentParser,
    RawDescriptionHelpFormatter,
    )
from contextlib import contextmanager
from glob import glob
import os
from sys import (
    argv,
    stdin,
    stderr,
    stdout,
    )
import sys
from textwrap import dedent


def expand_foreach_file(path, block, outfile):
    """Expand a "foreach" block for a single file path.

    Write the results to outfile.
    """
    basepath, _ = os.path.splitext(os.path.basename(path))
    for line in block:
        line = line.replace("###FILENAME###", path)
        line = line.replace("###BASENAME###", basepath)
        outfile.write(line)


def match_globs(globs):
    """List all files matching any item in globs.

    Eliminates duplicates.
    """
    return sorted({
        path
        for pattern in globs
        for path in glob(pattern)
        })


def expand_foreach(globs, block, outfile):
    """Expand a foreach block for each file matching one of globs.

    Write the results to outfile.
    """
    # We'll be iterating over block a variable number of times.  Turn it
    # from a generic iterable into an immutable array.
    block = tuple(block)
    for path in match_globs(globs):
        expand_foreach_file(path, block, outfile)


# Header to be prefixed to the generated file.
OUTPUT_HEADER = dedent("""\
    # AUTOMATICALLY GENERATED FILE -- DO NOT EDIT.
    #
    # This file is generated automatically by libpqxx's {script} script, and
    # will be rewritten from time to time.
    #
    # If you modify this file, chances are your modifications will be lost.
    #
    # The {script} script should be available in the tools directory of the
    # libpqxx source archive.
    """)


foreach_marker = r"###MAKTEMPLATE:FOREACH "
end_foreach_marker = r"###MAKTEMPLATE:ENDFOREACH"


def parse_foreach(line):
    """Parse FOREACH directive, if line contains one.

    :param line: One line of template input.
    :return: A list of FOREACH globs, or None if this was not a FOREACH line.
    """
    line = line.strip()
    if line.startswith(foreach_marker):
        return line[len(foreach_marker):].split(',')
    else:
        return None


def read_foreach_block(infile):
    """Read a FOREACH block from infile (not including the FOREACH directive).

    Assumes that the FOREACH directive was in the preceding line.  Consumes
    the line with the ENDFOREACH directive, but does not yield it.

    :return: Iterable of lines.
    """
    for line in infile:
        if line.strip().startswith(end_foreach_marker):
            return
        yield line


def expand_template(infile, outfile):
    """Expand the template in infile, and write the results to outfile."""
    for line in infile:
        globs = parse_foreach(line)
        if globs is None:
            # Not a FOREACH line.  Copy to output.
            outfile.write(line)
        else:
            block = read_foreach_block(infile)
            expand_foreach(globs, block, outfile)


@contextmanager
def open_stream(path=None, default=None, mode='r'):
    """Open file at given path, or yield default.  Close as appropriate.

    The default should be a stream, not a path; closing the context will not
    close it.
    """
    if path is None:
        yield default
    else:
        with open(path, mode) as stream:
            yield stream


def parse_args():
    """Parse command-line arguments.

    :return: Tuple of: input path (or None for stdin), output path (or None
        for stdout).
    """
    parser = ArgumentParser(
        description=__doc__, formatter_class=RawDescriptionHelpFormatter)

    parser.add_argument(
        'template', nargs='?',
        help="Input template.  Defaults to standard input.")
    parser.add_argument(
        'output', nargs='?',
        help="Output file.  Defaults to standard output.")

    args = parser.parse_args()
    return args.template, args.output


def write_header(stream, template_path=None):
    """Write header to stream."""
    hr = ('# ' + '#' * 78) + "\n"
    script = os.path.basename(argv[0])

    outstream.write(hr)
    outstream.write(OUTPUT_HEADER.format(script=script))
    if template_path is not None:
        outstream.write("#\n")
        outstream.write("# Generated from template '%s'.\n" % template_path)
    outstream.write(hr)


if __name__ == '__main__':
    try:
        template_path, output_path = parse_args()
    except ArgumentError as error:
        stderr.write('%s\n' % error)
        sys.exit(2)

    input_stream = open_stream(template_path, stdin, 'r')
    output_stream = open_stream(output_path, stdout, 'w')
    with input_stream as instream, output_stream as outstream:
        write_header(outstream, template_path)
        expand_template(instream, outstream)
