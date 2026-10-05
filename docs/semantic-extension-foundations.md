# Semantic extension foundations

This live companion to [the calculus](calculus.md) owns the internal affine,
exact-cover, independent-set, GF(3), direct-PHP and Tseitin transitions for
private adapters and public rules.
The [controller interface](controller-interface.md) owns their
action/session/replay shape; `PROGRESS.md` owns implementation and acceptance status.

## Shared publication contract

The live publication adapters pre-render a bounded provenance journal and separate semantic
variables from encoding or proof helpers. Recognition alone licenses no inference:
publication requires exact source premises, a complete proof/formula plan, immediate
stale-source validation, and one uninterrupted commit. Exhausted bounds, interruption,
unsupported proof modes, or incomplete plans have no solver effect. The controller may
later rank declared existing or introduced theory variables; producers do not alter
VMTF, EVSIDS, phases, or other heuristic state.

The public boundary grows additively: v4 gives affine, exact-cover and
independent-set separate actions; v5 adds exact selected GF(3)-pair and DirectCSP
row consequences plus terminal Direct PHP; v6 adds the complete authenticated
GF(3) Gaussian batch; v7 adds an exact selected Tseitin component. Structured
payloads are deep-copied by the corresponding C configuration, survive typed
replay, and carry explicit limits only. Durable source ownership, duplicate
keys, action/semantic identities, provenance streams and fresh variables stay
in solver-lifetime sessions. A productive nonterminal proof action may stage
neutral existing/introduced theory variables; a configured propagation
follow-up settles first, then the dispatcher publishes only still-branchable
variables as a current immutable candidate for the separate H-only
`SemanticVariableAdvice` action.

Every attempt reports raw status, effort, proof, yield and session-commit deltas
and charges one application. Applied, duplicate and complete
no-structure/no-outcome results consume the exact selection. Invalid,
inapplicable, unsupported-proof and stale results discard it until context
changes. Resource exhaustion and interruption retain the exact payload but
suspend eligibility until an explicit profile change retries it; formula,
session or owner change discards it. No outcome triggers fallback or rollback.
DefaultPolicy omits the unaccepted theory actions. Random selection may use only
exact caller-supplied or native-produced payloads. `PROGRESS.md` owns acceptance
status.

## Live population and source authentication

Source authentication keeps raw input, checker-visible clauses, kernel clauses,
and structural owners as separate populations. Durable clause identities use
signed external literals. Raw input retains multiset semantics, while a
source-eligible normalized clause uses set semantics; normalization, replacement,
and proof-ID rebinding require explicit lineage events. LRAT deletion is exact by
checker ID. DSR and DRAT deletion is only a literal-named request: a checker may
retain an implied unit, so the source layer must conservatively stale an owner on
a request for any of its canonical source buckets rather than claim an exact
checker multiplicity.

The always-compiled population observer is an allocation-free event seam. Its
clause views are callback-local, it owns no records, and it cannot authenticate
or pin a source by itself. Pending proof and bulk solver mutations are exposed as
transactions, and proof-only helpers are never source eligible. Representation
changes such as watch movement, arena collection, literal pose, and variable
compaction emit no semantic population event.

The bounded source layer built on this seam must register before the first input
event, preallocate all callback storage, and degrade permanently on an event it
cannot represent exactly. Trusted builders may retain only solver-issued handles;
they cannot submit clause bytes or proof IDs. A finalized owner yields an opaque
immutable snapshot and a move-only pin after exact route, liveness, occurrence,
eligibility, and bound checks. A pre-mutation permit hook must protect pinned
sources against retirement or rebinding through the noninterruptible commit.
Until that ledger, builder, and permit path are implemented and validated, TTF
and Z3 recognition remain internal and cannot supply public actions.

## Affine consequence

