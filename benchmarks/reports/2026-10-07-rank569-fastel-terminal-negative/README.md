# Rank569 FastEliminate candidate: terminal negative result

Slurm **1754535** is terminal `FAILED` because its independently selected
checker rejected the emitted DSR proof.  The solver itself returned UNSAT with
exit 20 in **3,627.980 seconds** and emitted a **4,115,525,606-byte**
certificate (`9dd8775c0b685071aefd276a52d51dc8369de2e55c6fb4fe147722b5f35ed6c0`).
This is slower than the saved Kissat-SC2024 authority at 3,359.975 seconds and
the earlier Tactix solve at 3,034.193 seconds, and the certificate is larger
than their saved 3.571 GB and 3.166 GB certificates.  The candidate therefore
misses the runtime and compactness targets before proof acceptance is
considered.

The audited LONGTYPE `dsr-trim` pin
`94f5fc0c122b7996207fb38f9010180f2d1e6b61b3008a3ef515ab5d971bc614`
parsed the empty clause after 90,194,257 proof lines and 45,073,931 additions.
After 223.949 seconds it failed closed at addition line 45,073,817, fourteen
additions before the final empty-clause ID:

```text
Fatal error: [line 45073817] Clause 45084048 not in watch pointers for literal 18
```

Peak checker RSS was 17,260,268 KiB inside the 32 GiB cgroup.  It did not time
out or hit the former signed-32-bit capacity failure.  The supported 64-bit
build's 33 directed controls establish correct width and fail-closed behavior;
they do not prove that every multi-billion-byte backward-checking watch-state
transition is correct.  Conversely, focused FastEliminate proof tests do not
validate this deleted 4.116 GB artifact.  The terminal evidence therefore does
not distinguish an invalid solver deletion sequence from a backward-checker
watch restoration/removal defect.

The failure occurs in `remove_wp_for_lit`, reached by backward uncommit or
user-deletion processing when a clause expected to own a watch on literal 18
does not.  A future rerun is justified only with a retention contract: keep the
original DSR on checker failure, run the same immutable LONGTYPE elaborator in
default backward mode and `--forward` mode under the same 32 GiB/14,400-second
bounds, and retain the exact failing prefix plus deletion neighborhood.  If
forward accepts and backward repeats the missing-watch failure, investigate the
checker lifecycle.  If both reject, reduce the solver proof sequence and route
the bounded FastEliminate/deletion reproducer to the cumulative fuzz owner.
No second 5,000-second solver run is authorized merely to repeat this result.

The mechanism counters also reject the original XOR hypothesis: parity found
24 equations of rank 24 but zero candidates; persistent affine search was
disabled and consumed zero implications/conflicts; affine initialization
declined an incomplete image.  FastEliminate performed two checks, eliminated
2,246 variables and generated 30,012 resolutions.  Those transformations did
not improve Rank569's solve time.

The authoritative result is
`benchmarks/runs/20261007-rank569-fastel-self-strengthen-candidate-r2/results/000.json`.
The proof and derived checker artifacts were deleted by the frozen campaign's
cleanup contract, so Rank569 remains open and no proof-prefix replay is
possible from this run.
