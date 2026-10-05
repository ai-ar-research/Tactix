# Repeated certified GF(2) basis action design

## Scope and decision

This is a source and saved-evidence design audit.  No solver, checker, target,
or regression experiment was run, and this report makes no recovery claim.
The broader objective, proof policy, N1--N4 requirements, and acceptance gates
remain owned by the
[`winner-integration plan`](../../docs/winner-integration-plan.md).  The saved
target classifications remain owned by the
[`target-to-Boolean action map`](2026-10-04-target-to-boolean-action-map.md).

The next GF(2) increment should turn the existing certified affine foundation
into one retained-owner, row-at-a-time, repeated inprocessing capability.  It
should extend the existing public `AffineConsequence` identity with a new exact
payload semantics rather than add another independent recognizer.  The
controller selects one solver-offered basis row; the action translates that row
into implied clauses or a compact conservative XOR extension, then ordinary
CDCL and separately selected inprocessing continue.

The older `ParityConsequence` selected-small-sum path and
`AffineConsequence` V1 remain replay-compatible during the transition.  Their
recognition and proof machinery should converge on the same retained owner and
row engine.  They must not remain competing sources of independently numbered
theory meaning.  The dedicated affine/parity model constructors remain retired
from the active path: candidate enumeration, low-weight search, and direct
assignment publication are not part of this action.

## Current foundation and blocking gaps

The current tree already contains most of the soundness foundation:

- `affine_image` recognizes a GF(2) equation only from its complete
  forbidden-assignment CNF and retains exact source occurrences;
- `affine_basis_plan` deterministically reduces equations and constructs direct
  rows or exact `z = a xor b` definitions with projection obligations;
- `affine_basis_live` preflights proof events, fresh variables, arena growth,
  journal bytes and source revalidation before atomic publication; and
- `AffineBasisActionSession` durably suppresses exact consumed rows.

Four boundaries prevent this from being the target-scale repeated action:

1. V2 discovers and attempts to publish the whole basis in one invocation.  It
   has no mutation-free deterministic offerer for one exact row.
2. It recaptures the live formula after its own publications.  Although exact
   consumed keys prevent literal replay, generated clauses can enter later
   recognition and change the semantic owner.
3. Its proof planner uses the bounded local `Deriver`; any selected row whose
   premise-variable union exceeds sixteen is `UnsupportedProof`.  This does not
   cover large Tseitin cancellations or target-scale affine bases.
4. Its guard requires an assignment-free root.  Repeated inprocessing needs an
   explicit contract for a propagated root with persistent level-zero units,
   rather than an accidental startup-only boundary.

The internal `parity_certificate::Certificate` supplies the scalable direction:
it accumulates exact equations through fresh RAT XOR gates and RUP transition
lemmas without the local sixteen-variable case split.  It currently treats all
fresh identities as proof-only and retires their definitions, so those
identities cannot simply become branchable live variables.  The repeated
action needs a publication-aware form in which every installed variable keeps
a complete Boolean definition.

## Exact owner and premise contract

A solver-lifetime `Gf2AffineOwner` is created only by a trusted live-clause
census.  For a sorted set of distinct positive external variables `S`, the
owner admits

```text
xor(S) = b
```

only when one live clause is retained for every assignment to `S` whose parity
differs from `b`.  Thus an arity-`k` equation has exactly `2^(k-1)` required
sign masks.  Duplicate copies do not fill missing masks.  Repeated-variable or
tautological clauses do not contribute.  Both complete parities may coexist;
their combination is an immediate contradictory row.  Partial masks, family
names, a promising model, and historical handler attribution are never
premises.

Each equation record contains:

- sorted stable external variables and its right-hand side;
- one exact source identity for each required sign mask;
- the canonical external clause, physical source kind and current locator;
- logical proof-event identity and checker naming mode, where available; and
- source population, mapping and variable-retirement generations.

Large-clause references and internal variables are only cached locators.  The
durable identity is the canonical external clause plus its authenticated live
occurrence.  In particular, the `kernel_binary_id` sentinel must never be
reported as a retained native proof identity.

