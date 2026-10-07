# Rank1–50 pinned recovery regression

Runtime source76e1f68b8, job1751525. Owning scorer accepts50/50 with zero failures/missing rows:25 independently checked SAT models,17 independently checked UNSAT proofs and eight bounded proof censors. Aggregate solve7493.99seconds stays below saved8521.52175second ceiling. Censors are not independently verified UNSAT certificates; task wrappers may exit1 on unverified checking while the scorer applies its explicit bounded-censor contract. All binary/input/checker/baseline, cleanup, verdict and resource bindings pass. Exact score and all50 rows are preserved.

This accepts this regression only on the pinned recovery source, not later b56 split-Walk or full fixed44 certification. WV100/WV2 and effective owner fuzz remain pending; full Debug retains documentation-size failure.
