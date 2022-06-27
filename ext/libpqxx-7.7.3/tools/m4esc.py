#! /usr/bin/env python3

"""M4-quote text, for use as a literal in configure.ac.

Produces M4 "code" which evaluates to the input text.

It's not easy to read plain text from an input file in M4, without having it
expanded as M4.  Sometimes all we want is literal text!
"""
from __future__ import (
    absolute_import,
    print_function,
    unicode_literals,
    )

from argparse import ArgumentParser
from sys import (
    stdin,
    stdout,
    )


def parse_args():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument(
        '--open', '-a', default='[[', help="Current open-quote symbol.")
    parser.add_argument(
        '--close', '-b', default=']]', help="Current close-quote symbol.")
    parser.add_argument(
        '--input', '-i', default='-', help="Input file, or '-' for stdin.")
    parser.add_argument(
        '--output', '-o', default='-', help="Output file, or '-' for stdout.")
    return parser.parse_args()


def open_input(in_file):
    if in_file == '-':
        return stdin
    else:
        return open(in_file)


def open_output(out_file):
    if out_file == '-':
        return stdout
    else:
        return open(out_file, 'w')


def escape(line):
    return (
        line
        .replace('[', '@<:@')
        .replace(']', '@:>@')
        .replace('#', '@%:@')
        .replace('$', '@S|@')
    )


def main(args):
    with open_input(args.input) as istr, open_output(args.output) as ostr:
        ostr.write(args.open)
        for line in istr:
            ostr.write(escape(line))
            ostr.write('\n')
        ostr.write(args.close)


if __name__ == '__main__':
    main(parse_args())
