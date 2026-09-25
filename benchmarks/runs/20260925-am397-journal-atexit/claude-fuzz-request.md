# AM397 journal exit lifetime — Claude Code Opus owner

Use Claude Code explicitly with `--model opus`; never Fable. You own immutable
campaign preparation, SLURM execution, independent model/proof grading,
bounded handoffs, and terminal `REPORT.md` under
`benchmarks/runs/20260925-am397-journal-atexit-claude-fuzz/`. Keep
`OWNER_STATUS.md` current. Do not edit maintained source/docs, mutate active
AM393/AM396 campaigns, submit Tier D, or claim combined N1--N4 completion.
Codex owns debugging. Source and comparator:

* Candidate commit `4039674b`, sibling `source.tar`, SHA-256
  `105b886f44cbbdb0f82280329fe5a513efd618e4ea0ffabbbe3baaa8a43ff421`.
* Parent commit `c58c1f68`, sibling `parent-source.tar`, SHA-256
  `dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9`.
* Cause, fixed parent-failing ASan trace, journal-byte comparison, and focused
  controls: `benchmarks/reports/2026-09-25-am397-journal-atexit-lifetime-checkpoint.md`.
  Verify candidate/parent archive hashes and the source diff before jobs.
  Pin independent checkers from AM369, not mutable aliases.

## Required affected-scope matrix

1. Build separate immutable Debug and ASan+UBSan parent/candidate binaries
   with journal and proof shadow enabled. Reproduce parent exit-time UAF and
   candidate clean exit on the two-session public API `Condition` fixture,
   including journal byte equality, replay success and extra-record rejection.
   Exercise explicit close/reopen and repeated public API sessions separately;
   the explicit close must still reset TLS, while implicit exit close must not
   access TLS. Include a multi-thread TLS-exit stress control if supported by
   the journal's stated thread-safety contract; do not assume process-wide
   concurrent journal writes are safe.
2. Vary legal journal selection and replay scenarios across default/random
   policy and fixed micro/formula seeds, with direct counts showing the journal
   path was reached. Freeze seeds, generator distributions, timeouts, build and
   checker hashes, resources, and acceptance rules before counted legs. Do not
   repeat passing unchanged AM393 or AM396 Factor-only legs.
3. For real solver cases, check every SAT model against the original CNF and
   every UNSAT proof through its complete pinned format-specific route (native
   DSR via elaboration and Trestle, startup PBP via VeriPB/CakePB, compatibility
   outputs as applicable); include known-bad model/proof controls. A crash,
   sanitizer error, journal replay mismatch, invariant failure, model/proof
   rejection, or unexplained verdict mismatch is a failure. Differential
   reference verdicts supplement, never replace, independent oracles.
4. Run real solver/fuzz legs through `sbatch` on `cpu-dense-hwulab-q` at full
   array width, no throttle/node constraint; use the project's normal
   50-physical-core fuzz contract unless a documented correctness/resource
   constraint requires a smaller nonaccepting checkpoint. Run harness selftest
   if any oracle/acceptance predicate changes. A fixed local microtest is okay.
5. Publish terminal `REPORT.md` with explicit affected-scope PASS/FAIL,
   generated/reached and checked SAT/UNSAT counts by leg, parent causal
   results, hashes, job IDs, timeouts/failures, and limitations. A failure
   hands Codex one bounded reproducer, command and failure class. Scoped
   success does not close universal N1/N2, N3/N4, retained tier gates,
   online-learning refactors, or the later Tier D comparison.
