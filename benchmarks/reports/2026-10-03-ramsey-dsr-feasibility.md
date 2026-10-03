# Native DSR feasibility for Ramsey counting

2026-10-03. Checkpoint: degree and R(3,4) proofs independently verified;
full original rank-630 certificate check pending. No production rule or target
acceptance is claimed. Source remains unchanged at the graph/guidance WIP snapshot.

## Result and decision

Tactix's existing BDD extension/implication engine can certify the conditional
R(3,3), degree-bound and parity-counting derivation for R(3,4) using native DSR.
No pseudo-Boolean proof commands are required by this demonstrated subproblem.
Continue the full DSR feasibility route before considering a PB extension for
this mechanism. This does not establish that all future PB reasoning is cheaply
representable in DSR, nor that DSR is smaller than the reference PBP certificate.

The proof-generating drivers are isolated harnesses using the current
`signed_cardinality_proof.cpp` BDD engine and native `Proof` API. They do not
invoke LymphoSAT or another solver. The owning reference algorithm is
`winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/ramsey-numbers/solver/solver`
and its `tools/cp_ramsey_proof.py`. Their mathematical mechanisms informed the
native derivation; reference source was not copied into production.

## Verified sequence

| Input | Original vars / clauses | BDD nodes | Proof additions | DSR bytes | Derivation seconds | Canonical check seconds |
|---|---:|---:|---:|---:|---:|---:|
| 5 vertices, every degree 1 | 10 / 35 | 246 | 1,443 | 18,558 | 0.000640 | 0.0821 |
| 9 vertices, every degree 3 | 36 / 882 | 9,192 | 64,231 | 1,101,817 | 0.019744 | 9.2255 |
| R(3,4) on 9 vertices | 36 / 210 | 39,062 | 216,798 | 3,872,140 | 0.081481 | 103.8755 |

The generation timer covers the derivation, excluding input generation, parsing
and setup. These are prototype measurements, not solver benchmark speedups.
Every input clause set is independently regenerated/compared by the runner.
All three certificates pass validated witness-free DSR-to-DRAT conversion,
drat-trim elaboration and independent CakeLPR checking. A separate drat-trim
forward audit validates every lemma. Each task also rejects a false empty-clause
proof of a satisfiable unit CNF through the same canonical route.

Degree cases ran in array 1747611 (both tasks COMPLETED/0:0); R(3,4) ran in
1747615 (COMPLETED/0:0). R(3,4)'s total generator/check/audit/negative-control
wall time was 216.369 seconds. Jobs used cpu-q, one CPU, 32 GiB and 30 minutes,
without throttles or node constraints. Generation was bounded at 300 seconds;
canonical and forward checks each had a 600-second limit. Explicit prototype
caps were one million BDD nodes and ten million additions, not proposed defaults.

## What the DSR derivation proves

For R(3,4), input clauses forbid red triangles and blue four-cliques.
At each vertex, four red neighbors are excluded by a RUP lemma: their internal
edges are forced blue, contradicting a blue four-clique. Six blue neighbors
would induce an R(3,3) instance. Under that six-edge context, twenty RUP clauses
exclude three red and three blue neighbors of one selected vertex. Their BDD
conjunction implies the context clause, excluding those six blue neighbors.
These deductions derive degree at most three and at least three at every vertex.

Each degree constraint is represented by fresh-variable BDD definitions with
RAT justification. Certified implication combines the constraints into a sum.
On nine vertices, the sum of degrees is 27; every edge occurs twice, so the
lower and upper bounds imply at least 14 and at most 13 edges. The BDD engine
certifies the integer rounding through Boolean semantics, without emitting CP
sum/divide commands. Its final implication yields the empty clause. The same
mechanism proves the smaller odd-degree control.

All recognition, original-input admission, interruption/preflight, caller effort,
public actions and policy scheduling for a production rule still need integration.
Using a private helper in this probe is not N1 acceptance.

## Full target prototype in flight

