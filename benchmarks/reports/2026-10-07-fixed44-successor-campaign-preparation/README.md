# Fixed-44 successor campaign preparation

This is a launch plan, not a result.  The successor candidate must contain the
archive-portability fix `55776b30c`, the effort-parser correction, the pending
affine assertion repairs, and the pending public noL selector/counters.  Its
full SHA, source tree, Release binary, and effective option vector are not yet
frozen.  No job described here has been submitted.

## Acceptance denominator

The immutable selection is [`selection.tsv`](selection.tsv), SHA-256
`74939b00a3395aa21d108d4ab20eb0925f569c1ac2bb501939e01432551a3333`.
It contains all 44 rows from the frozen miss ledger, in ledger order, and binds
each row to its original Tier-D rank, exact instance path, and expected verdict.
All 44 must run on the same successor binary and option vector.  Ranks 147, 263,
630, and 744 are retention obligations on this pin; their older accepted
artifacts do not establish same-candidate coverage.  The milestone is at least
12 independently verified clean results from these 44 records.

## Priority interpretation

The full array is required because the present evidence does not justify a
claim that eight particular rows are likely closures.  Read these rows first:

| Priority | Rank(s) | Verdict | Saved Tactix boundary | Successor evidence or question |
|---:|---|:---:|---|---|
| 1 | 401 | SAT | 5,000.005 s timeout | Final source consumes retained noL Walk phases at root.  The pre-fix off arm checked SAT in 148.426 s while the on arm timed out at 180 s with zero installations.  The pending public selector must settle the corrected on/off pair before choosing the global option vector. |
| 2 | 569 | UNSAT | solved in 3,034.193 s; `dsr-trim` route failed after 7,633.506 s | Primary persistent-GF(2) target.  Earlier retained-matrix run was about 3,018.531 s with zero exported XOR reasons; indexed on/off repeats and assertion repairs must settle admission before qualification. |
| 3 | 1004 | UNSAT | solved in 4,106.474 s; `dsr-trim` failed after 99.692 s | Second multiplier and the closest same-family test of whether persistent GF(2) or proof-shape changes generalize. |
| 4 | 627 | UNSAT | solved in 2,062.818 s; `dsr-trim` failed after 93.444 s | Fast proof-route recovery sentinel with no claimed theory mechanism. |
| 5 | 1161 | SAT | 5,000.005 s timeout | Source-authenticated guarded-cover work found three novel original-CNF RUP tuples and demonstrated one native reason use in a focused profile.  Ordinary coherent-candidate scheduling remains unproved. |
| 6 | 190, 453, 754, 887, 1144 | SAT | all 5,000 s timeouts | Requested domain/timetable/lockchart cohort.  Neutral equality ownership is present, but no saved ordinary-Default target yield supports a closure forecast. |
| 7 | 291, 541, 1075, 1129 | UNSAT | solver 245.978, 89.874, 176.838, 80.613 s; checker timed out | Cheap proof-shape sentinels.  The exact-certificate audit found no historical verification under 14,400 s, so they close only if the successor materially changes the certificate. |

The remaining 30 rows stay in the same full-width array.  This preserves the
chance of generic CDCL, scheduling, model, proof-route, or interaction gains
without promoting a filename-based theory hypothesis.

## Exact campaign contract

Use one task per row with no array throttle:

- partition `cpu-q`, 4 CPUs, 32,768 MiB per task;
- solver deadline 5,000 seconds, independent checker deadline 14,400 seconds,
  scheduler wall 20,700 seconds (5:45:00);
- automatic proof routing; SAT models checked against the complete original
  CNF and UNSAT accepted only after the canonical independently verified route;
- anonymous `O_TMPFILE` formula, certificate, model, and checker intermediates;
- only bounded JSON results and scheduler logs persisted; no failed proof
  retention in the qualification pass;
- no build, sanitizer, Debug, or other timed solver campaign sharing this
  campaign's execution window.

The 44 simultaneous tasks request 176 CPUs and 1,441,792 MiB (1,408 GiB) of
aggregate memory.  Their aggregate wall allowance is 253 task-hours.  The
largest saved fixed-44 Tactix certificate was 20,324,467,728 bytes, so node-local
scratch pressure is material even though home-quota growth should remain below
100 MiB.  Anonymous cleanup and a fresh storage check are mandatory before and
after the array.

The source and dependency package must be regenerated after the final SHA is
known.  It must include the ignored fixed-baseline manifest and orbitopal fuzz
terminal report, verify every packaged dependency hash after extraction, and
rely on `55776b30c` to find packaged Kissat/CaDiCaL headers without fake Git
metadata.  Cleanup may make only the job-local extraction writable; it records
chmod/removal status while preserving the original build or CTest exit code.

With `FINAL_SHA`, `FINAL_SHORT`, and `EFFECTIVE_OPTIONS` frozen, prepare and
validate the target as follows (repeat `--solver-argument=...` once per option):

```bash
python3 tools/bench/experimental_tierd.py \
  --campaign "benchmarks/runs/20261007-final-${FINAL_SHORT}-fixed44" \
  --source /home/hwu/SAT-solving \
  --binary "build/pin-final-${FINAL_SHORT}-rel/tactix" \
  --binary-source-commit "$FINAL_SHA" \
  --partition cpu-q --cpus-per-task 4 \
  --solve-timeout 5000 --checker-timeout 14400 \
  --certificate auto \
  --selection-table benchmarks/reports/2026-10-07-fixed44-successor-campaign-preparation/selection.tsv \
  ${EFFECTIVE_OPTIONS} --prepare

python3 tools/bench/experimental_tierd.py \
  --campaign "benchmarks/runs/20261007-final-${FINAL_SHORT}-fixed44" \
  --validate
```

Submit the validation job first.  Submit the array only after validation passes,
the Rank401 and Rank569 causal pairs are terminal, and the Release build job has
left the cluster.  Omit `--array-throttle`; do not submit the uncorrected 7e
campaigns.  The same exact effective options must appear in B28+C5, Rank1--50
WV, WV100, and WV2--89 manifests.  If either opt-in target does not demonstrate
admission or benefit, omit it everywhere rather than mixing target and
regression configurations.

## Symmetry follow-up

Saved evidence shows both effectiveness and startup risk.  The typed
orbitopal-to-graph program checked nine of 57 Satsuma-owned rows in 0.117 to
115.352 seconds, while the Rank6 diagnostics spent 848.346 seconds in structured
preprocessing and still took 916.643 seconds.  Validation and group planning
were large measured components, and broader all-clause or connected-column
screens added no closures.

The next causal experiment is therefore a 60-second, six-row pair on ranks 190,
453, 754, 887, 1144, and 1161: identical final binary and resources, one typed
startup prefix without structured symmetry and one with the accepted
orbitopal-to-graph prefix, recording preprocessing wall, detection/validation/
planning work, effective SR transitions, root assignments, residual clauses,
and final model status.  Advance only rows that show effective transitions and
a smaller search residual.  No safe final public on/off surface for this exact
pair is currently frozen, so this experiment is a bounded design proposal and
must not be simulated with environment knobs or a source-only patch.

## Packaging repair checkpoint

The first 7e archive omitted two ignored test inputs and assumed Git metadata.
An unsubmitted intermediate v2 template demonstrated the corrected dependency
overlay and cleanup trap.  The durable source fix is commit `55776b30c`; its
live option audit enumerates 418 options with zero failures, and no-Git archive
root plus missing-header controls pass.  The final source package must be newly
hashed after all successor commits rather than inheriting the intermediate 7e
hashes.
