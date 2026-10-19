#!/usr/bin/env python3
"""Check every immutable reference patch; use --target to select one."""
from pathlib import Path
import subprocess
import sys

subprocess.run([sys.executable, str(Path(__file__).with_name('uidfake-reference.py')),
                *sys.argv[1:]], check=True)
