# Mixed-formula affine support census

Commit `3676cff1c95eed187455fc327321d7e0e34c82d3` was probed with the
immutable Rank-263 binary
`88347e1c2a2835cefe6a92a9371d1203f412e64291ed16217c13b087865ec8aa`.
These are 15-second, no-proof mechanism censuses, not solve or acceptance
claims. The controller offered only `AffineMatrixInitialize`, then resumed the
ordinary `vmtf/adaptive-glue/focused-rotation` search configuration.

CPU-Q array `1751231` used a 200,000-group recognition limit on fixed Tier-D
ranks 540, 557 and 896. Ranks 540 and 557 exhausted recognition after the
first attempt: their first snapshots cost 1,502,248/2,390,050 work and their
recognition prefixes cost 784,813/800,004 work. Rank 896 admitted one matrix
in 53 ms, but its provenance contains only one row, `x90 xor x91 = 1` (external
variables 90 and 91). Across 71,049 service calls it produced zero events,
captured 82,817,599 trail assignments, performed 119,640 synchronization work,
used 69,109 incremental extensions, and rebuilt 1,939 times, including 1,769
backtracks. The final call was interrupted by the campaign timeout.

CPU-Q array `1751290` changed only the group limit from 200,000 to 400,000 for
ranks 540 and 557. Its action-profile SHA-256 is
`f9a7e2bb45c2a73ed2bb86a5c65161675dbdd0c683591be6a7e05764e73c0933`.
Both images then completed, confirming the group limit as the first failure.
Each admitted matrix was nevertheless only one binary XOR:

- Rank 540: `x2 xor x3 = 1`; 9,384 service calls, 9,383 no-events and one
  timeout interruption; 82,489,467 captured trail assignments; 25,515 matrix
  work; 9,326 extension hits and 57 rebuilds.
- Rank 557: `x46 xor x47 = 1`; 6,249 service calls, 6,248 no-events and one
  timeout interruption; 86,322,773 captured trail assignments; 8,793 matrix
  work; 6,226 extension hits and 22 backtrack rebuilds.

The version-8 feedback and version-2 affine service descriptions from all five
retained traces pass `tools/bench/inspect_policy_feedback.py` after its contract
was brought forward. The census establishes two separate boundaries. A larger
global recognition cap can admit these formulas, but the recovered affine
components are too small to justify scanning the full CDCL trail. Search
service capture must be proportional to the admitted matrix support, and an
unchanged support must be a zero-reduction no-event. Component usefulness
admission and touched-row scheduling remain necessary before enabling mixed
formula matrices in DefaultPolicy.

The raw pinned artifacts are under
`build/pin-affine-mixed-census-3676cff1c-v1` and
`build/pin-affine-mixed-census-3676cff1c-v2`.

## Exact-support follow-up

Commit `bf21ed1e98478b00c92fe314754e60362815cfe8` changed search-time
capture to probe only the sorted union of variables in admitted matrix rows
and made an unchanged support snapshot a zero-reduction fast path.  It did not
weaken publication: a selected consequence still recaptures and revalidates
the complete Boolean trail and assignment levels before committing its clause.

CPU-Q array `1751377` reran the same 15-second no-proof mechanism census with
immutable binary SHA-256
`5645864b8bd653157fc7e4e0939b9789e94933329a44dfaa8bee50bf91ff3ec2`.
Ranks 540 and 557 used the prior 400,000-group profile; rank 896 used its prior
200,000-group profile.  Each retained matrix again contained exactly one
binary XOR and produced no implication or conflict.

| rank | service calls before / after | trail probes before / after | matrix work before / after | events |
|---:|---:|---:|---:|---:|
| 540 | 9,384 / 98,554 | 82,489,467 / 197,108 | 25,515 / 1,008 | 0 / 0 |
| 557 | 6,249 / 53,620 | 86,322,773 / 107,240 | 8,793 / 27 | 0 / 0 |
| 896 | 71,049 / 141,930 | 82,817,599 / 283,860 | 119,640 / 4,428 | 0 / 0 |

The new trail-probe totals are exactly two probes per service call.  Relative
to the matched earlier runs, capture work fell by approximately 419x, 805x and
292x respectively.  The larger service counts show that full-trail capture
was a material throughput bottleneck, but they do not make these one-row
components useful: the next vertical is component-level usefulness admission
and scheduling, followed by touched-row wakeups for matrices that pass it.

The follow-up raw artifacts are under
`build/pin-affine-support-census-bf21ed1e9-v1`.
