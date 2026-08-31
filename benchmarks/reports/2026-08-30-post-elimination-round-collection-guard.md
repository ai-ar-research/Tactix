# Post-elimination-round collection cumulative guard

Date: 2026-08-30
Disposition: PASS; promotion awaits independently owned consolidation fuzz

## Scope

Campaign `20260829-1800-BC-round-collect-guard` compares the shipped binary
against the isolated exact Kissat lifecycle repair on the fixed 33-instance
B28+C5 set. The candidate collects after every completed, consistent authored
elimination round, including zero-yield rounds, and suppresses only the now
redundant generic phase-exit collection. It changes no rule, option, effort,
admission, formula classification, benchmark identity, rank predicate, or
consumer wiring.

The Slurm array contains 66 unthrottled tasks: 33 immutable instances times two
arms, one solver invocation per task, 12 requested CPUs, 48,000 MiB, no node
constraint, and independent model/proof checking. UNSAT solver timing was
published provisionally while the two long `T105.2.0` proofs remained alive;
this final report uses only terminal checker results.

## Terminal result

All 66 tasks are valid and match their expected verdicts. Each arm has 17 SAT
and 16 UNSAT results; all 34 models and all 32 proofs verify independently.
There are no solve losses, invalid artifacts, checker resource exits, cleanup
failures, or certificate residues.

Shipped/candidate PAR-2 and total solve time are 2,439.313/2,445.782 seconds,
a candidate ratio of 1.002652. This is a 0.2652% aggregate movement and passes
the declared 1.10 ceiling comfortably. Maximum sampled peak RSS is
3.950/3.951 GiB. Five pairs differ only in eager-subsumption attempts: exact
round collection resets the four-entry recent-clause register and avoids 7--76
stale attempts; formula/search outcomes remain identical.

The cumulative gate therefore passes and the general lifecycle repair should
be retained. It must not become conditional on formula shape or rank. The
remaining promotion gate is the required Claude-owned consolidation fuzz.
The environment rejected that external private-workspace access until the user
explicitly authorizes it; Codex must not substitute its own fuzz ownership.

## Frozen evidence

- Manifest SHA-256:
  `076e7c19f8809627bbc24edf33837ae75f668046b85f0151daa1b6ace1cf0eca`.
- Summary SHA-256:
  `5acabd9435adcbd02575fb3fc7f7bdb8aa8bde0dbb40a8123c9d47418fb23af0`.
- Ordered result-set digest:
  `e8159fb1e86f624d52a3fadee247cd14182fd9822b2452dd765a8462bba9851e`.

`completion.json` records terminal counts and hashes; `classification.json`
records the gate decision. Generated Python cache files were removed after
aggregation.
