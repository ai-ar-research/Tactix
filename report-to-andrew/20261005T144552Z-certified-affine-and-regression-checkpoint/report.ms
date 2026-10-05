.po 0.8i
.ll 6.9i
.ps 16
.ft B
.ce
Tactix winner-integration checkpoint
.ce
Certified affine rows, noL discovery, and regressions
.ps 11
.ft R
.ce
Development report for Andrew
.ce
October 5, 2026
.sp 2
.ft B
Outcome
.ft R
.sp
.ti 0.25i
The latest completed DefaultPolicy regression checkpoint is accepted on all
four standing sets. B28+C5, Rank1--50, and WV100 retain every expected
disposition; WV2--89 gains one checked solve, ramsey_4_4_19, relative to the
preceding accepted checkpoint. There is no wrong verdict, checker rejection,
or scorer failure. The complete Debug campaign passes 289/289.
.sp
.ti 0.25i
This benchmark evidence is for immutable Release source 5eb58e52c and Debug
source 64de46fe9. It predates the new certified affine and noL commits. The
current development head adds a shared-controller path for exact GF(2) rows and
hardens noL discovery, but has only focused and microproof evidence. Formal
fixed-target coverage remains 1/44: the Ramsey row. No portfolio claim is made
from the new code yet.
.sp
.TS
tab(|) allbox;
l r r l.
Campaign|Dispositions|Solver sum|Independent disposition
B28+C5|33/33|956.316 s|17 models, 15 proofs, one clean proof censor
Rank1--50|50/50|7,559.461 s|25 models, 17 proofs, eight clean proof censors
WV100|100/100|2,718.740 s|33 models, 65 proofs, two clean proof censors
WV2--89|89/89|3,990.643 s|48 models, 30 proofs, two clean censors, nine allowed timeouts
Debug|289/289|814.11 s|242 executables and 295 bindings reverified
.TE
.sp
.ti 0.25i
All proof censors are clean expirations of the single 1,800-second regression
checker allowance, rather than proof rejection. Completed comparisons remain
within their row limits. Relative to accepted a9f1e14d4, solver sums improve by
2.739 s on B28+C5, 110.413 s on Rank, 43.504 s on WV100, and 97.171 s on WV2.
WV2 solver timeouts fall from ten to nine because ramsey_4_4_19 now solves and
checks UNSAT in 0.167 s.
.sp 2
.ft B
Certified GF(2) integration
.ft R
.sp
.ti 0.25i
Commits 3787d6724, 2b2d7ecb8, and 405b56724 implement one vertical slice of
the requested architecture. X9 AffineConsequence owns an immutable
pre-publication clause view, reduces a bounded GF(2) basis, pages exact rows,
and commits one selected row at a propagated root. Direct rows publish only
missing parity CNF. Wide rows publish a compact conservative Boolean extension
instead of invoking a private local-search solver.
.sp
.ti 0.25i
Each wide transaction remaps authenticated source equations into a dense proof
namespace, derives a balanced XOR network, and retains exactly the final
definition clauses and signed root unit. Transition helpers absent from the
final network remain in the monotone namespace but are fixed true by vacuous
RAT units after an exact active-population check. Every retained definition
records its output and operands, so reconstruction evaluates the XOR network
topologically and omits the extensions.
.sp
.ti 0.25i
The transaction records source occurrences, exact selected equations, row and
RHS, proof additions/deletions, distinct RAT roles, introduced and proof-only
helpers, projection, action identity, work, yield, and extent. Preparation is
mutation-free. Source loss, stale ownership, unsupported proof format,
interruption, or any proof/work/namespace/journal bound declines before
publication. Proof-on and proof-off allocate the same namespace.
.sp
.ti 0.25i
The same direct or extended payload is available through additive C API v9,
deterministic replay, canonical DefaultPolicy execution, the opt-in gf2-affine
random producer, and future learned-policy submission. The learner therefore
chooses an ordinary typed action rather than entering a separate solver. M1
propagation and semantic-variable advice remain separate follow-ups.
.sp
.ti 0.25i
Sixteen focused Debug tests pass. The exact two-source arity-eight fixture has
15 source variables, 256 input clauses, 13 retained definitions, and 17 retired
helpers; its 972-lemma transaction proof is accepted by pinned drat-trim in
SAT-forward mode. A contradictory width-eight certificate is accepted as a
full refutation. Broader source overlaps, three-or-more-source rows, DSR
checking of the wide path, real targets, full Debug, and the regressions remain
pending for this new source.
.bp
.ps 14
.ft B
.ce
noL and X17
.ps 11
.ft R
.sp
.ti 0.25i
Lucky/25 remains diagnostic. On noL-11-10 and noL-11-4, exact recognition fired
repeatedly but both 600-second runs timed out after roughly 0.75 billion search
moves, without a model or consumed partial guidance. This is a search and
publication miss rather than a recognizer miss; it does not meet the Boolean
integration objective.
.sp
.ti 0.25i
Commit 8d13ca87a advances the certified route. ULC reauthenticates the
checker-visible live population, requires the unique irredundant ALO source,
chooses an unassigned endpoint, and emits pivot-first RAT through DSR and DRAT.
X17 replaces combination enumeration with a canonical relation-clause index
and durable bounded pages. Focused Debug and Release tests pass; a synthetic
noL census finds 363 domains, 2,486 relations, 7,458 tuples, and 22,374 exact
prefixes. The ULC microproof passes independent DSR and DRAT checking. The two
real noL targets have not yet been recovered.
.sp 2
.ft B
What the evidence permits
.ft R
.sp
.ti 0.25i
The accepted regression result says the pre-integration DefaultPolicy remains
strong and the Ramsey Boolean transfer improved WV2 without a regression. The
new affine/noL work establishes certified learner-selectable mechanics and
focused proof soundness. It does not establish target effect or portfolio
superiority. The fixed 36 and all 44 misses remain recovery obligations, with
B28+C5, Rank1--50, WV100, WV2--89, full Debug, and both-policy fuzz as gates.
.sp
.ti 0.25i
The next immutable checkpoint should run full Debug, then probe WV2-132
Tseitin, WV100-7 two-trees, WV100-51 MDP, rank 263 lightsout, and rank 569
lec_mult. Logs must distinguish absent structure, offer rejection, proof-work
exhaustion, successful publication, and post-publication search effect. Only
after coherent target evidence should the four full regressions be repeated.
.sp
.ti 0.25i
The saved d170b4a5 comparison remains the approved authority and was not
rerun. DSR remains the default proof format. A rule-specific pseudo-Boolean
route remains an option only if measured compact clausal certification proves
impractical.
