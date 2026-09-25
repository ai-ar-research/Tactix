# AM427 Claude-owned affected fuzz — Sweep pass-state lifecycle

Owner: Claude Code, explicitly `--model opus` (never Fable). Use immutable
candidate source `60abda17` and exact parent `9c6d6be9`; do not use the
mutable root checkout as a solver or harness pin. This is affected-scope
correctness and state-machine reachability fuzz, not combined N1--N4
acceptance. Preserve other owners, jobs, and their pins. Do not edit solver
source, `PROGRESS.md`, reports outside this campaign, or commit/push.

Read `CLAUDE.md`, `DEVELOPMENT_PLAN.md` §8.2, and repository `fuzz`,
`proof-check`, and `slurm-jobs` skills. The change is the Sweep pass-mark and
`sweep_incomplete` lifecycle described in
`benchmarks/reports/2026-09-25-am427-sweep-state-machine-candidate.md`.
Generate randomized/mutated/structured CNFs and legal public Sweep and
random-policy schedules that force: filtered retained front/tail entries,
an unmarked retained representative, fresh admission after completed passes,
unfinished-pass resume, compaction across external mark identities, and
budget/interruption/reconfiguration boundaries. Count actual reachability of
each transition in Debug, Release, and ASan+UBSan; a similar verdict
distribution is insufficient. Compare the exact parent on causal states.
Independently check every SAT model against the original CNF and every UNSAT
proof through the pinned canonical no-option DSR→LSR→Trestle route, including
negative proof controls. Exercise API-selected pure seeds without narrowing
N1. If an oracle or harness acceptance rule changes, run its self-test.
No theorem or performance claim follows from this campaign alone.

Matched AM427 Tier A/B are already terminal checker-clean; Tier C's AES pair
was unscorable at the 4,800-second checker deadline. The separate extended
diagnosis `1723443_0/1` independently verified both byte-identical DSRs
through the canonical route in 5,780/5,790 checker seconds
(`benchmarks/reports/2026-09-25-am443-aes-terminal-and-h2-contract-control.md`).
Do not rerun unchanged passed legs to fill this fuzz matrix. The public H2
no-option DSR failure is separate and remains open; preserve its bounded
reproducer if encountered, rather than calling a known parent-identical
failure a new Sweep regression.

Use the normal 50-physical-core SLURM shape unless a documented
correctness/resource reason warrants a bounded checkpoint. Submit arrays at
full width, without a throttle or node constraint. Claude owns preparation,
immutable source/build/harness/checker pins, job submission, monitoring,
triage, and terminal `REPORT.md`; update `OWNER_STATUS.md` as work proceeds.
Codex reads only that status, the terminal report, and explicit bounded
handoffs. Report exact checked counts, path counters, negative controls,
checker pins, incomplete legs, and final verdict. For a new failure, provide
one bounded reproducer, exact command, original CNF, failing proof/model,
failure class, and parent comparison.