The first implementation should admit original/source clauses and any later
clauses authenticated by the bounded proof-population owner/pin mechanism in
the
[`ledger design`](2026-10-04-live-proof-population-ledger-design.md).  If a
derived or replaced premise cannot be shown checker-visible and live, the
owner declines it.  The short final pin spans source revalidation, proof
publication, kernel publication, journal commit, and session commit.

The owner freezes the exact pre-publication theory.  Clauses emitted by this
GF(2) session carry a producer identity and are excluded from rebuilding that
owner.  This prevents the action from recognizing its own consequences as new
theory premises.  A later owner version is allowed only after the current
owner's offers are exhausted and newly authenticated non-self sources supply
new complete equations.

Repeated operation may occur at a propagated level-zero trail.  Root values do
not fill missing sign masks and never justify recognizing an incomplete input
equation.  After recognizing the complete equation, planning may condition it
by removing a fixed variable and toggling the right-hand side when needed.  Each
such substitution is an additional one-variable equation backed by the exact
live root-unit reason and, with proof output, its pinned checker-visible unit.
The conditioned row key records those unit sources.  A root value without an
authenticated reason, or a retired variable whose stable mapping cannot be
revalidated, makes that row ineligible.  A new root assignment on a
participating variable invalidates affected cached offers; an unrelated unit
does not rebuild the theory owner.

## Deterministic basis and exact action payload

The producer computes a deterministic reduced basis from the retained owner.
Every row stores sorted output variables, right-hand side, pivot/order profile,
and the sorted set of source-equation ordinals whose XOR equals that row.  The
producer independently recomputes this equality before offering it.

Eligible rows are:

1. a contradictory `0 = 1` row;
2. an implied unit, binary, or bounded direct parity relation with at least one
   missing live clause; or
3. a wider pivot relation for which a new bounded XOR-definition network and
   asserted root can be published.

A one-source direct row whose complete CNF is already live is recorded as
covered and is not offered.  A wide one-source relation is eligible only when
its compact representation is new and passes the same proof, projection and
cost limits as a multi-source row.

One exact selection contains the owner/version identity, elimination-order
profile, output variables and right-hand side, and source-equation ordinals.
The variable-length fields are bounded and deep-copied by the public/replay
boundary.  An implementation may pass an opaque offer token inside one live
controller, but deterministic replay must retain the complete key.  A bare row
ordinal or caller-supplied digest is insufficient: apply always reopens the
retained owner, recomputes the selected XOR, and requires exact payload
equality.

The pure offerer returns bounded deterministic pages in this order:

```text
contradiction, unit, binary, increasing output width, canonical row key
```

Its continuation is the last canonical row examined, including committed,
duplicate, structurally ineligible and resource-rejected rows.  An offer is
returned only if the exact current action could publish at least one new
constraint under the supplied limits.  The offerer changes no formula, proof,
owner/session state, action identifier, provenance stream, observation, or
heuristic score.

Once paging establishes that the immutable owner has no remaining offer, that
completed/no-structure census remains terminal across later search and restart
epochs.  It is reset by an owner or session change, a changed profile under its
typed reconfiguration rules, or another explicitly retryable terminal state;
ordinary learned-clause and restart churn cannot make the frozen owner acquire
new source equations.  Resource exhaustion and interruption remain held for
explicit reconfiguration rather than being misreported as completion.

Default, random, and future learned policies consume these same exact offers.
DefaultPolicy may eventually impose a concrete ordering over them; it does not
receive a private GF(2) implementation.  A selected resource-exhausted,
interrupted or stale row remains available only for explicit reconfiguration
or a later controller decision.  The action never substitutes a different row,
falls back to a constructor, or restores an earlier solver state.

## Boolean effects and proof obligations

### Direct row

For a row of bounded direct width, publish the complete parity CNF.  Every
published clause is implied by the exact source equations and has a checked RUP
derivation.  No new model variable is introduced, so projection is the
identity.  A root conflict publishes the checked empty clause and returns
`Unsat`.

### Compact wide row

