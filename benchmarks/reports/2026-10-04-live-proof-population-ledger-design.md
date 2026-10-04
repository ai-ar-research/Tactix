# Bounded live proof-population ledger and source-pinning design

## Decision

Public TTF and Z3 implication actions must not accept clause snapshots, proof
identities, refresh callbacks, or source vectors supplied by an API caller.  The
solver must create the source owner while observing the complete clause stream,
retain exact canonical external clauses for that owner, and issue opaque,
solver-lifetime handles.  Immediately before publication, the action converts
those handles into a short-lived pinned source set.  A pin authenticates the
current checker-visible premises, their current proof naming, and the complete
occurrence census claimed by the owner.  It remains live through proof, kernel,
journal, and session commit.

A full retained copy of every checker clause is not the production design.  The
audited TTO representative has 20,380,344 clauses, so such a copy would recreate
the input-memory cost the streaming parser deliberately avoids.  The production
design is a hybrid:

1. a bounded exact ledger for source-eligible clauses and their proof/kernel
   lifecycle;
2. trusted streaming owner builders that see every raw input and subsequent
   mutation but retain only candidate premises;
3. per-external-variable occurrence generations and counts that authenticate a
   closed root census without retaining unrelated clauses; and
4. a permanently conservative `Unavailable` state for an owner or ledger scope
   whenever a cap is exceeded or a transition cannot be represented exactly.

This is source authentication, not a proof checker.  A pin establishes that the
premises and census are the ones the solver actually observed and that the
required clauses are still proof-visible.  The rule still owes its RUP/RAT/SR
derivation, full theory preconditions, proof-format support, and model
projection.

No experiment was run for this report.  It is a source audit of the current
tree.  No production file was changed.

## Why the current TTF refresh is insufficient

[`TtfTruthTableLiveSnapshot`](../../src/rules/semantic_extension/ttf_truth_table_live.hpp)
contains a generation, external clauses, and claimed native proof IDs.  The
publisher canonicalizes that copy, recognizes structure, invokes a caller
refresh, and checks byte-for-byte equality before commit in
`ttf_truth_table_live_apply`.  This proves only that one caller returned the
same data twice.  It does not prove any of the following:

- that a clause ever occurred in the input, kernel, or proof;
- that its claimed proof ID names that clause;
- that input normalization did not replace or delete it;
- that a DRAT/DSR literal-named deletion did not retire its last live copy;
- that an LRAT ID was not deleted or rebound during input replay;
- that an inprocessing rule did not replace or retire it; or
- that the slice contains every occurrence required by a closed TTF/Z3 owner.

The late `clause_live` check in the TTF adapter searches only binaries and large
kernel clauses.  It cannot see raw input clauses discarded by normalization,
units, proof-only clauses, or clauses retained by a checker after an optional
solver-side deletion.  It also matches by literals rather than by the claimed
proof lineage.  The present refresh therefore cannot support a public action.

## Existing state that must remain distinct

The design keeps four meanings separate.  Conflating any pair recreates the
current authentication gap.

| State | Meaning | Existing authority |
|---|---|---|
| Raw input | Exact zero-delimited DIMACS clause at file position, including duplicates, repeated literals and tautologies | `ingest_clause[_generic]` before normalization |
| Checker population | Clauses currently visible to the selected proof checker, with format-specific deletion identity | `Proof` event stream |
| Kernel population | Units, binary watch pairs, and live large arena records used by search | `unit_ids`, mirrored watches/`binary_ids`, and `arena` |
| Structural owner | Exact premise set and complete occurrence census from which a theory meaning is claimed | New trusted owner builder and ledger handles |

The existing `TACTIX_ENABLE_PROOF_SHADOW` map is a valuable test oracle, but it
is not the production ledger.  It is compile-time diagnostic state, stores an
unbounded `unordered_map<string,...>`, is inert without a proof file, and keys
only the checker deletion namespace.  It neither records kernel liveness nor
issues authenticated owners or pins.  The production implementation should run
the new ledger and proof shadow together in tests and assert agreement on every
checker-population delta.

## Canonical identity and record layout

All durable literals use signed stable external DIMACS identities.  Internal
variables and arena references are representation-local and cannot be durable:
`kernel_compact_variables` remaps them, while `kernel_collect_garbage` relocates
every large-clause `Ref` and increments `arena_generation`.

Two canonical forms are required:

