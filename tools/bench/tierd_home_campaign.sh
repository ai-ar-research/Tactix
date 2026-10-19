#!/usr/bin/env bash
# Full Tier D Tactix campaign with certificates on the home quota.
#
#   tierd_home_campaign.sh prepare <CAMPAIGN> <BINARY> <BINARY_SHA256> <SOURCE_TREE> <SOURCE_COMMIT> [--solver-argument=ARG]... [--instance=REL]...
#   tierd_home_campaign.sh recheck CAMPAIGN SOURCE_CAMPAIGN [CLASSES [SOURCE_TASK...]]   (default CLASSES=memout; 64 GiB)
#   tierd_home_campaign.sh validate CAMPAIGN
#   tierd_home_campaign.sh submit CAMPAIGN
#   tierd_home_campaign.sh status CAMPAIGN
#
# Paths are relative to the repository root. prepare/recheck only write the new
# campaign; validate and submit call sbatch. Contract (user, 2026-10-09): all
# 1,166 rows of benchmarks/baselines/sc24-26-32g-5000s, cpu-q, 4 CPUs, 32768 MiB
# (recheck 65536 MiB), 5000 s solve, 28800 s proof-check window, automatic proof
# (PBP -> VeriPB/CakePB, witness-free DSR -> drat-trim/CakeLPR, witnessed DSR ->
# long64-fixed dsr-trim/Trestle), named scratch in <campaign>/scratch deleted at
# classification and on timeout/SIGTERM, array throttle %125 (user-requested),
# read-only quota monitoring every 10 minutes, no node-local scratch and no node
# constraint.
# Without --instance the prepared array must hold exactly 1,166 rows; with
# --instance it is a smoke campaign of exactly those rows.
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
T=$R/tools/bench/experimental_tierd.py
THROTTLE=125
THROTTLE_REASON="user-requested maximum of 125 simultaneous instances for a full Tier D campaign"
export PYTHONDONTWRITEBYTECODE=1
usage() { sed -n '4,9p' "$R/tools/bench/tierd_home_campaign.sh"; exit 2; }
new_campaign() {
  case "$1" in benchmarks/runs/*) ;; *) echo "campaign must be benchmarks/runs/<new>" >&2; exit 2;; esac
  [ ! -e "$R/$1" ] || { echo "refusing to reuse $1 (immutable campaign paths)" >&2; exit 2; }
}
check_array() {  # campaign rows memory
  local a="$R/$1/array.sbatch"
  grep -qx "#SBATCH --array=0-$(($2 - 1))%$THROTTLE" "$a" || { echo "array is not 0-$(($2 - 1))%$THROTTLE" >&2; exit 3; }
  grep -qx "#SBATCH --partition=cpu-q" "$a" && grep -qx "#SBATCH --cpus-per-task=4" "$a" &&
    grep -qx "#SBATCH --mem=$3" "$a" || { echo "allocation differs from cpu-q/4/$3" >&2; exit 3; }
  if grep -Eq '^#SBATCH --(constraint|nodelist|exclude)' "$a"; then
    echo "array has a node constraint" >&2; exit 3
  fi
}
common=(--partition cpu-q --cpus-per-task 4 --solve-timeout 5000 --checker-timeout 28800
        --home-scratch --array-throttle "$THROTTLE" --array-throttle-reason "$THROTTLE_REASON")
cmd=${1:-}; [ $# -ge 2 ] || usage; shift
case "$cmd" in
prepare)
  [ $# -ge 5 ] || usage
  campaign=$1 binary=$2 want=$3 source=$4 commit=$5; shift 5
  new_campaign "$campaign"
  got=$(sha256sum "$R/$binary" | cut -d' ' -f1)
  [ "$got" = "$want" ] || { echo "binary sha256 $got != $want" >&2; exit 3; }
  smoke=0
  for arg in "$@"; do
    case "$arg" in --instance=*) smoke=$((smoke + 1));; --solver-argument=*) ;;
      *) echo "unsupported extra argument: $arg" >&2; exit 2;; esac
  done
  python3 "$T" --prepare --campaign "$R/$campaign" --binary "$R/$binary" \
    --source "$R/$source" --binary-source-commit "$commit" \
    --certificate auto --dsr-checker-profile long64-fixed "${common[@]}" "$@"
  rows=$([ "$smoke" -gt 0 ] && echo "$smoke" || echo 1166)
  check_array "$campaign" "$rows" 32768M
  pinned=$(sha256sum "$R/$campaign/pins/tactix" | cut -d' ' -f1)
  [ "$pinned" = "$want" ] || { echo "pinned binary differs" >&2; exit 3; }
  echo "PREPARED $campaign ($rows rows, pins/tactix $pinned)"
  echo "next: $0 validate $campaign";;
recheck)
  [ $# -ge 2 ] || usage
  campaign=$1 source=$2 classes=${3:-memout}; shift 2
  [ $# -eq 0 ] || shift
  task_args=()
  for task in "$@"; do
    case "$task" in *[!0-9]*|'') echo "source task must be a nonnegative integer: $task" >&2; exit 2;; esac
    task_args+=(--recheck-source-task "$task")
  done
  new_campaign "$campaign"
  python3 "$T" --prepare --campaign "$R/$campaign" --recheck-from "$R/$source" \
    --recheck-classes "$classes" "${task_args[@]}" --memory-mib 65536 "${common[@]}"
  rows=$(($(wc -l < "$R/$campaign/matrix.tsv") - 1))
  check_array "$campaign" "$rows" 65536M
  echo "PREPARED $campaign ($rows $classes rows of $source at 64 GiB)"
  echo "next: $0 validate $campaign";;
validate)
  python3 "$T" --campaign "$R/$1" --validate
  echo "wait for $R/$1/validation.json (\"passed\": true), then: $0 submit $1";;
submit)
  campaign=$1
  if grep -qx "#SBATCH --array=0-1165%$THROTTLE" "$R/$campaign/array.sbatch" ||
      grep -q '"recheck_source"' "$R/$campaign/manifest.json"; then
    python3 "$R/tools/slurm/submit_full_tierd.py" \
      --campaign "$R/$campaign" --array-sbatch "$R/$campaign/array.sbatch"
    exit 0
  fi
  quota -s 2>/dev/null | tail -2 || true
  python3 "$T" --campaign "$R/$campaign" --submit;;
status)
  python3 "$T" --campaign "$R/$1" --summarize;;
*) usage;;
esac
