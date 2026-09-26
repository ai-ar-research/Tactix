# AM468: fixed-reference gate checker pins

Date: 2026-09-26. Tooling-only checkpoint; candidate solver source stays at `1d81d353`. No Phase 1 or N1--N4 gate result is claimed.

`tools/bench/fixed_runtime.py prepare` previously copied `tools/checkers/bin/dsr-trim` (SHA-256 `02824056...`) and omitted `dsr-compress` and `cake_lpr`. The runtime requires those latter pins for native DSR, while the frozen AM369 canonical `dsr-trim` is `3b5c7737...`. An attempted fixed-reference campaign would have failed its pin contract before solving; copying from the mutable path also risked checker drift.

Preparation now requires an explicit `--checker-pins` directory inside the repository, validates all five canonical DSR/DRAT checker SHA-256 values before creating a campaign, copies that complete bundle, and records the checker authority in the manifest. The current frozen bundle is `benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers/`; all five hashes were rechecked against the constants. The native DSR→LSR→Trestle route and checker flags are unchanged.

The new cross-component test prepares a minimal campaign and passes its manifest to `runtime_task.validate_runtime_pin_contract`; it also rejects a changed checker before campaign creation. The focused runtime tests pass 36/36, and `tools/fuzz/selftest.sh` passes, including its planted model/proof failure controls. This is a preparer repair, not evidence that the pending Rank 1--50 or B28+C5 candidate gates pass.

For a future Phase 1 preparation, pass `--checker-pins benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/pins/checkers` alongside the pinned candidate authority and reference. Verify the finished campaign's native DSR proofs and original-CNF SAT models before scoring. The latest-source Opus fuzz owner and full Debug job remain independent acceptance inputs.