- `RawMultiset`: sorted signed external literals retaining repetitions and
  complementary pairs.  It identifies the checker's exact input clause.
- `ClauseSet`: sorted signed external literals with no repetition and no
  complementary pair.  It identifies a normalized source-eligible clause.

Raw-to-normalized transitions explicitly link these forms; code must never
silently reinterpret a raw multiset as a set.  Canonical vectors live in a
bounded append-only literal arena and are referenced by offset/length.  Hashes
accelerate lookup but exact literals decide equality.

### Clause record

Each retained record needs these fields:

| Field | Purpose |
|---|---|
| `slot`, `version` | Opaque stable handle; slot reuse increments `version` so stale handles cannot alias new clauses |
| `canonical_kind`, `literal_offset`, `literal_count`, exact hash | Exact external clause identity |
| `origin` | Raw input, normalized restatement, asserted/derived unit, learned, theory action, ordinary rule, or proof-only temporary |
| `raw_input_id` | Original file-position ID, zero for non-input clauses |
| `logical_event_id` | Tactix's monotonically allocated clause ID; provenance only in literal-named formats |
| `checker_id` | Current LRAT identity; zero for DSR/DRAT where IDs do not name deletions |
| `predecessor`, `replacement`, `transition_kind` | Explicit normalization/rewrite lineage; no relationship is inferred merely from adjacent proof lines |
| `checker_state` | Pending, live, deleted, or unavailable |
| `solver_state` | Absent, asserted unit, binary, or large; include redundancy and current-copy count |
| `born_generation`, `last_generation`, `deleted_generation` | Mutation epochs for diagnostics and stale decisions |
| `source_eligible`, `proof_only` | Proof helpers are never silently promoted to semantic premises |
| `pin_count`, `owner_refcount` | Prevent slot reuse and source retirement while a short commit pin or durable owner references it |
| `action_id`, `semantic_id`, `proof_event_begin/end` | Link generated constraints to the existing provenance contract when applicable |

An optional physical locator may cache a unit variable, binary key, or large
`Ref` plus `arena_generation`.  It is an optimization only.  Revalidation is by
stable external identity and proof state; a stale locator must fall back to an
exact bounded lookup or decline.

### Canonical bucket

DSR and DRAT delete by literal multiset rather than by ID.  A canonical bucket
therefore stores `checker_live_copies`, `solver_live_copies`, retained member
handles, and a current representative logical ID.  Under LRAT, the primary
population map is `checker_id -> handle`; a canonical bucket remains useful for
duplicate and kernel membership queries but cannot substitute for the exact
ID.

The current field name `native_proof_id` is misleading for DSR/DRAT because the
serialized checker does not use it.  New source APIs should expose
`logical_event_id` and an explicit proof naming mode.  LRAT alone supplies a
checker-addressable clause ID.

### Owner record

An owner's source membership and identity are solver-created and immutable
after finalization; its lifecycle state may move one way from `Ready` to stale,
unavailable, or released:

- random solver-lifetime `ledger_nonce` and monotone `owner_id`;
- owner kind and format/support mask;
- exact retained source handles;
- root/structural variables whose occurrence census must be closed;
- captured occurrence generations and live occurrence counts for both
  polarities;
- raw-input completion generation and proof-population generation;
- external variable bound and source digest;
- retained-clause/literal/work charges and configured caps;
- state: `Building`, `Ready`, `Stale`, `Unavailable`, or `Released`;
- reason for the first terminal transition.

The owner digest is diagnostic/replay data, not authentication.  Authentication
comes from the nonce, opaque handles, exact records, and live generation checks.

## Required invariants

1. **Stable identity.** Every retained literal is external.  Variable and arena
   compaction cannot change a canonical key.
2. **Honest completeness.** A builder may claim a complete occurrence census
   only if it observed the full raw input boundary and every later population
   transition for its watched variables.  Missing one event makes the owner
   `Unavailable`; it never falls back to a partial slice.
3. **Dual liveness.** Checker liveness and kernel liveness are independent bits.
   A checker-retained but solver-deleted clause and a solver clause whose proof
   identity was rebound are representable without pretending they are equal.
4. **Format-correct deletion.** DSR/DRAT decrement one exact canonical
   multiplicity.  LRAT retires exactly the named ID.  A zero or stale LRAT ID is
   never treated as a literal-key deletion.
5. **Exact replacement.** Normalization and clause strengthening create a new
   record and an explicit lineage edge.  A source handle does not silently move
   to different literals or a different proof ID.
