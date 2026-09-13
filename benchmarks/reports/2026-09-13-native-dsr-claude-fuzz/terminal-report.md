# Native-DSR fuzz checkpoint — terminal audit of job 1688465

**Verdict: FAIL.** The canonical proof path (DSR → `dsr-trim` → LSR → Trestle
`srcheck`) did not accept 23 UNSAT cases in four independent legs. The DRAT
and native-LRAT compatibility legs passed. The model, differential, replay,
interrupt and sanitizer checks found no failure.

Scope: a read-only audit of `campaign/` (frozen harness, `out-1688465/`,
`slurm-1688465.log`) and of the pin's identity files and manifest. No job was
submitted, no solver or checker was run, and no campaign output was changed.
**Observed** means read directly from an artifact. **Inferred** means reasoned
from those artifacts but not directly shown by them.

## Job and pins (observed)

| Item | Value |
|---|---|
| Job | 1688465, `fuzz-nativedsr-8ad66789`, one non-array allocation on node018: 100 CPUs, 400000 MiB, 1:30:00 limit |
| Terminal state | wrapper `work exit=0 elapsed=839s`; job COMPLETED with exit 0 (as given). No `signal.txt`, so no USR1/TERM path ran |
| Source pin | `8ad667892abe2f73238f8643a345e36cc1224c0d` (a `git stash create` of the working tree on `c37aae03`, not a branch commit), tree `27dfc883481a74efd1a4c87a0ce911d2a58439f9` |
| Build pin | `build/pin-dsrfuzz-8ad66789`, `MANIFEST.sha256` = `0260bf6a…fc11b97` (4239 entries), `sha256sum -c` passed at job start |
| Release `tactix` | GCC 13.1.0 `-O3`, sha256 `eee6fc0d…4f33c6a5c` |
| Debug `tactix` | `-O1 -g -fsanitize=address,undefined`, `TACTIX_ENABLE_PROOF_SHADOW=ON`, sha256 `5a7a2710…646c7fa50` |
| Harness | All 9 files in `campaign/` hash-identical to the manifest's `harness/` entries (checked in this audit) |

Checker identities (manifest sha256): `dsr-trim` `114b3662…` (ffe486a4),
`dsr-compress` `34022e18…`, `srcheck` `f0eef5c4…` (Trestle 03ac9418, the
retained pin), `drat-trim` `f2cb7d45…`, `cake_lpr` `ab9dc6c4…`, `lrat-check`
`0c9a87d5…`, `kissat` `3aa8e41f…` (differential oracle).

## Results by leg (observed)

| Leg | Scope | Outcome |
|---|---|---|
| Harness self-test | 571 checks on planted-defect controls (the `FAIL` lines are expected detections) | `HARNESS SELFTEST OK`, exit 0 |
| Stock `selftest.sh` | Kissat 150 / CaDiCaL 100 clean. Planted lie-UNSAT-as-SAT, negated-model and truncated-proof defects all flagged; ddmin passed; duplicate-literal LRAT control passed | `SELFTEST OK` |
| `test_modelcheck.py` | — | exit 0 |
| Focused tests, Release + Debug | `test_proof`, `test_api_c`, `test_random_online`, `trace_replay` | 8/8 exit 0, 0 `FAIL` lines, 0 sanitizer lines |
| audit `regress` | 8 cases, 134 programs, 5 replays | 0 failing, no missing reachability, exit 0 |
| audit `interrupt` | 79 cases | 0 failing; **exit 1: 2 required reachability keys missing** (see weaknesses) |
| audit `random` | 4000 cases, 167613 programs | **4 failing (DSR `status=error stage=dsr-trim`)**, no missing reachability, exit 1 |
| audit `default` | 3000 cases | **3 failing (2 DSR, 1 DSRA; `status=error stage=dsr-trim`)**, no missing reachability, exit 1 |
| audit `progs` | 1000 cases | 0 failing, no missing reachability, exit 0 |
| fuzz.py `random-dsr` | corpus 30/30; n=1500, 1339 reached a verdict (161 hit the 10 s timeout), `--require-reached ParityModel` (137) | **9 `proof` failures: "dsr-trim did not verify and elaborate DSR"**, exit 1 |
| fuzz.py `default-dsr` (CLI default output) | corpus 30/30; n=1200, 1186 reached a verdict | **7 `proof` failures (same message)**, exit 1 |
| fuzz.py `random-drat` | corpus 30/30; n=600, 531 reached a verdict (UNSAT 215) | 0 failures |
| fuzz.py `random-lrat` | corpus 30/30; n=600, 531 reached a verdict (UNSAT 224) | 0 failures |
| fuzz.py `corpus-dbg-dsr` (ASan/UBSan) | 30 entries | 0 failures; 1 non-terminating (`026-sweep-drat-admission-gate`: its proof oracle did not run) |

Proof verdicts in the audit modes that passed all checks (random / default /
progs / regress):

- DSR verified: 915 / 599 / 60 / 2
- DSRA verified: 326 / 188 / 22 / —
- DRAT verified: 480 / 378 / 28 / 2
- LRAT verified: 484 / 384 / 30 / 2

DSR checker `error`: random 10 checks across 4 cases, default 6 checks across
3 cases. Checker timeouts: 0 in every mode. `dsr_proofs_with_sr_witness` = 0
everywhere, so no proof in this campaign exercised a non-RAT SR witness.

Models and differential (observed):

- `model_ok`: random 6623, default 5025, progs 884.
- Kissat agreement: random 3682/3682, default 2990/2990, progs 512/512,
  regress 8/8.
