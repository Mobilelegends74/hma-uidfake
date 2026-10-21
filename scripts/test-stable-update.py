#!/usr/bin/env python3
import importlib.util
import io
import zipfile

spec = importlib.util.spec_from_file_location('stable_update', 'scripts/sync-stable-update.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
downloads = {}


def release(code, prerelease=False, draft=False, suffix='', module_id='hma-uidfake'):
    url = module.ASSET_PREFIX + str(code) + suffix + '/hma-uidfake-fork-test.zip'
    blob = io.BytesIO()
    with zipfile.ZipFile(blob, 'w') as archive:
        archive.writestr('module.prop', f'id={module_id}\nversion=0.3.{code}-test-fork\nversionCode={code}\n')
    downloads[url] = blob.getvalue()
    return {'draft': draft, 'prerelease': prerelease, 'body': 'changes',
            'assets': [{'name': 'hma-uidfake-fork-test.zip', 'browser_download_url': url, 'size': len(blob.getvalue())}]}


stable = release(3014)
test = release(3015, prerelease=True)
draft = release(9999, draft=True)
metadata, notes = module.select_stable([test, draft, stable], downloads.__getitem__)
assert metadata['versionCode'] == 3014 and notes == 'changes'
assert metadata['version'].endswith('test-fork'), 'Published stable test-named tags must be supported'
test['prerelease'] = False
assert module.select_stable([stable, test], downloads.__getitem__)[0]['versionCode'] == 3015
for values in [[release(1, prerelease=True)], [release(4, module_id='other-module')],
               [release(7), release(7, suffix='-duplicate')]]:
    try:
        module.select_stable(values, downloads.__getitem__)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid update feed accepted')
print('OK: stable-only updates, draft/prerelease exclusion, promotion, real ZIP versionCode, module id and duplicate-code rejection')
