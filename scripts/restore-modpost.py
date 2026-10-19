#!/usr/bin/env python3
"""Undo DDK's export/CRC bypass before building; never alter target protections."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]) / 'scripts/mod/modpost.c'
before = source.read_text()
after = re.sub(r'(?m)^([ \t]*)//([ \t]*(?:check_exports\(mod\);|s->module = exp->module;))', r'\1\2', before)
after = after.replace('static void __attribute__((unused)) check_exports(', 'static void check_exports(')
assert re.search(r'(?m)^\s*check_exports\(mod\);', after), 'Cannot establish active standard export checks'
assert re.search(r'(?m)^\s*s->module = exp->module;', after), 'Cannot establish active CRC symbol association'
if before != after:
    source.write_text(after)
print('modpost: standard export and CRC association checks active')
