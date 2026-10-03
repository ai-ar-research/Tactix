# Owner report — public ParityConsequence (X8) snapshot fuzz (PRELIMINARY)

Status: **RUNNING** — no terminal evidence yet; nothing accepted.

- Owner: headless Claude session (started 2026-10-03 18:10 local).
- Candidate verified before use: manifest, PIN.json (916ad847…), `tactix`
  e5719daa…, all 187 executables, all 821 snapshot members, root checkout:
  0 differences, 0 drift.
- Checkers: owned frozen collection `build/pin-fuzz-checkers-parity-public-6fe14539`,
  SHA256SUMS 895f1989… (byte-identical to the accepted collection; dsr-trim 3b5c7737…).
- Coverage plan frozen before submission (sha256 711025dc…, private campaign).
- Seed windows: 2080–2089×10⁶ (the request's suggested 2070/2071×10⁶ are already
  used by the independent winner-graph owner's targeted rows). Freshness scan:
  no prior use of any leg seed.

## Job handles (status monitoring)

| Job | Purpose | State |
|---|---|---|
| 1747736 | L1 ordinary DefaultPolicy leg, 100,010 seeds 2080000000+ + corpus | submitted 2026-10-03 |
| 1747737 | L2 RandomControllerPolicy leg (`--controller-policy={seed}`), 100,010 seeds 2081000000+ + corpus | submitted 2026-10-03 |

Both legs have X8 off by construction (no provenance stream bound; DefaultPolicy
reference omits X8) and will never be counted as X8 coverage. X8 targeted
harness (XOR fixtures, configured DefaultPolicy, random-controller X8
collections, P1–P5 scenario driver, provenance/prefix oracles) is being built
and will be frozen and selftested before its job is submitted.
