#!/usr/bin/env python3
"""Regenerate the README download badge from current GitHub release assets."""
import json
import os
from pathlib import Path
from urllib.request import Request, urlopen


def count_downloads(repo: str, token: str) -> int:
    total = 0
    page = 1
    while True:
        url = f"https://api.github.com/repos/{repo}/releases?per_page=100&page={page}"
        request = Request(url, headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "hma-uidfake-download-badge",
        })
        with urlopen(request, timeout=30) as response:
            releases = json.load(response)
        if not isinstance(releases, list):
            raise ValueError("GitHub API returned a non-list releases response")
        for release in releases:
            if release.get("draft"):
                continue
            for asset in release.get("assets", []):
                name = asset.get("name", "")
                if name.startswith("hma-uidfake") and name.lower().endswith(".zip"):
                    total += asset.get("download_count", 0)
        if len(releases) < 100:
            return total
        page += 1


def make_svg(count: int) -> str:
    number = str(count)
    label_width = 76
    value_width = max(28, 14 + 7 * len(number))
    width = label_width + value_width
    label_center = label_width // 2
    number_center = label_width + value_width // 2
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="20" '
            f'role="img" aria-label="Downloads: {number}">\n'
            f'  <title>Downloads: {number}</title>\n'
            f'  <rect width="{label_width}" height="20" fill="#555"/>\n'
            f'  <rect x="{label_width}" width="{value_width}" height="20" fill="#e1ad01"/>\n'
            '  <g fill="#fff" text-anchor="middle" '
            'font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">\n'
            f'    <text x="{label_center}" y="14">Downloads</text>\n'
            f'    <text x="{number_center}" y="14">{number}</text>\n'
            '  </g>\n'
            '</svg>\n')


def main():
    repo = os.environ.get("GITHUB_REPOSITORY", "Mobilelegends74/hma-uidfake")
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        raise RuntimeError("GH_TOKEN or GITHUB_TOKEN is required")
    total = count_downloads(repo, token)
    path = Path("docs/assets/downloads.svg")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(make_svg(total), encoding="utf-8")
    print(f"GitHub release ZIP downloads: {total}")


if __name__ == "__main__":
    main()
