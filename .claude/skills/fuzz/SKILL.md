---
name: fuzz
description: Run or extend Tactix soundness fuzzing, investigate and shrink failures, validate checker reachability, or preserve regression cases. Use after kernel/rule/proof changes and whenever a verdict, model, proof, invariant, harness, or oracle is suspect.
---

# Soundness fuzzing

Harness commands, seed counts, timeouts, and tool paths live in
`tools/fuzz/README.md` and the harness itself. This skill defines coverage and
acceptance.

Follow the Claude-owned fuzzing contract in `DEVELOPMENT_PLAN.md` §8.2 and
`CLAUDE.md`. Codex supplies the immutable solver snapshot, scope, resources and
acceptance criteria; Claude owns preparation through the terminal report. Codex
may monitor job status but does not inspect the campaign or reinterpret the
owner report. Keep campaigns in the background while independent work proceeds.
A failure hands Codex one bounded reproducer, command and failure class for
direct debugging; pending fuzz never authorizes promotion.

## Choose legs by risk

Cover each changed surface with a risk matrix:

| Risk | Required evidence |
|---|---|
| SAT semantics or reconstruction | independent original-CNF model check |
| UNSAT reasoning or proof emission | native proof checker and negative proof |
| rule scheduling or guards | debug invariants, random policy, targeted scenario |
| public execution or scheduling | DefaultPolicy and the public-interface random client; plan §§8.2/9.4 acceptance |
| parser/frontend | malformed and adversarial DIMACS corpus |
| harness/oracle/setup | `tools/fuzz/selftest.sh` with known-bad artifacts |

Differential verdicts against pinned reference solvers supplement, but never
replace, independent model and proof validation.

## Public-policy campaigns

Use `DEVELOPMENT_PLAN.md` §5.6 as the action-space authority and §8.2 as the
acceptance authority; do not duplicate the action catalog in this skill.
Include separate, explicitly named DefaultPolicy and public-random legs on the
candidate pin. The latter must be `tactix-public-random`, not a private or
production random/stress/controller policy.

For shared execution/feedback preparation, use §9.4's fresh seed minimums and
targeted partial/full restart, continuation and interruption requirements.
Retained old reports alone do not accept a changed executor.

Before launch, attach the action-space version, policy configuration, seed
schedule and coverage matrix with declared minimums to the owner request.
Use targeted scenarios where unconstrained randomness cannot reliably reach a
required path. Counters/traces must distinguish requests, eligible execution,
effective changes and stop outcomes. Report each policy's proof/model and
invariant results separately, plus replay/observation checks and achieved versus
required coverage. A zero-failure run with missing required reach is incomplete.
After a repair, validate affected cases and rerun the affected legs/coverage;
retain passing unchanged evidence only when its snapshot and scope still apply.
The terminal owner report must explicitly disposition both policy legs and any
coverage gaps before public-execution acceptance.

Large feature-specific targeted campaigns are mandatory only when the candidate
changes their owning feature, scheduling or guards, or feature-specific harness
or oracle. Otherwise run the compact deterministic regression and reuse matching
immutable evidence. Retired one-off stress/controller scripts are not release
legs; promote their stable failures into the corpus or compact tests.

## Procedure

1. Record SHA, build mode, seed range, generator distribution, policy/scenario,
   timeout, and checker versions.
   Acceptance legs freeze the core checkers with `slurm.sh --checkers DIR`;
   `tools/fuzz/README.md` owns the collection format and replay selection.
2. Prove reachability with direct counters or traces for the changed rule/path.
   Similar verdict distributions are not reachability evidence.
3. Treat a crash, sanitizer finding, invariant failure, model rejection, proof
   rejection, or unexplained verdict disagreement as a failure.
4. Preserve the original artifact and metadata, then shrink while retaining the
   same oracle failure. Add the minimal stable case to `corpus/regressions/`.
5. Keep corpus cases by default. Deduplicate only deliberately when two cases
   exercise the same root cause and the retained case preserves coverage.
6. Run `selftest.sh` whenever an oracle, harness accept condition, checker
   invocation, or setup changes; require known-bad inputs to fail for the
   intended reason.

Solver fuzz legs run through `slurm-jobs`; fixed harness self-tests may run in
the session shell.
