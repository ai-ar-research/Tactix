# Algebraic proof bundle

This format remains a supported certificate and regression route. Whole-run
PBP checked by pinned VeriPB and verified CakePB is being added as an explicit
option; that work does not remove or deprecate this bundle, DSR, DRAT, LRAT,
Trestle or XLRUP.

Tactix exposes one UNSAT certificate bundle even when a solve uses several
proof calculi. The bundle binds every section to one original CNF and one
ordered DSR search proof. A result is verified only when every referenced
algebraic lemma batch and the DSR consumer have passed their pinned checkers.

## Bundle contract

The bundle manifest records:

- the original CNF SHA-256 and dimensions;
- the Tactix binary, source snapshot, checker binaries and checker arguments;
- a content-addressed lemma table with stable IDs and exact normalized clause
  bytes;
- one or more algebraic batches, each naming its backend, source identities,
  lemma-ID range, certificate digest and resource limits;
- the ordered DSR search proof digest and the exact augmented-CNF digest used
  for its check; and
- stage results, timing, peak memory, retained-artifact policy and cleanup.

The original CNF is streamed and hashed once. Each algebraic batch must prove
all clauses in its declared lemma-ID range. The composer constructs the DSR
checker input by appending the lemma table, in ID order, to the original CNF.
It rejects missing, duplicate, overlapping, reordered or byte-different lemma
bindings. The DSR proof may consume a lemma only after the batch that owns it
has verified. An empty lemma table reduces to the ordinary native DSR route.

SAT results do not carry this bundle. They are accepted only after an
independent model check against the original CNF.

## Backends

GF(2) batches use compact FRAT-XOR production, XLRUP elaboration and pinned
CakeXLRUP checking. One selector challenge binds all exported clauses in a
batch, avoiding one checker process and one repeated source scan per reason.

Unweighted-cardinality batches should use a verified PB/LPR route when the
recognized CNF-to-cardinality premises and the exported clausal conclusions
can be bound without adding an unverified translation. Until that route is
implemented, cardinality reasons remain ordinary clausal RUP/DSR derivations.

GF(3) has no pinned native verified checker in this campaign. Its current
production route is therefore a clausal RUP/DSR derivation. A future native
GF(3) section may be added only with an independently checked premise and
conclusion binding equivalent to the GF(2) contract.

The final DSR section remains canonical because Tactix search can emit RAT and
substitution-redundancy steps that FRAT-XOR/XLRUP cannot represent. DRAT and
LRAT remain compatibility projections for the event subsets they support.

## Efficient checking

The checker hashes and indexes the original clauses during one streaming pass.
It checks independent algebraic batches concurrently when resources allow,
using bounded workspace scratch. A backend batch verifies many lemmas in one
certificate. After all batches succeed, the checker writes the augmented CNF
once and checks the DSR consumer. Checker intermediates are deleted as soon as
their consuming stage finishes; only the manifest, certificate digests and
stage reports remain under the zero-retention evaluation policy.

Failure is fail-closed. A checker timeout, changed pin, source mismatch,
uncovered lemma, conflicting lemma owner, DSR rejection, missing cleanup, or
resource stop makes the solver result unverified and ineligible for CG or any
performance gate.

## Soundness boundary

For an original CNF `F` and exported clauses `L`, the composition theorem is
small: if every batch checker establishes `F entails L_i`, and the consumer
checker establishes that `F and L` is unsatisfiable, then `F` is
unsatisfiable. The selector challenge is equivalent to checking every clause
in one batch: it asks whether the authenticated source clauses can coexist
with the falsification of any selected lemma. Since each source is a clause
multiset subset of `F`, an UNSAT challenge establishes the needed entailments.

That mathematical argument does not by itself prove the implementation. The
current trusted computing base includes the manifest/CNF parsers, selector and
augmented-CNF construction in `check_algebraic_proof_bundle.py`,
`proof_oracle.py`, the DSR elaborator, the final DSR checker, and the usual
runtime, operating system and hardware. The pinned CakeXLRUP backend is
formally verified and checks the elaborator's XLRUP output, so `frat_xor` need
not be trusted for logical soundness. The exact pinned Trestle `srcheck` is an
independent Lean implementation, but its executable is not currently tied to
an audited end-to-end theorem from accepted bundle bytes to original-CNF
unsatisfiability: the executable entry point directly parses and runs the
checker, while the nearby soundness theorem for the older executable is
commented out, and the active parser/bounds modules still contain admitted
proofs. It must not be described as a formally verified final bundle checker.

Production-strength assurance therefore has two layers. Immediately, keep
strict hashes and parsing, require independent backends, run corruption and
mutation controls, fuzz both parsers and binding logic, and differentially
check generated small bundles by exhaustive truth tables. For the final small
trusted base, implement the manifest binding and composition kernel in a proof
assistant or a verified language and prove that an accepting result implies
`UNSAT(F)`. A verified checker must consume the exact manifest and reconstruct
the challenges and augmented formula itself; proving only the abstract
composition theorem leaves the Python binding code trusted. Until that final
kernel exists, results are independently machine-checked but are not claimed
to have a formally verified end-to-end bundle checker.

## Current evidence and remaining work

`tools/bench/check_algebraic_proof_bundle.py` implements the public multi-batch
contract and `tools/bench/check_xor_dsr_composition.py` retains the earlier
single-batch fixture. The bundle checker streams the original CNF, checks each
source multiset, creates selector challenges, requires the selected backend,
constructs the exact augmented CNF, and requires an every-lemma DSR check.
The real pinned backends accept the positive mixed fixture. Eight controls in
`tools/bench/test_algebraic_proof_bundle.py` pass: the positive case plus
rejection of a changed false lemma and changed DSR certificate even after their
digests are recomputed, a source outside the original, missing lemma ownership,
path traversal, a symbolic link and a wrong digest. A captured 33-reason `mdp`
batch also passes the earlier two-stage pattern.

`tools/bench/test_algebraic_bundle_semantics.py` calls the production selector
constructor and exhaustively enumerates every assignment for 10,000 seeded
random formulas of at most five original variables. Five thousand cases check
that its challenge is UNSAT exactly when every selected lemma follows from the
declared source. Another five thousand check the composition implication with
a source chosen as a clause subset of the original CNF. This is differential
fuzz evidence for the concrete transformation; it does not replace the planned
verified exact-byte kernel.

This evidence does not yet make the bundle a production solver certificate.
Completion requires direct bounded emission from the live GF(2) service, the
full real search DSR consumer, equivalent cardinality and GF(3) coverage or
their clausal fallback, parser/binding fuzzing, an end-to-end verified bundle
kernel, and matched total solve-plus-check performance under N1--N4.
