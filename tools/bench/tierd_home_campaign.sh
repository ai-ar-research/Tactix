#!/usr/bin/env bash
# Full Tier D Tactix campaign with certificates on the home quota.
#
#   tierd_home_campaign.sh prepare <CAMPAIGN> <BINARY> <BINARY_SHA256> <SOURCE_TREE> <SOURCE_COMMIT> [--solver-argument=ARG]... [--instance=REL]...
#   tierd_home_campaign.sh recheck CAMPAIGN SOURCE_CAMPAIGN [CLASSES]   (default CLASSES=memout; 64 GiB)
#   tierd_home_campaign.sh validate CAMPAIGN
#   tierd_home_campaign.sh submit CAMPAIGN [GOVERNOR_JOBS_JSON]
#   tierd_home_campaign.sh governor CAMPAIGN        (prepare a fallback governor; never submits)
#   tierd_home_campaign.sh status CAMPAIGN
#
# Paths are relative to the repository root. prepare/recheck only write the new
# campaign; validate and submit call sbatch. Contract (user, 2026-10-09): all
# 1,166 rows of benchmarks/baselines/sc24-26-32g-5000s, cpu-q, 4 CPUs, 32768 MiB
# (recheck 65536 MiB), 5000 s solve, 28800 s proof-check window, automatic proof
# (PBP -> VeriPB/CakePB, witness-free DSR -> drat-trim/CakeLPR, witnessed DSR ->
# long64-fixed dsr-trim/Trestle), named scratch in <campaign>/scratch deleted at
# classification, array throttle %125 (user-requested), no node constraint.
# Without --instance the prepared array must hold exactly 1,166 rows; with
# --instance it is a smoke campaign of exactly those rows.
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
T=$R/tools/bench/experimental_tierd.py
GOVERNOR_JOBS=benchmarks/baselines/sc24-26-rerun-quota-governor/jobs.json
THROTTLE=125
THROTTLE_REASON="user-requested concurrency cap of 125 for the Tier D campaign (2026-10-09); certificates on the home quota under the 10-minute quota governor (hold pending above 750 GB, release below 700 GB)"
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
  campaign=$1 source=$2 classes=${3:-memout}
  new_campaign "$campaign"
  python3 "$T" --prepare --campaign "$R/$campaign" --recheck-from "$R/$source" \
    --recheck-classes "$classes" --memory-mib 65536 "${common[@]}"
  rows=$(($(wc -l < "$R/$campaign/matrix.tsv") - 1))
  check_array "$campaign" "$rows" 65536M
  echo "PREPARED $campaign ($rows $classes rows of $source at 64 GiB)"
  echo "next: $0 validate $campaign";;
validate)
  python3 "$T" --campaign "$R/$1" --validate
  echo "wait for $R/$1/validation.json (\"passed\": true), then: $0 submit $1";;
submit)
  campaign=$1 jobs=${2:-$GOVERNOR_JOBS}
  quota -s 2>/dev/null | tail -2 || true
  if [ "$jobs" = "$GOVERNOR_JOBS" ]; then
    # The shared governor re-reads jobs.json every tick; it must be running.
    live=$(squeue -h -u "$USER" -n sc24-26-quota-governor -t R -o '%i %L')
    [ -n "$live" ] || { echo "shared quota governor is not running; use: $0 governor $campaign" >&2; exit 3; }
    echo "shared governor (job, time left): $live"
  fi
  python3 "$T" --campaign "$R/$campaign" --submit --governor-jobs "$R/$jobs"
  echo "registered with $jobs";;
governor)
  campaign=$1 g=$R/$1/governor
  [ -d "$R/$campaign" ] && [ ! -e "$g" ] || { echo "need a prepared campaign without a governor" >&2; exit 2; }
  mkdir "$g"
  cat "$R/tools/slurm/quota_governor.py" > "$g/quota_governor.py"
  chmod 0444 "$g/quota_governor.py"
  printf '{\n  "jobs": [],\n  "expect_more": false,\n  "policy": "user 2026-10-08: every 600 s read home usage; above 750G scontrol hold PENDING tasks of these arrays; release only below 700G; never touch running jobs"\n}\n' > "$g/jobs.json"
  cat > "$g/governor.sbatch" <<EOF
#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=tierd-quota-governor
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=7-00:00:00
#SBATCH --no-requeue
#SBATCH --output=$g/governor.out
export PYTHONDONTWRITEBYTECODE=1
exec /cm/local/apps/python3/bin/python3 $g/quota_governor.py --dir $g --interval 600 --hold-above 750 --release-below 700
EOF
  sha256sum "$g/quota_governor.py" "$g/governor.sbatch" > "$g/governor.sha256"
  echo "prepared $g (not submitted): sbatch $g/governor.sbatch; then $0 submit $campaign ${campaign}/governor/jobs.json";;
status)
  python3 "$T" --campaign "$R/$1" --summarize;;
*) usage;;
esac
