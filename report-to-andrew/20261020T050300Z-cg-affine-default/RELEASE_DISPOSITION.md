# Release disposition

Date: 2026-10-20
Overall disposition: **FAIL**

This disposition reads the completed regression campaigns in place. No
benchmark task was resubmitted to produce it. This is the approved one-time
reuse of old results for the current release decision.

## Evidence boundary

The four fixed-set campaigns measured candidate commit
`68af6bea9b41461ac385bd7c5077a144604a0193`. This disposition was first recorded
at `d97fae8ce`; later proof-only changes compacted ordinary PBP binary-deletion
tracking without changing the strategy trajectory. The current direct affine
implication change does alter applicable trajectories. The old campaigns are
therefore reused only as historical correspondence and negative release
evidence, not as measurements of the current binary, its current performance,
memory use, proof bytes, or affected affine rows.

Current proof behavior is covered by full current-source Debug coverage,
focused current-source Release proof tests, `default_public_correspondence`,
competition compliance, and targeted VeriPB/CakePB checks. These checks do not
erase the frozen aggregate performance risk or complete the missing
paper-comparable measurement.

The current solver commit is `70ff89e828512cd2155b1ffc8adcd869697edfeb`.
The direct-implication source passed the complete Debug coverage. The later
retained-XOR-buffer delta passes all 22 affected affine-matrix tests under
ASan/UBSan and 23/23 focused Release tests including public correspondence,
but has not yet had another complete 372-test run.

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

The frozen four-arm manifests do not provide that paper-comparable comparison:
production, public, and hardcoded ran the candidate's default PBP route, while
the paper arm ran `--auto-proof`. Across the complete stored rows, production
wrote 3.89x, 3.61x, 4.32x, and 3.61x as many certificate bytes as the paper arm
on B28+C5, Rank1--50, WV100, and WV2--89, respectively. Its retired-instruction
ratios were 1.24x, 1.44x, 1.96x, and 1.22x. Persistent affine search serviced
one B28+C5 row and no row in the other three sets. This is strong evidence that
proof-format cost materially contributes to the observed wall-time ratios, but
it is not a causal same-binary proof-mode A/B and does not measure the later
PBP formatting and deletion changes.

The paper +10% ceiling therefore remains **incomplete**, not failed, for the
current candidate. Under the clarified transition contract, 0.2.0 establishes
the first default-PBP operational baseline rather than forcing PBP under a
different proof route's +10% wall-time ceiling. This does not absorb a search
regression or waive proof validity, complete records, cleanup, or predeclared
resource caps. Per the approved one-time evidence reuse, no completed
regression campaign was resubmitted for this disposition.

## Direct affine-implication checkpoint

Pinned `cpu-q` job `1795208` compared current production, current public, and
the exact predecessor on CG ID 284. Production and public solved UNSAT in
30.768 and 30.970 seconds and emitted byte-identical 39,899,363-byte PBP proofs;
both passed VeriPB and CakePB. They match at 19,303 conflicts, 27,891 decisions
and 1,683,943 propagations, with zero public fallbacks or declines. The exact
predecessor timed out at 120.006 seconds.

This is decisive causal evidence for the implementation change, but it is one
draw rather than the predeclared paired-median release gain, and 30.768 seconds
does not meet the fixed 7.550-second CG closure target. It therefore changes
neither the overall FAIL disposition nor the two retained CG closures.

Profile job `1795211` identified sorted row XOR and allocator churn as the
current trajectory's leading matrix costs. Matched job `1795214` measured the
bounded retained-XOR-buffer implementation at 30.59/30.69 seconds versus the
exact predecessor at 30.95/30.96 seconds on one node and CPU. Mean wall and
affine overhead improved 1.02% and 1.06%; verdict, proof hash, trajectory and
all affine work/event counters were identical. This is accepted development
evidence but falls below the 5% threshold for a release performance claim.

## Gate results

| Gate | Disposition | Evidence |
| --- | --- | --- |
| Full Debug suite | INCOMPLETE for current delta | Direct-implication source ran all 372 tests in 2,052.64 s with ASan and UBSan: 367 passed immediately and five stale fixture repairs passed; the later retained-buffer delta passes its complete 22-test affine-matrix sanitizer slice but still needs the eventual full rerun |
| Full Release suite | INCOMPLETE for current delta | 372/372 at solver tree `9f0852545`; current retained-buffer source passes 23/23 affected Release tests including proof/model checks and public correspondence |
| Default PBP and competition compliance | PASS | Pinned VeriPB and CakePB pipeline passed; `tools/compete/check.sh` reports `COMPLIANCE OK` |
| Public DefaultPolicy functional correspondence | PASS | Current-source correspondence test passed; job `1795208` production/public match verdict, proof hash, conflicts, decisions and propagations with zero fallback or decline |
| Public-port overhead | PASS (reused) | Frozen public/production totals differ by +1.42%, -0.22%, -0.94%, and -0.32%; production/hardcoded totals differ by -0.55%, +1.10%, +1.30%, and +1.00% |
| Paper-comparable regression ceiling | INCOMPLETE | Candidate default PBP and paper `--auto-proof` are different proof modes; the stored 1.1615x, 1.2407x, and 1.5521x scorer ratios cannot be charged wholly to search, and WV2--89 also has a retention mismatch |
| Default-PBP baseline characterization | INCOMPLETE | The frozen records quantify larger certificates and instruction counts, three solver timeouts, and checker censors, but later PBP changes lack compatible complete operational-baseline evidence |
| Claimed performance improvement | FAIL as a release gate | Job `1795208` is a material one-draw causal win over a 120-second timeout; job `1795214` adds a proof-identical 1.02% two-draw mean improvement, still below the predeclared 5% release threshold |
| CG retained closure | PASS | IDs 83 and 609 remain the independently checked UNSAT closures; ID 284 improved to checked 30.768 s but remains above its 7.550 s target |
| Retention repair | FAIL | 9/18 valid; three solver timeouts and six proof-checker timeouts remain explicit censored outcomes |
| Release fuzzing | INCOMPLETE | Required fresh 100,000-case DefaultPolicy and 100,000-case random-policy campaigns lack independent owner completion |
| Artifact cleanup | PASS | All fixed-set rows, all three job `1795208` rows and profile/screen jobs `1795211`--`1795214` report terminal cleanup; audits found no generated `.cnf`, `.pbp`, `.proof`, `.opb`, `.lrat`, or raw `perf.data` artifact |

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
create a version bump, or create a release tag. The completed cells must not be
resubmitted merely to recreate them. A future measurement, if authorized after
another material candidate change, should add only the missing paper-comparable
proof-mode arm or other genuinely incompatible, missing, or censored evidence.