For a wide row, introduce an acyclic balanced XOR network.  Each new variable
has the exact meaning

```text
z = a xor b
```

through four pivot-first RAT definition clauses.  The final root literal
asserting the row parity is implied by the source equations and must be emitted
by a checked RUP/DSR derivation.  Omitting the new variables projects any
extended model to an original model; extending an original model evaluates the
definitions in allocation order.  This is a conservative extension plus an
implied root assertion, not a satisfiability-only transformation.

The scalable certificate should adapt `parity_certificate::Certificate`.  A
sound first implementation may retain every accumulator XOR definition whose
variable becomes branchable, while deleting proof-only transition lemmas.
Every installed helper then has a unique topological Boolean meaning.  It is
not sound to delete a helper's defining clauses and expose that variable to
search.  A later compact-DAG optimization is acceptable only after it derives
the same final root and preserves exact reconstruction.

The clausal DSR/forward-DRAT route is the default.  No-proof mode must perform
the same preflight and allocate the same external identities.  LRAT or PBP
declines before mutation unless a complete rule-specific implementation exists.
A future PBP form may replace the whole certificate for a measured large-parity
case when the clausal proof is impractical; it must not splice PB commands into
a DSR stream or act as a silent fallback.

Proof additions, deletions, literal occurrences, namespace growth, arena words,
journal bytes, source pins and reconstruction storage are all preflighted before
one commit.  Every recoverable decline leaves the kernel, proof stream,
provenance stream, action sequence and retained session unchanged.  Publication
leaves root propagation as a separately selected controller action.

## Duplicate, relevant-change and lifecycle contract

The session retains two notions of identity:

- the exact action key: owner/version, sources, output relation and
  representation; and
- the semantic output key: output variables, right-hand side and installed
  definition identity.

Committed exact actions remain consumed even if a later pass deletes their
published clauses.  A pre-existing live relation is skipped without consuming
an action key, so it may become eligible if that unrelated live copy is later
deleted.  Session bounds prevent distinct proofs of one semantic relation from
creating unbounded duplicate extensions.

Relevant change is source-local.  The fast key consists of participating
external-variable occurrence generations, exact source liveness, mapping and
retirement generations, and owner version.  Global `touch_epoch`, arena
relocation, watch order, unrelated learned clauses and ordinary search do not
invalidate an owner.  Arena collection may refresh a locator only after the
same canonical source occurrence and proof lineage are found.  Source deletion,
replacement, proof-ID rebinding, variable retirement, or a missed ledger event
makes the affected row stale.

Creation records are insufficient for the stated provenance requirement.
Every later deletion or replacement of an installed definition/consequence must
be linked back to its action and semantic identifier through the population
observer.  A duplicate scan uses the same exact canonical clause population;
hashes accelerate equality but never decide it.

## Bounds, records and raw feedback

No zero means unlimited.  The action profile independently bounds source
clauses/literals/groups/patterns and owner bytes; arity; recognition work;
equations and rank; elimination rows/work/intermediate width/source and premise
counts; offer candidates/offers/enumeration work; direct width; XOR nodes and
external extent; duplicate and revalidation work; proof additions/deletions and
literal occurrences; arena words; journal bytes; and session owners/keys/bytes
and work.  The controller attaches one configured effort allowance to the
chosen row.  There is no global effort budget.

The action journal records the owner, exact clauses, equations, selected source
set, derived row, every new variable and its operands/definition/role, installed
clauses, proof event range, projection and reconstruction operations, action
parameters, terminal status, and subsequent deletion/replacement.  Raw feedback
separates source capture, recognition, elimination, offer enumeration,
duplicate scanning, revalidation, proof work, proof size, namespace/arena
growth, journal cost, rows/clauses/variables published, propagation debt and
session commits.

All installed extension variables use the shared `IntroducedTheory` role.
Existing row, pivot and free-coordinate variables use `ExistingTheory`.  The
producer stages neutral candidates only; the separately selectable semantic
advice action applies one configured VMTF/EVSIDS dose.  The observation remains
read-only and reward computation remains outside this increment.

