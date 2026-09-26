# AM457: interrupted Sweep direction and Debug trace fixtures

Date: 2026-09-26. Base checkout `05cc481d`; current source changes are the
general Sweep interruption repair and focused Debug fixture corrections. This
is an affected-scope checkpoint, not N1--N4 acceptance. Native no-option DSR
and every checker contract remain unchanged.

## Trigger and cause

The terminal Opus AM455 campaign reports acceptance **NOT MET**. Its D1
Probe→CCE native DSR rejection remains live. Its separate C2 handoff reproduces
two parent-identical Debug assertions under `TACTIX_STOP_FUZZ=101`:
`kernel_check_invariants` sees a binary in physical watches but not the dense
ordered stream, and duplicate compaction expects a copy absent from that
stream. The owner report and bounded inputs are in
`benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/REPORT.md` and
`handoffs/C2-debug-binary-copy-invariants-under-stop-fuzz/` beneath that run.

On the 13-clause handoff, compute-node GDB jobs `1727580`--`1727583` identify
the exact transition: the first Sweep action returns `NoChange` after adding
one certified binary (`127 ∨ 131`, internal literals 252 and 260), while its
second general-direction proof is interrupted at `sweep.cpp:2958`. The
session had primed four dense streams before emission. At the round boundary,
`SweepSubstitutionSession::collect` republished those older streams over the
kernel's updated ordered image, removing the just-added binary from the image
but not from physical watches. The invariant then fails on literal 252. This
is a partial-commit lifecycle error, not a bad model or certificate in the
ordinary Release runs. The first binary's complete RUP derivation and kernel
addition had already finished; the interrupted reverse plan removes its own
proof-only prefix and does not add its final clause.

The repair reconciles the primed streams against physical watches when that
second direction stops, reports the committed first binary as progress, and
keeps the incomplete pair queued. It does not infer an equivalence or run its
substitution until both directions complete. V5's effect and dense-session
lifecycle in `docs/calculus.md` and `docs/implementation-notes.md` now state
this behavior. The new `sweep_interrupted_direction` regression uses the
original 13-clause CNF and independently checks the SAT model.

The former full Debug job `1724325` passed 184/187 tests. Its trace recorder
fixture called header-inline kernel hooks from a trace-enabled translation
unit linked to a trace-disabled production library; Debug's out-of-line weak
copy can resolve to the disabled hook. The fixture now tests the recorder and
standalone BVE producer directly. The zero-overhead symbol inventory now
recognizes the exact empty `rule_journal_flush()` Debug stub; protocol strings
and every real diagnostic implementation remain forbidden. The prior comment
hygiene failure was already repaired in the base checkout.

## Focused validation

- Debug job `1727585` completed `0:0`: 7/7 kernel, Sweep, interruption,
  random-online, event-trace, zero-overhead and comment-hygiene CTests passed.
  Both stopped and ordinary 13-clause runs returned SAT; `tools/fuzz/checkmodel`
  checked each complete assignment against the original 13 clauses. Both
  stopped and ordinary 332-clause runs returned UNSAT; the pinned AM455
  CakeLPR independently verified each complete LRAT against the original CNF.
- ASan+UBSan job `1727586` completed `0:0`: 2/2 Sweep and interruption CTests
  passed. The stopped 13-clause SAT model passed the same original-CNF model
  checker, and the stopped 332-clause LRAT passed pinned CakeLPR. Neither run
  reported a sanitizer error.
- Earlier trace-only job `1727579` passed all three formerly failing focused
  CTests. The exact job scripts and logs are under
  `benchmarks/probes/scratch/am457-debug-event-trace/` and
  `benchmarks/probes/scratch/am457-c2-repair/`. These are local diagnostics,
  not frozen benchmark authorities.

No performance conclusion follows from these small cases. The AM455 owner did
not accept the source because D1 and C2 were open; this checkpoint closes C2
only at focused scope. A fresh full Debug gate and Opus-owned affected fuzz are
required on the new source pin. D1's canonical no-option DSR failure and the
broader N1--N4 proof/model, fuzz, and performance gates remain open. No Tier D
submission is authorized.
