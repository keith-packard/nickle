#!/usr/bin/env python3
# Copyright © 2026 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.

'''
Generate Ctype's Unicode tables from the Unicode Character Database:

    ctype-class.h	runs of code points sharing a set of classes
    ctype-case.h	runs of code points sharing a case mapping

The class definitions follow picolibc's mkctype.py. Usage:

    mkunicode.py [--ucd DIR] OUTDIR

DIR defaults to /usr/share/unicode, as installed by Debian's
unicode-data package.
'''

import argparse
import copy
import os
import re
import sys

CHAR_MAX = 0x10FFFF


class CodePoint:
    def __init__(self, line):
        fields = line.split(';')
        self.code = int(fields[0], 16)
        self.name = fields[1]
        self.category = fields[2]
        self.decomposition = fields[5]
        self.uppercase = self.case_value(fields[12])
        self.lowercase = self.case_value(fields[13])

    def case_value(self, field):
        if field:
            return int(field, 16)
        return self.code

    def new_code(self, code):
        new = copy.copy(self)
        new.code = code
        new.uppercase = code
        new.lowercase = code
        return new


def load_unicode_data(name):
    '''Map each assigned code point but the surrogates to its CodePoint'''
    ret = {}
    first = None
    with open(name) as f:
        for line in f:
            cp = CodePoint(line.strip())
            if cp.category == 'Cs':
                continue
            match = re.fullmatch('<([^,]*), ([^>]*)>', cp.name)
            if match:
                if match.group(2) == 'First':
                    first = cp
                    first.name = match.group(1)
                elif match.group(2) == 'Last' and first:
                    for code in range(first.code, cp.code + 1):
                        ret[code] = first.new_code(code)
            else:
                ret[cp.code] = cp
    return ret


def load_derived_properties(name):
    '''Map code points to their derived core properties'''
    ret = {}
    with open(name) as f:
        for line in f:
            line = line.split('#')[0].strip()
            if not line:
                continue
            fields = [field.strip() for field in line.split(';')]
            codes = fields[0].split('..')
            first = int(codes[0], 16)
            last = int(codes[-1], 16)
            for code in range(first, last + 1):
                ret.setdefault(code, set()).add(fields[1])
    return ret


def load_version(name):
    with open(name) as f:
        for line in f:
            m = re.match(r'.*Version ([1-9][0-9.]*) ', line)
            if m:
                return m.group(1)
    return 'unknown'


def to_upper(code):
    if code in CodePoints:
        return CodePoints[code].uppercase
    return code


def to_lower(code):
    if code in CodePoints:
        return CodePoints[code].lowercase
    return code


def properties(code):
    return Properties.get(code, ())


def is_digit(code):
    # C says only the ASCII digits need apply
    return 0x30 <= code <= 0x39


def is_xdigit(code):
    return is_digit(code) or 0x41 <= code <= 0x46 or 0x61 <= code <= 0x66


def is_alpha(code):
    if 'Alphabetic' in properties(code):
        return True
    cp = CodePoints.get(code)
    return cp is not None and cp.category == 'Nd' and not is_digit(code)


def is_alnum(code):
    return is_alpha(code) or is_digit(code)


def is_space_separator(cp):
    # Space separators, but not no-break spaces
    return cp.category == 'Zs' and '<noBreak>' not in cp.decomposition


def is_blank(code):
    if code == 0x09:
        return True
    cp = CodePoints.get(code)
    return cp is not None and is_space_separator(cp)


def is_space(code):
    if code in (0x20, 0x0c, 0x0a, 0x0d, 0x09, 0x0b):
        return True
    cp = CodePoints.get(code)
    if cp is None:
        return False
    return cp.category in ('Zl', 'Zp') or is_space_separator(cp)


def is_cntrl(code):
    cp = CodePoints.get(code)
    if cp is None:
        return False
    return cp.name == '<control>' or cp.category in ('Zl', 'Zp')


def is_graph(code):
    cp = CodePoints.get(code)
    if cp is None or cp.name == '<control>':
        return False
    return not is_space(code)


def is_print(code):
    cp = CodePoints.get(code)
    if cp is None or cp.name == '<control>':
        return False
    return cp.category not in ('Zl', 'Zp')


def is_punct(code):
    return is_graph(code) and not is_alpha(code) and not is_digit(code)


def is_lower(code):
    return to_upper(code) != code or 'Lowercase' in properties(code)


def is_upper(code):
    return to_lower(code) != code or 'Uppercase' in properties(code)


def classes(code):
    '''
    Return the classes of 'code'. A letter with a case mapping gets
    'case' in place of 'lower' or 'upper', which the lookup resolves
    from the case table, so that runs of alternating case share one
    class run
    '''
    ret = ()
    for name, test in (('alnum', is_alnum), ('alpha', is_alpha),
                       ('blank', is_blank), ('cntrl', is_cntrl),
                       ('digit', is_digit), ('graph', is_graph),
                       ('print', is_print), ('punct', is_punct),
                       ('space', is_space), ('xdigit', is_xdigit)):
        if test(code):
            ret += (name,)
    if is_lower(code):
        if to_upper(code) != code:
            ret += ('case',)
        else:
            ret += ('lower',)
    if is_upper(code):
        if to_lower(code) != code:
            if 'case' not in ret:
                ret += ('case',)
        else:
            ret += ('upper',)
    return ret


