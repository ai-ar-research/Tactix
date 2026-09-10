# Sweep copy-cost control; Vivify timing does not justify adoption

## Outcome and comparison contract

Current6730 remains the runtime authority, production remains unchanged, and
the fixed50 result remains8300.164/7239.010=1.146588277. All50 current results
are checked/clean; the runtime target and memory parity remain unmet.
The user prioritizes inprocessing unit cost together with essentially
reference-equivalent scheduling and CDCL. These are separate obligations.

Vivify6822 is **not adopted** after its complete three-wave solver comparison.
Sweep6838 is a separate, unadopted same-work experiment based directly on6730;
it does not compose the unpriced/neutral Vivify branch. No speedup is claimed.

## Completed Vivify timing

[Strict6833](../runs/20260910-6831-vivify-snapshot-runtime/solver-checkpoint-6833.json)
has all18 expected UNSAT results and exact same6730 per-rank non-timing
statistics and certificate bytes/hash for both arms. Four certificates are
checked/clean at that frozen solver snapshot; remaining checks continue.
Waves1678932/1678938/1678944 began00:57:11/01:05:01/01:14:40EDT, respectively.
The middle wave reverses arm order and placements cross node017/node018.

| Rank | Median candidate/parent | Full paired range | Candidate/parent arm spread |
|---|---:|---:|---:|
|32|1.002870921|1.001647105–1.009075983|1.4253% /0.6840%|
|39|0.999647819|0.996736411–1.000085971|0.9477% /0.6132%|
|38|1.001348269|0.998284202–1.005143009|0.9317% /0.2467%|

Both arm spreads remain below2%, but these measurements do not show a useful
total-runtime improvement.32 is slower in all three pairs;39 is near neutral;
38 is mixed. No repeat campaign to select favorable noise, adoption or full50
run is justified by this runtime hypothesis.

[Timer6849](../runs/20260910-6831-vivify-snapshot-runtime/timer-cost-6849.json)
preserves every pair's same-scope Vivify, slot and outside-slot times. Median
Vivify ratios are0.991459561/0.995130179/1.007901288 on32/39/38. The small
component changes do not establish an overall win; they are not native unit
prices. Peak RSS ratios are0.974626896/0.972044236/1.0. The localized memory
decrease is real in these records, not proof of general memory competitiveness.
Published final records may supply affinity/runtime context missing from the
provisional snapshot; missing fields were never inferred.

## Sweep implementation and reference review

[Build6838](../runs/20260910-6838-sweep-rootfree-build/manifest.json) binds source
`.claude/worktrees/runtime-sweep-rootfree-6838`, incremental/cumulative patches,
full source hashes and three independent builds. Release/BC SHA256 is
`37578a00f5585b088aab4b90de1db2506d726865cb7027af27bce9e0de8d04a8`;
trace is`ff7e588efdbf22a53b8e1106d87f97bc8b251cf1c6fe686ca2aec9970ddbd9d9`.

The existing Rank39 profile6817 places2.21% inclusive samples in environment
building,1.54% in `env_add_clause`, including0.65% self under vector append.
These samples motivate a bounded copy-cost experiment, not a promised speedup.
Kissat and CaDiCaL filter main clauses into a temporary stack before importing
them into the embedded solver. Tactix additionally encodes an environment for
proof mapping. Its helper copied all kept literals into scratch and then into
that environment, even when no literal needed filtering.

The new helper scans up to the first root-false literal. If none exists, it
passes the original clause directly to the unchanged environment insertion.
Otherwise it copies the known non-false prefix and resumes the original
proof-aware filter. Main-arena and stack-binary inputs remain stable because
insertion mutates only SweepEnv and statistics, never the source arena/pair.
Strip-off root units, missing-LRAT-unit-ID fallback, short-clause decline,
dedup charges, clause IDs, BFS variable ordering and structural limits remain.
Temporary scratch contents/capacity may differ; encoded semantic state may not.

