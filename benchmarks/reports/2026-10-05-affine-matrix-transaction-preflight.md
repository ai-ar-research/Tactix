# Authenticated affine-matrix transaction preflight

Base commit: `556c15ee6540fe4a650195f280d784e85fc36d22`

## Implemented boundary

`AffineMatrixTransaction` is a pure bridge from one synchronized affine-matrix
implication or conflict to a complete parity-certificate plan. It does not
write a proof, mutate a kernel, install a clause, enqueue a literal, create a
conflict, or schedule another action.

Preparation authenticates the immutable owner, the complete copied source
image, the current conflict-first/implication-second event, and the exact
source ordinals. Every selected source must carry distinct nonzero occurrence
IDs and a premise-clause multiset equal to its complete parity CNF. The
transaction retains the exact selected source identities so a later live
publisher can verify that those occurrences are still live.

The same bounded work contract meters owner/source comparison, premise
occurrence and clause validation, canonicalization, retained-identity copying,
and certificate construction. Preparation precomputes clause IDs, proof
additions and deletions, monotonically consumed helper extent, and kernel arena
words. Balanced-XOR helpers are retired; the arbitrary-width RUP reason is the
only surviving proof clause and the only event marked for kernel installation.
Every non-`Prepared` result leaves the caller's previous transaction unchanged.

Focused tests cover conflict and implication events, exact identity rejection,
missing/tampered/duplicate premise rejection, proof/addition/deletion/helper/
arena limits, interruption at early/middle/late work cutoffs, and a resource
cutoff inside selected-source CNF accounting.

## Verification

Both jobs used `cpu-q`, the repository Debug preset (`-fsanitize=address,undefined`),
and base commit plus the staged patch. Checker paths were bound to the pinned
repository tools under `tools/checkers/bin`.

- Job `1750727`: `COMPLETED`, exit `0:0`, node006, 6:08. This pre-metering
  baseline passed `affine_matrix_transaction_proof` (0.93 s), `affine_matrix`
  (0.76 s), and `affine_matrix_transaction` (0.21 s).
- Job `1750746`: `COMPLETED`, exit `0:0`, node010, 6:00. This is the
  authoritative metered-source patch. It passed
  `affine_matrix_transaction_proof` (0.35 s), `affine_matrix` (0.75 s), and
  `affine_matrix_transaction` (0.20 s); 3/3 tests passed in 1.30 s.

The proof test emits the complete transaction DSR for an eight-literal matrix
conflict, then runs `proof_oracle.py check-dsr --every-lemma`: the forward DSR
check is supplemental, while `dsr-trim -> dsr-compress -> srcheck` supplies the
canonical Trestle acceptance route.

## Deliberate next boundary

No Rank263 run was made. The next change is a thin live publisher that
revalidates owner identity and every retained source occurrence, reserves the
preflighted proof/namespace/database resources, emits the planned proof, and
installs only the marked reason before ordinary propagation consumes it. That
publisher needs focused atomic publication/rollback and proof tests before a
single compact-telemetry Rank263 screen.
