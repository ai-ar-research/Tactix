# Rank263 retained conditioned-row A/B

This is a short mechanism gate, not a solve, performance, or CryptoMiniSat
parity claim. CPU-Q array job `1751022` compared the unchanged full-snapshot
reducer plus counter-only instrumentation with commit `167fc3cc812a8df8d500639c3730f9093330d7cc`'s
retained conditioned-row implementation. Both eight-second arms used the same
Rank263 CNF, DSR output, action profile, feedback, structural provenance, and
search configuration. The baseline patch only exposes the new counters.

The immutable inputs were base commit
`1736f36f3301b16e11ef595080b84f03da0dddb5`, source archive SHA-256
`73c1066656ce9710f5e2bdb04b1dd39159ab3d937b90910c9f841d70e8ec1c1c`,
candidate patch SHA-256
`9ed86d2b56bce21faa3ce4ca8778a8c170e0999853c543c7f73df82e04bc7bdd`,
and compressed input SHA-256
`64a2eba5d44ff850789de5fa7ff854c3ff90c428c5c25a5ece08f91bd6057ac4`.
The retained run manifest and exact JSON results remain under
`/home/hwu/.cache/r263-retained-ab/`; the result hashes are respectively
`b66116803be3f1ecba8c1ebe1920357ea787ba3ac45f3be4aa074b465743e1d1`
and `5ca998a894101e92f7278e28d99e8ae9d0c2766632e8fbf02758d2d684f24d18`.

| measurement | full-snapshot baseline | retained-row candidate |
|---|---:|---:|
| service calls | 672 | 678 |
| applied proof transactions | 309 | 311 |
| extension hits | 0 | 1 |
| full rebuilds | 671 | 677 |
| rebuilds after prior partial result | 0 | 677 |
| retained-prefix assignments | 0 | 0 |
| conditioned assignments | 578,844 | 583,174 |
| separate full-trail capture work | 579,469 | 583,174 |
| matrix reduction work | 193,649,789 | 196,375,222 |
| decisions / conflicts | 343 / 70 | 346 / 72 |
| DSR bytes | 45,760,558 | 45,767,391 |
| provenance bytes | 378,844,856 | 378,906,789 |

The candidate took the retained path only for its initial empty prefix. That
conditioning returned `Partial`; every later call therefore correctly rebuilt
from the immutable admitted rows. The current 160-source basis cap prevents
the implementation from retaining a complete conditioned state, so this
vertical has no measured Rank263 benefit. The small throughput differences
come from an unmatched-node short run and do not establish improvement or
regression.

Focused ASan/UBSan tests pass for exhaustive small conditioned-rowspace
equivalence, independent reason validity, assignment extension, multiple
implications followed by conflict, atomic interruption and retry, shorter
backtrack, same-length reassignment, prior-partial fallback, controller
publication, and raw-counter exposure. Local proof CTests were present but
skipped because their external checker was unavailable. No broad regression
campaign was run.

The code-level CryptoMiniSat parity ledger is:

- implemented here: retained exact source combinations and a complete
  conditioned basis across monotone trail extension; full-current-trail
  reasons; atomic fallback after backtrack, reassignment, interruption, or a
  prior partial reduction;
- already present in Tactix: authenticated source ownership, ordinary clausal
  publication before BCP, DSR obligations, work and byte accounting, feedback,
  and replay identities;
- still missing: a complete high-support root matrix, repeated root closure,
  overlap-component selection, touched-row watches, local pivot repair,
  incremental assignment bitsets, and useful-work scheduling;
- intentionally different: Tactix must publish certified ordinary clauses;
  CryptoMiniSat can enqueue matrix implications directly and construct a
  clausal reason lazily.

The next decisive vertical is the source-cap/root-closure boundary. Exact
offline reduction finds a Rank263 root contradiction supported by 670 source
equations and reaches 1,041-source intermediate rows, while the current
160-source cap drops rows and first yields an event only at decision level 23.
The matrix needs an explicitly bounded high-support algebraic state and a
certified root-closure loop before row watches or local pivot repair can affect
this target. A conflict in the current retained-row vertical still causes the
later backtrack service to rebuild; this is not CMS-style incremental
backtracking.
