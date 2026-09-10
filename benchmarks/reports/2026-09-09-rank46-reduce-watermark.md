# Rank46: historical Reduce boundary repair

Experimental candidate6730 retains a historical collection boundary when its
clause becomes irredundant. This general representation repair closes all
2,167,922 native decision rows through one million conflicts on Rank46. It
changes neither the public Reduce guard/instance surface nor proof vocabulary,
victim policy, phase admission or production source. No benchmark identity,
clause identity or tuned constant enters solver code.

## Evidence and cause

Sealed6723 extends the preceding implication-history investigation to every
watch scan containing variable18623 over61030..73636. The first516 observations
of the exact14-literal reason agree. At62677/245584/level28 its cursor is6 in
both solvers; at62680/245587/level28 its pose, blocker and assignments still
agree, but the cursor remains6 in6654 and is2 in Kissat. Subsequent watch
movements diverge. Probe61030 is not the first cause.

The full entry formula in6726/6729 has24830 records per arm, including1335
garbage records. The reason is irredundant in both, at physical ordinal11568;
the first currently redundant record follows at11569. Native's selected entry
and exit frames retain reference33712, resetting the cursor6 to2 without
changing pose or assignments. Its verbose Reduce21 log identifies33712 as the
collection start. Both Reduce ledgers agree on tiers7/19,12064 live redundant
clauses,3610 candidates and2271 victims: this is not a budget or ranking split.

Kissat's `reduce.c` saves `first_reducible` into the current call's local
collection start before `collect_reducibles` advances the stored marker past
promoted irredundant clauses. Promotion leaves the historical marker intact.
Tactix6654 instead recomputed the earliest currently redundant record and used
that for the same call's collector, incorrectly retaining the earlier cursor.
Native `collect.c` resets the entire selected tail. CaDiCaL's vector-based
Reduce ranking was reviewed; it does not justify changing Kissat's collector
boundary or enabling the explicit alternative arena-order arm.

6728 initially required a clause-pose-series observer absent from6654. Its
failure and helper remain unchanged; no summary was published. Successor6729
explicitly records the native-only observer, validates its schema, binds the
source absence, and preserves every original per-arm1M decision/proof/standalone
replay obligation. The current cursor evidence comes from6723 and the full
entry formula, not an invented missing frame.

## Repair and regression coverage

The immutable6730 build manifest owns incremental/cumulative patches, source
hashes and three independent binaries. `Kernel::first_reducible` records the
generation-local historical position; learned allocation lowers/initializes it,
and promotion does not erase it. Reduce captures the current cut before
advancing next-call metadata. Genuine collection recomputes the packed marker;
implementation-only packing forwards the old cut to its first surviving tail
record, including a retired origin. Existing source-position aliases remain
authoritative. No new kernel primitive or abstract S2 operation was introduced.

New `test_reduce_watermark` executes288 fixed states covering promotion, retired
origin, originally irredundant control, genuine versus implementation-only
packing, aliases and id zero, root/nonroot collection, zero-victim calls,
compacting collection, pure public enumeration and repeated reductions.
Linked against6654's own headers and objects, it exposes32 expected cursor
failures while256 controls pass; all288 pass against6730. Complete suites pass
112/112 in each Release build and111/111 in the trace build. Release and B/C
SHA256 is `7b0a7666b28644422d6cc4b3a648a677b6745bd8a7ffb9c27c6d3e5660725132`.

Native6733 invokes the actual pinned Kissat Reduce caller in three fixed
promotion/retirement/control cases, confirming tail reset and prefix retention.
The preceding6732 compile-only failure used a reference instead of a clause
pointer and an unavailable statistics field; its source/log are preserved.
6734 exercises inherited root/pending/nonroot collector controls on the new
binary and independently checks eight DRAT/LRAT proofs. These cover their
stated fixed cases, not corpus fuzzing. The changed source remains unfuzzed.

## Causal replay and runtime gates

Candidate-only6735/job1678588, sealed6739, stops cleanly at1M conflicts and
accounts for every decision row. All2,167,922 rows equal frozen native6721;
the predecessor first differs at78797/296583. All117 portable watch rows in
62676..62682 also match native, including all three observations of the target
reason and its corrected cursor at62680. Native was not rerun and its timing
reference was not refreshed. The partial proof was hash-bound and cleaned;
this diagnostic does not claim a checked final UNSAT result.

B28+C5 campaign6737/job1678589 has66 expected outcomes and no solve loss;
frozen6740 records64 checked/clean and two pending proofs. Three of33 pairs
change reported work; changes are retained, not rejected as non-neutrality.

Three paired full Rank46 waves are predeclared in6741, with middle order
reversed and independent jobs1678655/1678657/1678696. Wave0 is83.347/148.204s
candidate/parent; wave1 is83.197/147.749s; wave2 is83.200/147.503s. All candidates report
3,082,376 conflicts and5,883,417 decisions, exactly the fixed native totals.
Frozen6745 verifies exact per-arm work/certificates and all six expected solves;
the paired median is0.563096874x parent, range0.562380233–0.564056324. Both arm
spreads are below2% (candidate0.1803%,parent0.4745%); candidate median83.200s.
Proofs run asynchronously; none is treated as checked merely
because the expected verdict appeared. Fixed native time is80.043s, so exact
work does not by itself close the remaining runtime gap.

Full50 candidate-only6743/job1678659 was submitted21:34:47 EDT after all B/C
solver outcomes and the causal closure. It preserves the same fixed12CPU,
48000MiB,1200s solver-only contract, full width and separate certificate budget.
No aggregate gain is claimed before its complete classified solver view. The
preceding6654 total8240.560/7239.010=1.138354554 remains frozen and above1.01.

## Handoff and limits

Audit6747 owns later published certificate/job states, maintained218 benchmark
and19 watcher tests, six documentation gates and frozen artifact hashes. Both
living doc edits remain within their existing line caps. Andrew's corresponding
TeX/PDF is local and unsent under `report-to-andrew/20260909-2135-reduce-watermark/`.

Next: close pending timing certificates, classify the complete6743 per-rank view,
preserve Rank4/32 and strongerRank33 closures, and choose the next demonstrated
current-source gap. No promotion, FlatLists rewrite, fuzzy artifact inspection,
denied transfer or push retry occurred. Memory and unavailable Claude-owned
source fuzz remain independent open acceptance requirements. Existing campaign
scripts, submitted pins and successful summaries were not modified. No manual
deletion was performed; terminal diagnostics removed only their owned scratch.

The user's session deadline is2026-09-10 06:00EDT (10:00UTC), with wind-down at
05:45EDT. A session-local watcher owns the immutable deadline and mutable
heartbeat under `benchmarks/probes/scratch/session-cutoff-20260910-0600/`.
Six fixed tests cover timezone and exact wind-down/cutoff boundaries. Check
the clock before each new task; at cutoff do safe wrap-up/handoff only. The
watcher cannot interrupt an agent or survive loss of this session; the durable
PROGRESS instruction is independent authority. No jobs are cancelled automatically.
