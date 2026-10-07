# Rank899 actual Satsuma handoff boundary — 2026-10-07

The measured transformation difference is one deliberately declined symmetry unit. This does not support adding missing row/product stages or promoting singleton-unit publication. The next useful comparison is the Boolean/search trajectory against the earlier singleton-declining G/LG control.

## Frozen experiment

Slurm **1754883** completed three sequential presearch arms on node006, CPU5, with 300 seconds per arm, 32GiB memory, four allocated CPUs and no node restriction. Campaign: `benchmarks/runs/20261007-rank899-actual-handoff-boundary-r3/`. `evidence.json` binds exact commands, effective diagnostic environment, source/binary/input identities, and artifact hashes. Native trace source is exact committed **3ecfa8dfedb6d0dc4aaeeebaddb1a674d390f12e**, archive SHA256 `94f4a79c6f4dec825587b6feac671dba0847ff6bf50ad14b9a3fc11922aba045`; EVENT_TRACE is enabled, proof-shadow and rule-journal disabled. Production source was not edited. The graph producer is explicitly off for this boundary comparison.

Input `mp1-Nb7T42.cnf.xz` has 52,731 variables and 208,936 clauses. Actual pinned Satsuma emits its residual with `fix --silent --full-skip-limit 100000000 --add-reduced-as-unit --bsr --proof-file ... --out-file ...`. Native snapshots stop at `preprocess_begin`, before ordinary preprocessing and search. These are transformation observations, not SAT/UNSAT acceptance or solve-time measurements.

| Arm | Presearch wall seconds | Root units | Canonical clauses | Active queue |
|---|---:|---:|---:|---:|
| Actual Satsuma emission | 4.880 | +40310 in residual suffix | — | — |
| Integrated native SR | 2.121 | 0 | 208,936 | 52,731 |
| Fresh parse of Satsuma residual, SR off | 1.571 | +40310 | 208,933 | 52,730 |

The emitted residual retains the original 208,936 clauses in order and adds `40310 0`; its raw first-import variable sequence equals the original input. Fresh parsing normalizes eight affected ternaries into four binaries plus the root unit. Its canonical multiset exactly matches the independently parsed reference residual. The integrated/fresh queue first differs at index26,827, and the entire fresh queue equals the integrated queue with variable40310 removed. Thus the observed queue difference is fully explained by the transformation difference.

Native trace records five discovered generators, all five exactly validated, and one candidate negation unit, with no backend budget limit. The large-domain `RequireCompoundProgress` admission in `src/rules/substitution_redundancy/symmetry.cpp` withholds a singleton batch. This is an existing accepted deviation in `docs/scheduling-parity-ledger.md` §13.8, explicitly naming this instance. The missing >50,000-variable AMO/row/product stages are a separate measured admission policy; this experiment does not establish that they would add useful output.

## Existing negative control matters

Historical campaigns `20260921-wv2-large-domain-admission{,2}` show unit-enabled A0/L at UNKNOWN1200s versus singleton-declining G/LG/LG2 at SAT174.5/173.3/174.4s with the same 2,150,089 conflicts. Their outputs are `out/{52,53,54,55}.tsv` and successor `out/10.tsv`. These old probes did not independently check models, so they are trajectory leads, not newly verified closures. They also used a mutable source probe; its present contents cannot prove the old binary's exact source identity. The retained probe already enables initial FastEliminate; assuming an old fastel0 baseline would be unsupported.

Current paired screen1754670 gives identical 2GiB capped proof SHA256 `ddd30e1ed50bf380e34bd58069a75ee9da70735da76901e3a3a0d5aa91db8473` for the before/after Rank401 policy profiles on899. Both results are storage-censored, not solve-time regressions; this confirms that the newest cut-point/original-seed change does not explain this row's current divergence.

## Preserved failures and limits

Predecessor1754800 was canceled during build after correcting its artifact bound, before solver arms. Predecessor1754807 hit the 64MiB diagnostic event-trace cap during speculative Lucky watch logging. The successful successor retains root/macro diagnostics and uses existing skip-watch/imply/heap controls; neither failed attempt is scored. Each subprocess file has a64MiB limit, finite file inventory, shared-scratch quota admission and an8GiB campaign allowance. No source or default policy promotion follows from this report.
