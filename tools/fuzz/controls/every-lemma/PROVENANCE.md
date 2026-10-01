# Every-lemma control fixtures

Read by `../every_lemma_controls.py`. Hashes are SHA-256.

| File | SHA-256 | Origin |
|---|---|---|
| `f2-forward-elaboration.cnf` | e81598c4…bc59925 | witness-free F2 bundle 000508 (controller fuzz owner eba47800, `handoff/audit-000508/checker.cnf`) |
| `f2-forward-elaboration.dsr` | 93e5ad15…263a3d44f | same bundle, `proof.dsr` (valid, witness-free) |
| `f2-historical-forward.lsr` | 3329b2e9…ef81307ea | `dsr-trim -f` output of the historical defective checker 02824056… on the F2 pair, expanded by `dsr-compress -d` 6bc416a0… (Codex evidence `benchmarks/probes/scratch/controller-checker-clean-build/current-witness-free-f2/proof.lsr`) |
| `f2w-witnessed.cnf` | 2fab2886…df139c4 | witnessed F2w L1 bundle `003214-proof-seed2026103315` (controller fuzz owner 12e3c4f0, `case.cnf` = `checker.cnf`) |
| `f2w-witnessed.dsr` | ef2c118c…b5a72d3 | same bundle, `proof.dsr` (valid, witnessed) |
| `f2w-historical-forward.lsr` | 819bbe34…737c994a | historical 02824056… `dsr-trim -f` + `dsr-compress -d` output on the F2w pair (`…/current-witnessed-f2w/proof.lsr`) |

The historical checker reported `s VERIFIED UNSAT` and the expansion succeeded
on both pairs; Trestle rejects both elaborated LSR files (`s INVALID`). The
maintained-patch checker 3b5c7737… (upstream 0b8cc0ef plus only patch
edd70e50) verifies both pairs through Trestle. The historical executable is not
needed: the controls check Trestle's rejection of the retained LSR artefacts.
