# Release disposition

Date: 2026-10-20
Overall disposition: **FAIL**

This disposition reads the completed regression campaigns in place. No
benchmark task was resubmitted to produce it. This is the approved one-time
reuse of old results for the current release decision.

## Evidence boundary

The four fixed-set campaigns measured candidate commit
`68af6bea9b41461ac385bd7c5077a144604a0193`. This disposition was first recorded
at `d97fae8ce`; the current solver change compacts ordinary PBP binary-deletion
tracking without changing the strategy trajectory. The old campaigns are
therefore reused for trajectory correspondence and as negative release
evidence. They are not presented as measurements of the current binary, its
current memory use, or its current proof bytes.

Current proof behavior is covered separately by the complete Debug suite,
focused current-source Release proof tests, `default_public_correspondence`,
competition compliance, and targeted VeriPB/CakePB checks. The earlier full
Release suite remains valid for the unchanged behavior outside the focused
proof delta. Those checks do not erase the frozen aggregate performance
failure.

## PBP binary bookkeeping update

Ordinary whole-run PBP no longer retains the exact key-and-ID arrays for every
binary clause. It uses the existing no-false-negative binary liveness index
(four bits per entry, rounded to whole power-of-two blocks, or the bounded exact
bitmap where selected) and emits canonical `del spec` records for binary
constraints. Non-binary constraints continue to use exact `del id` records.
LRAT still retains exact binary IDs, and a late-attached API PBP proof can use
them when they already exist.

The root-reason regression now checks a mixed stream with nine exact ID
deletions and one binary specification deletion. A second serializer regression
forces divergent input IDs and verifies that a binary deletion does not rebuild
the binary archive or disturb the following exact unit deletion. Both Debug and
Release focused tests pass, including pinned VeriPB/CakePB checks and corruption
controls.

This change does not establish that PBP caused the frozen performance failure.
The paper +10% ceiling remains a paper-comparable solver-time gate. Any default-
PBP production overhead is a separate matched comparison of the same current
solver with and without PBP; it cannot be used to relax or absorb a search
regression. No such new performance experiment was launched for this
disposition.

## Gate results

| Gate | Disposition | Evidence |
| --- | --- | --- |
| Full Debug suite | PASS | Current source: 372/372 in 2,052.72 s; ASan and UBSan enabled |
| Full Release suite | PASS | 372/372 at solver tree `9f0852545`; current-source focused proof/PBP slice 40/40 |
| Default PBP and competition compliance | PASS | Pinned VeriPB and CakePB pipeline passed; `tools/compete/check.sh` reports `COMPLIANCE OK` |
| Public DefaultPolicy functional correspondence | PASS | Current-source correspondence test passed with no fallback or decline; frozen production/public/hardcoded rows preserve verdict, certificate, and trajectory correspondence |
| Public-port overhead | PASS (reused) | Frozen public/production totals differ by +1.42%, -0.22%, -0.94%, and -0.32%; production/hardcoded totals differ by -0.55%, +1.10%, +1.30%, and +1.00% |
| Paper regression ceiling | FAIL (reused) | Release scorer: B28+C5 1.1615x, Rank1--50 1.2407x, WV100 1.5521x; WV2--89 is unqualified because of a retention mismatch |
| Claimed performance improvement | FAIL | No qualifying 5% paired-median or PAR-2 gain; the paper regression ceiling is already exceeded |
| CG retained closure | PASS | IDs 83 and 609 are independently checked UNSAT closures within 2x matched CryptoMiniSat time |
| Retention repair | FAIL | 9/18 valid; three solver timeouts and six proof-checker timeouts remain explicit censored outcomes |
| Release fuzzing | INCOMPLETE | Required fresh 100,000-case DefaultPolicy and 100,000-case random-policy campaigns lack independent owner completion |
| Artifact cleanup | PASS | All fixed-set rows report terminal cleanup; retention audit found no `.cnf`, `.pbp`, `.proof`, `.opb`, or `.lrat` artifact |

## Frozen campaign provenance

| Set | Rows | Manifest SHA-256 | Result-set SHA-256 |
| --- | ---: | --- | --- |
| B28+C5 | 132 | `b1c968445d3e735445652507d2a762c6f0d6e636bf79ee02c22901a9433e50f8` | `31073dfda08dcc7ea6899980bb837083bac1432423378823e8b2f44022df9e44` |
| Rank1--50 | 200 | `44f2b0dd52923289fa33caaeafba518cbc81be4b56446a6d957c9f5d1d19fb2c` | `6b577c142fee15c6bb12128cb4e848bbac83fdb056b758ab2f4f630c22af8f16` |
| WV100 | 400 | `3ff9a459f4b915b775bb39467f201893e6834fc30a12cf720d140b772bfa52f0` | `bfb8e04a80906d42d5e84ae39e9ae9a956c2a6b0f6dbc91378d688052bbb0108` |
| WV2--89 | 356 | `1816f255a49b72cf72d3bde9381b7ed6bc85483eb50bd7c52509c77929d607a5` | `f78be93499b7f77724673b861816b3a01ad74fed85f88431b80f629d138d5303` |

The result-set digest is the SHA-256 of the sorted per-result SHA-256 listing.
Together the campaigns contain 1,088/1,088 expected result records and all
1,088 records report complete primary and derived-artifact cleanup.

## Publication consequence

This candidate is not releasable. Do not merge or force-push it to `main`,
create a version bump, or create a release tag. Further broad reruns of these
completed cells are unnecessary for this decision; future work should change
the candidate first and then target only evidence made incompatible by that
change or cells that remain genuinely missing or censored.