Capture a bounded exact live source view and accept an affine equation only when all
forbidden assignments of its distinct variables occur as source clauses. Bounded
GF(2) elimination either preserves the V1 selected small consequence or, in V2,
reduces a deterministic basis. V2 emits small rows directly and represents wide rows
with fresh exact XOR-tree definitions plus a root-parity unit. DSR/DRAT RAT establishes
each four-clause definition and RUP derives each row from its exact source equations.
Projection drops the fresh variables; reconstruction evaluates the dependency-ordered
XOR tree, so the extension is conservative. Count-only preflight fixes proof, arena,
extent, journal, and retained-session costs before source-ID/literal revalidation and
commit. Durable row/RHS/source/native-clause keys suppress repeated rows after deletion
without blocking newly exposed rows. Completing the retained owner's census with no
remaining offer is also durable across restart/search epochs, so an immutable empty
census is not rescanned; owner/session/profile changes and explicit retry states retain
their typed reset rules. Existing and introduced theory variables are reported
separately; publication leaves propagation to `Propagate`. An explicitly configured
propagation follow-up defers the one semantic-variable publication until the settled
root, while an independent later propagation simply stales a previously published
candidate. V2 decline never falls back to V1 and has no solver or session effect.

## Exact-cover occupancy extension

At an assignment-free propagated root, require the complete current formula to be one
signed SGEN image of arity `k >= 3`. For each recognized block
`B = {x_1, ..., x_k}`, allocate one fresh theory variable `b_B` and add
`(b_B or -x_i)` for every `i`, followed by
`(-b_B or x_1 or ... or x_k)`. These RAT clauses define exactly
`b_B iff OR(B)`. Every old assignment has one unique extension, and model
reconstruction drops each `b_B`; the transition is a conservative extension rather
than an implied restriction on old variables.

The internal action plans all blocks from one retained source because the first
definition breaks strict recognition. It records action/slot meaning, source
occurrences and available native proof IDs, generated proof IDs, literal/addition
cost, lifetime, and projection. Its durable owner session stores exact signed-external
duplicate keys, so removing the generated definition clauses cannot authorize the same
extension again. Whole-batch recognition, planning, proof, formula, and journal work
are atomic; consumed keys commit only after successful publication.

## Independent-set cover

Strict recognition requires the complete layered exact-count encoding used by the
historical LymphoSAT constructor. The source contains both base clauses
`(x_1 or S[1,0])` and `(-x_1 or S[1,1])`; for every `i >= 2`, both transition forms
`(-S[i-1,s] or x_i or S[i,s])` and
`(-S[i-1,s] or -x_i or S[i,min(s+1,K)])`; pairwise at-most-one clauses for every
state layer `i >= 2`; every terminal implication `(-S[N,s] or S[N,K])` for `s < K`;
and the terminal cover `(S[N,0] or ... or S[N,K])`. Negative vertex binaries are the
graph edges, and the source must contain at least one edge. Every supplied clause must
be classified exactly; ambiguity, interruption, malformed input, or a missing or extra
clause declines without an image.

The exact-count clauses imply that every model selects at least `K` vertices, while
the edge clauses make the selection independent. For a selected set `S` of
`N-K+1` distinct stable vertex identities, the theory entails `OR(S)`: otherwise at
most `K-1` vertices remain available. The certificate enumerates forbidden source
assignments and resolves them to this positive cover, emits each step as RUP, publishes
the conclusion over existing variables, and deletes proof-only intermediates. It adds
no variable or reconstruction obligation.

An exact retained owner snapshot permits later unrelated clauses and repeated distinct
covers, but every owner clause is revalidated by native ID and canonical literals before
commit. Unit covers are currently declined because their propagation proof delta is not
yet part of the bounded transaction.

## GF(3) affine consequence

At an assignment-free propagated root, recognize each ternary domain only from
one positive at-least-one clause and all three negative pair exclusions. Recognize
an equation only when the source contains every forbidden tuple for its ordered
domain list, coefficients, and right-hand side. A caller-selected pair of equations
may be eliminated only when they share a domain with nonzero coefficients; scaling
and subtraction in GF(3) removes that domain and produces the exact resultant
equation. The publisher emits the complete set of forbidden-tuple clauses for the
resultant, using only the existing one-hot literals. It introduces no variables and
has identity model projection.