- Debug and Release identical: default 2990, progs 1000.

## Specific checks

- **Online-random effective transitions (observed).** 3077 of 4000 random
  cases had an effective program transition.
  - Debug and Release replays each 3682/3682 identical; transition sequences
    identical 3682 (`replay:transitions_identical` feature 3152).
  - All 82 required random-mode keys were reached, including `Lucky`
    instances 6 and 7, every xor/one-hot family drawn and checked, and parity
    and one-hot SAT-model acceptance with identical replays.
  - `progs`: determinism and Debug/Release identity 1000/1000; all 20
    required keys reached.
- **Replay (observed).** `regress` reproduced the recorded cases
  913501804 / 913503173 (CNF sha matches job 1686850) with identical Debug
  and Release replay and transitions. `trace_replay.sh` passed on both builds.
- **Negative controls (observed).** All required controls fired and were
  rejected:
  - Random mode: DSR empty-only 234, DSR constructed/truncated
    no-conflict 234/223, LSR hints-removed 234, DRAT 119/115, LRAT
    hints-removed/truncated 241/236, corrupted models 2723, corrupted replay
    verdicts 198/198.
  - `progs`: malformed one-hot tuples, invalid one-hot parameters and
    wrong-rule parameters rejected 125/125 each.
  - Controls that could not apply were counted separately
    (`*_inapplicable_*`) and not credited.
- **Interruption accounting (observed).** Every SIGTERM outcome is accounted
  for:
  - Random mode: Release 93 SIGTERMs = 92 clean UNKNOWN + 1 verdict at
    SIGTERM; Debug 55 = 54 + 1; timeout arms Debug 318 = 317 + 1, Release
    199 = 199.
  - Interrupt mode: sweep Release 48/48 and Debug 6/6 clean UNKNOWN, 24 traces
    exhausted before the SIGTERM, F2 fixed plus three sweep fractions clean.
  - Maximum termination latency: Release 326 ms, Debug 1955 ms, F2 1367 ms.
- **Sanitizers (observed).** ASan/UBSan Debug binary ran in all audit modes
  and in the corpus leg. No failure of class `crash` (whose alarm strings
  include `Sanitizer` and `runtime error`) was recorded; focused Debug tests
  showed 0 sanitizer lines. *Limitation:* only failure summaries were
  promoted, not full per-case stderr.
- **Scratch and residue (observed).** `scratch_exists=no`;
  `new_entries_vs_job_start=0`; `user_entries_newer_than_start=0` on node018.
  Promoted output totals about 320K. The bounded reproducer is one bundle
  (3 KB CNF plus outputs); `bundles_available=6`, matching the 3+3 kept audit
  bundles, and `fuzzpy_bundles=16`, matching the 9+7 fuzz.py failures.

## Blocking weakness

1. **Native DSR is not accepted (N2, canonical format).**
   - *Observed:*
     - 23 UNSAT cases, in both the random-policy and default configurations
       and in both builds, ended with `dsr-trim` status `error`. Status
       `error` means neither `s VERIFIED UNSAT` nor a recognized rejection
       token appeared.
     - The DRAT and LRAT legs on the same generators had 0 failures.
   - *Reproducer:* `reproducer/` (`d000848`, seed 914600848) is UNSAT by
     propagation alone (0 decisions, 0 conflicts), with Release and Debug
     output byte-identical. Its CNF has 264 clauses:
     - 10 with duplicate literals
     - 7 tautologies
     - 3 duplicate clauses
   - *Inferred, not established:* the failure may be in how `dsr-trim` or
     Tactix's DSR stream handles normalized or restated input clauses. The
     proof file and the checker's stdout/stderr were not promoted, so whether
     Tactix or the checker is at fault is undetermined. Either way, DSR
     acceptance cannot be claimed at `8ad66789`.

## Non-blocking weaknesses

2. **Interrupt reachability key mismatch (harness defect).**
   - *Observed:* `audit.py` requires `interrupt:sweep_{unlimited,finite}_interrupted`.
     The credit path emits only the `…_interrupted_any` and per-format keys,
     and those were reached 18 times each (6 each for DSR, DRAT and LRAT).
   - The behaviour was exercised. The mode's failure verdict comes from key
     naming, but the required-key contract must be fixed before it can count
     as a gate.
3. **Coverage gaps.**
   - No DSR proof carried an SR witness, so non-RAT SR steps went unexercised.
   - `VariableCompaction` never fired in the random legs (known to be
     unreachable there).
   - Timeouts removed 10.7–11.5% of random fuzz.py cases from all oracles.
   - Corpus entry 026 did not reach its proof oracle under Debug.
4. **Memory cap not applied to the audit modes.** *Inferred from grep:*
   `audit.py` does not reference `capwrap.sh`, and no `capwrap.log` was
   promoted, so the 16 GiB resident cap did not apply to the audit modes.
   Observed peak RSS was at most 872 MB, so this had no effect here.

## Terminal verdict

**FAIL** for job 1688465 at source `8ad66789` and build manifest `0260bf6a…`.

- The pin, harness identity, compatibility-proof, model, replay,
  negative-control, interruption, sanitizer and residue checks are clean.
- The checkpoint fails on its central obligation: 23 native-DSR `dsr-trim`
  errors.
- Next step: reproduce `d000848` against the pinned `dsr-trim`, keeping the
  proof and the checker output, and classify the fault as solver or checker
  before any rerun. Separately, fix the interrupt required-key names.
