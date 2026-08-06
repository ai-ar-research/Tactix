#!/usr/bin/env bash
# RV-S26: prove the new TACTIX_RANDEC knob is WIRED, at the source.
# `randec_enabled` was initialised true and never assigned, so SCHED-C7's random
# decision sequences had no off arm. A knob is only real if a counter moves:
# `c randec <sequences> <random decisions>` must be non-zero with the knob at its
# default/1 and exactly zero with it at 0, on the same instance and binary.
# No timing claim — counters only.
set -uo pipefail
cd "${SLURM_SUBMIT_DIR:-$(dirname "$0")/../..}" || exit 1
ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT" || exit 1
BIN=${BIN:-build/pin-rvs26/tactix}
CNF=${TMPDIR:-/tmp}/rv-s26-randec.${SLURM_JOB_ID:-local}.cnf

# a random 3-SAT at the phase transition: enough conflicts (>= randecinit=500)
# for a sequence to start at all
python3 - "$CNF" <<'PY'
import random, sys
random.seed(20260806)
n, m = 250, 1060
with open(sys.argv[1], "w") as f:
    f.write(f"p cnf {n} {m}\n")
    for _ in range(m):
        vs = random.sample(range(1, n + 1), 3)
        f.write(" ".join(str(v if random.random() < .5 else -v) for v in vs) + " 0\n")
PY

for arm in default 1 0; do
    case $arm in
        default) env -u TACTIX_RANDEC "$BIN" "$CNF" > "${CNF%.cnf}.$arm.out" ;;
        *) TACTIX_RANDEC=$arm "$BIN" "$CNF" > "${CNF%.cnf}.$arm.out" ;;
    esac
    printf 'arm TACTIX_RANDEC=%-8s %s | %s\n' "$arm" \
        "$(grep -E '^c randec' "${CNF%.cnf}.$arm.out")" \
        "$(grep -E '^s ' "${CNF%.cnf}.$arm.out")"
done
rm -f "$CNF" "${CNF%.cnf}".*.out
