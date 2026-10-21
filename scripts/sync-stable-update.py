#!/usr/bin/env python3
"""Publish root-manager metadata from real, non-prerelease module ZIPs only."""
import io
import json
import os
from pathlib import Path
import urllib.request
import zipfile

REPO = 'Mobilelegends74/hma-uidfake'
ASSET_PREFIX = f'https://github.com/{REPO}/releases/download/'
RAW = f'https://raw.githubusercontent.com/{REPO}/main/'


def select_stable(releases, fetch_zip):
    candidates = []
    for release in releases:
        if release.get('draft') is not False or release.get('prerelease') is not False:
            continue
        for asset in release.get('assets', []):
            name = asset.get('name', '')
            url = asset.get('browser_download_url', '')
            if not (name.startswith('hma-uidfake-fork-') and name.endswith('.zip')
                    and url.startswith(ASSET_PREFIX) and 0 < asset.get('size', 0) <= 33554432):
                continue
            with zipfile.ZipFile(io.BytesIO(fetch_zip(url))) as archive:
                members = [f for f in archive.infolist() if f.filename == 'module.prop']
                if len(members) != 1 or members[0].file_size > 65536:
                    raise ValueError('Invalid module.prop in release ZIP')
                props = {}
                for line in archive.read(members[0]).decode('utf-8').splitlines():
                    if line.startswith('#') or '=' not in line:
                        continue
                    key, value = line.split('=', 1)
                    if key in props:
                        raise ValueError('Duplicate module property')
                    props[key] = value
            if props.get('id') != 'hma-uidfake':
                raise ValueError('Unexpected module id in update ZIP')
            code = int(props['versionCode'])
            if not 0 < code <= 2147483647 or not props.get('version'):
                raise ValueError('Invalid release version')
            candidates.append((code, props['version'], url, release.get('body') or ''))
    if not candidates:
        raise ValueError('No stable UIDFAKE module release; keeping existing metadata')
    candidates.sort(reverse=True)
    code, version, url, body = candidates[0]
    if any(c[0] == code and c[2] != url for c in candidates[1:]):
        raise ValueError('Multiple different stable ZIPs have the same versionCode')
    return {'version': version, 'versionCode': code, 'zipUrl': url,
            'changelog': RAW + 'update-changelog.md'}, body[:65536]


def fetch(url, api=False):
    headers = {'User-Agent': 'hma-uidfake-stable-update', 'Accept': 'application/vnd.github+json'}
    # Never forward the API credential to public asset redirect hosts.
    if api and os.environ.get('GH_TOKEN'):
        headers['Authorization'] = 'Bearer ' + os.environ['GH_TOKEN']
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=60) as response:
        data = response.read(33554433)
    if len(data) > 33554432:
        raise ValueError('Update response exceeds size limit')
    return data


def main():
    releases = []
    for page in range(1, 11):
        batch = json.loads(fetch(f'https://api.github.com/repos/{REPO}/releases?per_page=100&page={page}', api=True))
        releases.extend(batch)
        if len(batch) < 100:
            break
    metadata, changelog = select_stable(releases, fetch)
    # Do not silently roll back existing users if a newer stable is deleted.
    path = Path('update.json')
    if path.exists() and json.loads(path.read_text())['versionCode'] > metadata['versionCode']:
        raise ValueError('Refusing stable update metadata rollback')
    path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n')
    Path('update-changelog.md').write_text(changelog.rstrip() + '\n')
    print(f"Stable update: {metadata['version']} / {metadata['versionCode']}")


if __name__ == '__main__':
    main()
