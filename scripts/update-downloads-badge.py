#!/usr/bin/env python3
"""Count every public release asset in this fork, including prereleases."""
import json
import os
from pathlib import Path
from urllib.request import Request, urlopen


def graphql(query: str, variables: dict, token: str) -> dict:
    request = Request("https://api.github.com/graphql",
        data=json.dumps({"query": query, "variables": variables}).encode(), headers={
            "Accept": "application/vnd.github+json",
            "Content-Type": "application/json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "hma-uidfake-download-badge",
        })
    with urlopen(request, timeout=30) as response:
        result = json.load(response)
    if result.get("errors") or not result.get("data"):
        raise ValueError(f"GitHub GraphQL error: {result.get('errors')}")
    return result["data"]


def count_downloads(repo: str, token: str) -> int:
    # REST /releases may incorrectly return [] for this fork. GraphQL is also
    # used by gh release list and exposes the complete repository release list.
    owner, name = repo.split("/", 1)
    total, cursor, release_count = 0, None, 0
    while True:
        after = ",after:" + json.dumps(cursor) if cursor else ""
        query = '''query {
          repository(owner:OWNER,name:NAME) { releases(first:100AFTER) {
            nodes { id tagName isDraft releaseAssets(first:100) {
              nodes { downloadCount name } pageInfo { hasNextPage endCursor }
            } } pageInfo { hasNextPage endCursor }
          } }
        }'''.replace("OWNER", json.dumps(owner)).replace("NAME", json.dumps(name)).replace("AFTER", after)
        data = graphql(query, {}, token)
        releases = data["repository"]["releases"]
        for release in releases["nodes"]:
            if release["isDraft"]:
                continue
            release_count += 1
            assets = release["releaseAssets"]
            while True:
                total += sum(int(a["downloadCount"]) for a in assets["nodes"])
                if not assets["pageInfo"]["hasNextPage"]:
                    break
                assets = graphql('''query($id:ID!,$cursor:String) {
                  node(id:$id) { ... on Release { releaseAssets(first:100,after:$cursor) {
                    nodes { downloadCount } pageInfo { hasNextPage endCursor }
                  } } }
                }''', {"id": release["id"], "cursor": assets["pageInfo"]["endCursor"]},
                    token)["node"]["releaseAssets"]
        if not releases["pageInfo"]["hasNextPage"]:
            break
        cursor = releases["pageInfo"]["endCursor"]
    if not release_count:
        raise ValueError("No public releases returned; preserving existing badge")
    return total


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
    print(f"Total GitHub release asset downloads (all versions): {total}")


if __name__ == "__main__":
    main()