Job 1747619 uses the exact saved rank-630 input,
`847c8843ee292b9b31f0c346574af260-ramsey_4_4_19.normalised.cnf.xz`.
The harness independently checks the complete 171-variable/7,752-clause signed
K4 encoding and preserves its original bytes for verification. The driver adds
16 witnessed prefix-order symmetry clauses on a 17-edge star, then a witnessed
color-complement/reversal reduction. It derives nine incident red edges, reduces
to the checked R(3,4) subproblem, and emits the BDD counting proof.
Witnesses map the complete original variable set, including the extra vertex;
there is no stored satisfying assignment or benchmark-name dispatch.

Generation returned zero, reporting 39,062 BDD nodes and 216,891 counted
non-symmetry proof additions in 0.070899 derivation seconds. The check is still
pending at this checkpoint. It uses pinned no-option dsr-trim, LSR conversion
when needed and Trestle, followed by the every-lemma witnessed audit and a false
proof control. Limits are 300 seconds generation, 1,800 seconds per canonical/
every-lemma pipeline, and a 40-minute allocation wall limit. This is an isolated
feasibility run, not the formal target or regression acceptance protocol.

The dsr-trim and compressor binaries match the retained clean-build authority;
the maintained patch hash matches. Trestle is at the pinned commit with exactly
the maintained parser patch, and its binary hash is retained. No checker was
rebuilt and no private fuzz campaign was inspected. This job reruns no historical
solver arm or comparison. Pending verification must not be reported as checked
UNSAT or as a second integrated target.

## Concrete production integration review

If full verification passes, extract the reusable BDD proof machinery into a
shared internal module and add the complete native derivation through a public,
typed terminal-certificate action. Keep formula recognition and proof validity
in the rule; policy owns selection and effort. Recognize the full clause family
and variable mapping, with caller-bounded resources, not filename/rank dispatch.

Initially use the existing explicit original-input certificate boundary: the
witnesses have been constructed for that formula. A root restart alone cannot
justify applying them after learned clauses, prior symmetry or other transformations.
General live symmetry remains the separate planned validation/review workstream.
The new action must decline unsupported proof modes before mutation. Prefer DSR;
no mixed-stream PB or fallback is introduced.

Preflight the entire derivation under interruption/workspace/node/addition limits
before any proof publication, then perform the admitted atomic proof transaction
with actual work/overshoot feedback. Handle proof-only auxiliary identities,
including the temporary witnessed-unit variable, without collisions. Expose
eligibility, parameters, serialization/replay and raw measurements to DefaultPolicy
and the random controller. Demonstrate effective execution and negative controls
through both policies. A standalone success cannot replace these requirements.

Keep graph-profile scheduling and current-pin acceptance open. The graph owner
has now bound immutable Debug pin `1f2a3b98` and launched 100,010-seed default and
random legs (1747613/1747614), plus focused smoke 1747609. Later source edits need
their own acceptance. Diagnostic full Debug still has 216/217 passing with its
scripted example live. All N1–N4, current-pin fuzz/replay/Debug and four preservation
gates remain required; reward/training and the full 44-target scope are unchanged.

## Evidence and hygiene

- `benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/` and
  `benchmarks/runs/20261003-ramsey-r34-dsr-feasibility/`: checked original inputs,
  native proofs, results, forward logs, negative controls, manifests and accounting.
- `benchmarks/runs/20261003-ramsey-r44-dsr-feasibility/`: active full-input job;
  consult its eventual result rather than inferring completion from this report.
- `benchmarks/runs/20261003-ramsey-dsr-feasibility-checkpoint/`: retained three
  source drivers/builders/runners, immutable-build manifests and submission record.
- Immutable pins: `build/pin-ramsey-{degree,r34,r44}-dsr-bcf71ced-01/`.
- After terminal independent checks, 29,506,596 bytes of regenerated checker
  intermediates were removed from the degree/R(3,4) cases. Exact names, hashes and
  reasons are in each cleanup manifest. Native proofs, original CNFs, checker
  pins and all unique evidence remain. Active full-target artifacts are untouched.
- Andrew handoff: `report-to-andrew/20261003T201500Z-ramsey-dsr-feasibility/`.