The live adapter retains exact physical source identities, native proof IDs, and the
recognized image in a caller-owned snapshot. It bounds capture, recognition, pair
planning, duplicate scanning, proof size, arena growth, journal rendering, external
extent, and a complete final source revalidation. Every emitted clause is RUP from
the two source equations plus the eliminated domain's at-least-one clause; focused
DSR and forward-DRAT replay check all 54 additions in the reference fixture. The
commit leaves propagation to a later controller action and publishes existing
semantic variables as advice metadata. Repeated clauses are detected against the
live database. Unsupported proof modes, incomplete encodings, stale sources,
interruption, or exhausted bounds decline before mutation. The public v5 action
retains the recognized owner, ordered equation-pair
keys and exact replay state in a kernel-lifetime session. Its C and replay forms
carry the same bounded parameters plus a canonical `first < second` pair;
structured execution, raw feedback, and the distinct GF(3) advice producer are
implemented under the shared contract. Target recovery remains pending.

### Gaussian unit/conflict certificate boundary

The internal Gaussian bridge is stricter than the public two-equation action.
It accepts only a theory whose source is exactly consumed by certified ternary
domains and equations: unique occurrence/native-proof identities, three
globally distinct atoms per domain, the exact ALO plus three AMOs, strictly
ordered nonzero-coefficient rows, and every forbidden tuple. Replay checks each
`Source`/`Scale`/`Add` parent, coefficient, provenance recomposition, outcome,
accounting item and reduced-basis rank.

The v2 certificate handles only unit rows and nonzero zero-row conflicts. Each
consequence stores compact indices into the validated authentication table and
is RUP-checked from exactly its declared source slice. Bridge bounds cover
source clauses/literals, domains, equations, terms, authentication references,
Gaussian work/fill/width/provenance/derivations and retained plan bytes;
certificate bounds cover work, events/literals, active clauses, retained source
references, event width, branch domains and consequences. The retained-byte
limit is a logical plan bound, not an allocator/RSS promise. Invalid,
unsupported, interrupted or exhausted construction exposes no partial plan.

The live adapter and public X14 action complete this boundary through publication. They
capture the exact current GF(3) owner, build the full chained Gaussian plan,
then independently validate both the algebra and every physical source before
commit. A prepared transaction binds all recognition, bridge, Gaussian,
certificate and publication limits, action/semantic identities, and its record
stream. Its bounded journal records each source, authenticated equation image,
derived row/RHS, original source combination, published clause and proof event.
Only unit clauses and an empty conflict enter the solver; intermediate RUP
clauses remain proof-only. DSR, DRAT and no-proof publication are supported,
while LRAT/PBP and proof-backed split publication decline. The implementation
offers one deterministic complete-batch identity committing every consequence,
derivation and exact premise. Apply freshly rebuilds and exact-matches it before
atomic publication. C v6, typed replay, opt-in random production and raw
feedback are implemented; effective fuzz, scheduling and performance evidence remain open.

## Direct PHP terminal certificate

The internal live adapter recaptures and revalidates at admission, preparation
and publication. Its stale identity includes the complete source snapshot,
clause namespace, arena generation, variable/external/reconstruction extents
and proof pointer. Native identities authenticate large clauses. Parser-finalized
non-LRAT binaries have no retained native ID, so capture assigns explicitly
tagged, deterministic local replay identities. Those tokens are zeroed before
the DSR/DRAT literal-based deletion call and can never be used as LRAT
references. Generated proof and variable IDs remain monotone. Bounds cover
snapshot clauses/literals/work, recognition clauses/literals/work/candidates/
combinations/mapping trials/holes, certificate additions/deletions/literals/
fresh variables/events, and action-record replay work. Publication is the
noninterruptible commit boundary.

