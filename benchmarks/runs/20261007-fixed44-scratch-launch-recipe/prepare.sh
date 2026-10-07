#!/usr/bin/env bash
# Prepare (never submit) the full44 shared-floor campaign for one candidate.
# Usage:
#   prepare.sh CAMPAIGN BINARY BINARY_SHA256 SOURCE_TREE SOURCE_COMMIT [--solver-argument=...]...
# Paths are relative to the repository root. Without extra solver arguments
# the b26 arm (--auto-proof --default-graph-labeling=on) is reproduced.
set -euo pipefail
R=/home/hwu/SAT-solving
P=$R/benchmarks/runs/20261007-fixed44-scratch-launch-recipe
if [ $# -lt 5 ]; then
  sed -n '2,7p' "$P/prepare.sh"; exit 2
fi
campaign=$1 binary=$2 want=$3 source=$4 commit=$5; shift 5
case "$campaign" in benchmarks/runs/*) ;; *) echo "campaign must be benchmarks/runs/<new>" >&2; exit 2;; esac
if [ -e "$R/$campaign" ] || [ -e "$R/$campaign-preparation" ]; then
  echo "refusing to reuse $campaign (immutable campaign paths)" >&2; exit 2
fi
sha256sum --quiet -c "$P/runner-source.sha256" || { echo "frozen recipe changed" >&2; exit 3; }
got=$(sha256sum "$R/$binary" | cut -d' ' -f1)
[ "$got" = "$want" ] || { echo "binary sha256 $got != $want" >&2; exit 3; }
extra=("$@")
[ ${#extra[@]} -gt 0 ] || extra=(--solver-argument=--default-graph-labeling=on)
PYTHONDONTWRITEBYTECODE=1 python3 "$P/prepare-wrapper.py" \
  --campaign "$R/$campaign" --source "$R/$source" --binary "$R/$binary" \
  --binary-source-commit "$commit" \
  --partition cpu-q --cpus-per-task 4 \
  --solve-timeout 5000 --checker-timeout 14400 \
  --certificate auto --dsr-checker-profile long64-fixed \
  --scratch-capacity-profile fixed44-shared-floor-v1 \
  "${extra[@]}" \
  --selection-table "$R/benchmarks/reports/2026-10-07-fixed44-successor-campaign-preparation/selection.tsv" \
  --prepare
C=$R/$campaign
pinned=$(sha256sum "$C/pins/tactix" | cut -d' ' -f1)
[ "$pinned" = "$want" ] || { echo "pinned binary differs" >&2; exit 3; }
grep -q '^#SBATCH --array=0-43$' "$C/array.sbatch" || { echo "array is not full width" >&2; exit 3; }
if grep -Eq '^#SBATCH --(constraint|nodelist|exclude)|^#SBATCH --array=.*%' "$C/array.sbatch"; then
  echo "array has a node constraint or throttle" >&2; exit 3
fi
mkdir "$C-preparation"
python3 - "$C" "$want" "$commit" "$P" > "$C-preparation/PREPARED.json" <<'PY'
import hashlib, json, sys
from pathlib import Path
c, want, commit, recipe = Path(sys.argv[1]), sys.argv[2], sys.argv[3], Path(sys.argv[4])
h = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
m = json.loads((c/'manifest.json').read_text())
print(json.dumps(dict(
    campaign=str(c.relative_to('/home/hwu/SAT-solving')), submitted=False,
    validation_submitted=False, manifest_sha256=h(c/'manifest.json'),
    matrix_sha256=h(c/'matrix.tsv'), array_sha256=h(c/'array.sbatch'),
    validation_script_sha256=h(c/'validate.sbatch'), solver_sha256=want,
    solver_source_commit=m['source_commit'], requested_source_commit=commit,
    recipe=str(recipe.relative_to('/home/hwu/SAT-solving')),
    recipe_hashes_sha256=h(recipe/'runner-source.sha256'),
    scratch_capacity_profile=m['scratch_capacity_profile'],
    scratch_capacity=m['anonymous_scratch_capacity'],
    resource_contract=m['resource_contract'],
    dsr_checker_profile=m['dsr_checker_profile'],
    solver_arguments=m['arms']['tactix']['arguments']), indent=2))
PY
echo "PREPARED $C"
echo "next: python3 $P/prepare-wrapper.py --campaign $C --validate"
