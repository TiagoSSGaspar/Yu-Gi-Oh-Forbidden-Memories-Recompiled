#!/usr/bin/env python3
"""Copy the official release's HD pack into this fork's release.

The fork's releases carry the HD pack the official project publishes
(Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled; art by Unchiga and
X@nder): this takes the asset yfm-redecomp-hd-mod-*.zip of its latest
release, checks it against the SHA-256 and size GitHub lists for it, and
saves it as yfm-redecomp-hd-mod-<tag>.zip, the name the game's HD pack...
looks for first (src/pc/mods/hd_pack.h). Any failure exits non-zero, so the
draft-release job never makes a release without it.

  python3 tools/pc/mirror_hd_pack.py --tag v1.0.0 --out release
"""
import argparse, hashlib, json, os, sys, urllib.request

SOURCE = "Unchiga/Yu-Gi-Oh-Forbidden-Memories-Recompiled"
PREFIX = "yfm-redecomp-hd-mod-"


class MirrorError(Exception):
    pass


def pick_asset(release):
    """(name, url, sha256, size) of the release's HD pack: the asset
    yfm-redecomp-hd-mod-<its tag>.zip, else the first yfm-redecomp-hd-mod-*.zip
    (hd_pack.c's rule). An asset without a SHA-256 is refused."""
    assets = [a for a in release.get("assets", [])
              if a.get("name", "").startswith(PREFIX) and a["name"].endswith(".zip")]
    if not assets:
        raise MirrorError(f"release {release.get('tag_name')!r} has no {PREFIX}*.zip")
    named = [a for a in assets if a["name"] == f"{PREFIX}{release.get('tag_name', '')}.zip"]
    chosen = (named or assets)[0]
    digest = chosen.get("digest") or ""
    if not digest.startswith("sha256:") or len(digest) != len("sha256:") + 64:
        raise MirrorError(f"{chosen['name']}: GitHub lists no SHA-256 for it, so a copy could not be checked")
    return chosen["name"], chosen["browser_download_url"], digest[len("sha256:"):].lower(), int(chosen.get("size") or 0)


def sha256_file(path):
    value = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            value.update(block)
    return value.hexdigest()


def request(url, token=None):
    headers = {"User-Agent": "yfm-mirror-hd-pack", "Accept": "application/vnd.github+json"}
    if token and url.startswith("https://api.github.com/"):
        headers["Authorization"] = f"Bearer {token}"
    return urllib.request.Request(url, headers=headers)


def mirror(release, tag, out, token=None):
    """Downloads the release's HD pack to out/yfm-redecomp-hd-mod-<tag>.zip,
    checked; returns that path. On a failure nothing is left in `out`."""
    name, url, sha256, size = pick_asset(release)
    os.makedirs(out, exist_ok=True)
    target = os.path.join(out, f"{PREFIX}{tag}.zip")
    part = target + ".part"
    try:
        with urllib.request.urlopen(request(url, token), timeout=300) as response, open(part, "wb") as stream:
            for block in iter(lambda: response.read(1 << 20), b""):
                stream.write(block)
        if size and os.path.getsize(part) != size:
            raise MirrorError(f"{name}: {os.path.getsize(part)} bytes, GitHub lists {size}")
        got = sha256_file(part)
        if got != sha256:
            raise MirrorError(f"{name}: SHA-256 {got}, GitHub lists {sha256}")
        os.replace(part, target)
    finally:
        if os.path.exists(part):
            os.remove(part)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--tag", required=True, help="this release's tag, which names the copy")
    parser.add_argument("--out", required=True, help="the folder of the release's files")
    parser.add_argument("--source", default=SOURCE, help="the repository whose latest release has the pack")
    args = parser.parse_args()
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    try:
        latest = f"https://api.github.com/repos/{args.source}/releases/latest"
        with urllib.request.urlopen(request(latest, token), timeout=60) as response:
            release = json.load(response)
        path = mirror(release, args.tag, args.out, token)
    except (MirrorError, OSError, ValueError, KeyError) as error:
        sys.exit(f"mirror_hd_pack: {error}")
    print(f"mirror_hd_pack: {args.source} {release['tag_name']} -> {path}")


if __name__ == "__main__":
    main()
