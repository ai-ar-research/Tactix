# Sparse collector forwarding: isolated implementation and gates

September 11, 2026. This is a prototype checkpoint, not a runtime gain or
production promotion. Retained7144 remains the control and rollback point.

## Decision and design

7228/7229 locate roughly32.8B sampled periods in garbage collection below
Reduce's sparse collector, including23.8–24.0B self. The watch loop repeats
logical-position and garbage tests, then separately forwards surviving refs.
Sampled instruction addresses do not establish intrinsic instruction prices.

Isolated7233, directly on7144, reuses the old arena's forwarding fields. During
the existing retention pass, excluded records receive `INVALID_REF`; every
retained record receives its real packed reference during copying. The logical
prefix is contiguous in the packed arena, preceding the stably partitioned
tail. A forwarded reference below the packed prefix boundary therefore selects
exactly a retained prefix record. This includes intentionally retained prefix
garbage and appended live replacements, while excluding dead alias sources.

`WatchLists::compact_map_large` preserves binaries and combines stable large
watch filtering with reference forwarding in one pass. Blockers, live entry
order, layout flags, holes and later append behavior remain unchanged. No new
global array or allocation is introduced. Old whole-arena/physical-dense paths
remain in place. Proof vocabulary, guards, public actions, strategy, logical
effort, retry advice and timers are unchanged. This is an internal representation
change under S2 and the existing engineering contract, not a new proof rule.

Risks include mistaking a small old search cursor for a forwarded prefix Ref,
altering alias retention, corrupting protected reasons, or trading lookups for
more expensive stores. Dead spare watch words need not be byte-identical;
live layout and future append behavior must be. Complete GC/Reduce cost,
whole runtime and RSS decide whether to retain the prototype. Current-source
fuzz remains unavailable and is not waived.

## Validation

- Build7233: Release117/117, independentBC117/117, trace116/116, no skips.
  Release and BC binary SHA256:
  `59b3d57e0edec7667e07d4ba392710640b60a294db89b9663d8ad375d70913bb`.
- New watch fixture:26240 fused versus old two-pass transactions, including
  empty/mixed lists, duplicate references, Ref0 forwarding, blockers, holes,
  unrelated lists and subsequent appends; exact live representation.
- New collector fixture:896 actual parent/candidate states, seven physical
  cuts,16 garbage masks, aliases, dense/sparse modes and eager preservation.
  Arena, live watches/layout, occurrences, eager/OTFS registers and selected
  collector metadata have identical digest `aa4e7d0b54ec7945` in all builds.
  Existing kernel/retry/root-lock fixtures cover fixed literals, protected
  pending/nonroot reasons, mixed tails, and whole/dense transitions.
- Sanitizers7234: six actual-library fixtures twice under Debug ASan/UBSan/LSan,
  including both new fixtures; all12 runs pass outside ptrace.
- Proofs7235:216 checked UNSAT certificates (DRAT ASCII/binary and LRAT),216
  original-formula SAT-model checks, two rejected negative controls; exact
  parent outputs and certificates. Uses corrected7224 inverse-import fixture;
  earlier7219/7222 failed fixtures remain preserved.
- Root-lock7237:16 additional checked DRAT/LRAT certificates, two rejected
  controls, exact parent certificates; root/elevated compact/tail cases and
  protected reasons. Its SAT cases also check every original clause.

7238/job1680773 was submitted02:12:28 with immutable separate B/C pins and the
unchanged66-task runner. At02:15,26 expected outcomes/13 exact pairs and21
checked-clean. This is provisional broad coverage, not a closed B/C gate.
After66 exact solver outcomes, prepare three paired31/35/38 runtime waves and
matched complete Reduce/sparse/GC profiles. Do not wait for remaining proofs;
any invalidity pauses affected dependent work.

## Independent root-binary experiment

7226 first two waves have12 exact outcomes; only2 independently checked at the
02:12 snapshot. Final runtime wave1680782 submitted02:12:53. No overall gain
is established: Rank31 candidate526.006/527.412 versus524.106/524.640s parent;
Rank35 357.051/358.489 versus357.571/355.987; Rank38 339.654/341.626 versus
338.411/338.302. Rank31 RSS597848 versus563464KiB, a tradeoff to retain.

7230 has four exact profile outcomes at02:15, with checks pending. Final pair
1680807 submitted02:15:40. Wave0 extraction7231 and nested partition7236 give:

| Scope | Candidate sampled periods | Parent | Ratio |
|---|---:|---:|---:|
| Complete Reduce |44153380982|57249170072|0.771249276|
| Sparse collector inside Reduce |36708776044|49608436129|0.739970435|
| GC inside that collector |32116341175|33036877510|0.972136097|

Sparse wrapper self drops from13.691B to1.621B. This supports the intended
root-discovery local mechanism but leaves GC as the larger component. These
inclusive scopes overlap and cannot be summed. They are one pair, not a
repetition median or whole-solver gain; outside costs can offset local savings.
7236 tests missing/reversed/foreign ancestry and descendant conservation,
and reconciles every child envelope to its exact parent. Missing phase frames
are retained explicitly; Reduce is outside Probe/Eliminate in these streams.

## Goal, authority and hygiene

Full retained7167 remains50/50 independently checked-clean,8072.059s versus
fixed Kissat7239.010s=1.115077752x. The<=1.01x goal and memory parity remain
unmet. Preserve closures4/32/33/49/46; Rank21 remains scored but shelved for
debugging;7077 Rank43 stays unsubmitted. No production, existing runner,
accepted binary, old report, fuzz ownership or denied access/push boundary changed.

The runtime-bookkeeping recheck confirmed the same Release pins: heavy tracing,
journaling, proof shadow, conflict stop and assertions OFF; optional binary
counters/rule clocks ON.7184's joint-removal experiment did not establish a
net gain on31/35. This prototype retains normal instrumentation and does not
change the common timing contract.

Fixed proof gates removed only39542+3998 bytes of consumed owned inputs/proofs.
Wave0 extraction removed only1201168920 bytes of verified temporary expansion;
compressed profiles and extracted streams remain. Any further completed wave
and current checked counts are bound in successor audit7239, not retroactively
inserted here. No broad cleanup or fuzz artifact access was performed.
Andrew's local unsent report is `report-to-andrew/20260911-0214-gc-forward/report.pdf`.
