#!/usr/bin/env bash
# skip_unless.sh FILE... -- COMMAND...
# Run COMMAND, or exit 77 (ctest SKIP_RETURN_CODE) when a required external
# tool or fixture is missing, e.g. proof checkers not yet built with
# tools/checkers/setup.sh or pinned binaries that only a development tree has.
set -euo pipefail
while [ "$#" -gt 0 ] && [ "$1" != "--" ]; do
    if [ ! -e "$1" ]; then
        echo "skipped: $1 is not available"
        exit 77
    fi
    shift
done
shift
exec "$@"
