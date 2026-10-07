# Rank899 proof-output control — 2026-10-07

Suppressing proof output does not recover the historical ~173-second trajectory. The current solver times out at 300 seconds both with and without an output destination; the proofless arm completes only about 4.5% more conflicts per second. This rules out proof writing alone as the explanation of the old/current solve gap. Equal-time counters do not establish full trajectory equality.

Slurm 1755062 ran both arms sequentially on the same allocated CPU with 32 GiB memory,300 seconds per arm, and a 4 GiB cap on each subprocess file. Both use the exact production 3ec solver SHA256 `10942fbedbb8a66712ffb439c93601d6a74639f4f2548e43c05a0f477598299e`, original Rank899 CNF, and `--auto-proof --default-graph-labeling=on`. Only the presence of the proof-output positional argument differs, preserving the automatic startup-certificate mode. Input, executable, wrappers, checker and result identities are bound in `evidence.json` and campaign `benchmarks/runs/20261007-rank899-current-proofmode/`.

| Output destination | Verdict | Wall seconds | Conflicts | Decisions | Proof bytes |
|---|---|---:|---:|---:|---:|
| None | UNKNOWN |300.020 |4,143,125 |74,569,745 |0 |
| Automatic proof file | UNKNOWN |300.118 |3,964,517 |69,487,755 |3,025,283,372 |

Neither arm is storage-censored. Both report the same initial FastEliminate result: 28,741 eliminated variables and 822,766 resolutions; the same 52,731-variable activation seed; zero symmetry applications and zero residual handoffs; and 7,658,486 structured-discovery ticks with no effective transformation. Structured startup costs 0.584/0.586 seconds. Later totals come from different equal-time prefixes, so they cannot isolate proof-conditioned branching or every low-level difference.

Historical `20260921-wv2-large-domain-admission{,2}` G/LG/LG2 scripts invoke the old solver without a proof destination or independent model check. Their 173–174-second SAT output and 2,150,089 conflicts remain a useful lead, not verified coverage or a matched baseline. This current-source control removes the simplest output-cost explanation; source/trajectory attribution would require a further bounded matched experiment.

No SAT/UNSAT candidate completed, so there is no certificate to accept and no instance closure. The runner would independently verify any complete SAT model; a proofless solve would still not count as coherent automatic-proof coverage. Production policy and checker acceptance were unchanged.
