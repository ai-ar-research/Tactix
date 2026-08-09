#!/usr/bin/env bash
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../../../.." && pwd)
SOURCE=${KISSAT_TRACE_SOURCE:-$ROOT/kissat}
DEST=${KISSAT_TRACE_DEST:-$HERE/worktree}

python3 "$HERE/prepare.py" --source "$SOURCE" --dest "$DEST"
(cd "$DEST" && ./configure --statistics)
make -C "$DEST/build" --jobs
printf '%s\n' "$DEST/build/kissat"
