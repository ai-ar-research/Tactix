# Rank 50: actual subsumption-only Eliminate admission

## Decision

The next portable repair is the missing added/subsumption-material invitation
in Eliminate's watermark, not a deadline adjustment or an instance-specific
budget. Candidate6269 remains isolated: its full50 runtime is not an improvement.
This report does not promote it or claim full-formula identity.

## Validated observations

[6276](../runs/20260909-6276-vivify-binary-rank32-prefix/summary.json), validated
by6280, reproduces all96,083,379 native decision rows through1M conflicts,
including the inherited69,828,932-row300K hash and253-row overlap. The known
coarse Sweep clause-count difference at19375 remains. Prefix agreement is not
full-run convergence: current Rank32 takes565.639s and11,219,629 conflicts,
versus parent6231's373.565s and5,972,020 conflicts. These timings are
cross-window; the changed deterministic work is directly observed.

[6286](../runs/20260909-6286-rank50-admission/summary.json), job1677430 submitted
2026-09-09T03:54:38 and completed, uses three independent tasks: new trace-only
candidate6283, original same-semantic-source6269 trace control, and native6283
cloned from cached6090. All use12logicalCPUs/48000MiB, one bound solver thread,
1200solver-only seconds,25minute allocation, full-width/no node constraint.
Both new observers replay every ordinary trace and partial-proof hash against
their controls; native replays cached6211. All tasks stop at1M and delete scratch.
The inherited139633-row hash is236dcfa1a362af4d on every arm.

Immediately before the first differing decision, both are at conflict823562,
decision1226742, with deadline823562:

| Operand | Tactix6269 | Native Kissat |
|---|---:|---:|
| Admission | false | true |
| Removal counter / saved counter | 18210 / 18210 | 2251 / 2251 |
| Root-fixed / saved count | 49 / 49 | not this guard |
| Subsumption counter / saved counter | absent from guard | 2510 / 2508 |

Tactix is initialized, not preprocessing-only, round8, watermark enabled.
Native elimination is enabled,4753 irredundant clauses remain, last Reduce is
817616. Thus the native guard passes solely because subsumption material grew;
Tactix's implemented guard declines because neither of its two inputs grew.
The first1,226,946 decision rows match; next decision1226743 is Tactix level14
literal91 versus native level1 literal-586. First488 portable boundaries match;
native enters the forward part of Eliminate, while Tactix's next recorded
boundary is Congruence at837978. This proves the guard-input omission at this
boundary, not which exact additions produced native's two increments.

## Scope and tests

[6283 build](../runs/20260909-6283-admission-observer-build/manifest.json) changes
only trace-only admission logging plus its fixed unit test in Tactix, and a
read-only wrapper around the actual native guard. Release SHA remains exactly
6269's8080aff0ccef604120556e6f5f5861274d61d0dca64c96afef724724d54d078d.
Release105/105 and trace104/104 tests pass. The native fixture calls the real
guard for disabled, empty, same-Reduce, before-deadline, closed-watermark,
removal-growth and subsumption-only-growth cases. The validator includes12
negative parser fixtures and recomputes each actual guard answer from all its
recorded operands before interpretation. No inference/proof semantics change.

Preserve SP-SLOT-5's removal-event semantics: the old mark-bit-flip translation
wedged Tactix when scheduled BVE declined without consuming retry bits. Native
subsumption flags have their own clear/rearm lifecycle; forward tail rearming
does not increment its material counter. Do not substitute the shared
touch_epoch blindly: it includes removals, root and other consumers. Audit the
existing canonical forward-added producer/consumer lifecycle, then add a
separate faithful material invitation and focused no-new-material/rearm/reuse
tests. Preserve public explicit actions and existing root/removal invitations.

## Full-run checkpoint

[6290](2026-09-09-vivify-full50-6290/summary.json) records all50 expected solver
outcomes,37checked at that snapshot, no observed rejection,9070.883s versus
fixed7239.010s =1.253055736x. Original40:1.155800252x; added10:2.797118039x.
Ten certificate hashes change versus6231; the other40 are exact. Rank50 stays
proof/work-exact to6231 at11,797,027 conflicts and427.408s versus fixed49.047s.
Rank49 also remains proof/work-exact,158.720s versus22.555s.

[6287 checkpoint](2026-09-09-vivify-binary-checkpoint-6287.json) has B/C66expected,
64checked, B0.728109114x and C0.928366334x production; parent6169 full50 remains
49checked,1.160931951x. Checkers continue asynchronously. Cross-window ratios
are descriptive, not causal gains; source fuzz, three paired timing waves and
memory acceptance remain open. Production, fixed baselines and cpu-q5784
campaign scripts/pins are unchanged; no denied push or fuzz transfer retry.
