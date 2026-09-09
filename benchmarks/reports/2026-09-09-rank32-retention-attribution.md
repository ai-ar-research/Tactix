# Rank32: deprioritize the scan-only optimization

## Decision

The proposed three-site binary-retention fast path is not selected as the main
Rank32 gap-closing step. Existing samples attribute only0.619868% of candidate
event periods to the three sites, including bookkeeping that a fast path would
still need. This does not disprove a small gain, but it is weak leverage for
the current runtime gap. Keep6527's baseline contract tests; do not launch a
new timing campaign or implement this proposal merely because it is small.
Its previous approval request does not authorize another implementation.

Next, decompose the larger Rank32 inprocessing costs, starting Factor and
Congruence: align timer/call boundaries and portable work before choosing a
repair. Tactix's wall-time rule/slot counters and native process-time profile
counters are not equal-operation measurements. No blanket BCP, scheduling or
budgeting diagnosis follows from their difference.

## Offline evidence

6530 annotates four functions from the already checked6372 Release pair:
candidate rebuild/collection and native reconnect/flush.6532 annotates the
third candidate retention site. Each uses the archived compressed profile and
exact executable hash, verifies the decompressed hash, and emits period and
percentage views without executing a solver. Both arms have zero lost samples.
The6357 and6424 kernel.cpp and watch.hpp files are byte-identical; current6424
Rank32 also exactly replays6357's full non-timing statistics and proof bytes/SHA.
That supports source applicability, not exact newer-linked-binary timing.

6531 records reviewed half-open instruction ranges from the actual annotated
binary, never benchmark-address-based solver controls. The final three-site
sum, including6532, is recorded by audit6533.

| Attributed region | Sampled event periods | Share of candidate program |
|---|---:|---:|
| Rebuild retention, list bookkeeping and empty-list branch |4961628461|0.404679%|
| Collection pure retention, metadata and empty-list branch |651139912|0.053108%|
| Entire drop-large-watches function |1987225509|0.162081%|
| Combined, without overlap |7599993882|0.619868%|

The candidate program denominator is1226065909430 sampled-event periods.
The rebuild entry-loop subset alone is0.173085%; collection's entry-loop
subset0.025682%. Do not add these subsets to their containing regions.
The shared collection reference-mapping loop is separately0.088238% and is
not part of the proposed retention replacement.

The full rebuild function has1468 samples and2.258955% of program periods;
collection1429 and2.186335%. Drop has105 samples. Native reconnect has1368
samples and2.510068% of its own program, and native flush396 and0.727508%.
Native boundaries differ: its flush also deduplicates. These are locations,
not operation prices or a proof that native/Tactix maintenance work is equal.

Sampling skid, frequency-weighted periods and one profiled pair limit
precision. Attributed instructions do not establish the cause of a hardware
stall. A fast path cannot eliminate all attributed bookkeeping, must retain
mixed-list scans, and could affect costs elsewhere. The0.619868% observation
is therefore neither a guaranteed saving nor a rigorous speedup ceiling.

## Validation, ownership and cleanup

New maintained perf_annotation.py reads integer periods without floating-point
loss, validates the requested single symbol, rejects malformed/duplicate/
unordered instruction records, and requires exact interval boundaries. Seven
focused tests cover valid/zero-weight intervals, integers above2^53, wrong
mode/event, missing samples/content, wrong/repeated symbols and bad ranges.
Frozen input hashes establish complete artifact identity; schema checks alone
cannot detect arbitrary truncation on a complete-line boundary.

No solver, observer, strategy, proof rule, campaign script or reference timing
changed. No source suite or fuzz rerun is needed for this offline/tooling-only
increment; owning benchmark-tool and documentation tests are rerun by6533.
Current full6439 remains50/50 checked/clean and8731.653/7239.010=1.206194355;
memory and all50 remain scored. No promotion or denied transfer/push retry.

Helpers deleted1560523560 bytes of their own temporary profile expansions.
The original compressed profiles and all derived annotations remain retained;
the expansions can be reproduced. No certificate or live-checker scratch was
opened or manually removed. The six pending6464 checker tasks remain owned
by their existing launches; audit6533 records their current accounting.
Prior6528 owns the previous full50/source integrity checkpoint.

Andrew's new local report is
`report-to-andrew/20260909-1215-retention-attribution/report.pdf`.
Its initial PDF had a38.5pt overfull line; the retained overflow log records it.
The prose was reflowed before two clean compilations and the final audit.
Frozen6530/6531/6532 outputs must not be regenerated in place.