6. **Conservative source validity.** Any source deletion/replacement makes the
   handle stale unless that transition has a separately reviewed equivalence
   closure.  The first implementation should support exact-live premises only;
   input-normalization closure is a later stage.
7. **No long-lived blocking.** Owners are weak durable references and become
   stale on mutation.  Only the short final `PinnedSourceSet` prevents source
   retirement, from final validation through atomic commit.
8. **Pin safety.** A mutation that would retire or rebind a pinned record is a
   precommit failure (or a debug invariant violation inside a supposedly
   noninterruptible commit).  Unrelated additions remain legal.
9. **Semantic generations only.** Proof/kernel additions, deletions,
   replacements, ID rebindings, and raw-input completion advance population
   generations.  Watch relocation, literal pose changes, arena collection, and
   variable compaction do not, because the external clause population is
   unchanged.
10. **Occurrence closure.** Every live add/delete/replacement updates the
    per-external-variable occurrence count and generation for every distinct
    variable in the clause.  An owner pin requires exact equality with every
    captured generation/count or a successful owner-specific incremental
    reclassification.
11. **Duplicate ownership.** Literal-named formats authenticate a canonical
    live copy, not an imagined unique ID.  LRAT owners retain exact IDs.  A
    duplicate removal that leaves a matching live copy may preserve a
    canonical DSR/DRAT source but changes its bucket generation and must refresh
    the representative provenance.
12. **Proof helpers stay private.** Temporary SR/DSR helper clauses may update
    checker accounting transactionally, but are never source eligible and
    cannot appear in public instances.
13. **One-way degradation.** Cap overflow, event loss, unsupported proof mode,
    or late attachment mismatch marks the affected scope unavailable for the
    rest of the solver lifetime.  Re-enabling requires an exact rebuild, never
    an assumption.
14. **Proof-independent soundness.** The no-proof path can authenticate exact
    live kernel clauses and trusted initial owners, but it must conservatively
    stale an owner after a transformation whose model correspondence does not
    preserve the claimed clause meaning.

## Mutation hook inventory

The implementation should hook the small set of authoritative primitives
below.  Instrumenting individual proof rules would be incomplete.

### Input and proof attachment

| File/function | Current transition | Required hook |
|---|---|---|
| `src/frontend/parse.cpp::ingest_clause_generic` | Allocates one raw input ID, records exact raw shape, then deduplicates, drops tautologies, strips root-false literals, drops root-satisfied clauses, and admits unit/binary/large survivors | `observe_raw_input` before the first rewrite; then explicit `drop`, `normalize`, or `solver_admit` transition |
| `src/frontend/parse.cpp::ingest_clause` fast binary path | Bypasses the generic vectors for an ordinary binary | The same raw observation and binary admission/duplicate transition; this path must not bypass the ledger |
| `src/frontend/parse.cpp::restate_input` | Adds a normalized RUP replacement and, outside LRAT, deletes the raw predecessor | One prepared replacement transaction linking raw and current records |
| `src/frontend/parse.cpp::finish_input` | Emits deferred LRAT restatements and directly rebinds unit, binary, or large IDs | `rebind_checker_id`; finalize trusted builders only after all restatements and input refutation work are settled |
| `src/frontend/parse.cpp::attach_replayed_input_proof` | Replays exact input into a temporary kernel, then directly changes existing large/binary/unit proof identities | Build a temporary checker ledger during replay and atomically swap/rebind it into the current kernel; a cap/mapping failure leaves public source actions unavailable |
| `src/frontend/parse.cpp::parse_dimacs_with_block` | Temporarily detaches native LRAT and later attaches replay | Preserve the same builder/ledger mode transition; do not double-count raw clauses |

Raw observation must be unconditional on `TACTIX_ENABLE_PROOF_SHADOW` and must
also work when no proof was requested.  When a clausal proof route is active,
`observe_raw_input` seeds the checker's initial population because input clauses
are checker premises rather than emitted proof additions.  Without a proof it
seeds only the trusted input/logical ancestry.  The current `startup_input`
cannot be the owner: it is intentionally released at the first ordinary action.

### Proof-population endpoints

Every direct proof call in `src/` funnels through the following `Proof`
methods.  These are the checker-population hook points:

