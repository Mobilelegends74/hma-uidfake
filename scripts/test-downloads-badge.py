#!/usr/bin/env python3
"""Offline regression checks: repository total, pagination and stale responses."""
import importlib.util
import io
import json

spec = importlib.util.spec_from_file_location("badge", "scripts/update-downloads-badge.py")
badge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(badge)


def response(data):
    stream = io.BytesIO(json.dumps(data).encode())
    return stream


def assets(counts, next_page=False):
    return {"nodes": [{"downloadCount": c} for c in counts],
            "pageInfo": {"hasNextPage": next_page, "endCursor": "next-assets"}}


def releases(nodes, next_page=False):
    return {"data": {"repository": {"releases": {"nodes": nodes,
            "pageInfo": {"hasNextPage": next_page, "endCursor": "next-releases"}}}}}


seen = []
pages = [response(releases([
    {"id": "stable", "isDraft": False, "releaseAssets": assets([10, 2], True)},
    {"id": "prerelease", "isDraft": False, "releaseAssets": assets([7])},
    {"id": "draft", "isDraft": True, "releaseAssets": assets([999])},
], True)), response({"data": {"node": {"releaseAssets": assets([1])}}}),
    response(releases([{"id": "older", "isDraft": False, "releaseAssets": assets([5])}]))]


def fake_open(request, timeout):
    assert request.full_url == "https://api.github.com/graphql"
    seen.append(json.loads(request.data))
    return pages.pop(0)


badge.urlopen = fake_open
assert badge.count_downloads("owner/fork", "test") == 25
assert seen[1]["variables"] == {"id": "stable", "cursor": "next-assets"}
assert 'after:"next-releases"' in seen[2]["query"]
assert "Downloads: 25" in badge.make_svg(25)
pages[:] = [response(releases([]))]
try:
    badge.count_downloads("owner/fork", "test")
except ValueError as error:
    assert "preserving" in str(error)
else:
    raise AssertionError("Stale zero must not replace the aggregate count")
pages[:] = [response(releases([{"id": "new", "isDraft": False, "releaseAssets": assets([0])}]))]
assert badge.count_downloads("owner/fork", "test") == 0
print("Download badge tests passed")
