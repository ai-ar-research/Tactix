# HANDOFF — FirstOpen owner (current state, 2026-10-08)

**Terminal: c2 PASS** — see `FINAL_REPORT.md` / `status.json`. No jobs running. b26 and c1 SUPERSEDED (`SUPERSEDED.md`).
Disposable when root no longer needs them (no job references them): owner build pins
`build/pin-fo-fuzz-lib-{rel,asan}`, `build/pin-fo-fuzz-drv1`, `build/pin-fo-fuzz-diag-am23` (b26),
`build/pin-fo-fuzz-c1stg-*`, `build/pin-fo-fuzz-c1-drv2`; extracted trees `src-fo`, `src-d576stg`, `src-c1stg`,
`src-c1`, `src-c2stg` and the matching `harness-root*` (c2 trees/pins back the final report).

The section below is the earlier (b26-era) handoff, kept for history.

---
# HANDOFF — FirstOpen owner (Claude session 385a3588), stopped 2026-10-07 at user request

State: **no jobs running or pending** from this owner. No commits/pushes. No production edits.
b26 / archive `ab70cbb5…` / Release `b26efc0f…` is **SUPERSEDED / INCOMPLETE** (`SUPERSEDED.md`, `status.json`).
Nothing is accepted. Next step needs root's exact new source + Release binding (r3 focused nonforcing fixture failed;
do not adopt r3 or any unbound source).

## Product findings
1. **am-rel-23** (`handoff/am-rel-23/README.md`, sha list `handoff/am-rel-23/SHA256SUMS`): well-formed persistent
   `AffineMatrixInitialize` root-maintenance request (op 3) returns `InvalidRequest` after a root Sweep at level 0.
   Deterministic; also on 1e8295106 (job 1755494); traced in 1755508 (`diag/am-rel-23/`). Root carries it as a
   required open API repair, **not a soundness failure**. On the new candidate: rerun the reproducer first; until
   repaired, AM rows hitting it stay FAIL (never reclassified).
No soundness finding (verdict/model/proof) was observed anywhere.

## Evidence kept (b26, informational only — does not transfer)
- Selftest 1755497: harness `selftest.sh` **OK** + helper-free oracle selftest OK (`status/oracle-selftest-1755472/`,
  logs `logs/selftest-*`): no dsr-transport, fallback reached, bare-empty / truncated / foreign proofs and falsified
  models rejected; one half-truncation acceptance classified UP-legitimate. Source-independent only if the new
  archive's `tools/fuzz`, `tools/bench`, `tests` bytes are unchanged — otherwise rerun `tools/v2/selftest.sbatch`.
- Calibration smoke 1755466/1755467 (`targeted/out-smoke1`, `targeted/out-afsmoke1`, `status/smoke1-summary.json`).
- Broad legs 1755222/3/4 cancelled while PENDING (0 cases); their harness snapshots kept unmodified in
  `harness-root/build/pin-fuzz-harness-*`.
- Immutable pins (do not mutate): `build/pin-fo-fuzz-lib-{rel,asan}`, `build/pin-fo-fuzz-drv1`,
  `build/pin-fo-fuzz-diag-am23`; `targeted/snap-s1`. Disposable once no record needs them.