| Function | Population effect |
|---|---|
| `Proof::add_rup`, `add_rat`, `add_sr`, `add_empty` | Add one final checker clause; empty is terminal and not source eligible |
| `Proof::add_derived_unit` | Adds a unit in LRAT/DSR; DRAT may emit nothing, so the format-specific effect must be explicit |
| `Proof::add_external_rup`, `add_external_rat` | Add proof-visible clauses that may use proof-only variables |
| `Proof::del`, `del_external` | Retire one clause according to LRAT-ID or DSR/DRAT-literal semantics |
| `Proof::flush_pending_dsr_units` | Materializes delayed unit transactions and temporary split helpers |
| `Proof::commit_dsr`, `begin_pb`, `finish_pb` | Fix or change the whole-run proof route; PB makes live clausal source actions unavailable |

The ledger must bind to `Proof` explicitly; it must not infer proof events from
kernel changes.  Several rules emit proof-only temporary clauses or deletions
without a corresponding kernel mutation, including BVE, Congruence, Sweep,
Substitute, startup certificate paths, and the semantic-extension certificate
adapters.  Conversely, proofless execution has kernel changes without proof
events.

Pending DSR units need special handling.  The current proof shadow counts a
unit when `add_rup` stages it even though `flush_pending_dsr_units` may later
serialize helper additions, a deletion block, the final units, and helper
deletions in a different order.  A public snapshot must call a bounded
`settle_population()` boundary first, or reject `Pending` records.  It must
never expose a staged unit as already serialized proof support.

`Proof::add_sr` and DSR unit splitting generate balanced temporary helpers
within one noninterruptible proof transaction.  The production ledger can
apply their net effect atomically, provided tests compare the full event trace
against the proof shadow and ensure no temporary clause is source eligible.

### Kernel clause endpoints

| File/function | Current transition | Required hook |
|---|---|---|
| `src/kernel/kernel.cpp::kernel_add_large_clause` | Creates every large kernel clause | Attach/increment solver membership for the prepared clause record after arena admission |
| `src/kernel/kernel.cpp::note_binary_add` via `kernel_add_binary_clause`, `kernel_add_new_binary_clause`, `kernel_add_binary_mirror`, and `kernel_publish_binary_clause` | Creates a binary pair, duplicate physical copy, or rebuilt mirror | Distinguish logical add/copy from physical mirror publication; a mirror alone must not create another checker clause |
| `src/kernel/kernel.hpp::kernel_delete_clause` | Emits proof deletion, retires reasons, marks one large record garbage | Prepared checker delete plus solver retirement; idempotent garbage return emits no event |
| `src/kernel/kernel.cpp::kernel_delete_binary` | Deletes one unique binary and its proof clause | Exact single-copy retirement |
| `kernel_delete_binary_one_copy` and `kernel_remove_duplicate_binary_copies` | Remove duplicate physical copies; proof deletion depends on format | Update solver multiplicity and the actual format-specific checker multiplicity separately |
| `delete_binary_all_copies` through both public wrappers | Removes all physical copies; LRAT may retain checker copies when it lacks every ID | Preserve the checker-retained/kernel-absent distinction |
| `kernel_replace_clause` | Delete old, then produce empty/unit/binary/large replacement | One explicit replacement transaction; unit semantics remain owned by the caller |
| `kernel_replace_large_clause_in_place` | Proof-deletes and overwrites one arena record with a new ID/literals | New version/record and lineage edge even though the physical `Ref` is reused |
| `kernel_flush_root_clause` | Deletes root-satisfied clauses or proves and installs a shortened replacement/unit | Use the underlying delete/replace hooks; record its normalization-under-root transition kind |
| `kernel_sparse_collect_root` | Deletes satisfied binaries and invokes root flushing over large clauses | No extra semantic hook beyond the underlying transitions |

Current callers of these primitives span Analyze, BCE, BVE, CCE, Condition,
Congruence, Factor, HyperBinary, Instantiate, Reduce, Strengthen, Substitute,
SubsumeForward, Sweep, Ternary, TransitiveReduce, Vivify, Walk, and the affine,
exact-cover, independent-set, direct-CSP, GF(3), row-cover, disjunction, parity,
and TTF adapters.  Hooking the primitives covers those callers.  A static test
should reject any new direct assignment to a clause's semantic `size`, `id`,
`GARBAGE` state, or binary logical membership outside the audited primitives.

### Nonsemantic representation changes

These paths must not create proof-population mutations:

- `kernel_collect_garbage`: forwards live large clauses, clears position
  aliases, and increments `arena_generation`;