The public v1 action session is constructed with one Kernel, cannot be
copied or moved, and must be destroyed first. There is no Kernel generation
token, so it does not claim to detect address reincarnation. It validates a
committed request and expected record before reporting `AlreadyApplied`.
Proofless callers may split prepare from publish. With a proof attached, the
split API returns `UnsupportedProofSplit`; one-shot apply/replay keeps exact
validation and terminal publication in one call because `Proof` exposes no
complete mutation epoch.

The append-only registry exposes `DirectPhpRefutation` as rule 36, disabled
StepId 25 and C parameter tag 39. It has a canonical terminal payload and typed
snapshot, recognition, certificate and replay limits. The online executor
returns terminal UNSAT after successful publication, rejects a propagation
follow-up, and reports raw status, capture/recognition/replay work and proof
counts. DefaultPolicy omits it; the random controller and explicit replay can
select it through the same action. This completes one public vertical but does
not establish target recovery or production scheduling.

## Tseitin-cycle live step

The internal snapshot includes exact internal/external maps, arena generation,
touch epoch, root-assignment count, next clause ID, variable count, every live
large clause, and both directions of every binary watch. Malformed or retired
variables, duplicate proof IDs, asymmetric/duplicate binaries and exhausted
clause/literal/work/variable limits decline with an empty output. Separate
bounds cover group admission, cycle planning and witness length, transaction
events/additions/deletions/helpers/witness literals, semantic validation ticks
and proof namespace.

Public X16 enumerates every eligible connected component as an exact `tcx2:`
payload. The complete stored-clause census derives width-2--4 XOR groups,
quarantines ambiguous/even/bad-degree components and conservatively rejects any
unclaimed stored clause touching a component edge, even when root satisfied.
Exact root units are a separate authenticated part of the live formula: they
may fix off-cycle edges, while available-edge Kruskal planning selects a cycle
whose substituted edges are all unassigned. This physical residual boundary is
the declared public domain; it is stricter than the mathematical X7 condition.

Offer is mutation free and records no proof or journal event. Apply recaptures,
selects only the named component, rebuilds admission/planning/transaction and
exact-matches source occurrences, native proof IDs, canonical clauses, root
image, incidence, cycle, witness and substitution. The Kernel-bound session
stores a semantic `tcd1:` key distinct from replay identity. Prebuilt JSONL
records the action/parameters, mappings and root units, exact sources, proof
events, installed unit, projection/deletion state and effort/proof volume.
Native DSR emits two temporary helpers, one RUP unit and two deletions; no-proof
performs the same kernel transition. DRAT/LRAT/PBP decline before mutation.
Propagation remains separate. Fresh discovery after a changed root may expose
another component; typed replay, C v7, random production and raw feedback share
the same path, while DefaultPolicy remains disabled pending acceptance.

## Direct CSP support consequence

At an assignment-free propagated root, recognize two complete signed one-hot
domains and the exact directed table of forbidden value pairs. Selecting one
left-domain row entails
`not left[row] or OR(compatible right values)`: resolve the right-domain cover
against every forbidden pair in that row. Emit the clause directly when it fits
its bound. Otherwise, when enabled, define one fresh theory variable exactly as
the OR of the compatible values by RAT clauses and publish the shorter support
clause. The extension has a unique value, is omitted from the external model,
and requires no reconstruction witness; direct publication has identity
projection.

The public v5 action carries both full canonically ordered signed domains and
the selected row in C and replay payloads. A kernel-lifetime session retains one
exact owner per ordered domain pair and committed row keys across later clause
deletion. Atomic preflight, source revalidation, DSR/DRAT proof publication,
raw feedback, and the distinct DirectCSP advice producer use the shared contract
above. Target recovery remains pending.

## N-ary CSP prefix foundation

The next finite-domain increment generalizes the Boolean representation without
pretending an n-ary relation is a collection of unrelated binary domains. Its
pure planner accepts at least three disjoint, complete signed one-hot domains
and a sparse, strictly lexicographic list of exact forbidden tuples. A typed
selection fixes one value in every domain except a named residual domain. The
planner introduces one fresh semantic helper `p` exactly equal to the
conjunction of that prefix and derives a binary `(~p or ~v)` for each forbidden
extension value `v`. When all residual values are forbidden, it additionally
derives `~p` from those binaries and the residual ALO.