## Unmet coverage (all of `COVERAGE.md`, for the new candidate)
Every row is unmet: BD/BC/BG broad legs (fresh 10k/10k/5k), SRP, FE, GL/GW, GLC, GCTL, RO, AF/SR/AM/ACTL, AFL,
RS/RSX, OF, RF, ORACLE, ASAN. Plus, per the transition request, **targeted single-unit public/default/replay/proof
paths** (certified singleton forcing units now always published; live-unit binary cuts counted as forcing).
Calibration gaps already seen on b26 and their harness repairs (untested):
| Gap on b26 smoke | Repair prepared |
|---|---|
| residual handoff never fired (0/44) | none needed in harness; expected to change with unit admission — verify |
| srctl: 0 graph-producer SR cut requests | `srdrv --emit-actions` typed FirstOpen/LargestOrbitLast collection; srctl uses it on odd seeds; request sample stored per row |
| Window never resumed across slices | gldrv: half of continuations under `ScopedRuleWorkBudget` (Default's slice shape) |
| CLI fastel eliminated 1/30 | `fe_fixtures` `*-hard` variants (near-threshold random base); fecli uses them, fedrv on 1/3 of rows |
| GLC/FE counts marginal | glc 60, fe 60, fecli 45 per scale unit |
| smoke/full seed blocks overlapped numerically | full `2720000000+fam·1e6`, smoke `2760000000+…`, AFL `2770000000`/`2775000000`, broad `2780000000`/`2780500000`/`2781000000` |
**Not implemented:** `srdrv --mode public` (public executor run of the shipped structured graph program with
native vs wire-roundtrip proof-bound twin, per-call root-unit delta, then checked Default solve) — needed for the
single-unit public/replay path; design notes in this session, API `OnlineStrategyExecutor::configure/advance`.
**COVERAGE.md must be re-declared for the new candidate before launch** (new seeds above, single-unit rows, new
source/Release hashes); the current file describes b26.

## Exact owned changed paths (all under this campaign dir unless noted)
- Records: `COVERAGE.md` (b26 declaration, sha in `status/coverage-declared.sha256`), `IN_FLIGHT.md`, `SUPERSEDED.md`,
  `status.json`, `HANDOFF.md`, `status/*`, `logs/*`, `legs/submit-*`, `pins/*`, `diag/am-rel-23/*`, `handoff/am-rel-23/*`.
- Frozen trees: `harness-root/` (manifest `pins/harness-root.sha256`), `src-fo/` (`pins/src-fo.sha256` 676c97da…).
- b26 scripts (executed, keep as record): `tools/{setup_harness.sh,verify_pin.py,submit_legs.sh,build_libs.sbatch,
  build_drv.sh,build_drv.sbatch,selftest.sbatch,summarize.py}`, `targeted/{make_snapshot.sh,job.sh,tgt.sbatch}`.
- Candidate-parameterized pipeline (dry-run verified deterministic; never submitted): `tools/v2/{prepare_candidate.py,
  verify_pin.py,build_libs.sbatch,build_drv.sh,build_drv.sbatch,make_snapshot.sh,job.sh,tgt.sbatch,selftest.sbatch,
  submit_legs.sh}`.
- Owner harness sources: `targeted/src/{tgt.py,aftgt.py,af_plan.py,gl_fixtures.py,affine_fixtures.py,affine_extra2.py,
  sr_fixtures.py,fe_fixtures.py,oracle_selftest.py,emit-fixed-model.sh,profiles/}`.
- Drivers `tools/drv/`: slotdrv, amdrv2, afdrv (unchanged parent copies), fedrv (as built in drv1), **gldrv and srdrv
  edited after drv1** (Window slices; `--emit-actions`) — not yet compiled; the v2 driver build compiles them.
- Outside the dir: `build/pin-fo-fuzz-lib-rel`, `build/pin-fo-fuzz-lib-asan`, `build/pin-fo-fuzz-drv1`,
  `build/pin-fo-fuzz-diag-am23`; `tools/fuzz/failures/checkers/895f1989…` pre-existed (read only).

## Resume recipe (new session, after root's binding)
1. `python3 tools/v2/prepare_candidate.py <binding.json> <tag>` (verifies archive + Release hashes, refuses
   native transport, freezes `harness-root-<tag>/`, `src-<tag>/`).
2. Re-declare `COVERAGE.md` for `<tag>`; implement `srdrv --mode public`.
3. `sbatch tools/v2/build_libs.sbatch tools/v2/candidate-<tag>.json` (must reproduce the bound Release; exits 5 if not),
   then `sbatch --dependency=afterok:… tools/v2/build_drv.sbatch tools/v2/candidate-<tag>.json`, and
   `sbatch tools/v2/selftest.sbatch tools/v2/candidate-<tag>.json`.
4. `tools/v2/submit_legs.sh tools/v2/candidate-<tag>.json` (broad legs, dense partition, full width).
5. `tools/v2/make_snapshot.sh tools/v2/candidate-<tag>.json s1`; smoke `sbatch --array=0-3 tools/v2/tgt.sbatch <tag>-s1 <sha> tgt.py smoke smoke-<tag>`;
   rerun the am-rel-23 reproducer; then full profile (array shards on cpu-q) and `aftgt.py full`.
6. Summarize with `tools/summarize.py`; publish FINAL_REPORT.md/status.json.