- `kernel_compact_variables`: remaps internal literals, watches, reasons, unit
  arrays, and `binary_ids`, while stable external clauses remain unchanged;
- `kernel_promote_watched_lit`, propagation watch relocation, Analyze pose
  swaps, and Vivify's temporary literal ordering: change clause pose only;
- `kernel_promote_large_irredundant`: changes reduction metadata, not clause
  membership (update cached solver metadata if retained, without advancing a
  population generation); and
- `kernel_drop/rebuild/flush_large_watches` and dense occurrence rebuilds:
  change indexes only.

The ledger may invalidate a cached physical locator on these paths, but it must
not bump a semantic source generation.

### Variable and root-unit boundaries

`kernel_reserve_vars`, `kernel_new_extension_vars`, and
`kernel_reserve_proof_variables` advance a separate variable-universe
generation.  Existing external clause handles remain valid.  Owners that claim
an exact universe size may require equality; root-census owners normally do
not.

Asserted input units enter through raw ingestion.  Propagation-derived proof
units enter through `kernel_emit_root_units` /
`kernel_materialize_dsr_root_unit` and `Proof::add_derived_unit`.  Root
assignment, backtracking, and `unit_ids` cache changes are solver state, not
proof-population deletion.  Actions must recheck current root values separately
from source pinning.

## Snapshot, census, and pin API

The public rule never receives this mutable machinery.  A proposed internal API
is:

```text
SourceOwnerId begin_owner(OwnerKind, OwnerLimits)
observe_raw_input(raw_id, external_multiset)
observe_proof_delta(prepared_delta)
observe_kernel_delta(prepared_delta)
finalize_owner(owner_id) -> Ready | Stale | Unavailable

inspect_owner(owner_id, SnapshotLimits) -> immutable SourceSnapshot
pin(owner_id, snapshot_token, PinLimits) -> PinnedSourceSet | reason
commit_generated(pinned_set, prepared_delta)
release(pinned_set)
```

`SourceSnapshot` contains opaque handles, canonical clauses copied under the
snapshot bounds, current logical/checker IDs, owner kind, and captured
generations.  It is returned only by the kernel/session.  A C/replay action
selects an offered owner and component/ordinal; it never submits clause bytes or
proof IDs.

`pin` performs, in order:

1. nonce, owner, token, cap, proof-format, and owner-state checks;
2. exact handle version and canonical-clause checks;
3. checker liveness under the selected naming semantics;
4. required kernel liveness/active-variable checks declared by the rule;
5. occurrence generation/count equality for every closed-census variable;
6. current proof ID resolution and pending-DSR settlement;
7. duplicate and already-published-key checks; and
8. pin-count increments only after every check and allocation succeeds.

The returned pin owns the authoritative source references used in the journal.
It supplies proof IDs where the format has them; caller data cannot override
those values.

### Streaming complete-census builders

For Z3 implication, per-variable occurrence counters run from the first raw
clause while a builder watches candidate OR roots.  Once a root is a candidate,
the builder classifies and retains exact forward/backward definition clauses,
fixed-false units, and guards, and separately counts every occurrence of both
root polarities.  At input finish it accepts a component only if all definition
clauses exist, classified counts equal the complete counters, every seen
occurrence fits the declared component contract, all retained premise handles
are live, and all bounds hold.  If the root was recognized after an earlier
unretained occurrence, the count mismatch declines the owner; arbitrary clause
order is never assumed.  Later additions, deletions, or replacements containing
the root mark the owner stale.  This shape permits an exact census without
retaining unrelated clauses, but actual retention on the 20-million-clause TTO
class remains an implementation measurement rather than a claim of this audit.

For TTF, the builder may retain a bounded raw candidate snapshot because the
recognizer needs a wider cross-clause reconstruction.  It must be fed directly
by the frontend, not by an API callback.  Finalization maps every semantic raw
premise to an exact current checker source.  The first safe version accepts only
premises that remain exact-live after input normalization.  Supporting a raw
clause that was normalized away requires an explicit, independently tested
equivalence closure containing the replacement clause and named root units; a
mere predecessor link is not enough.

## Generation and revalidation semantics

Use separate counters:

- `raw_generation`: raw input admission and input completion;
- `checker_generation`: every final proof-population add/delete/rebind;
- `solver_generation`: every logical kernel add/delete/replacement;
- `variable_generation`: stable external universe extension;
- `occurrence_generation[external_var]`: any checker-live clause change
  mentioning that variable; and
