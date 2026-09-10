# Approved BVE data-path prototype: stage1 and validation checkpoint

The user explicitly approved the [structural prototype](2026-09-10-eliminate-lifecycle-cost.md)
after73a78fd8. This increment implements its first stage in isolated7087,
retaining immutable7030 and leaving production and scheduling unchanged.
The20% complete-Eliminate improvement target belongs to the complete prototype;
stage1 has no real-instance runtime result and is not a claimed speedup.

## Implementation

[7087 authority](../runs/20260910-7087-bve-views-build/manifest.json) owns the
source, three build hashes and incremental/cumulative patches. Large root-free
antecedents now use the existing16-byte slice representation over the arena
vector, with a literal offset and length. Every access reacquires the buffer;
arena growth cannot invalidate a stored pointer because none is stored.
At the first false root, preparation copies the preceding free prefix once,
then retains a filtered pool snapshot and ordered LRAT unit hints. Satisfied
clauses roll both pools back. Binary preparation and all consumers are unchanged.

The view contract ends after witness construction. Within an attempt, immediate
units and garbage marking do not rewrite the literal array; large resolvent
adds can grow the arena, and vector defragmentation affects occurrence/watch
storage only. Backward in-place rewriting and collection occur after the last
view consumer. No new kernel primitive, descriptor expansion, FlatLists
redesign, gate-family change, scheduling policy or public-instance restriction.
Calculus E1's abstract guard, effect, proof and reconstruction obligations are
unchanged. Mapping/index fusion remains a later approved prototype stage.

## Tests and limitations

- Full own-source suites:114/114 Release,114/114 independent B/C build,
  113/113 trace. Release and B/C executable hashes agree. Existing compiler
  warnings remain. Existing BVE tests exercise all gate families and70,754
  reconstructed models.
- New actual preparation tests cover162 root-assignment/proof-mode poses,
  borrowed versus owned storage, root hint order, rollback, pool/arena
  reallocation, late roots, duplicate physical clauses and garbage marking.
  Sparse/dense public BVE tests force arena growth before witness reads.
- [7089](../runs/20260910-7089-bve-view-proofs/summary.json):96 fixed old/new
  certificates pass48 cake_lpr and48 DRAT checks, exactly matching parent
  bytes; two known-bad proofs reject. **Limited:** DRAT can establish UNSAT by
  input propagation, so this does not establish that the BVE lemmas were needed.
- Fully instrumented7090 **does not pass**. The new fixture aborts when it
  invokes search-state invariants after model extension; source suggests that
  model completion intentionally changes values without updating search-trail
  accounting. Separately, LeakSanitizer fails under the execution environment's
  ptrace restriction on the two other tests. Neither is silently waived.
- Stronger7095 **does not pass**, on its first unchanged-parent DRAT case.
  The emitted proof omits manually derived units: the fixture uses
  `add_derived_unit`, whose contract deliberately emits only LRAT. This is
  source evidence of a fixture API error, not evidence that7087 changed DRAT.
  Original failed files/logs are retained; a successful summary does not exist.

## Required next step

Dependent B/C/runtime submissions are paused. The plan requires bounded Claude
review after a unit-test abort. [7096 prompt](../probes/scratch/claude-bve-view-review-7096.txt)
and its approval-gated launcher are prepared but **not executed**: a separate
question requests permission to send the bounded source/log context externally;
the earlier denied fuzz transfer is not retried. Review must preserve frozen7087
and repair fixtures in new paths, not relax solver invariants or proof contracts.
Then complete supported sanitizer checks, strong positive/negative certificates,
B28+C5 and repeated31/35/38 exact-work/certificate runtime controls. Current-source
fuzz and cumulative full50 remain adoption obligations; no promotion.

7091 B/C helpers are prepared only; no campaign exists or job was submitted.
No old scripts, pins, jobs or report artifacts were modified. No material
deletion; tiny fixed certificates and failure evidence remain local. All old
7053/7073 proofs have now verified with published cleanup, so7030's full50 is
50/50 checked/clean and its fixed-denominator ratio remains1.123176512x.
7097 owns the checkpoint audit, unchanged gate authorities and document checks.
