# Tactix — proof-rule-driven SAT solver

`DEVELOPMENT_PLAN.md` owns stable scope, architecture, roadmap, and gates.
`PROGRESS.md` owns current status and the next step. Rule semantics live in
`docs/calculus.md`; implemented guards and deviations in
`docs/implementation-notes.md`; evidence in frozen reports. One fact has one
home; other documents link to it.

## Non-negotiables

- **N1:** every applicable proof rule, including branch variable and phase, is
  selectable through the public API at its declared action boundary. Search
  rules operate during search; inprocessing rules, Lucky, and Walk operate at
  level zero after an explicit Restart-to-root transition.
- **N2:** every UNSAT path emits a machine-checkable proof and every SAT path
  emits a valid reconstructed model. Native DSR is Tactix's canonical proof
  output: `dsr-trim` elaborates it to LSR and the pinned Trestle checker verifies
  that LSR. DRAT and native LRAT are compatibility outputs, not requirements for
  new Satsuma-derived rules.
- **N3:** DefaultPolicy must be competitive with Kissat on Tier D.
- **N4:** DefaultPolicy's scheduling must be at least as sophisticated as
  Kissat's; close or explicitly accept every parity-ledger deviation.

DefaultPolicy starts from faithful Kissat/CaDiCaL mechanisms. Read both; use
Kissat as the tie-break when equally strong, preserve a transliterated baseline
when adding something stronger, and never copy reference code verbatim. An
unbacked constant must be removed or derived, not tuned. N1 protects API
capability, not DefaultPolicy's choice to decline an offered action.

## Repository rules

- Keep everything under this repository. Work directly in the root checkout
  unless the user explicitly requests an isolated worktree; never leave
  authoritative changes under `.claude/worktrees/`. Reference clones are
  untracked; put patched variants in `tools/solvers/variants/`, probes in
  `benchmarks/probes/scratch/`, and temporary job output in `$CLAUDE_JOB_DIR/tmp`.
- All solver executions use SLURM through the `slurm-jobs` skill. Submitted
  arrays own immutable `build/pin-*` directories; a simultaneous Tier B/C pair
  needs two builds from the same commit with matching binary hashes.
- Submit benchmark arrays at full width: do not set an array-task throttle or a
  node constraint. A throttle is allowed only when the user explicitly asks for
  one or a documented correctness/resource constraint requires it; record that
  reason with the run.
- Plan edits use `revise-plan`; rule/guard edits use `calculus`; completed work
  updates PROGRESS in the same commit. Never edit frozen design, report, or log
  artifacts.
- Before creating documentation, consult `docs/README.md`. New technical
  evidence goes in `benchmarks/reports/`; Andrew-facing LaTeX/PDF deliverables
  go in `report-to-andrew/` through `generate-report`.
- Whether you are CLAUDE or Codex, you should use the skills in .claude/
- Codex must invoke Claude Code for fuzzing; Claude owns execution and reporting.
  Follow `DEVELOPMENT_PLAN.md` §8.2 and `fuzz`; Codex retains direct debugging.

## Work loop

1. Read the PROGRESS resume block and take its first actionable item.
2. Implement the smallest complete increment using the matching project skill.
3. Validate correctness with the required proof/model oracles; make performance
   claims only from the benchmark protocol.
4. Keep PROGRESS factual and concise; revise the plan instead of silently
   diverging from it.
5. Run `wrap-up` before stopping. It validates, generates Andrew's report,
   commits and pushes coherent work, and leaves a self-contained next step.
   Documentation compaction happens only when explicitly requested.

Project skills: benchmarks, calculus, codex-revisit-docs, compact-docs, compete,
debug-performance, fuzz, generate-report, multi-agent, proof-check, revamp,
revise-plan, revisit-codebase, rule-dev, slurm-jobs, stress-policy, wrap-up.
