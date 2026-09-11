# Complete allocator transfer: retain the tradeoff, diagnose the regressions

Frozen solver-phase evidence, September 11, 2026. The preceding checkpoint is
[7284](2026-09-11-realloc-tradeoff-7284.md). No submitted source or runtime pin
has been changed, and the allocator candidate is not promoted to production.

## Full Rank 1–50 outcome

[7283 summary](2026-09-11-realloc-full50-7283/summary.json) and the
[complete per-rank table](2026-09-11-realloc-full50-7283/comparison.tsv) contain
all fifty outcomes: 25 SAT and 25 UNSAT, no solve loss or timeout, and exact
actual7253 work/certificate fingerprints including every physical dense counter.
Every rank remains scored, including shelved21 and closures4/32/33/49/46.
Independent checks were36/50 complete at the seal. Pending checks are not
accepted correctness or clean residue; the unchanged runners continue checking
and removing their own artifacts asynchronously.

Candidate7267 totals8096.416s / fixed native7239.010s =1.118442439x. The runtime
target remains unmet by785.0159s. The full predecessor7253 view was8057.066s,
so this cross-window transfer is39.350s higher, not an overall speedup. It also
exceeds retained7144/fresh7167 (8072.059s) by24.357s. These cross-window differences
are not causal attributions; the three paired7281 waves remain the decision-grade
runtime/RSS evidence for the five targeted witnesses.

The largest positive full-view predecessor time differences are38 (+19.445s),
29 (+10.465s),30 (+9.651s),26 (+4.325s), and31 (+3.183s). The repeated Rank38
median penalty of5.627% and its matched propagation profile corroborate that
specific regression. Ranks29/30/26 have not received paired causal attribution;
do not assign their complete cross-window differences to the allocator.

## Memory is a mixed independent result

Fourteen ranks reduce RSS, twenty are unchanged, and sixteen increase. Maximum
RSS is15.271507263GiB, a measured compatibility projection, not a hard16GiB gate.
Rank9/25 save268.1758/363.7891MiB versus7253, exactly reproducing all three paired
RSS measurements. They still exceed native by1083.98/1558.67MiB, respectively;
global memory parity is not achieved.

Every observed RSS increase over7253 is retained here (MiB):

| Rank | Increase | Rank | Increase |
|---:|---:|---:|---:|
|5|1.0000|10|0.1797|
|16|0.4688|17|2.0117|
|21|4.0430|24|16.0000|
|27|59.5820|31|66.1758|
|32|9.8203|33|3.7656|
|34|6.0000|36|0.0039|
|40|16.7344|45|8.0000|
|47|0.7813|49|0.5391|

Ranks31 and27 are the largest adverse memory witnesses. Rank31 repeats exactly
at562920KiB versus495156KiB. The broader increases make a general allocator-
history explanation worth testing, but no particular allocator mechanism is yet
proven. Rank21 remains scored and shelved, not selected for diagnosis.

## Decision and next discriminating experiment

Keep the immutable candidate and its useful savings as investigation material;
do not silently discard it, claim an aggregate runtime gain, or promote a mixed
tradeoff without the remaining gates. Productionc2a95a6 and rollback7144 remain
unchanged. The full50 performance measurement requested before integration is
now available; independent validation continues separately.

The heavy7277 event-trace pair reversed the Release Rank31 RSS relationship,
despite exact structural trajectories. It cannot identify the Release peak.
The next isolated diagnostic separates existing slot/collector/BVE-capacity
memory checkpoints from general event tracing. Both observer-disabled controls
must be byte-identical to original Release; observer builds retain full fixed
tests and original whole-work/proof gates. Their own absolute peaks and residual
perturbation must be reported before attributing the Release penalty. This adds
no bookkeeping to normal runtime evaluation pins.

The earlier Rank38 evidence still localizes additional sampled periods to
ordinary propagation self, with changed generated code despite identical
kernel/watch sources. Neither smaller machine-code size nor identical logical
work proves cheaper execution. No alignment tuning, cache/predictor claim, or
speculative hot-loop rewrite is justified by the current evidence alone.

Test consideration: the full50 reader reuses strict identity, outcome, cleanup,
raw-counter and certificate controls. The memory-only diagnostic adds explicit
enabled/disabled/bad-path, repeated-capacity suppression, temporary-owner and
semantic-state fixtures, plus existing collector boundary tests. Failed observer
compile attempts retain their sources/logs; no test gate is weakened.

[7291 build](../runs/20260911-7291-memory-only-build/manifest.json) now passes all
four complete suites: candidate control/observer122 each, parent control/observer
120 each. Both OFF binaries are byte-identical to original7267/7253 Release;
observer builds link no general EventTrace implementation. Initial7285/7287/7289
compiler failures exposed the scheduler, candidate-order, and kernel-owner
storage accessor guards; each failed source/patch/log remains preserved. The
corrected build runs both syntax preflights before full compilation. No process
was cancelled, no failed observer was submitted, and no solver failure occurred.

[7286 launch](../runs/20260911-7286-memory-only-rank31/launch.json) is job1681163,
submitted06:12:17EDT, with the unchanged12CPU/48000MiB/1200solver-only/16000checker/
5h fullwidth two-task contract. The existing paired memory wrapper and strict
whole-work/trace reader are unchanged in behavior. The pair is pending evidence,
not proof that the observer now reproduces the Release RSS regression.

Original runtime/memory targets, remaining checks and zero residue,
current-source Claude-owned fuzz, and consolidation remain open. Codex continues
direct debugging; no denied fuzz access/transfer or push is retried.
