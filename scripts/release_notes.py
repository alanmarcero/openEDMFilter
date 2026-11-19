"""Build a GitHub release body from a CHANGELOG section or the tag message.

The matching changelog section wins. Otherwise the annotated tag message is
used, unless that message is only the version. The editor screenshot stays at
the bottom. GitHub's generated notes append the compare link.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

CHANGELOG_CANDIDATES = ("CHANGELOG.md", "CHANGELOG", "docs/CHANGELOG.md")
HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
VERSION_IN_HEADING = re.compile(
    r"^(?:version\s+)?\[?v?(\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?)\]?(?:\s|$|[:(-])",
    re.IGNORECASE,
)
PGP_SIGNATURE = "-----BEGIN PGP SIGNATURE-----"
SCREENSHOT = (
    "![openEDMFilter native editor]"
    "(https://raw.githubusercontent.com/{repo}/{tag}/docs/screenshots/preview.png)"
)


def version_from_tag(tag):
    if len(tag) > 1 and tag[0] in "vV" and tag[1].isdigit():
        return tag[1:]
    return tag


def changelog_section(markdown, tag):
    version = version_from_tag(tag).lower()
    lines = markdown.splitlines()
    start = None
    level = None
    for index, line in enumerate(lines):
        match = HEADING.match(line)
        if match is None:
            continue
        found = VERSION_IN_HEADING.match(match.group(2).strip())
        if found is not None and found.group(1).lower() == version:
            start = index + 1
            level = len(match.group(1))
            break
    if start is None:
        return ""
    end = len(lines)
    for index in range(start, len(lines)):
        match = HEADING.match(lines[index])
        if match is not None and len(match.group(1)) <= level:
            end = index
            break
    return "\n".join(lines[start:end]).strip()


def find_changelog(root):
    for relative in CHANGELOG_CANDIDATES:
        path = root / relative
        if path.is_file():
            return path
    return None


def useful_tag_message(raw, tag):
    text = raw.split(PGP_SIGNATURE, 1)[0].strip()
    if not text:
        return ""
    version = version_from_tag(tag)
    lines = text.splitlines()
    first = lines[0].strip()
    rest = "\n".join(lines[1:]).strip()
    if first in {tag, version, f"v{version}", f"V{version}"}:
        return rest
    return text


def read_tag_message(tag, root):
    try:
        raw = subprocess.check_output(
            ["git", "for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"],
            cwd=root,
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""
    return useful_tag_message(raw, tag)


def release_body(notes, repo, tag):
    image = SCREENSHOT.format(repo=repo, tag=tag)
    notes = notes.strip()
    if not notes or image in notes or "docs/screenshots/preview.png" in notes:
        text = notes or image
    else:
        text = f"{notes}\n\n{image}"
    return text if text.endswith("\n") else text + "\n"


def notes_for(tag, root, changelog_path=None):
    path = Path(changelog_path) if changelog_path else find_changelog(root)
    if path is not None and path.is_file():
        section = changelog_section(path.read_text(encoding="utf-8"), tag)
        if section:
            return section, "changelog"
    message = read_tag_message(tag, root)
    if message:
        return message, "tag"
    return "", "none"


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repo", required=True, help="owner/name used in the screenshot URL")
    parser.add_argument("--output", required=True)
    parser.add_argument("--root", default=".", help="Repository root to search")
    parser.add_argument("--changelog", help="Changelog file. Defaults to the usual locations.")
    args = parser.parse_args(argv)

    root = Path(args.root)
    notes, source = notes_for(args.tag, root, args.changelog)
    if source == "changelog":
        print(f"Release notes: {args.tag} section from the changelog", file=sys.stderr)
    elif source == "tag":
        print(f"Release notes: annotated message for {args.tag}", file=sys.stderr)
    else:
        print(
            f"Release notes: no changelog section or tag message for {args.tag}; screenshot only",
            file=sys.stderr,
        )
    Path(args.output).write_text(release_body(notes, args.repo, args.tag), encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1:])