def class_runs():
    '''Return (first, classes) for each run of code points sharing classes'''
    runs = []
    prev = None
    for code in range(CHAR_MAX + 1):
        c = classes(code)
        if c != prev:
            runs.append((code, c))
            prev = c
    return runs


def case_kind(code):
    '''Return ('lo', delta), ('up', delta), ('both', 0), or None'''
    up, lo = to_upper(code), to_lower(code)
    if up != code and lo != code:
        if lo != code + 1 or up != code - 1:
            sys.exit(f'U+{code:04X}: unexpected title case mapping')
        return ('both', 0)
    if lo != code:
        return ('lo', lo - code)
    if up != code:
        return ('up', up - code)
    return None


def case_runs():
    '''
    Return (first, last, mode, delta) for each run of code points
    sharing a case mapping. In a TO1 run, capitals and small letters
    alternate, the capitals on even codes for EVENCAP
    '''
    codes = sorted(c for c in CodePoints if case_kind(c))
    runs = []
    i = 0
    while i < len(codes):
        first = codes[i]
        kind = case_kind(first)
        # Alternating capital, small, capital, ...
        n = 0
        while (i + n < len(codes) and codes[i + n] == first + n and
               case_kind(first + n) == (('lo', 1), ('up', -1))[n % 2]):
            n += 1
        if n >= 2:
            parity = ('EVENCAP', 'ODDCAP')[first & 1]
            runs.append((first, first + n - 1, 'TO1', parity))
            i += n
            continue
        n = 1
        while (i + n < len(codes) and codes[i + n] == first + n and
               case_kind(first + n) == kind and kind[0] != 'both'):
            n += 1
        mode = {'lo': 'TOLO', 'up': 'TOUP', 'both': 'TOBOTH'}[kind[0]]
        runs.append((first, first + n - 1, mode, kind[1]))
        i += n
    return runs


def check(cruns, kruns):
    '''Check that looking up every code point in the tables, as
    builtin-ctype.c does, gives the data's classes and mappings'''
    def find(runs, code, key):
        lo, hi = 0, len(runs) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if key(runs[mid]) <= code:
                lo = mid
            else:
                hi = mid - 1
        return runs[lo]

    for code in range(CHAR_MAX + 1):
        want = set(classes(code))
        upper, lower = to_upper(code), to_lower(code)
        got = set(find(cruns, code, lambda r: r[0])[1])
        conv = find(kruns, code, lambda r: r[0])
        if not conv[0] <= code <= conv[1]:
            conv = None
        if 'case' in got:
            if conv is None:
                sys.exit(f'U+{code:04X}: case class without a case run')
            got.discard('case')
            mode, delta = conv[2], conv[3]
            if mode == 'TO1':
                capital = (code & 1) == (delta == 'ODDCAP')
                got.add(('lower', 'upper')[capital])
            elif mode == 'TOLO':
                got.add('upper')
            elif mode == 'TOUP':
                got.add('lower')
            else:
                got |= {'upper', 'lower'}
            want.discard('case')
            if is_lower(code):
                want.add('lower')
            if is_upper(code):
                want.add('upper')
        if got != want:
            sys.exit(f'U+{code:04X}: classes {got} want {want}')
        got_upper = got_lower = code
        if conv:
            mode, delta = conv[2], conv[3]
            if mode == 'TO1':
                if (code & 1) == (delta == 'ODDCAP'):
                    got_lower = code + 1
                else:
                    got_upper = code - 1
            elif mode == 'TOLO':
                got_lower = code + delta
            elif mode == 'TOUP':
                got_upper = code + delta
            else:
                got_lower, got_upper = code + 1, code - 1
        if (got_upper, got_lower) != (upper, lower):
            sys.exit(f'U+{code:04X}: case mapping')


HEADER = '''/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/* Generated by mkunicode.py from Unicode {version}; do not edit */
/* clang-format off */
'''


def class_name(c):
    if not c:
        return 'CTYPE_none'
    return '|'.join('CTYPE_' + name for name in c)


def write_tables(outdir, cruns, kruns):
    with open(os.path.join(outdir, 'ctype-class.h'), 'w') as f:
        f.write(HEADER.format(version=Version))
        for first, c in cruns:
            f.write(f'{{ 0x{first:04x}, {class_name(c)} }},\n')
    with open(os.path.join(outdir, 'ctype-case.h'), 'w') as f:
        f.write(HEADER.format(version=Version))
        for first, last, mode, delta in kruns:
            f.write(f'{{ 0x{first:04x}, 0x{last:04x}, CASE_{mode}, '
                    f'{delta if isinstance(delta, int) else "CASE_" + delta} }},\n')


parser = argparse.ArgumentParser(description='Generate Ctype Unicode tables')
parser.add_argument('--ucd', default='/usr/share/unicode',
                    help='Unicode Character Database directory')
parser.add_argument('outdir', help='directory for the generated headers')
args = parser.parse_args()

Version = load_version(os.path.join(args.ucd, 'ReadMe.txt'))
Properties = load_derived_properties(os.path.join(args.ucd, 'DerivedCoreProperties.txt'))
CodePoints = load_unicode_data(os.path.join(args.ucd, 'UnicodeData.txt'))

cruns = class_runs()
kruns = case_runs()
check(cruns, kruns)
write_tables(args.outdir, cruns, kruns)