[Scope6853](../runs/20260910-6838-sweep-rootfree-build/scope-6853.json) verifies
that only this helper and one new fixed test differ from6730. The rest of
Sweep, its guard/public instances/emitters, all policies, CDCL, Kernel and
public headers are byte-identical. V5 calculus and implemented guards were
reviewed without a semantic change. No new kernel primitive, FlatLists
redesign, SubSolver mapping change, layout policy or tuning constant.

## Tests, failed fixtures and background gate

[Control6843](../runs/20260910-6843-sweep-rootfree-control/control-6843.json)
links actual old/new Sweep objects to the same parent objects.300 fixed cases
produce1500 built environments,3115 stripped literals and3080 proof bytes,
with identical digest`5c9480aed6b712d1`. Cases cover five root configurations,
five literal positions, strip on/off, no-proof/DRAT/LRAT, tight/full limits,
duplicate clauses/binaries and repeated environment reuse. Exhaustive model
sets remain equal after each build; public enumeration is pure. Existing
Sweep controls also agree:86501 environments and5344 claims,17 non-RUP.
Independent Release/BC/trace suites pass113/113/112.

Two fixture-only failures are retained, not hidden:6839 did not compile because
its vector-only hashing template could not accept ReservedTrail.6841 then
failed the parent's pre-build nonempty-model assertion: roots-5,-6 together
with(5 or1) and(6 or5 or-1) made that test input contradictory. Neither ran the
candidate. Archives/classifications6840/6842 preserve both; the corrected
generic range helper and positive final literal pass the original model oracle.

[Proof6845](../runs/20260910-6845-sweep-rootfree-proofs/summary.json) checks120
independent DRAT/LRAT proofs,60 per arm, with exact old/new outputs and proof
bytes. Root-free/all-false/mixed clauses, five positions and strip on/off reach
the general emitter. The final contradiction uses the emitted equivalence
with opposing assumptions1,-2, not checker-only contradictory units. The
owning checker removed41208bytes of generated inputs/proofs; logs, hashes and
the fixed emitter remain. These are fixed controls, not current-source fuzz.

[B28+C5 gate6846](../runs/20260910-6846-sweep-rootfree-BC-gate/manifest.json),
job1678952 since01:20:56EDT, retains the existing33-instance composition.
Require all66 expected outcomes and all33 exact-work/certificate pairs through
6848 before targeted timing. Checks continue asynchronously; pending checks
permit provisional development, not acceptance. Audit6854 owns the final
checkpoint counts. A premature seal attempt at64 records was correctly
rejected before writing a summary; it did not weaken the complete-gate check.

## Next steps, parity and hygiene

Advance the predeclared [6850 timing](../runs/20260910-6850-sweep-rootfree-runtime/manifest.json)
on32/39/38 as three independent six-task waves via6851. Require exact preceding
solver outcomes before each next wave, not completed proofs.6852 requires all18
results; price total and Sweep timers, retaining every adverse outcome.
Every task requests12logicalCPU/48000MiB,1200solver-only seconds and16000checker
seconds in5hours, full width/no node constraint. Existing arrays/pins are immutable.

Scheduling parity still means matching admission, cadence, effort currency,
allowance, overshoot, candidate frontier and semantic yield at common states.
CDCL parity includes propagation/reasons, analysis/backjump, learning, branch
updates and reduction/retention. Cost-only exact replay protects the current
behavior; it does not prove global native parity. Ranks31/35 still have roughly
twice native conflicts;21 changed-work remains open and its probes stay shelved.
Preserve stronger33 and established4/32/49/46 closures rather than force identity.

The existing plan already owns unit-test consideration and these parity gates;
no plan change is needed. Project performance/benchmark/rule skills required
the independent controls and unpromoted disposition. Wrap-up checks the live
campaign map and existing authorities; docs remain within their enforced caps.
No live scratch inspection/deletion, fuzz artifact access, denied transfer/push
retry or promotion. The local watcher socket failure affected observation only;
its local process was replaced with an authorized read-only watcher, not a job
cancellation. Andrew's new0128 report is local/unsent. Wind down05:45EDT;
no new tasks after06:00EDT September10. Current-source fuzz and memory remain open.
