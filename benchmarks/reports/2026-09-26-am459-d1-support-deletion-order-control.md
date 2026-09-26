# AM459: D1's first unit depends on every deferred deletion

Date: 2026-09-26. This is a checker-only control on the frozen AM455 D1
29-variable, 27-clause public Probe→CCE handoff. It changes no solver,
public action, proof emitter, checker, or canonical contract. D1/N2 remain open.

The original ASCII DSR starts with `-14 0`, then ten deferred deletions.
The later general SR addition fails against the no-option checker, whose
root closure retains a reason that the kernel deleted. AM448 showed that a
selective early deletion order can rescue bounded H2. Here, we enumerated
all `2^10 = 1,024` subsets of moving D1's ten existing deletion lines
immediately before `-14`, preserving their internal order and leaving every
other proof line unchanged. Each candidate was checked forward by frozen
AM369 `dsr-trim -f -V` against the exact original CNF.

| Early deletions | Count | First checker failure |
|---|---:|---|
| None | 1 | Addition 13: no UP contradiction for RAT clause 14, the original later SR failure |
| Any nonempty subset | 1,023 | Addition 1: no UP contradiction for RAT clause 4, the first `-14` unit |

There is no forward-valid member of this bounded order family, hence no
candidate for the required forward/backward LSR→Trestle checks. The result
shows that the ten deferred deletions all participate in the checker-visible
derivation of the first unit, at least under this unmodified prefix. A
simple rule that moves a selected support deletion ahead of `-14` cannot
repair D1. A general canonical repair needs a different proof derivation or
a dependency-aware transaction that supplies equivalent support without
leaving a later SR witness inconsistent with checker-retained roots. This
does not prove that every DSR derivation or checker-visible transaction is
impossible; no checker-option change or X7 restriction follows from it.

As a separate checker-only countercheck, the archived DRAT-mode proof from
the same public program, `min-rel.drat.txt`, is valid DSR ASCII syntax. Frozen
no-option `dsr-trim` accepts it in both forward and backward directions, and
pinned Trestle verifies each resulting LSR against the original CNF. That
proof contains `6 0; 0` after the deletions and has no SR transactions. This
shows the canonical checker can certify this UNSAT formula through a different
search trajectory; it does **not** certify the three accepted X7 SR additions
of the failing DSR-mode run or supply a general emitter repair. The legal
public X7 surface remains unchanged.

Reproduce with `python3 benchmarks/probes/scratch/am458-d1-order/sweep.py`.
Recheck the DRAT-mode stream under the canonical no-option DSR/Trestle route
with `bash benchmarks/probes/scratch/am458-d1-order/check-witnessless.sh`.
The input is `benchmarks/runs/20260925-am455-latest-combined-claude-fuzz/handoffs/D1-dsr-probe-cce-unit-rejection/min.cnf`
(SHA-256 `3cc012ce...`), the proof is adjacent `min-rel.dsr.txt`
(`76ca9589...`), and the checker is
`benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim`
(`3b5c7737...`). The script prints `forward 0`, `both 0`, and the exact
failure histogram above. It writes candidates only under ignored scratch.
