# Semantic extension foundations

This live companion to [the calculus](calculus.md) owns the internal affine,
exact-cover, independent-set, and GF(3) transitions for private adapters and
public rules.
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

The public v4 boundary gives these transitions separate append-only actions:
`AffineConsequence` and `ExactCoverExtension` use canonical payloads;
`IndependentSetCover` uses a copied structured vector of stable positive
external vertices. Typed parameters contain explicit limits only. Durable
source ownership, duplicate keys, action/semantic identities, provenance
streams, and fresh variables stay in solver-lifetime sessions. A productive
proof action stages an immutable existing/introduced theory-variable candidate;
the separate H-only `SemanticVariableAdvice` action may later apply bounded
priority doses. Declines and failed recognition expose raw consumed work and
charge the controller's one-application dose without committing logical state.

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
without blocking newly exposed rows. Existing and introduced theory variables are
reported separately; publication leaves propagation to `Propagate`. V2 decline never
falls back to V1 and has no solver or session effect.

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
interruption, or exhausted bounds decline before mutation. A public C++ action now
retains the recognized owner, selected equation-pair keys and exact replay state;
shared controller/C/replay/feedback wiring and target recovery remain pending.

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
reintroducing constructor search. `direct_csp_support` recognizes complete two-domain one-hot variables
and forbidden pairs, then plans direct or exact-OR support clauses.
`z3_amo_recognition` recognizes a closed Z3 totalizer/Tseitin
`AtMost(...,1)` component only when its asserted root, complete OR gates,
root-guarded exclusions, and every leaf pair are owned exactly once. These are
foundations until their complete live producers are wired through the public
actions and independently checked; recognition by itself publishes nothing.
