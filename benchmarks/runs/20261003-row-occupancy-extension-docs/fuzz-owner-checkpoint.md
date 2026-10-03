# Owner report — Ramsey startup (Lucky 22) full-snapshot fuzz (PRELIMINARY)

Status: **RUNNING** — no terminal evidence yet; nothing accepted.

- Owner: headless Claude session (started 2026-10-03 19:37 local). Sole owner
  of this handoff; the other live headless owner is the independent
  parity-public campaign.
- Candidate verified before use: manifest bindings, PIN.json (dba7236b…),
  `tactix` 610a7ac4…, 195/195 pin entries, 832/832 archive members (pin
  `source.tar.gz` = handoff snapshot e4a92954…), root checkout: 0 drift.
- Checkers: owned frozen collection `build/pin-fuzz-checkers-ramsey-full-fb174175`,
  SHA256SUMS 895f1989… (byte-identical to the accepted clean collection;
  dsr-trim 3b5c7737…). Fuzz harness snapshot sha256 1fceb82e….
- Seed windows: L1 2090000000+, L2 2091000000+ (100,010 each); targeted rows
  2092–2099×10⁶. Freshness scan: no prior seed-schedule use.

## Job handles

| Job | Purpose | State |
|---|---|---|
| 1747985 | L1 ordinary DefaultPolicy, 100,010 seeds 2090000000+ + corpus | submitted 2026-10-03 |
| 1747986 | L2 RandomControllerPolicy (`--controller-policy={seed}`), 100,010 seeds 2091000000+ + corpus | submitted 2026-10-03 |

Ordinary legs have Ramsey and X8 off and are never counted as Ramsey/X8
coverage. Targeted Ramsey (Q1–Q5), X8 (P1–P5) and shared-controller (R1–R13)
harness is being built from the immutable archive; it will be selftested and
frozen before submission.
