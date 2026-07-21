# BVE structural-skip feasibility — NEGATIVE, the static predicate is dead (2026-07-20)

Frozen report. Follows `2026-07-20-bve-tierC-ab.md` (BVE stays ON; the remaining
lever was a policy-slot skip on the harmful structure). This study asks: **does a
cheap static CNF feature separate the BVE-harmful from the BVE-helpful instances?**
If yes, gate BVE on it. Answer: **no.**

## Method

Labels from the Tier B + C on/off A/B (harmful = BVE slower / loses a solve;
helpful = BVE faster / enables a solve). Per-instance static features from one CNF
parse (no solver): variable occurrence-degree coefficient of variation (`deg_cv`),
binary-implication-degree CV (`bindeg_cv`), binary-clause fraction (`binfrac`),
at-most-one fraction among binaries (`amofrac`), clauses/vars, a degree-uniformity
proxy. Then: best single-feature threshold, by min misclassifications over 13
labelled instances (7 harmful, 6 helpful).

## Result: no separator

| feature | best split, misclassifications |
|---|---|
| deg_cv | 5 / 13 |
| binfrac | 4 / 13 |
| amofrac | 5 / 13 |
| bindeg_cv | 4 / 13 |

Best is 4/13 (~31 % error) — chance is ~6/13, a usable gate would need ~0–1. On
every feature the harmful and helpful values fully interleave.

**The killer pair** — structurally indistinguishable, opposite BVE effect:

| instance | label | deg_cv | binfrac | amofrac | bindeg_cv |
|---|---|---|---|---|---|
| clique_n2_k10 | **harmful** (38.7×) | 0.000 | 0.997 | 1.000 | 0.000 |
| qwh.50.1250 | **helpful** (2.3×) | 0.156 | 0.989 | 0.254 | 0.168 |

Both are dense, near-uniform-degree, high-binary cardinality formulas. No
combination of these features can put them on opposite sides. Likewise mod4block
(helpful) is cardinality-heavy (`amofrac` 0.978) like clique; hwmcc (harmful) is
maximally heterogeneous (`deg_cv` 8.2). SAT/UNSAT also fails to split (rbsat SAT
harmful, qwh SAT helpful).

## Conclusion + routing

A **static** BVE-skip predicate is not achievable — the harm is dynamic (how
partial elimination reshapes the *search tree*), not a property of the CNF's
static structure. This closes the skip-predicate lever from
`2026-07-20-bve-tierC-ab.md`. Two directions survive, both out of near-term scope:

1. **Dynamic try-both** — run a short bounded search with and without the BVE
   round and keep the better formula. This is real adaptivity (what the
   kissat-family gets from interleaving elimination with search), but it ~doubles
   preprocessing cost and needs its own stability work.
2. **Learned scheduling** — BVE-on/off is exactly the kind of per-instance rule
   decision the plan's RL endgame (§5.6) is meant to make from features a static
   threshold can't combine. This is the natural home for the decision.

**Action taken: none on the default** — BVE stays ON (net-positive on Tier C).
The lever is parked, not patched. Next performance work redirects to the Factor
S2 wide-grid matching (verified premise, `2026-07-20` metric check) which is
independent of BVE.
