# AM404: H2 implied-unit deletion option, fixed-corpus controls

Date: 2026-09-25. Checker-only SLURM job `1721075` (`COMPLETED 0:0`, three
seconds; `cpu-dense-hwulab-q`, two CPUs, 8 GiB). No solver run or change, no
checker replacement, and no change to Tactix's canonical no-option DSR route.
The frozen [run](../probes/am404-h2-option-corpus/README.md) contains the exact
script, checker and input hashes, all 14 per-mode logs, resulting LSRs and
`out/1721075/results.tsv`; the raw output tree is tracked byte-identically
as `job-1721075.tar.gz` (SHA-256 `d797326832b9371f4035e52f20e438af65251107084331593b42abc55d71e5d7`).
The eight externally archived proof/CNF inputs are separately tracked in
`inputs-1721075.tar.gz` (SHA-256
`e000c5243e8bede112eafa7523a702afeab2d7e67a01196d81bad9be5ffdd0ac`).

## Result

The run used the AM369 checker pins: `dsr-trim`
`3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`,
`dsr-compress` `a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55`,
and Trestle `srcheck`
`19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`.
The option mode changes only the `dsr-trim` invocation to
`--delete-implied-units`; both modes use the repository's clause-preserving
checker-CNF spelling, backward elaboration, forward `-f` every-lemma audit,
binary-LSR expansion where needed, and independent Trestle verification.

| Exact input/proof pair | No option | With option |
|---|---|---|
| H2 parent-identical 21-variable handoff | Rejects at `Clause 60 not in watch pointers for literal 22` | Verifies, including `-f` and Trestle |
| AM401 fixed H1 seed 3965000531 | Verifies | Verifies |
| AM401 fixed H1 seed 3966003082 | Verifies | Verifies |
| AM397 terminal observer `t9003690`, DSR | Verifies | Verifies |
| Deletion-dependent implied-unit witness | Rejects | Verifies |
| Same witness with the deletion omitted | Rejects | Trestle rejects `s INVALID` |
| Invalid witnessed unit on five-variable input | Rejects | Trestle rejects `s INVALID` |

All 14 observed outcomes matched the predeclared controls. The two option
negative controls reached Trestle and were rejected there; an exit-zero or
`dsr-trim`-only reading would have been insufficient. The deleted-reason proof
becoming valid is **expected** under the changed proof-state contract: the
source binary is actually removed, so its former implication does not remain
an antecedent for the later witnessed unit. AM390 explains this semantic
distinction and its paired negative control. The three archived positive pairs
are not a statistically broad corpus; they exercise H1 public schedules and
the AM397 journal-observer path, not every DSR rule interleaving.

## Disposition

The option is a viable *candidate* for a fully audited checker contract, not
an accepted N2 fix. The run does not prove every legal X7 action can emit a
valid proof, every checker option path is sound, or that unchanged solver
proofs remain valid across the retained Rank/B+C/WV gate corpus. Before any
production switch, the user must settle the checker contract, then the
calculus/oracle/test contract needs an explicit change, broader original-CNF
positive and adversarial negative coverage, current-source Claude fuzz, and
all applicable full gates. If no-option is required, the general proof-state
emission problem remains unsolved; this diagnostic does not substitute a
case-specific reorder. H2 and combined N1/N2 remain open. No Tier-D run was
submitted.
