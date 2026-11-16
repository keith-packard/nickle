#!/usr/bin/env python3
#
# Copyright © 2026 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#

"""Format Nickle sources with nickle --format, or check them with
nickle --check-format.

Nickle runs in each file's directory, so code loaded with 'load' is
found there.
"""

import argparse
import os
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--check', action='store_true',
                        help='report files formatting would change, and '
                        'exit with status 1 if there are any')
    parser.add_argument('--nickle', default='nickle',
                        help='nickle program to run')
    parser.add_argument('files', nargs='+')
    args = parser.parse_args()
    nickle = os.path.abspath(args.nickle)

    by_dir = {}
    for path in args.files:
        path = os.path.abspath(path)
        by_dir.setdefault(os.path.dirname(path), []).append(os.path.basename(path))

    failed = False
    for d, names in sorted(by_dir.items()):
        if args.check:
            cmd = [nickle]
            for name in names:
                cmd += ['--check-format', name]
            got = subprocess.run(cmd, cwd=d, capture_output=True, text=True)
            sys.stdout.write(got.stdout)
            # Name the files with their directories
            for line in got.stderr.splitlines(keepends=True):
                if line.split(':', 1)[0] in names:
                    line = os.path.join(d, line)
                sys.stderr.write(line)
            if got.returncode != 0:
                failed = True
            continue
        for name in names:
            got = subprocess.run([nickle, '--format', name], cwd=d,
                                 capture_output=True, text=True)
            if got.returncode != 0:
                sys.stderr.write(got.stderr)
                failed = True
                continue
            path = os.path.join(d, name)
            with open(path, encoding='utf-8') as f:
                text = f.read()
            if got.stdout != text:
                with open(path, 'w', encoding='utf-8') as f:
                    f.write(got.stdout)
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
