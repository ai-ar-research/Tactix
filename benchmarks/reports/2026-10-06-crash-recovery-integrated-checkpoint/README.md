# Integrated crash-recovery checkpoint

The recovered affine root publication/session repair (`95b0fad13`) is composed
with bounded complete predecessor reuse (`dea87f523`, `375e95e2c`) and exact X20
controller status/retry handling (`2ddfcbedf`). The final integration separates
an exact predecessor restore from an actual Gaussian rebuild in service results
and raw rebuild counters; an empty-trail rebuild remains a rebuild. Fixed service
coverage tests both paths rather than inferring restore from zero conditioning.

Fresh cumulative Debug and ASan/UBSan with proof-population shadow and leak
detection each pass all ten focused affine matrix/transaction/live/search,
X20 proof/controller and restart-controller checks. The added root-unit and
empty-clause fixtures additionally pass the canonical witness-free DSR to
DRAT/drat-trim/CakeLPR route and every-lemma DSR/Trestle, in both builds. The final root fixtures preserve the original 3/5-variable DIMACS headers rather than counting proof-only helper identities.
[Debug](debug.log), [sanitizers](asan.log), [canonical Debug](canonical-root-debug.log)
and [canonical sanitizers](canonical-root-asan.log) retain gate outcomes. The final original-header fixtures pass [Debug](canonical-root-original-cnf-debug.log) and [ASan/UBSan](canonical-root-original-cnf-asan.log) rechecks. The
sanitizer tests run outside the ptrace sandbox so leak detection works.

The [predecessor report](../2026-10-06-affine-predecessor-microgate.md) owns bounded
memory populations and charged-work evidence. The
[DirectCSP short probe](../2026-10-06-directcsp-repeated-native/README.md) confirms
repeated unique publication but reports no root deduction or solve; it does not
justify longer target runtime or DefaultPolicy promotion.

Section and proof-rule references, citations and progress checks pass. The
existing document-size failures remain: winner-integration subplan 251/240 and
implementation notes 569/560. Their line counts were not increased. The plan
stays at 750 lines while adding the user-directed 700-GB storage gate.

Claude owns fresh fuzz of the recovered immutable affine pin `95b0fad13`:
jobs 1751457/1751458 cover DefaultPolicy/random-controller, with targeted and
reach calibration jobs 1751459/1751460/1751461. Their outcomes are pending;
Codex does not inspect or reinterpret campaign results. The later predecessor
and X20 changes still require effective fuzz on the final coherent pin. Full
Debug plus B28+C5, Rank1--50, WV100 and WV2--89 remain acceptance gates under the
standing 1,800-second proof deadline and slowdown contracts. No broad acceptance,
new target recovery or solver timing improvement is claimed here.
