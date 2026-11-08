# Shared nonlinear definitions: research experiment

This experiment pursues the real-instance goal in
`Tactix-dev/docs/eth-practical-sat-plan.md`. It does not change PR24 or promote a
default policy. Run commands only from `/home/hwu/Tactix`.

`shared_anf.hpp` authenticates complete signed AND and small XOR encodings from
CNF, unfolds bounded gate cuts, and normalizes their exact Boolean-ring
polynomials. It looks for nonlinear monomials or nonlinear polynomial parts
shared by at least two distinct outputs. A function already represented by an
existing variable, including its complement, is excluded from novelty. Cut
boundaries are expression leaves; crossing occurrences are retained, and no
original clause is deleted. The arithmetic uses Boolean idempotence, XOR
cancellation and signed-literal complement. Every admitted cut and every reused
monomial representative is separately checked by exhaustive AST evaluation.

The research adapter introduces exact AND/XOR definitions through the existing
ER rule, constructs alternative output expressions sharing the new summaries,
and adds semantically authenticated equivalences to the original outputs.
These equivalences currently have no emitted clausal derivation. The installer
therefore forbids named proof output in research arms, and the runner labels
UNSAT outcomes independently unchecked. This follows the user's explicit
no-proof performance phase; it is not a production proof-rule implementation.
Original clauses are retained; every SAT model is checked against the original
CNF. Fixed tests exhaust all three-input Boolean polynomials, all signed binary
gate encodings, and all input assignments and corrupted outputs of the
shared-term fixture in each arm. This is not fuzz or formal verification.

The hook runs once after ordinary startup preprocessing, initial elimination
and late Lucky, before search. It leaves all later default inprocessing enabled.
Thus summaries reach the first search period before further elimination; no
permanent freeze guarantee is claimed. Fresh variables are demoted neutrally
in the definitions-only arm. The priority arm then applies an additive dose
proportional to each summary's consumer count. The existing-variable control
performs identical discovery, introduces no variables or links, and gives the
same planned doses to distinct existing consumer outputs. Actual counts/doses,
first-decision survival, summary decisions and helper decisions are recorded.

The frozen experiment includes all twelve training hardware miters and all
sixteen training LEC multipliers from the existing corpus manifests. ASCON,
SIMON and validation are untouched. Four arms use the same 300-second limit:
unchanged Tactix, definitions, definitions plus priority, and existing-variable
priority control. Each instance has three repetitions; arms share a CPU and
rotate order. The unchanged executable is built from an unmodified source
archive; the other arms use a separate copy with the research installer.
Baseline and probe-disabled fixed-fixture results must agree before readiness.
All parsing, ordinary startup, discovery, local truth validation, definition
construction, publication and search costs are inside process wall time.
Common decompression and model validation are recorded separately. Retain all
instances, timeouts, crashes, missing tasks and invalid models in the evidence.

Acceptance remains at least a 25% per-family mean PAR-2 reduction in the priority
arm, without a paired baseline solve lost, on both families. Controls determine
whether ordinary priority explains any gain. No performance benefit has yet
been established. A summary cannot mark the full goal complete automatically;
independent qualification and the causal interpretation still need review.

After committing the exact source, prepare and submit an immutable campaign:

```sh
python3 tools/nonlinear/freeze.py shared-nonlinear-v1-20261108
sbatch --parsable benchmarks/runs/shared-nonlinear-v1-20261108/build.sbatch
# Submit measure.sbatch with afterok:<build job>, then record both job IDs.
python3 tools/nonlinear/summarize.py benchmarks/runs/shared-nonlinear-v1-20261108
```

The installer modifies only a fresh source pin below this repository's build
folder. Every submitted source, binary, runner, manifest and result is retained.
Never modify pins while their jobs are live. The small-cut approach is a
representation experiment, not an implementation of the ETH counting algorithm
and not a claim that ANF reasoning is new.
