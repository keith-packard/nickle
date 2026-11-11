#!/usr/bin/env python3
#
# Copyright © 2026 Keith Packard and Bart Massey.
# All Rights Reserved.  See the file COPYING in this directory
# for licensing information.
#
# Check that C functions leave their MemStack scopes properly.
#
# A function opens a scope with ENTER() at the top level of its body,
# and must leave through RETURN, RETURN2, NOREFRETURN, or EXIT()
# followed by return; a bare return leaves its temporaries referenced.
# Scopes inside a function use an explicit mark from MemMark() and end
# with MemRelease or MemReturn. A goto must not leave a scope.
#
# This relies on clang-format's layout: each function body opens with
# '{' alone at the start of a line and is preceded by its signature.
#
# Usage: check-memstack.py file...

import re
import sys

LEAVE_ENTER = re.compile(r"\b(RETURN|RETURN2|NOREFRETURN)\s*\(")
MACROS = re.compile(r"\b(RETURN|RETURN2|EXIT|NOREFRETURN)\s*\(")
RETURN = re.compile(r"\breturn\b")
GOTO = re.compile(r"\bgoto\s+(\w+)")
LABEL = re.compile(r"^\s*([A-Za-z_]\w*):\s*$")
SIGNATURE = re.compile(r"^([A-Za-z_]\w*)\s*\(")


def blank(match):
    """Replace a match with spaces, keeping its newlines"""
    return re.sub(r"[^\n]", " ", match.group())


def strip(text):
    """Remove comments, literals, and #if 0 regions, keeping line numbers"""
    text = re.sub(r"/\*.*?\*/", blank, text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(\\.|[^"\\\n])*"', '""', text)
    text = re.sub(r"'(\\.|[^'\\\n])*'", "''", text)
    lines = []
    skip = 0
    for line in text.split("\n"):
        directive = line.strip()
        if skip:
            if directive.startswith("#if"):
                skip += 1
            elif directive.startswith("#endif"):
                skip -= 1
            elif directive.startswith("#else") and skip == 1:
                skip = 0
            lines.append("")
        elif re.match(r"#\s*if\s+0\b", directive):
            skip = 1
            lines.append("")
        else:
            lines.append(line)
    return lines


def released(body, n, release):
    """Whether the statement before line n, or line n itself, releases the scope"""
    return any(release in line for line in body[max(0, n - 2) : n + 1])


def check_function(path, source, start, body, name, problems):
    def report(n, message):
        problems.append(f"{path}:{start + n + 1}: {name}: {message}: {source[start + n].strip()}")

    end = len(body)
    enter = next((n for n, line in enumerate(body) if "ENTER()" in line), end)
    mark = next((n for n, line in enumerate(body) if "MemMark()" in line), end)
    scope = min(enter, mark)
    labels = {}
    for n, line in enumerate(body):
        match = LABEL.match(line)
        if match:
            labels[match.group(1)] = n

    for n, line in enumerate(body):
        if "ENTER()" in line and not line.startswith("    ENTER()"):
            report(n, "ENTER not at the top level; use MemMark")
        if enter == end and MACROS.search(line):
            report(n, "RETURN or EXIT without ENTER")
        if RETURN.search(line):
            if n > enter and not LEAVE_ENTER.search(line) and not released(body, n, "EXIT()"):
                report(n, "return without leaving the ENTER scope")
            if n > mark and "MemReturn" not in line and not released(body, n, "MemRelease"):
                report(n, "return without leaving the MemMark scope")
        match = GOTO.search(line)
        if match and n > scope and labels.get(match.group(1), -1) < scope:
            report(n, "goto out of the scope")


def check_file(path, problems):
    with open(path) as file:
        source = file.read().split("\n")
    lines = strip("\n".join(source))
    i = 0
    while i < len(lines):
        if lines[i] != "{":
            i += 1
            continue
        start = i
        depth = 0
        while True:
            depth += lines[i].count("{") - lines[i].count("}")
            if depth == 0:
                break
            i += 1
        name = "?"
        for k in range(start - 1, -1, -1):
            match = SIGNATURE.match(lines[k])
            if match:
                name = match.group(1)
                break
        check_function(path, source, start, lines[start : i + 1], name, problems)
        i += 1


def main():
    problems = []
    for path in sys.argv[1:]:
        check_file(path, problems)
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
