# Three terminal regressions on frozen 0e6b95c91

All three standing regression scorers accept, with no missing rows or reported failures.
This does not accept the newer development source or certify censored proofs.

| Suite | Rows | Independently checked | Proof censors | Solver seconds | Disposition |
|---|---:|---:|---:|---:|---|
| b28c5 | 33 | 32 | 1 | 945.859 | accepted |
| rank1-50 | 50 | 42 | 8 | 7804.489 | accepted |
| wv100 | 100 | 98 | 2 | 2646.389 | accepted |

B28+C5 is below its 999.87615-second aggregate ceiling; Rank1–50 is below
8521.52175 seconds. Completed proof comparisons meet their standing bounds.
WV100 includes one baseline-completed proof that is now cleanly censored within
its permitted comparison bound; the other proof censor is inconclusive in both
baselines. Clean censor acceptance is the established bounded regression rule,
not proof verification or a fixed-44 closure.

## Exact configuration and provenance

- Source: `0e6b95c91ca14309852f8e33f19c902ba89c66a8`.
- Release binary: `7d71dc9154e3d2a81c0e2b796b6286ead884908a177d68497cd78508d66bfc53`.
- Effective options: `--auto-proof --default-nol-walk=on`.
- Jobs: B28+C5 `1753470`, Rank1–50 `1753471`, WV100 `1753472`.
- Campaigns: `benchmarks/runs/20261007-final-0e6b95c91-regression-{b28c5,rank1-50,wv100}/`.
- Full-width arrays, one solver per task; resource contracts are retained in each manifest.
- Scoring command: `python3 tools/bench/controller_regression.py score <campaign>`.
  Complete scorer output is copied beside this report. A wrapper's failed Slurm
  exit for a clean proof deadline is not itself a source/checker rejection.

Some node017 measurements overlap Debug/fuzz allocation. These runs satisfy the
standing scorer gates; they do not establish isolated causal runtime improvements.
Any new performance claim needs its own controlled measurements.

WV2–89 and the full fixed-44 campaign are still running at this checkpoint.
Full Debug's original invocation remains failed (355/356, citation archive
packaging only), with a separately recorded successful packaging supplement.
Claude-owned fuzz and qualification of later production changes remain open.
