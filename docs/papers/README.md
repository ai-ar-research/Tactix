# Reference papers (local copies)

Kept in-repo when their license permits redistribution; provenance recorded
here for the licence/attribution pass that competition packaging needs
(plan §9, phase 6).

- `DynamicSAT-CP2025.pdf` — Shi, Jiang, Zhang, Luo, Liang, Chu, Xu:
  *DynamicSAT: Dynamic Configuration Tuning for SAT Solving*, CP 2025,
  LIPIcs 340, article 34. DOI 10.4230/LIPIcs.CP.2025.34. License CC-BY 4.0
  (per the paper's front matter). Grounding for plan §5.6 (learned
  proof-rule scheduling); code at github.com/cure-lab/DynamicSAT.
- `CubingForTuning-AAAI2025.pdf` — Wu, Barrett, Narodytska: *Cubing for
  Tuning* (TACO), AAAI 2025; arXiv:2504.19039 preprint copy (first author =
  this project's owner). Grounding for §5.6's episode-online mode: cubes as
  self-generated tuning data, true terminal cost, tune/validate/fallback,
  MCMC over strategy spaces.
- `OrbitopalFixing-SAT2026.pdf` — Anders, Codel, Heule: *Orbitopal
  Fixing in SAT*, arXiv:2601.16855v1 (2026). Reference for static
  orbitopal fixing, ULC/AMO recognition, unit-only SR consequences, and the
  low-overhead baseline that the integrated implementation should preserve.
  Source: https://arxiv.org/abs/2601.16855. SHA-256
  `b865a755d4e18a1dc60da6af4fda1d0359953cd5ec205cb291cd74c773d360cc`.
- `SimplifyOrderBreakRepeat-SAT2026.pdf` — Anders, Codel, Heule:
  *Simplify, Order, Break, Repeat*, SAT 2026, LIPIcs 377, article 4. DOI
  10.4230/LIPIcs.SAT.2026.4; CC-BY 4.0. Primary reference for the
  incremental Schreier-Sims chain, clique/AMO-aware ordering, binary and unit
  SR steps, simplification, and repeated symmetry detection. Source:
  https://doi.org/10.4230/LIPIcs.SAT.2026.4. SHA-256
  `2ee7589d916749b418d44148064b5afbf1c97a0c9f89d70f6e13acb080aa48fb`.