- existing `arena_generation`: physical large-clause relocation only.

Global generation equality is too strict during an action: publishing the first
new consequence would bump it and invalidate the remaining prepared additions.
Pins therefore validate exact handle versions and watched occurrence
generations.  Unrelated population changes do not invalidate a pin; a source or
census mutation does.  A batch commit preallocates all records and applies one
noninterruptible delta, assigning its result generation consistently.

Durable owner handles survive unrelated learned clauses.  If a learned or
theory clause mentions a watched root, occurrence generation changes and the
closed owner becomes stale.  A later version may build a new owner through an
explicit bounded reclassification; it must not mutate the source membership of
the existing owner.  This is conservative and avoids silently treating an old
census as current.  Consequently the first exact-census implementation is most
available before search has learned clauses involving the watched roots.  A
repeated inprocessing version needs a separately tested monotone-census policy
that classifies trusted derived additions while still staling on any retained
premise deletion or replacement; it must not weaken the first implementation
implicitly.

## Proof-format constraints

### DSR and DRAT

Deletions are literal-multiset operations.  Track exact multiplicity by
canonical external clause.  A logical event ID is useful provenance but does
not authenticate a checker copy.  Before an addition, the rule still must
establish RUP/RAT/SR; a live source bucket alone does not do so.  DSR temporary
SR helpers and split-unit helpers are internal transactions.

The checker may contain clauses absent from the kernel, and vice versa on a
proofless path.  Source actions must declare which state they require.  TTF and
the first Z3 implication consequence require checker-live premises; their
output clauses additionally require active existing kernel variables.

### LRAT

Each source needs its exact current checker ID.  Input restatement and late
proof attachment are ID-rebinding transitions.  Duplicate checker clauses whose
IDs were deliberately retained after solver deletion remain representable, but
a public LRAT action also needs an explicit valid hint chain.  The ledger does
not manufacture that chain.  TTF/Z3 should continue to decline LRAT until their
publishers implement and test the chain.

### PBP and automatic format selection

Once `begin_pb` commits the automatic stream to PBP, no live clausal suffix is
legal.  Owners become unavailable for clausal actions.  Before format commit,
owners may be built, but a pin requires a resolved compatible route.  The
ledger does not bridge PB and DSR.

### No-proof execution

There is no checker population.  The ledger can authenticate exact live kernel
clauses and trusted input ancestry, but it must not represent a deleted input
clause as a live proof premise.  The first implementation should require exact
kernel-live premises in no-proof mode.  Broader ancestry use needs a reviewed
model-correspondence argument for every intervening transformation.

## Duplicate, deletion, and ownership interactions

- Raw duplicate clauses get distinct raw records and one canonical bucket.
  Normalization may retain only one kernel copy while the checker multiplicity
  depends on emitted deletions.
- A DSR/DRAT source pin is to a canonical bucket plus minimum multiplicity.  It
  remains valid when an unrelated duplicate is retired only if the bucket still
  satisfies that multiplicity; its provenance representative is refreshed.
- An LRAT source pin is to one exact ID.  Retiring that ID stales the handle even
  if an identical clause remains under another ID, unless the owner is rebuilt
  explicitly.
- Kernel `binary_ids` is size-monotone and retains stale values after deletion.
  It cannot be the checker ledger.  Binary watches remain the kernel liveness
  authority; the ledger records the current proof representative separately.
- Large garbage records remain in the arena until collection.  `GARBAGE`, not
  physical presence, determines solver liveness.
- A durable action duplicate key is separate from source ownership.  It survives
  output-clause deletion as current semantic sessions already require, while
  source owners become stale when their premises cease to be authentic.
- A source owner does not prevent ordinary solver deletion until it is pinned.
  This avoids retaining large obsolete proof populations indefinitely.  A
  mutation marks weak owners stale in bounded time through reverse
  handle/variable watcher lists.

## Memory and work bounds

All limits are configured before input and are observable in action feedback:

- maximum retained clause records;
- maximum retained literal occurrences and literal-arena bytes;
- maximum canonical buckets and hash-table slots;
- maximum owners, handles per owner, and reverse owner references;
- maximum watched variables and owner watchers per variable;
- maximum raw/build work, mutation-notification work, snapshot work, pin work,
  and revalidation work;
- maximum simultaneously pinned records/literals; and
- maximum pending prepared-delta records/literals.

