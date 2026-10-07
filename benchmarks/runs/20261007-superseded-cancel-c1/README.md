# Superseded jobs canceled for candidate c1 (2026-10-07)

The user confirmed the exact IDs: "Yes cancel them".

- b26 full44 remnants: 1755331 (tasks 0, 1, 3, 18, 29), 1755422_6 and 1755462_4.
  b26 is superseded and these rows were reference-only. Canceled rows are
  incomplete, never a timeout or closure.
- d576abaeb regression suites: graph labeling on (1755593/95/97/99) and off
  (1755594/96/98/1755600). Candidate c1 (`e509d1d08` = d576 + affine repair)
  supersedes them for gate purposes. On the eight regressed rows, graph labeling
  on/off left the proofs byte-identical
  ([attribution](../../reports/2026-10-07-wv289-nol118-regression/README.md)).
  Partial results are retained unscored.

`queue-before.txt` is the pre-cancel queue; `sacct-after.txt` is the terminal state.
