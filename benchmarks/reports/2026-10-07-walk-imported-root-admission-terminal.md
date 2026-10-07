# Imported-root Walk pilot: no comparison reached

Array `1751827` completed all three tasks with scheduler exit `0:0` (27 s,
6 s, and 5 min 28 s). This means the runner finished, not that the solver
experiment succeeded. All nine driver invocations exited **3**, omitted their
terminal record, executed **zero Walk actions**, and were graded unmatched and
unsolved. All nine retained native-DSR prefixes passed the independent full
forward binary route, ending with Trestle exit 0 and `s VALID`.
There is no Walk admission, export, performance comparison or target-closure
result from this pilot.

The immutable solver source was `b56b1b0cef0cee4c9b7acfb01b3b0197f9a4ec7d`,
Release binary SHA-256
`e9dc413d2fbb72a7d19f4b28e42fe93dec1db3b599b011ae55c48a9ff0071e26`.
The scratch driver SHA-256 was
`9bf83a79f2c75262abd305bc2810d0d43003d9794a387de8349bb26de1d647b6`.

| Imported row | Actual import/search ticks, each arm | Lucky10 outcome, each arm |
|---|---:|---|
| `26_5_ttf` | 43,432 | Forced-TTF recognized; zero restarts/steps and no publication |
| `19_4_ttf` | 12,690 | Forced-TTF recognized; zero restarts/steps and no publication |
| `112_5_ttf` | 318,372 | One restart, 34 theory steps, three completion rounds; publication 260 followed by constructor SAT |

The immediate driver failure was its incorrect assertion that imported-root
`search_ticks` must be zero. The input parser intentionally closes input units
through propagation and charges that currency. Every row therefore returned
before dose selection or Walk. Applying the existing reference formula to the
observed ticks and initial Eliminate anchor 0 still yields `E=500,000`, since all
three observations are below the 10,000,000 minimum. This arithmetic does not
repair the missing producer-consumer boundary.

The first two rows independently have no usable publication. The current
fixed-row producer declines before search when a coordinate has an odd number
of nonzero entries: their coordinate populations are `[26,26,17,17,17]` and
`[19,10,13,12]`. On `112_5_ttf`, publication 260 supplied 89,824 known phases
among 184,202 external variables, with producer objective 0 and effort 34
SignatureSteps. Its constructor then installed a complete model trail at level
89,515. The failed driver exported no model for independent original-CNF
checking, so its internal SAT outcome is **not a checked recovery** or a
root-level partial-guidance consumption result.

Before submission, seven fixed checker controls passed locally against the
exact pinned checker hashes: nonempty valid RUP, actual empty native prefix,
invalid empty addition, truncated DSR, independent invalid binary LSR, valid
repeated-unit LSR, and truncated binary LSR. The empty-prefix route records
exact source/proof/zero-byte hashes and independently checks empty binary LSR
with Trestle; it neither accepts the converter's empty-file refusal nor invents
a lemma. Checker deadlines were diagnostic 120-second caps, not formal
benchmark acceptance. The prior `1751820` zero-hash-bucket and `1751824`
empty-converter failures remain preserved and bound by the successor manifest.

Authoritative artifacts are the immutable
[`v3 package`](../probes/scratch/walk-root-admission-b56-v3/manifest.json),
[`fixed controls`](../probes/scratch/walk-root-admission-b56-v3/fixed-controls.json),
and its three compressed result records under `results/`. A compact extraction
of all nine producer/driver/checker outcomes is retained in
[`terminal data`](2026-10-07-walk-imported-root-admission-terminal.json).
The package binds source, driver, input and checker hashes, public parameters,
microcontrol evidence, and both predecessor failure packages.

The next diagnostic must first obtain an observed live publication while
remaining at root, rather than repeat this matrix. A smaller explicit producer
completion dose on `112_5_ttf` is a candidate to test, not an established partial
producer: `complete_assignment` may still yield a valid model after fewer
passes. Derive effort from actual import ticks, require no search-policy
actions and a root-level nonterminal publication, and stop on terminal SAT,
no hint, interruption or identity mismatch. At this row's 184,202 variables,
the split profile's charged three-phase-buffer allocation alone costs 552,606,
already exceeding `E`; the preceding full-CNF construction costs more. An
`E/E` attempt can establish bounded preparation starvation, but cannot be
represented as an admitted seed or a successful search comparison.
