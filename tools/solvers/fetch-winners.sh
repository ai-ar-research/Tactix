#!/usr/bin/env bash
# Fetch the source of the 2024--2026 SAT Competition Main Track winners,
# plus LymphoSAT, the 2026 Main Track SAT winner.
#
# Sources land in winners/ at the repository root.  That directory is ignored
# by git because these are third-party reference trees, not Tactix sources.
# Competition archives retain the organizers' original layout; in 2026 they
# are grouped by submitting author and may contain more than one solver.
#
# Usage: tools/solvers/fetch-winners.sh

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$ROOT/winners"
mkdir -p "$DIR"

SC25=https://satcompetition.github.io/2025/downloads/solvers/main
SC26=https://satcompetition.github.io/2026/downloads/solvers
KISSAT_REPO=https://github.com/arminbiere/kissat.git

archive=""
repairs=""
stage=""
cleanup () {
    if [ -n "$archive" ] && [ -f "$archive" ]; then
        rm -f -- "$archive"
    fi
    if [ -n "$stage" ] && [ -d "$stage" ]; then
        rm -rf -- "$stage"
    fi
    if [ -n "$repairs" ] && [ -f "$repairs" ]; then
        rm -f -- "$repairs"
    fi
}
trap cleanup EXIT

validate_tar () { # archive repair-list
    python3 - "$1" "$2" <<'PY'
import posixpath
import sys
import tarfile

archive = sys.argv[1]
repair_file = sys.argv[2]
with tarfile.open(archive, "r:xz") as tf:
    members = {member.name: member for member in tf.getmembers()}
    repairs = []
    for member in members.values():
        name = member.name
        normalized = posixpath.normpath(name)
        if "\n" in name or "\n" in member.linkname:
            raise SystemExit(f"newline in archive path: {name!r}")
        if name.startswith("/") or normalized == ".." or normalized.startswith("../"):
            raise SystemExit(f"unsafe archive member: {name!r}")
        if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
            raise SystemExit(f"unsupported archive member type: {name!r}")
        if member.issym():
            target = posixpath.normpath(posixpath.join(posixpath.dirname(name), member.linkname))
            if member.linkname.startswith("/") or target == ".." or target.startswith("../"):
                # Several official packages accidentally captured an absolute
                # build-tree link from src/makefile to the adjacent makefile.
                # Repair only that exact, independently verifiable pattern.
                expected = posixpath.normpath(posixpath.join(posixpath.dirname(name), "../makefile"))
                expected_member = members.get(expected)
                if (posixpath.basename(name) == "makefile"
                        and posixpath.basename(posixpath.dirname(name)) == "src"
                        and posixpath.basename(member.linkname) == "makefile"
                        and expected_member is not None
                        and expected_member.isfile()):
                    repairs.append(name)
                else:
                    raise SystemExit(f"unsafe symlink target: {name!r} -> {member.linkname!r}")
        if member.islnk():
            target = posixpath.normpath(member.linkname)
            if member.linkname.startswith("/") or target == ".." or target.startswith("../"):
                raise SystemExit(f"unsafe hardlink target: {name!r} -> {member.linkname!r}")
with open(repair_file, "w", encoding="utf-8") as out:
    for name in repairs:
        print(name, file=out)
PY
}

fetch_tar () { # destination-name source-url
    local name="$1" url="$2"
    if [ -d "$DIR/$name" ]; then
        echo "== $name: already present =="
        return
    fi

    echo "== $name: fetching =="
    archive="$(mktemp "$DIR/.${name}.archive.XXXXXX")"
    repairs="$(mktemp "$DIR/.${name}.repairs.XXXXXX")"
    stage="$(mktemp -d "$DIR/.${name}.stage.XXXXXX")"
    curl --fail --location --silent --show-error --retry 3 \
        --output "$archive" "$url"
    xz --test "$archive"
    validate_tar "$archive" "$repairs"
    tar --extract --xz --file "$archive" --directory "$stage" \
        --no-same-owner --no-same-permissions --exclude-from "$repairs"
    while IFS= read -r member; do
        ln --symbolic ../makefile "$stage/$member"
    done < "$repairs"
    printf '%s\n' "$url" > "$stage/SOURCE_URL"
    sha256sum "$archive" | cut -d ' ' -f 1 > "$stage/SOURCE_SHA256"
    mv -- "$stage" "$DIR/$name"
    stage=""
    rm -f -- "$archive"
    archive=""
    rm -f -- "$repairs"
    repairs=""
}

fetch_git_tag () { # destination-name repository tag
    local name="$1" repository="$2" tag="$3"
    if [ -d "$DIR/$name" ]; then
        echo "== $name: already present =="
        return
    fi

    echo "== $name: cloning tag $tag =="
    stage="$(mktemp -d "$DIR/.${name}.stage.XXXXXX")"
    git clone --depth 1 --branch "$tag" "$repository" "$stage/source"
    printf '%s\n' "$repository" > "$stage/source/SOURCE_URL"
    printf '%s\n' "$tag" > "$stage/source/SOURCE_REF"
    git -C "$stage/source" rev-parse HEAD > "$stage/source/SOURCE_COMMIT"
    mv -- "$stage/source" "$DIR/$name"
    rmdir -- "$stage"
    stage=""
}

# The SC2024 downloads page was left as "tbd".  The winning source is
# available as the upstream Kissat repository's sc2024 tag.
fetch_git_tag kissat-sc2024 "$KISSAT_REPO" sc2024

fetch_tar AE_kissat2025_MAB \
    "$SC25/AE_kissat2025_MAB.tar.xz"
fetch_tar satsuma-iter-kissat-sc2026 \
    "$SC26/anders.tar.xz"
fetch_tar lymphosat-sc2026 \
    "$SC26/green.tar.xz"

echo "done:"
find "$DIR" -mindepth 1 -maxdepth 1 -type d -printf '  %f\n' | sort
