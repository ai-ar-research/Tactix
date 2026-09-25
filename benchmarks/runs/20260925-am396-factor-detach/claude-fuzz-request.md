# AM396 detached-H Factor extension — Claude Code Opus owner

Use Claude Code explicitly with `--model opus`; never use Fable. You own
immutable campaign preparation, SLURM execution, independent model/proof
grading, bounded handoffs, and the terminal report. Put all owner artifacts
under `benchmarks/runs/20260925-am396-factor-detach-claude-fuzz/`. Do not edit
maintained source/docs, mutate another campaign, submit Tier D, or claim
combined N1--N4 completion. Keep `OWNER_STATUS.md` current. Codex owns debugging
and maintained repairs. The AM393 Opus campaign is independently live on its
own frozen source; do not inspect or modify its working campaign.

## Frozen inputs and comparator

* Candidate commit `0059a9e6`, sibling `source.tar`, SHA-256
  `7f9c0a3fc8abd95308797168c58fee8addf6b0314dbd1946f666b0bd2a4c9202`.
* Parent commit `83ee5124`, sibling `parent-source.tar`, SHA-256
  `f161ccbcca932249ef50a0e9283a0cab50da44d3f4d1ca2885bea9449bccc07a`.
* Cause, old-failing test-object control, and local checks:
  `benchmarks/reports/2026-09-25-am396-detached-factor-extension-checkpoint.md`
  and `tests/test_factor.cpp`. The source diff is confined to
  `src/kernel/kernel.cpp`, `tests/test_factor.cpp`, and
  `docs/implementation-notes.md`; other archive differences are AM395/396
  reports and run metadata. Verify this and all archive hashes before jobs.
* Freeze/check SHA-256 of independent checker pins from
  `benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/` or
  byte-identical AM341 pins. Never use mutable `tools/checkers/bin` aliases.

## Required affected-scope matrix

1. Build separate immutable Release, Debug, and ASan+UBSan binaries from both
   source archives. Reproduce parent failure and candidate success on the
   exact attach–detach–Factor–reattach path, including the variant that
   compacts away an unactivated input identity before detached Factor. Preserve
   the old-failing assertion and the repaired compact order/inverse/active
   counts. The parent may need the candidate fixture linked against parent
   objects; do not let assertion-disabled Release substitute for Debug.
2. Generate varied *internal-only* kernel scenarios that first seed a compact
   population, detach H, then create one or several extensions through Factor
   or the shared `kernel_new_extension_vars` support path. Vary import and
   activation order, header-only/root-fixed/retired variables, compaction
   before and after extension, repeated detach/reattach, and focused/stable
   decision/phase updates. Require direct changed-path counters and check
   compact order/inverse uniqueness, extension external identities, and active
   population. Distinguish this internal seam from public N1 reach.
3. Cover public Factor under default/random/explicit legal strategies where H
   is attached, to ensure ordinary append behavior and proof/model routes
   remain unchanged. Every SAT verdict needs an independent original-CNF
   model check; every UNSAT verdict needs the full format-specific original-CNF
   checker route (native DSR via pinned elaboration and Trestle, LRAT/DRAT
   compatibility when applicable), with known-bad certificate/model controls.
   Differential reference verdicts supplement, not replace, independent
   oracles. Any crash, sanitizer error, invariant failure, model/proof
   rejection, or unexplained verdict mismatch is a failure.
4. Freeze seed ranges, generators, scenario/policy matrix, timeouts, resources,
   checker hashes, builds, and acceptance rules before counted legs. Use
   meaningful randomized coverage beyond the fixed regression and prove reach
   by transition counters. Run real solver/fuzz legs through `sbatch` on
   `cpu-dense-hwulab-q`, full array width, no throttle or node constraint.
   Use the project's 50-physical-core fuzz contract unless a documented
   correctness/resource constraint requires a smaller nonaccepting checkpoint.
   Do not repeat an unchanged passing AM393 journal-only leg; journal code is
   unchanged. Run harness selftest if an oracle/acceptance predicate changes.
5. Publish terminal `REPORT.md` with explicit affected-scope PASS/FAIL,
   generated/reached/applied/declined and checked SAT/UNSAT counts by leg,
   parent causal results, hashes, job IDs, timeouts/failures, and limitations.
   A failure hands Codex one bounded reproducer, command and failure class.
   No scoped verdict closes universal N1/N2, N3/N4, retained Rank/B+C/WV
   gates, online-learning refactors, or the later Tier-D comparison.
