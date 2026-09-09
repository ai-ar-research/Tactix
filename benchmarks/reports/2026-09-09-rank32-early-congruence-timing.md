# Rank32: early diagnostics and a bounded indexing opportunity

## Decision

Do not select the over-arity Congruence tail scan as the primary Rank32 repair.
The entire small-clause indexing stage costs 1.969 s in the unchanged candidate's
326.714 s instrumented solve. Only part of that stage is avoidable scanning;
the measurement is not a predicted saving or a rigorous upper bound.
Keep the new wide padded-XOR controls. No solver implementation, scheduling,
budget, proof rule, production binary or fixed reference changes here.

The useful implementation change is in the benchmark harness: publish the
already-bounded solver stderr tail in the provisional result, before checking.
This lets development consume diagnostics while independent proof validation
continues. Provisional records still cannot satisfy a correctness gate.

## Evidence and boundaries

[6541](../runs/20260909-6539-rank32-early-congruence-time/summary-6541.json)
freezes all 38 cumulative timer rows, their deltas, input/source/pin hashes and
exact own-work/proof comparisons. Job 1678210, submitted at
2026-09-09T12:29:09, uses current 6424 Release, SHA
`5946f59800adbf1bbbb20c64ac4bcbcd00ff8dc980ebfdc04edd844db2cd7eb4`.
The one-task allocation retains 12 logical CPUs, 48,000 MiB, one bound solver,
1200 solver-only seconds, 16000 checker seconds and five hours total on
`cpu-dense-hwulab-q`, without a node constraint or throttle.

The solve returns UNSAT/20 without timeout, RSS 590456 KiB and VmPeak
701500 KiB. Every non-timing statistic and the 1527700786-byte proof match
independently checked current 6439 task 031 and diagnostic 6535 task 000.
Proof SHA is `0ebf7c4cb79e8137af8fb410b0e2cbf77963a17b6d524b4a68f4d46a2d129640`.
The new jobs' independent proof checks remain separate obligations; neither
matching a checked proof hash nor a provisional verdict marks them accepted.

| Existing cumulative stage | Milliseconds |
|---|---:|
| Apply, at diagnostic publication | 10839 |
| AND extraction | 2167 |
| Small-clause indexing, including index finish | 1969 |
| Canonical-key lookup/sort wrapper | 0 |
| XOR extraction | 756 |
| ITE extraction | 562 |
| Binary rebuild | 1513 |
| Binary extraction / units / import | 531 / 152 / 302 |
| Setup | 29 |
| Closure | 2066 |
| Canonicalization / match / proof subsets | 95 / 173 / 36 |
| Forward subsumption | 1652 |

These fields overlap: closure includes forward work, and match includes proof
work. Do not add the table as disjoint phases. Counters are instrumented wall
times, rounded to integer milliseconds; zero is not proof of zero work. The
apply diagnostic is printed before the final return/destruction. The index is
18.17% of that instrumented Congruence total and 0.603% of the solve, not a
large enough standalone target to explain the roughly 50 s Rank32 gap.
Two diagnostic captures are not the required three paired performance waves.

Source review of `build_clause_index` finds an avoidable tail after the sixth
unassigned literal: the clause can no longer enter the size-3-to-5 index, incur
indexed-literal ticks or qualify for this phase's satisfied-clause cleanup.
Kissat `init_xor_gate_extraction` and CaDiCaL's corresponding method stop at
that point. Any future shortcut must count root-effective open literals, not
physical clause width, and preserve the value-before-size cleanup boundary.
No such shortcut is implemented or selected by this report.

## Factor: useful alignment, not a unit-cost conclusion

Checked 6372 reports 86 Factor applications, 7357 factorings and 428217051
ticks in both solvers. Its Tactix timer is 14.531 wall seconds versus native
8.76 process CPU seconds. Source boundaries include dense entry, factoring and
sparse reconnection; Tactix's timed rule also owns local destruction. Aggregate
counts do not prove equal per-call/order/material work, and unlike clocks must
not be subtracted as an isolated representation cost.

Current Factor already uses a borrowed binary prefix, 8-byte suffix records,
cached remainder IDs and flat remainder interning. Do not redo superseded
node/map removal. Next inspect the existing profile's Factor/index call paths
and allocation ownership to identify a substantial residual cost before any
new instrumentation or structural proposal. The small Congruence and retention
scan opportunities are now both deprioritized as primary levers.

## Tooling and fixed tests

`tools/bench/runtime_task.py::provisional_result` preserves the old provisional
whitelist and adds only `solver_stderr_tail`. It excludes final `valid`,
proof/model-check and cleanup fields and always sets `validation_pending`.
Three new tests cover diagnostic/work preservation, acceptance-field exclusion
and empty/nonmatching diagnostics. Existing submitted runners remain immutable;
only the separately submitted 6539 snapshot contains the fix.

`tools/bench/congruence_timing.py` has eight tests for cumulative rows/deltas,
unrelated output, exact large integers, malformed/missing/count-mismatched rows,
decreases and overlapping fields. All 140 benchmark-tool tests pass.

[6538](../runs/20260909-6538-wide-padded-xor-control/summary.json) records 16
public-rule controls on 12 variables: clauses have physical width 11 but
root-effective width 3, with eight false padding literals at four block
positions, complete/missing-one-clause encodings and both DRAT/LRAT modes.
Pure extraction leaves the arena unchanged; eight complete cases refute with
independently verified proofs (four per mode). Eight incomplete SAT cases
preserve exact model sets across all 4096 assignments. These are deterministic
tests, not fuzz or runtime measurements. They preserve the stronger padded-XOR
closure established by 6519 rather than importing native's later filter omission.

Failures are classified, not hidden: 6535 preparation first assumed an absent
manifest checker field, then resumed its exact unsubmitted scaffold. The first
fixed fixture compile used `out` instead of `output`; `build.log` is retained,
and `build-2.log` records the corrected build before any case ran. The first
diagnostic solve (1678209, submitted 12:19:08) completed exact work/proof in
326.810 s, but its old provisional projection omitted stderr. A read-only
inspection raised KeyError; no result was rewritten. Corrected 6539 is a new
capture, not a restart or mutation of 6535. Neither issue is a solver,
soundness, model or proof-check failure.

The fixture helper hashed and removed 8288 bytes of its own CNFs/proofs after
validation; summaries and check logs remain. Live checker scratch was not
inspected or removed. Audit 6542 owns current scoped accounting, all tests,
integrity and report hashes. No frozen writer is rerun.

## Aggregate status and next action

Full 6439 remains 50/50 checked and clean, 8731.653 / fixed 7239.010 =
1.206194355. This is not an aggregate winner over checked 6169 (1.160932).
Ranks 41–50 already have the requested matched 12-CPU reference from 6108:
all 20 Tactix/Kissat results are checked. Do not reuse cpu-q timings or refresh
the denominator. All fifty instances and excess memory remain scored.

Finish the two new independent checks asynchronously and retain their cleanup
results. Continue source/profile-aligned Factor cost attribution; implement no
redesign on an unanswered approval. FlatLists/Rank50 proposals remain pending,
and this evidence does not authorize them. Claude-owned fuzz remains unavailable
and blocks promotion, not this provisional investigation. No fuzz inspection,
denied transfer retry or push. Andrew's new local report is under
`report-to-andrew/20260909-1240-early-congruence-timing/`; it is not sent.
