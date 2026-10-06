# Rank 291 binary-LSR causal gate

## Scope and selection

This checker-only gate compared the three requested fixed-36 certificate gaps:
ranks 82, 226, and 291. The saved Tier-D records contain only certificate
hashes for ranks 82 and 226. Rank 291 alone has a retained certificate: the
424,530,578-byte DSR stream with SHA-256
`09bcea351f43e00b9b9117f77cb0ccc759ab3397c290c33cf44bef1359c94a47`.
It is byte-identical between the historical `d170b4a5` result and the current
`9b3508b5` regeneration. No solver or historical baseline was rerun here.

The exact retained stream contains 21,900,719 lines: 3,088,426 additions and
18,812,293 deletions. Only 20 additions carry an SR witness. Those lines hold
1,510,652 encoded tokens and occupy 4,371,684 bytes, 1.03% of the certificate,
but their presence sends the complete certificate through the witnessed-DSR
route. The full census is in [census.json](census.json).

## Causal probe

SLURM job `1750934` ran on `cpu-q` with four CPUs, 32 GiB, and one shared
1,800-second checker allowance. It bound the exact compressed input, canonical
checker CNF, retained proof, no-option `dsr-trim`, and patched Trestle
`srcheck` by SHA-256. `dsr-trim` independently verified the DSR in 723.776
seconds and emitted a 656,051,430-byte binary LSR with SHA-256
`ac1c89a832bbc07fbfeb427dc3a06b8b6137bffc4957b0db2ef7ee422bda27f6`.
Trestle then consumed that binary LSR directly through its pinned `c` mode.
It exhausted the remaining 1,075.928 seconds after reporting that it had parsed
the 56,064-variable, 15,834,432-clause formula. The complete pipeline took
1,800.158 seconds and did not verify within the cap. [result.json](result.json)
is the authoritative outcome; [manifest.json](manifest.json), [probe.py](probe.py),
and [probe.sbatch](probe.sbatch) preserve the immutable invocation.

The historical text route expanded the same binary LSR to 1,458,602,075 bytes,
2.22 times the binary LSR and 3.44 times the original DSR. Direct binary input
removes that expansion, but this gate shows that expansion is not the decisive
Rank 291 acceptance bottleneck. The remaining failure is localized to the
independent Trestle stage after formula parsing. This does not show that the
certificate is invalid: `dsr-trim` verified it in both saved runs. It also does
not show that any of the 20 witnessed additions can be omitted or replaced by
RUP/RAT.

No production checker route, solver source, rule, registry, controller, or
documentation contract changed. Rank 291 remains unaccepted under the
1,800-second checker cap.

## Next certificate-only target

Rank 226 is the next target by expected checker bottleneck. Its saved
20,324,467,728-byte witness-free DSR spent 11,779.583 of 14,400.587 checker
seconds outside the individually recorded stages; `drat-trim` received only
2,621.004 seconds before the deadline. The current DSR-to-DRAT path validates
the stream and rewrites its binary tags in Python, so a generic single-pass,
fail-closed conversion is a concrete bounded engineering hypothesis. An exact
certificate is no longer retained, so no acceptance claim or target check is
possible until a current proof is regenerated and retained under an immutable
manifest.

Rank 82 is lower priority for this lane. Its 2,058,524,846-byte proof reached
`drat-trim`, which itself consumed and exhausted 13,148.005 seconds. Route
conversion is therefore not its observed dominant stage, and recovering it
requires proof-core or checker-work reduction rather than the Rank 226
conversion improvement.

## Highest-reuse theory gaps already supported by evidence

These are the next reusable families in the existing fixed/WV/WV2 action map;
this report adds no new benchmark evidence or recovery claim:

1. **GF(3) affine closure:** twelve WV100 sole-LymphoSAT rows share the exact
   `gf3-sat` owner family. WV rank 9 / Tier-D rank 187 is a representative.
   The public `Gf3AffineConsequence` proof boundary already exists, so the
   common missing increment is deterministic repeated pair enumeration,
   duplicate suppression, useful consequence publication, and semantic-variable
   advice rather than another family-specific solver.
2. **GF(2) affine/parity closure:** existing evidence covers both WV two-tree
   rows, four MDP rows, at least nine WV/WV2 Tseitin/parity rows, and strong
   fixed-target signals at ranks 263, 569, 762, and 1004. Rank 263 is the
   strongest structural representative; ranks 569 and 1004 expose the compact
   proof side. The reusable increment is a shared basis-then-small-consequence
   schedule, with count/weight reasoning kept as a separately certified layer.
3. **Finite-domain and n-ary CSP consequences:** fixed rank 401 and WV/WV2
   `noL` rows share ternary hypergraph structure, while WV ranks 93/98 provide
   exact RB-style signed one-hot domains and forbidden pairs. Rank 401 is the
   n-ary representative and WV rank 93 / Tier-D rank 792 is the binary-domain
   representative. Existing binary `DirectCspConsequence` and n-ary proof
   primitives make native offer generation, repeated scheduling, and useful
   semantic publication the common remaining integration boundary.

The first two have the broadest repeated-family evidence. The third has a
clear Boolean action boundary but spans binary and n-ary relations, so recovery
must still be demonstrated separately on each relation class.

Source-record SHA-256 bindings:

- current Rank 291 regeneration result: `4e6acec6fbf1e521cd803f2357609d29dcfc55bdfbd18b928be536a1dc8215b0`
- historical Rank 291 result: `d51ff95b64a2a567354d8b33c48398c23b39a386f58a73a9158142359c9ab585`
- historical Rank 226 result: `7c46e4532bb364c0b997ce763db7ca8f6bf1ca8254f67bc0b074fa95ac6e33f2`
- historical Rank 82 result: `bc27a7c5ce5fb97fa7d9ed0a4eec5c5a4ff4e22ba95ed3639931560c1a521bbf`
