# Rank569 frontier-cube gate: partial clauses, no unit

## Disposition

The bounded two-variable support-frontier decomposition failed its declared
unit gate.  Two of four cubes produced independently checked conditional
clauses; the other two exhausted the 100-million-Kitten-tick per-case cap.
Because the four-cube cover is incomplete, no unconditional unit or conflict
was composed, no clause was published to Tactix, and no full Rank569 run is
justified.

The two completed clauses are nontrivial partial evidence.  They are not root
RUP on the original CNF and their native derivations required 35,861 and 37,614
subsolver conflicts.  A separate, explicitly diagnostic 60-second causal pair
could test whether adding exactly these two checked clauses changes ordinary
search enough to matter.  This report does not run or claim that experiment.

## Frozen contract

The campaign is
`benchmarks/runs/20261007-rank569-frontier-cube-gate`.  Its source archive is
commit `fd6ede71a`, SHA-256
`2d92d0743ba3f25a7610505829dd81aa4e00c53a9e61887679bfa4864388ef4f`.
The probe binary was built by job `1755099` and has SHA-256
`95df14529a4abda9022c6f2392b92b820bde4c04840702cdb3963f579c9bcd0a`.
The compressed original CNF SHA-256 is
`dbadc0aa9040c4394a12709e838d677c584934186a9154284e70051e2ed69446`;
the decompressed SHA-256 is
`06d08c3b44a98a2eea13d37b2bb5fe60913fc1d38989997595ddedbcb10b1d9b`.
The independent `drat-trim` SHA-256 is
`f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a`.

Selection was structural rather than keyed to a benchmark literal.  For each
authenticated terminal XOR, the diagnostic computed the transitive primary
support of its two input cones.  The first fresh subsolver failure after an
independently proved immediate predecessor was selected when the predecessor
support was nested and the set difference contained exactly two variables.
On this input, output 3,410 was the checked support-16 predecessor, output
3,411 was the support-18 failure, and the support frontier was `{9,21}`.  Those
numbers are frozen diagnostic results, not a production dispatch rule.

Each of the four cases assumed positive output 3,411 plus one assignment to
the frontier.  Limits were 100,000,000 Kitten ticks and 64 MiB serialized proof
per case, with a 256 MiB total proof ceiling.  Every completed final target had
to be a subset of the negated assumptions.  Completion required all four
signed cubes, an explicit cover check, independently checked case prefixes,
and an independently checked final resolution to `-3411`.  Missing-cube,
wrong-assumption-sign and wrong-target-sign controls had to reject.

The manifest is `manifest.json` in the campaign.  The predecessor result and
checked-prefix hashes are recorded there.

## Case results

Array `1755105` ran all four cases at full width without a throttle or node
constraint.

| cube | assumptions after `3411` | result | Kitten ticks | conflicts | final checked clause | proof bytes |
|---:|---|---|---:|---:|---|---:|
| 0 | `-9, -21` | UNSAT under assumptions | 86,070,964 | 35,861 | `21 9 -3411` | 12,582,802 |
| 1 | `9, -21` | budget | 100,000,866 | 38,158 | none | 0 |
| 2 | `-9, 21` | budget | 100,002,991 | 40,795 | none | 0 |
| 3 | `9, 21` | UNSAT under assumptions | 73,089,405 | 37,614 | `-21 -9 -3411` | 12,509,185 |

The two proof SHA-256 values are respectively
`4a0f39e358ef00eded03bbba018beda259f557155f64d9ff911061e029640ff4`
and
`e54b3ba0ffb397ef814b5928ee719522406d4c7b5cad7f87b5e4c43c015a5973`.
Their combined size is 25,091,987 bytes.
The two successful derivations consumed 159,160,369 Kitten ticks in total.
Including the two exhausted cases, the failed four-cube discovery consumed
359,164,226 Kitten ticks and 152,428 conflicts.  Any later search comparison
must charge this complete discovery cost before claiming a net benefit.

Both clauses are valid consequences of the original formula: each complete
subsolver derivation ends in a clause containing only negations of its actual
assumptions.  Independent forward `drat-trim -f -U` checks against the exact
original CNF returned documented exit 1 and printed both
`VERIFIED derivation: all lemmas preserve satisfiability` and `s DERIVATION`.
Both transcripts report zero RAT lemmas, and the exported prefixes contain no
deletion records.  Thus this is forward every-lemma RUP evidence over the
unchanged original clause population, rather than RAT-only
satisfiability-preserving additions being treated as entailed reason clauses.
The final proof lines are exactly the two clauses shown in the table.
The machine-readable evidence is
`completed-prefix-verification.json`, SHA-256
`b1bd4915c8f773dd572ea9de766b345aeb26d17ca12308f25a72e43b2938064f`.

The first case array invoked `drat-trim` with the options before its two path
arguments, so its checker steps reported file-open failures.  Verification-only
job `1755123` used the documented path-first order and the checker accepted
both prefixes.  Its scheduler tasks still exited 20 because the wrapper
anchored `s DERIVATION` at byte zero while the checker prefixes status lines
with a carriage return.  The preserved exit files contain `1`, and the exact
transcripts contain both acceptance markers.  This is a wrapper-parser failure,
not a proof rejection; the report retains both failed scheduler records.

The actual four-cube combiner did not run because cubes 1 and 2 were absent.
Its pure validator self-test passed complete-cover and rejected missing-cube,
wrong-assumption-sign and wrong-target-sign variants.  The test source and
output SHA-256 values are
`f248cc313a2ed8714192d142a508e92917072bde30e2be698967b7725dd27976`
and
`3cd9c4d2048fc5b9d8ccc13c9dd0b14432c51291800e7044b4abcda20e60f6a9`.

## What the failure says

Fixing the two newly introduced primary variables was insufficient to reduce
every support-18 case to the difficulty of the checked support-16 predecessor.
The opposite corners completed, but the mixed assignments consumed the whole
cap.  Raising the cap or trying alternate frontier grids would violate the
declared stop rule and has no measured basis.

The partial clauses answer a narrower question from the theory plan: a failed
complete analysis can expose checked consequences that are not immediate
Boolean propagation.  A direct root-RUP replay over the original 10,231
clauses rejects both clauses.  Their derivations also cost tens of thousands
of conflicts, so they clear a basic Boolean-hardness screen.  They have not
shown consumption or causal solving benefit and therefore do not clear the
publication bar.

The smallest follow-up, if separately authorized, is one same-binary,
same-CPU, sequential pair capped at 60 seconds per arm: original CNF versus the
original CNF plus exactly these two checked clauses.  It must use identical
search/proof settings and report decisions, conflicts, propagations and proof
prefix bytes.  This is only a usefulness test.  An UNSAT result would still
need a composed proof from the original CNF, and a fixed-time counter change
alone would not close Rank569 or justify Default integration.
