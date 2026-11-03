# Checker plumbing shared by the proof/model CTest scripts; source it after
# `set -euo pipefail`. Every multi-step helper chains its steps with
# `|| return` (keeping the failing step's status, so a checker's own exit
# status still decides skip versus failure) because set -e is suspended in a
# function called under if, || or &&. Each check keeps the checker argv and
# acceptance predicate the scripts used before.

# checker_paths ROOT: the pinned checker binaries under ROOT; an environment
# value of DSR_TRIM, DSR_COMPRESS, SR_CHECK, DRAT_TRIM, CAKE_LPR, VERIPB,
# CAKE_PB or CAKE_PB_CNF takes precedence. The model checker is not
# overridable.
checker_paths() {
  DSR_TRIM=${DSR_TRIM:-$1/tools/checkers/bin/dsr-trim}
  DSR_COMPRESS=${DSR_COMPRESS:-$1/tools/checkers/bin/dsr-compress}
  SR_CHECK=${SR_CHECK:-$1/tools/checkers/bin/srcheck}
  DRAT_TRIM=${DRAT_TRIM:-$1/tools/checkers/bin/drat-trim}
  CAKE_LPR=${CAKE_LPR:-$1/tools/checkers/bin/cake_lpr}
  VERIPB=${VERIPB:-$1/tools/checkers/bin/veripb}
  CAKE_PB=${CAKE_PB:-$1/tools/checkers/bin/cake-pb-cnf}
  CAKE_PB_CNF=${CAKE_PB_CNF:-$1/tools/checkers/bin/cake-pb-cnf}
  CHECKMODEL=$1/tools/fuzz/checkmodel
  PROOF_ORACLE=$1/tools/bench/proof_oracle.py
}

# require_tools MESSAGE TOOL...: skip (exit 77) unless every TOOL is
# executable, printing MESSAGE first when it is not empty.
require_tools() {
  local message=$1 tool
  shift
  for tool in "$@"; do
    if [[ ! -x "$tool" ]]; then
      [[ -z "$message" ]] || echo "$message"
      exit 77
    fi
  done
}

# make_scratch NAME: a fresh temporary directory in variable NAME, removed
# when the script exits. Call it directly, never through $(...): the EXIT
# trap must belong to the script's own shell.
make_scratch() {
  local directory
  directory=$(mktemp -d) || exit
  printf -v "$1" '%s' "$directory"
  trap 'rm -rf "'"$directory"'"' EXIT
}

# check_dsr CNF PROOF: the proof oracle checks every DSR lemma.
check_dsr() {
  python3 "$PROOF_ORACLE" check-dsr "$1" "$2" \
    --dsr-trim "$DSR_TRIM" --dsr-compress "$DSR_COMPRESS" \
    --srcheck "$SR_CHECK" --every-lemma
}

# check_dsr_full CNF PROOF: the proof oracle's full DSR/DRAT/CakeLPR check.
check_dsr_full() {
  python3 "$PROOF_ORACLE" check "$1" "$2" \
    --dsr-trim "$DSR_TRIM" --dsr-compress "$DSR_COMPRESS" \
    --srcheck "$SR_CHECK" --drat-trim "$DRAT_TRIM" --cake-lpr "$CAKE_LPR"
}

# dsr_valid CNF DSR LSR TRIM_LOG SRCHECK_LOG [TRIM_FLAG]: dsr-trim -f -c
# elaborates DSR into LSR and srcheck verifies LSR; both print `s VALID`.
dsr_valid() {
  "$DSR_TRIM" -f -c ${6:+"$6"} "$1" "$2" "$3" > "$4" 2>&1 || return
  grep -q '^s VALID$' "$4" || return
  "$SR_CHECK" "$1" "$3" c > "$5" 2>&1 || return
  grep -q '^s VALID$' "$5"
}

# drat_derivation LOG CNF PROOF [FLAG...]: drat-trim accepts a nonrefuting
# derivation, which it reports with exit status 1.
drat_derivation() {
  local log=$1 status=0
  shift
  "$DRAT_TRIM" "$@" > "$log" 2>&1 || status=$?
  [[ "$status" -eq 1 ]] || return
  tr -d '\r' < "$log" | grep -q '^s DERIVATION$' || return
  grep -q 'VERIFIED derivation: all lemmas preserve satisfiability' "$log"
}

# reject LOG MESSAGE CHECKER ARG...: CHECKER must not accept; when it does,
# print MESSAGE and fail. The caller then matches LOG's rejection reason.
reject() {
  local log=$1 message=$2
  shift 2
  if "$@" > "$log" 2>&1; then
    echo "$message"
    exit 1
  fi
}