Every definition is fresh-pivot RAT and every relation consequence is RUP from
the exact tuple clause plus the definition. The planner verifies all domain
ALO/AMO clauses, tuple coordinates, signed literals, premise occurrences and
native proof IDs; an extra, missing, duplicate, unordered, overlapping or
mis-spelled source item declines. Bounds cover source clauses/literals, domains,
domain values, tuples, prefix arity, additions, addition literals and charged
work. Declines return no partial additions or projection record. Exhaustive
small-model tests show that old models extend uniquely with
`p = AND(prefix)` and that dropping `p` projects every extended model to the
original formula; independent unit propagation replays every RUP step.

The live adapter captures those exact domains and tuples from the current
formula, preflights proof/namespace/arena/journal capacity, immediately
recaptures the source, and atomically publishes DSR/DRAT plus complete JSONL
provenance. A Kernel-bound action session retains exact owners and durable
prefix keys across safe namespace growth; repeated keys cannot recreate a
helper. A pure native offerer discovers canonical signed exact-one domains,
forms bounded fixed-arity disjoint collections, revalidates retained owners
before discovery, enumerates every tuple/residual prefix, suppresses committed
keys, and preflights session plus live resources. Hard census bounds clear
partial output; the separate offer limit has an exclusive deterministic cursor.
It exports original values and the exact-AND helper through neutral semantic
metadata but changes no heuristic state. Registry/C/replay/controller exposure,
fuzz reach, and noL recovery remain pending; the internal action is not a solve
claim.

## Truth-table parity frontier

The TTF recognizer starts from a caller-retained checker-formula snapshot because
import-time root units can simplify away clauses needed to establish the original
structure. It accepts repeated truth-table stars only after classifying the full
root census, exact OR definitions, forced roots, zero/nonzero links, coordinate
incidence, and every source occurrence. A separately supplied AMO witness is not
trusted: each component is re-recognized from exact source clauses, and shared
root ownership is explicit.

For the first odd coordinate whose forced atoms are all nonzero, the current
frontier planner publishes one reduced participation clause per domain. Each
clause is RUP from recorded truth-table premises; the corresponding exact AMO
leaf sets remain obligations for later reasoning. The increment deliberately does
not claim a full refutation. It uses only active, unassigned existing variables,
adds no reconstruction state, and leaves propagation to the controller.

The live publisher bounds snapshot preparation, recognition, planning, proof and
formula additions/literals, arena growth, journal size, and an owner refresh. Its
durable duplicate key contains the coordinate leaf sets plus all truth and AMO
source identities, including native proof IDs. It pre-renders provenance, refreshes
the retained snapshot exactly, then commits clauses, proof events, journal, and the
session key together. Focused DSR and DRAT certificates verify the micro-frontier;
strict-warning, Debug, and ASan/UBSan checks pass. That refresh authenticates only
the retained copy, not its relationship to the current checker population. The
attempted public action was therefore removed: a caller could otherwise supply a
forged snapshot, and later parser normalization or proof deletion can invalidate
premise identities invisible to the kernel. Public scheduling requires a
solver-lifetime proof-population ledger (canonical external clauses, raw/current
proof IDs, live/deleted/replaced state and mutation generation), or equivalent
source pinning, before configuration. Subsequent parity consequences, target
recovery, and a complete UNSAT argument also remain pending.

## Broader recognition foundations

The next integrations generalize these exact source contracts rather than
reintroducing constructor search. `z3_amo_recognition` recognizes a closed Z3
totalizer/Tseitin `AtMost(...,1)` component only when its asserted root, complete
OR gates, root-guarded exclusions, and every leaf pair are owned exactly once.
It remains a foundation until its complete live producer is wired through a
public action and independently checked; recognition by itself publishes nothing.