## Target cohort and honest limits

Saved evidence gives the following GF(2) cohort.  These are development targets,
not current-action admission or recovery results.

| Cohort | Rows | What this action can establish | Additional gap |
|---|---|---|---|
| Fixed 44, strong affine/parity signal | 263, 540, 557, 569, 896, 1004 | Exact basis rows, compact XOR coordinates, contradiction where present | 569/1004 also require checker-compact proofs; 540/557/896 may need scheduling/count composition |
| Fixed 44, unresolved signal | 97, 410, 479, 627, 762 | Current-source admission census only | Evidence is weak, recognition exhausted, or the name was the only parity clue |
| WV100 SAT parity | ranks 7/16, two-trees | Basis coordinates and compact definitions instead of assignment construction | CDCL/advice must recover a model |
| WV100/WV2 SAT MDP | WV100 51/63/64/65; WV2 187/188 | Exact affine subsystem and useful coordinates | The decisive low-weight/cardinality syndrome needs a separate source-bound Boolean count/weight action |
| WV100/WV2 UNSAT Tseitin | WV100 80/83/86; WV2 132/133/139/141/143/146 | Complete parity sum and certified contradiction | Proof size/checking cost must remain bounded |
| WV2 ordering XOR | rank 160 | XOR rail definitions and parity consequences | The ordering invariant remains a separate action gap |

Hardware/circuit rows whose saved evidence merely reports many XOR gates are
not automatically in this cohort.  Complete gate recognition and a useful
source-backed relation are still required.

## Smallest target-validation corpus

Microproof tests precede all target runs: every incomplete-mask negative, both
parities on one scope, deterministic paging, forged/stale payload rejection,
source deletion and arena relocation, unit/binary/small/wide/contradictory rows,
every resource boundary, interruption atomicity, proof-on/off allocation
identity, projection, and deletion-aware duplicate behavior.  Every addition is
checked forward and every SAT extension is projected to the original CNF.

After those controls, the smallest representative real corpus is:

| Order | Membership and file | Purpose |
|---:|---|---|
| 1 | WV2 rank 132, `tseitin_d3_n158` (237 variables, 632 clauses) | Small terminal `0 = 1`, checked DSR proof, repeat and duplicate lifecycle |
| 2 | WV100 rank 7, `two-trees-511v` (511 variables, 2,039 clauses) | SAT basis/extension and original-model projection without the old constructor |
| 3 | WV100 rank 51, `mdp-32-11-sat` (1,038 variables, 5,842 clauses) | Separate affine progress from the still-missing weight bridge |
| 4 | fixed rank 263, `lightsout...corner_0_unsat` (2,475 variables, 8,172 clauses) | Dense affine image plus totalizer residual and target-scale proof cost |
| 5 | fixed rank 569, `lec_mult_CvW_12x12` (3,426 variables, 10,231 clauses) | Checker-sensitive UNSAT and compact-certificate representative before near-duplicate rank 1004 |

The first expansion adds WV2 rank 139 `tseitingrid6x200_shuffled` as a
clause-order/renaming control.  Fixed rank 762 is an admission negative until a
complete clause-backed XOR image is actually found.  Failure on MDP must lead to
the planned count/weight interface, not restoration of low-weight local search.

## Implementation sequence and acceptance boundary

1. Add the immutable owner/session and pure bounded row offerer without public
   scheduling changes.
2. Add one-row direct publication and its exact replay payload.
3. Replace the sixteen-variable wide-row proof boundary with the
   publication-aware scalable certificate and complete projection records.
4. Integrate the offerer into the shared controller action cache; exercise
   repeated closure with DefaultPolicy still disabled.
5. Run the microproof suite and the five-instance corpus with independent
   proof/model checking, then broaden to the full cohort and standing gates.

No step is accepted merely because the mechanism is disabled.  Promotion still
requires the enabled DefaultPolicy configuration, both-policy fuzz, full Debug,
B28+C5, Rank1--50, and the row-level WV100/WV2 recovery/preservation gates
specified by the integration plan.  The fixed 44 target remains unchanged.