Use contiguous vectors and open-addressed indexes; do not use one heap node or
`std::string` allocation per clause.  With 32-bit external literals, retained
literal storage is exactly four bytes per occurrence before vector capacity.
Records should use fixed-width indices into arenas.  The implementation must
publish actual allocated/retained bytes, not only logical clause counts.

An owner-specific builder charges every observed clause header and literal it
inspects, even when it retains nothing.  Mutation notification charges every
distinct watched variable visited.  Cap checks happen before vector growth or
state mutation.  If a global exact index cap is exceeded, exact indexing may
continue for already watched owners, but no new owner can claim a complete
historical census.  If an existing owner's watcher notification cannot be
recorded within bounds, that owner becomes `Unavailable` immediately.

Tombstones are bounded.  A record can be reclaimed only when deleted, unpinned,
unreferenced by owners/sessions, and already written to the streamed provenance
journal.  Slot reuse increments `version`.  Raw records needed by a ready owner
remain retained; unrelated raw records need not be stored after their builders
have classified them.

## Failure atomicity

Proof output is append-only and cannot be rolled back.  Every mutation uses a
two-stage delta:

1. **Prepare:** canonicalize; resolve exact old records; check pins and proof
   format; reserve all ledger, kernel, owner-invalidation, journal, and session
   capacity; compute new IDs and generations; and render bounded journal data.
   Interruption and resource exhaustion are legal only here.
2. **Commit:** emit proof bytes, apply checker ledger state, apply kernel state,
   apply solver ledger state, append the already-rendered journal, and commit
   durable session keys.  The commit is noninterruptible and allocation-free.
   I/O or invariant failure aborts rather than returning a result from a
   partially published proof.

Existing kernel additions often follow proof emission.  The ledger integration
must not introduce a fallible allocation between those operations.  Prepared
records therefore exist before `Proof::add_*`; the proof hook only flips their
preallocated state.  Deletes similarly resolve and reserve invalidation work
before `Proof::del` writes a line.

Late LRAT attachment is a special whole-population transaction: construct the
new checker-ID image beside the current logical/kernel image, validate every
rebind, then swap.  If exact source tracking was capped, mark source actions
unavailable; do not retain a half-replayed owner.

## TTF and Z3 action changes after the prerequisite

TTF should replace `owner`, `refresh`, `refresh_context`, and caller-authored
AMO source records with a solver-issued `owner_id` plus selected frontier
ordinal.  The recognizer receives an immutable snapshot from the owner.  Final
revalidation becomes `ledger.pin(...)`; journal premise IDs come from the pin.
The existing formula/proof/arena/journal bounds and durable output key remain.

The first `Z3ImplicationConsequence` action should likewise select an offered
owner, component ordinal, active-input ordinal, guard ordinal, and mode.  It
does not carry clause bytes or proof IDs.  A pin verifies the complete root
census and the two selected RUP premises, after which the publisher adds at
most one clause and leaves propagation separate.  DSR/DRAT/no-proof are the
initial formats; LRAT/PBP decline before mutation.

Neither action should become enabled in DefaultPolicy merely because source
authentication exists.  N1 wiring, replay, raw feedback, focused proof/model
checks, fuzzing, full Debug, and the regression gates remain separate
acceptance requirements.

## Staged implementation and tests

### Stage 0: behavior-preserving observer seam

Add a nullable kernel-owned population observer, bind it to `Proof`, and route
the audited raw/proof/kernel endpoints through typed callbacks.  The null path
must compile to the existing behavior.  Add a static inventory test that fails
on unaudited semantic writes or direct proof population serializers.  No public
action changes in this stage.  The binding is non-owning and must remain valid
through `Proof::~Proof`, which flushes pending output; an RAII binding guard must
unbind first in any construction order that does not already destroy `Proof`
before its kernel-owned observer.

Tests:

- every input outcome: unchanged, repeated literal, tautology, root-satisfied,
  root-shortened, duplicate unit/binary/large, and empty;
- every add/delete/replace primitive, including duplicate binaries;
- observer event order versus proof shadow under DSR, DRAT, LRAT, and no proof;
- no events for GC, watch pose changes, or variable compaction.

### Stage 1: bounded exact ledger and pins

Implement canonical arenas, records, buckets, generations, caps, opaque
handles, snapshots, and short-lived pins.  Keep it internal and default off for
source actions.

Tests:

