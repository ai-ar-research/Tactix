# Single-image occurrence cutover: consumers migrated, performance unproven

The complete7253 isolated consumer migration is implemented and frozen. It
replaces Eliminate's duplicate large-reference payload with the already-tested
mixed owner, cached large populations and allocation-free filtered ranges.
Retained7144 remains the investigation baseline; productionc2a95a6 is unchanged.
The checked full50 still totals8072.059/7239.010=1.115077752x fixed Kissat;
the original1.01x goal needs760.6589s reduction and remains unmet.

## What changed

Kernel construction, append, first-copy removal, binary duplicate cleanup,
collection and phase transitions now maintain only the active owner. Eliminate
releases the old large-only payload; Probe releases the mixed owner and retains
its contiguous Ref path. Large consumers dispatch once outside their loops.
Forward's partial index and required post-forward/collection rebuilds remain.

BVE unit cleanup and legal-prefix garbage compaction publish one stable mixed
stream while updating the same scheduling populations. Notification order,
oversized-clause stopping, lazy stale membership and exact duplicates remain.
Sweep/Substitute dirty publication and bulk retirement maintain counts through
the owner. Congruence, ternary resolution, standalone C1 and policy counts use
the projected interface; free N1 dense sessions do not acquire policy-only
restrictions. The two cached-counter queries are O(1).

Memory diagnostics include the new count array and retain actual allocation
statistics. No timer/counter ablation, guard change, proof vocabulary change,
budget change, source-order sorting change or arbitrary scheduling constant is
included. The code is original C++ over existing FlatLists; Kissat dense/watch
and CaDiCaL occurrence sources supplied lifecycle context, not copied code.

## Tests and exact predecessor evidence

The first development build passed all116 existing Release tests. A new
boundary fixture initially failed to compile because it named nonexistent
`Stats::bve_ticks`; the fixture was corrected to the real `bve_resolutions`
currency before the formal build. Its ternary registry name was also corrected
before execution. No solver crash, assertion, rejected valid certificate or
sanitizer finding occurred in these completed gates.

[7253 build](../runs/20260911-7253-occurrence-cutover-build/manifest.json)
records all changed paths, source and object hashes, commands and patches:

- Full suites117/117 Release,117/117 independently built B/C,116/116 trace;
  no skipped or unrun test. Eight focused fixtures also pass in each build.
- The same new C++ fixture is linked against actual immutable7249 predecessor
  objects and the candidate. All52 cases/364 boundaries match, digest
  `6da9d99c105a60e1`. It asserts old large-only sequence equals the mixed
  projection in exact order, not merely as a multiset. Candidate checks also
  assert the removed owner has no payload capacity.
- Seven rule families, both connected modes where applicable, delayed
  binaries, duplicate copies, stale large entries, in-place replacement,
  collection and reversal of the active dense mode are exercised. BVE uses
  its required mixed scan. This is a fixed surface, not unrestricted equivalence.
- Existing840 replacement cases match actual predecessor state digest
  `f86da0be45055db3`, including retry/score/count and physical watch state.
- Storage keeps21845 sequences/16filters each, every-position mutation and
  first-duplicate deletion,300000push growth/packing; new retained-capacity
  rebuild and moved-owner/reinitialization tests pass.

Release and B/C SHA:
`dff64d5119d1be200cd289f85d21c259fc25da65aa657e99a32be845ce8cedb8`.
Trace SHA:
`d825b22de429014dcdb8dd4800940fe28ce20cda37bb982bdabff6b9d43c263d`.
Unlike unused7249 scaffolding, this is a changed solver binary.

[7254](../runs/20260911-7254-occurrence-cutover-sanitizers/summary.json)
passes eight actual DebugASan/UBSan/LSan fixtures twice each outside ptrace,
with leak checking enabled and no suppression. Storage, boundary, replacement,
Kernel, BVE, Sweep, Substitute and Strengthen all pass.

[7255](../runs/20260911-7255-occurrence-proofs/summary.json) links an
original-formula witness to both real libraries, explicitly invokes each of
the seven migrated rule families in connected mode, then collects and solves.
All156 UNSAT proofs verify through drat-trim or cake_lpr, all156 SAT models
satisfy every original clause after reconstruction, and two known-bad controls
are rejected. Input/proof fingerprints and output match predecessor exactly.
All seven families show progress or UNSAT, not merely an unexercised no-op.
Only63972 bytes of generated fixed CNFs/proofs were removed; their fixtures,
hashes and checker logs remain. This is not current-source fuzz acceptance.

## Predeclared evaluation and remaining work

[7256 B/C](../runs/20260911-7256-occurrence-BC/manifest.json) uses the
unchanged66-task B28+C5 matrix and copied runtime runner. Job1680871 was
submitted03:45:10EDT to cpu-dense-hwulab-q, fullwidth/no node constraint,
12logicalCPUs/48000MiB/1200solver-only/16000checker/5h allocation. Pins are
independent and immutable. Watcher44991 tracks this job. Published later
results belong to the current audit, not this submission snapshot.

Before any candidate benchmark outcome,7256 predeclares the only new
cross-arm exclusion: strictly parsed `c denseocc` slice-move/defrag counts.
These physical storage counters have no policy consumer, and deleting an
owner changes them legitimately. Raw records retain them; same-arm repeats
must remain exact. All other non-timing work and certificate bytes/hashes must
match retained7144. Six changed semantic fields and ten invalid row/schema
controls test this narrow contract; no blanket statistics exclusion is used.

7257 prepares three matched31/35/38 runtime waves only after all66 expected
B/C solver outcomes and33 exact pairs. It intentionally does not wait for
independent proof completion. The copied runner and resource shape are
unchanged. That solver gate passed with64/66 checked-clean; runtimewave0
job1680937 was submitted03:52:10EDT while the last two B/C proofs continued.
Waves1/2 remain unsubmitted. Complete Eliminate profiles and RSS are required: filtered views
scan binary entries that the old direct large mirror skipped, so a smaller
payload can cost more time. No performance or memory improvement is claimed.
Full50 transfer, independent checks, zero residue, current-source Claude-owned
fuzz and consolidation remain required before promotion.

The preceding user-requested bookkeeping audit was read-only confirmation,
not a new performance lead. Heavy tracing is compiled out but diagnostics
remain; the7184 joint ablation showed no net31/35 gain. The last goal turn
therefore did not advance implementation; this turn completes actual cutover,
new differential/correctness gates and the B/C launch. Prior7249/7252 and all
GC/AND/root-binary/arena campaigns remain immutable. Existing GC proofs keep
running asynchronously, with no invalidity inferred from pending status.

Preserve N1--N4, closures4/32/33/49/46, Rank21 scored/shelved and7077 unsubmitted.
No Claude debugging, denied fuzz-artifact access/transfer, denied push retry,
production promotion, scheduling repair substitution or gate waiver occurred.
