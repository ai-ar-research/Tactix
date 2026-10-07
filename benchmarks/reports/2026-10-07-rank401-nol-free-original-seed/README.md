# Rank401 ordinary search after noL retirement

Slurm1754297 independently verifies SAT in **43.021 seconds** (1,434,581 conflicts), checking all7,831 original clauses and assigning all1,419 variables. The candidate is noL-free1e8295106 with exactly one diagnostic header change: the residual startup seed handoff defaults to false. Effective arguments are only `--auto-proof`; generic graph search remains off. Binary/source and checker identities are retained in the campaign and checked-target.json.

Full structured symmetry still applies1,070 steps and takes0.378 seconds. This reproduces the earlier matched44.528-second recovery through ordinary search after the noL implementation was deleted. No target matcher or special repair search is involved. The earlier [causal report](../2026-10-07-rank401-residual-seed-causal/README.md) owns the matched reseed-on300-second timeout control.

This is a diagnostic recovery, not final candidate acceptance. Removing residual reseeding slows regression ranks16 and23; production retains its current default. FullRank1–50 policy gate1754296 compares the general original-seed policy before any promotion. Other policy gates are prepared but not submitted. Current unchanged noL-free qualification proceeds separately.
