# AM455 latest-source combined soundness fuzz — Opus owner request

Use Claude Code explicitly with `--model opus`, never Fable. Follow `CLAUDE.md`,
`DEVELOPMENT_PLAN.md` §8.2, `.claude/skills/fuzz/SKILL.md`, and
`.claude/skills/slurm-jobs/SKILL.md`. You own campaign preparation, immutable
pins, execution, triage, `OWNER_STATUS.md`, and terminal `REPORT.md` within
this directory only. Do not modify solver source, checkers, old campaign
artifacts, or the main worktree. Codex owns any bounded source debugging.

Current combined source is pushed commit `66fed7e5` (verify full Git SHA and
clean tracked tree). Read-only diagnostic Release/Debug binaries are
`build/pin-am455-sweep-retarget/tactix-{release,debug}`, SHA-256 respectively
`17d7fda73751a836dd090aa71c87b673c67b4bfd511238c9b4d1bd64603feb72`
and `057c033ba05f1c35ad109e79c0662fdc09895ba3c6fc15ff138082f471b07dd2`.
Rebuild all campaign modes from the immutable commit in your own directories
and compare relevant hashes before submitting. Use frozen AM369 canonical
checker pins, including `dsr-trim` SHA-256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`;
the repository-local dsr-trim clone is stale. Native DSR acceptance is the
no-option backward/core-trimmed DSR→LSR→Trestle route against the original
CNF, with forward every-lemma as an extra diagnostic. Never change checker
flags or classify a checker rejection as success.

This campaign supersedes stopped older AM427, AM444, AM452 and AM454 owners.
Use their frozen reports and bounded handoffs for controls, not as latest-source
acceptance. Cover the cumulative changed surfaces in one economical risk
matrix, avoiding duplicated random streams while recording enough independent
format/build/policy evidence:

* AM427 Sweep lifecycle and random/DefaultPolicy reachability;
* AM444 H1 LRAT target-specific core-unit chains and its bounded C1
  `handoffs/C1-sweep-retarget-lrat` API/CLI cases;
* AM452 H2 failed-literal unit plus later CCE/deletion, preserving every legal
  public X7 action and no-option native DSR checking;
* AM454 rejected SR/external-RUP additions amid a pending DSR unit; and
* AM455 binary-to-unit Sweep temporary LRAT target conditioning.

For C1, run the bounded API UNSAT case on current source and require a
complete original-CNF CakeLPR-verified proof; the SAT-path partial LRAT
regression in `tests/sweep_retarget_lrat.sh` is focused evidence only. Include
old-pin negative controls where useful, and do not silently accept a changed
verdict or skip a still-reachable public action. For all counted legs, validate
SAT models against original CNFs, UNSAT certificates through the canonical
route or explicit DRAT/LRAT compatibility oracles, Debug/ASan invariants,
differential verdicts, and direct reach counters for the changed branches.
Exercise legal default/random/stress schedules, uniform/structured/mutated/
circuit/degenerate inputs, and known-bad checker controls. Record all seeds,
distributions, source/binary/checker hashes, timeouts, resource shape, and
coverage denominators. Run harness selftest if any oracle/setup changes.

Use the normal full-width 50-physical-core SLURM shape unless a documented
correctness/resource constraint requires a smaller diagnostic. Submit no
array throttle or node constraint. Preserve and shrink failures and hand Codex
one bounded reproducer, exact command and failure class per cause. Pending
or failed legs never authorize promotion. Report the terminal verdict and
limitations only after all counted legs and independent oracles finish.
