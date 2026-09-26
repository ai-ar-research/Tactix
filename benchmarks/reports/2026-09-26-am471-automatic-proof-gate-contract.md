# AM471: fixed-reference gate matches the approved automatic-proof contract

Date: 2026-09-26. Tooling-only correction. The solver source and plain Release pin remain `1d81d353` and SHA-256 `b7962004...`; no solver rule, public action, checker option, or accepted proof interpretation changed.

The AM469 fixed gate was prepared as explicit DSR, although its default CLI arguments selected automatic proof output. Its witness-free DSR rows were checked through the approved DRAT/CakeLPR path; AM469 was stopped when the gate's DSR label obscured that route. AM470 then required a second canonical DSR check for witness-free proofs, but Rank 41 emitted a legal complete startup PBP and exposed the larger mistake: an explicit-DSR-only gate cannot validate the approved automatic mode. Both sets of arrays were stopped and their completed results preserved as diagnostics (AM469 Rank 21, B28+C5 0; AM470 Rank 21, B28+C5 0). Neither is scored as Phase 1 acceptance.

Plan §5's existing content route is authoritative. The v6 fixed gate now invokes `--auto-proof` and pins all seven independent proof checkers from the frozen AM369/AM455 bundle. Its scorer requires the exact recorded route for each valid UNSAT row:

| Certificate | Required proof stages |
| --- | --- |
| Complete startup PBP | VeriPB, CakePB |
| Witnessed DSR | no-option `dsr-trim`, binary-LSR expansion, pinned Trestle |
| Witness-free DSR | `drat-trim`, CakeLPR |

SAT rows still require an original-CNF model check. The campaign manifest, checker pins, solver binary, input hashes, resource contract, row identity and stage outcomes are checked before scoring. The v6 preparer and scorer leave old v4/v5 campaigns identifiable and do not relabel their results. This is a correction of the gate description and completeness, not a change to the proof oracle's accepted contract.

The route matrix ran on frozen checker pins in SLURM `1728870`: all three positive routes verified; false PBP and false witness-free DSR were rejected; the AM443 satisfiable checker-state negative control reached `dsr-trim` but was rejected by pinned Trestle. The 68 focused runtime/preparer/oracle tests and full `tools/fuzz/selftest.sh` pass. Both freshly prepared v6 manifests pass `fixed_runtime.verify_campaign` and `runtime_task.validate_runtime_pin_contract`, including VeriPB and CakePB pin presence and hashes.

The submitted v6 Phase 1 arrays are Rank 1--50 `1728871` (`benchmarks/runs/20260926-am471-rank1-50/`) and B28+C5 `1728898` (`benchmarks/runs/20260926-am471-b28c5/`). They use 12 CPUs, 32 GiB, a 1,200-second solve cap and 16,000-second checker cap per row; the required aggregate solver bounds remain below 8,115.735 and 952.263 seconds. These jobs are pending terminal results. Latest-source Opus fuzz, full Debug, N1--N4, online-learning readiness and Tier D remain open.

The Rank campaign's frozen Kissat vector has 45/50 previously checked reference results. The retained `rank1-50-tactix-a08f9f6a-v1` reference has 50/50 and the same exact rank/input identities; a partial `score --reference` comparison succeeded. Final Rank certificate-completeness scoring will use that fully checked comparison reference while preserving the submitted candidate rows and the explicit 8,115.735-second target. The B28+C5 frozen reference is 33/33 checked. No reference solver is rerun.