- forged nonce/slot/version/clause/ID rejection;
- literal-key multiplicity versus LRAT-ID deletion;
- stale after source delete, replacement, ID rebind, and watched-root add;
- still valid after unrelated add, arena collection, watch relocation, and
  variable compaction;
- cap overflow and allocation preflight are mutation-free and permanently
  honest;
- pin blocks a source mutation through commit and releases afterward;
- ASan/UBSan plus proof-shadow differential sequences.

### Stage 2: trusted input builders

Add the scalable Z3 implication census builder and the bounded frontend-owned
TTF capture builder.  Owners finalize only after input normalization and proof
identity settlement.  Start with exact-live premises only.

Tests:

- omitted, extra, duplicated, ambiguous, or later-added root occurrence makes a
  Z3 owner decline/stale;
- large unrelated clause streams do not increase retained source memory beyond
  configured bounds;
- caller-authored snapshots and IDs have no route into either recognizer;
- TTF source normalized/deleted at input is rejected in the first slice;
- deterministic owner IDs/digests/replay under identical input and action
  sequence.

### Stage 3: public Z3 implication action

Wire one selected implication consequence through C++, C vNext, replay,
feedback, default-disabled/random offered instances, durable duplicate keys,
and semantic-variable advice after any configured propagation follow-up.

Tests include independent DSR and forward-DRAT checking, model preservation,
all terminal statuses, interruption at every precommit charge site, exact
feedback accounting, deterministic random reach, and both-policy fuzz.

### Stage 4: TTF public action

Replace the removed caller snapshot action with the authenticated owner/pin
path.  First expose only exact-live sources.  If target formulas require clauses
lost to parser normalization, add a separate reviewed normalization-equivalence
closure with named replacement/unit premises and independent proof checks.

Tests include all Stage 3 controls plus shared AMO ownership, raw/current
lineage corruption, normalized-source cases, source mutation between snapshot
and pin, and checked frontier proofs.  The action remains an incomplete parity
frontier until later reasoning closes the declared theory obligation.

### Stage 5: acceptance

Run substantial fuzzing through the repository's Claude-owned workflow, full
Debug, N1--N4 review, and the configured B28+C5, Rank1--50, WV100, and WV2--89
gates.  This design work itself makes no recovery or performance claim.

## Minimal safe first slice

The behavior-preserving observer seam in Stage 0 is the only implementation
slice justified directly by this audit.  It centralizes coverage and makes
event-order tests possible without yet choosing memory caps, owner scheduling,
or normalization lineage semantics.  It should include no TTF/Z3 public wiring
and no default policy change.

Before coding that slice, confirm the following implementation boundary with
the integration owner: the observer is always compiled but nullable; raw input
observation is independent of proof selection; `Proof` receives a non-owning
observer binding from `Kernel`; and all callbacks are preallocated/noexcept at
commit.  The larger ledger and owner builders should follow only after the
Stage-0 event trace proves the mutation inventory complete.

## Source references

- Clause arena and proof IDs: [`src/kernel/clause.hpp`](../../src/kernel/clause.hpp)
- Kernel state and mutation contracts: [`src/kernel/kernel.hpp`](../../src/kernel/kernel.hpp)
- Kernel mutation implementations: [`src/kernel/kernel.cpp`](../../src/kernel/kernel.cpp)
- Input normalization and proof replay: [`src/frontend/parse.cpp`](../../src/frontend/parse.cpp)
- Proof serializers and diagnostic shadow: [`src/proof/proof.hpp`](../../src/proof/proof.hpp),
  [`src/proof/proof.cpp`](../../src/proof/proof.cpp)
- Removed-action internal publisher: [`src/rules/semantic_extension/ttf_truth_table_live.hpp`](../../src/rules/semantic_extension/ttf_truth_table_live.hpp),
  [`src/rules/semantic_extension/ttf_truth_table_live.cpp`](../../src/rules/semantic_extension/ttf_truth_table_live.cpp)
- Z3 implication recognizer: [`src/rules/semantic_extension/z3_implication_recognition.hpp`](../../src/rules/semantic_extension/z3_implication_recognition.hpp),
  [`src/rules/semantic_extension/z3_implication_recognition.cpp`](../../src/rules/semantic_extension/z3_implication_recognition.cpp)
- Prior source-shape census: [Residual Oddball/Z3 implication and theory census](2026-10-04-oddball-z3-implication-census.md)
