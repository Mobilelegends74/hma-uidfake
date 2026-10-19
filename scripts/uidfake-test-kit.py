#!/usr/bin/env python3
"""Offline syntax, protocol, documentation-link and read-only workflow checks."""
import ast
from pathlib import Path
import re
import subprocess
import urllib.parse

root = Path(__file__).resolve().parent.parent
assert (root / 'src/include/kaux.h').read_bytes() == (root / 'uidfake/kernel/include/kaux.h').read_bytes(), 'Protocol header drift'
for p in (root / 'scripts').glob('uidfake*.py'):
    ast.parse(p.read_text(), filename=str(p))
for name in ('uidfake-stage-anykernel.sh', 'uidfake-integrate-kernel.sh'):
    subprocess.run(['bash', '-n', str(root / 'scripts' / name)], check=True)
subprocess.run(['sh', '-n', str(root / 'scripts/uidfake-kernel-service.sh')], check=True)

workflows = list((root / '.github/workflows').glob('*.yml'))
assert len(workflows) == 1 and workflows[0].name == 'native-patches.yml'
workflow = workflows[0].read_text()
assert 'contents: read' in workflow
assert not re.search(r':\s*write\b|gh\s+release|git\s+push|action-gh-release', workflow)

docs = [root / 'README.md', root / 'README_EN.md', root / 'patches/README.md']
docs += list((root / 'docs/native').glob('*.md'))
for doc in docs:
    for link in re.findall(r'\]\(([^\s)]+)', doc.read_text()):
        if link.startswith(('https://', 'http://', '#', 'mailto:')):
            continue
        target = urllib.parse.unquote(link.split('#', 1)[0])
        assert (doc.parent / target).exists(), (str(doc), link)
print('Native kit: protocol, Python/shell syntax, documentation links and read-only CI passed')
