"""Immutable source fixtures shared by patch generation and compatibility CI."""
import base64
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
FILES = ['kernel/sys.c', 'block/ioprio.c', 'kernel/user.c', 'kernel/cred.c',
         'fs/open.c', 'include/linux/sched.h', 'include/linux/android_kabi.h',
         'drivers/misc/Kconfig', 'drivers/misc/Makefile',
         'arch/arm64/configs/gki_defconfig', 'Makefile']


def targets():
    return json.loads((ROOT / 'patches/targets.json').read_text())['targets']


def fetch(target, tree):
    if target['source'] == 'aosp':
        url = f'https://android.googlesource.com/kernel/common/+/{target["commit"]}/'
        encoded = True
    else:
        url = f'https://raw.githubusercontent.com/OnePlusOSS/android_kernel_common_oneplus_sm8650/{target["commit"]}/'
        encoded = False

    def download(relative):
        location = url + relative + ('?format=TEXT' if encoded else '')
        for attempt in range(7):
            try:
                with urllib.request.urlopen(location, timeout=45) as response:
                    data = response.read()
                if encoded:
                    data = base64.b64decode(data, validate=True)
                assert data and b'<html' not in data[:128].lower(), location
                path = tree / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                return
            except Exception:
                if attempt == 6:
                    raise
                time.sleep(min(2 ** (attempt + 1), 30))

    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(download, FILES))
    # Dependencies of the real newer KABI header, not substituted macros.
    if '#include <linux/args.h>' in (tree / 'include/linux/android_kabi.h').read_text():
        for relative in ('include/linux/args.h', 'include/linux/compiler_attributes.h'):
            download(relative)
