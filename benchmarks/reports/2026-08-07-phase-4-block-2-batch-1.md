# Phase 4 · block 2 — pricing batches 1 and 1b (Tier B, 300 s)

**Date:** 2026-08-07 · **Commit:** `d4db510` · **Windows:**
`benchmarks/runs/20260807-151657-B-shipped` (batch 1, array `1588654`, 9 arms × 28) and
`benchmarks/runs/20260807-152618-B-shipped` (batch 1b, array `1588909`, 16 arms × 28), both
`cpu-dense-hwulab-q`, 1 core / 8 GB per task, two pin dirs built from the same commit with matching binary
SHAs. Criteria were fixed **before** either ran, in
[`2026-08-07-phase-4-block-2-preregistration.md`](2026-08-07-phase-4-block-2-preregistration.md); nothing
below restates one after seeing a number.

**The one-line result.** `R9` is real, mechanism-specific and replicated — **`veer_axi…p23_transition`
is solved by exactly the two arms that attack HyperBinaryRes's cost, and by none of the other twelve**.
Everything else that looked like a win is four instances sitting on the 300 s cliff: **9 of 14 arms in batch 1b
"gain solves", including arms that move opposite mechanisms in opposite directions**, which is why the
replicate was pre-registered and what it was for.

## 1. The window

**In-window noise bar: `R` = 0.55 PAR-2** (`shipped` 134.67 vs the bit-identical `shipped2` 134.12, both
23/28, largest single mover `b22_1` −7.7 s). Every criterion below therefore reads against
`max(R, 1.39) = 1.39`. `correctness.clean = true`: no verdict mismatch, no invalid certificate, no arm scored
on an unverified proof.

| arm | PAR-2 | solved | ΔPAR-2 vs shipped | verdict |
|---|---|---|---|---|
| `shipped` | 134.67 | 23/28 | — | the post-landing baseline |
| `shipped2` (replicate) | 134.12 | 23/28 | −0.55 | the noise bar |
| **`r9off`** `TACTIX_SLOT_DISABLE=HyperBinaryRes` | **101.76** | **25/28** | **−32.91** | **SHIP-ARM** (clause 1) |
| **`n1glueoff`** `TACTIX_SHRINK_GLUE=0` | **89.64** | **25/28** | **−45.04** | **SHIP-ARM, pending its replicate** |
| **`n2tumbleoff`** `TACTIX_TUMBLE=0` | **104.61** | **25/28** | **−30.06** | **SHIP-ARM, pending its replicate** |
| `e3ceiloff` `TACTIX_EFFORT_CEIL=0` | 134.08 | 23/28 | −0.60 | **KEEP-SHIPPED** |
| `d3nowm` `TACTIX_SLOT_WATERMARK=0` | 134.23 | 23/28 | −0.44 | **KEEP-SHIPPED** |
| kissat | 10.21 | 28/28 | — | reference |
| cadical | 40.86 | 27/28 | — | reference |
| *oracle over the 7 scored arms* | *64.24* | *26/28* | — | *not a runnable policy* |

**#solved before PAR-2, and the instances by name.** All three gains are on instances the shipped
configuration times out on, and **no arm loses a solve**:

| instance | shipped | `r9off` | `n1glueoff` | `n2tumbleoff` | kissat |
|---|---|---|---|---|---|
| `veer_axi…p23_transition` | UNKNOWN 300 s | **UNSAT 20.4 s, proof VERIFIED** | UNKNOWN | UNKNOWN | UNSAT 3 s |
| `mp1-klieber2017s-0500-023-t12` | UNKNOWN 300 s | SAT 239.2 s | SAT 123.5 s | SAT 289.9 s | SAT 13 s |
| `mod4block…autoenc-sc2009` | UNKNOWN 300 s | UNKNOWN | SAT 29.7 s | SAT 197.7 s | SAT 22 s |
| `or_randxor_k3_n540_m540` | UNKNOWN | UNKNOWN | UNKNOWN | UNKNOWN | UNSAT 12 s |
| `unsat-set-b-fclqcolor…` | UNKNOWN | UNKNOWN | UNKNOWN | UNKNOWN | UNSAT 16 s |

## 2. `R9` — the row that was going to cost a worktree solves `veer_axi` in 20 seconds

`veer_axi…p23_transition` is the sharpest single target in the project: kissat takes 3 s, we took 300 s and
printed `c conflicts 0`, i.e. **the solve never reached search**. Block 1 stack-sampled it into
`hyperbinary_try → congruence_rup_chain → kernel_propagate_probing`. This window closes that loop with three
independent readings that agree:

- **Counters (job `1588542`, shipped config, 300 s):** `c hbr 0 rounds, 4 probes, 32,125 binaries,
  6,798,135,271 ticks` against `global ticks 6,800,905,567`. **Four probes consumed 99.96 % of the solve** —
  ~1.7e9 ticks *per probe*.
- **This window:** with the entry disabled, **UNSAT in 20.4 s with a `drat-trim`-VERIFIED certificate**, and
  `mp1-klieber` converts too. +2 solves, none lost, −32.91 PAR-2.
- **A third arm reaches the same place from the other side** (§4).

**The verdict is SCHEDULING, not retirement.** Every implemented proof rule ships ON (standing user
baseline), and the pre-registration said so before the number existed: the follow-up is a dose or a guard,
never deleting X6. Two things the arm cannot tell us, both recorded now rather than after the next window:

1. **It conflates `collect` with `emit`.** `TACTIX_SLOT_DISABLE` removes the probing propagation *and* the
   RUP replay. The stack samples sit in `congruence_rup_chain`, which is `emit`'s side, so the discriminating
   experiment is a resolvent-only suppression — a one-line guard at `hyperbinary.cpp:163` that does not exist
   yet.
2. **The cost is not all in the ledger.** `hyperbinary_try` allocates `std::vector<Lit> parent(k.nvars,
   INVALID_LIT)` per call (`hyperbinary.cpp:202`) — the per-call linear-scratch hazard this project has hit
   four times — and `hbr_ticks` cannot see it, because only `kernel_charge_ticks` feeds the sink.

**The shape of the defect is one we have fixed before.** Sweep had exactly this: an effort ceiling that was
never enforced *where the work is*, so one seed outspent its round dose 73× and the scan was uninterruptible
(`2026-08-05-sweep-governor-diagnosis.md`). Four HBR probes spending 1.7e9 ticks each is the same picture:
the dose is checked between probes and the probe itself is unbounded.

## 3. `N1` and `N2` — the two N4 transliterations that landed this morning both measure NEGATIVE

Both fire SHIP-ARM by the letter of their criteria (+2 solves each, nothing lost). **Neither is acted on from
this window**, and the reason is in the data rather than in hindsight:

- **All three broad-perturbation arms gain the same instance, `mp1-klieber`** (at 123.5 s, 239.2 s and
  289.9 s — the last one 10 s inside the cap), while the two near-inert arms (`e3ceiloff`, `d3nowm`, which
  the counters prove bit-identical on 23 of 28) gain nothing. That is the signature of an instance that flips
  under *any* trajectory perturbation, not of three mechanisms that each independently help.
- **The other gain is `mod4block` in both cases**, which is in the may-not-gate set and is a measured lottery
  for both solvers. Discounting it leaves +1 solve each.
- **`N2`'s own pre-registered criterion demands a replicated distribution**, because it changes the entire
  pre-first-conflict sequence and every tie-break by construction. The same reasoning applies to `N1`.

**The replicate answered it: NO.** In batch 1b both arms gain exactly `mod4block` + `mp1-klieber` again — and
so do `c6eagoff`, `f1touchoff` and `f67bandon`, three arms with no mechanism in common with them or with each
other (one turns eager subsumption OFF, one turns the touched-epoch filter OFF, one turns the redundant band
ON). **With the two cliff instances excluded, `n1glueoff` and `n2tumbleoff` gain nothing** (23/26, like every
other arm except `r9off`). Neither is acted on: the transliterated default stands under decision (7215698), and the
apparent +2 was the tier's cliff, not the clamp or the decision order.

**One correction to the anchor while we are here.** The pre-landing headline was 103.75 PAR-2, 24/28 at
`edfba14`; this window reads 134.67, 23/28 at `d4db510`. Those are different windows and the absolute may not
be compared across them — but the solve count can be, and the instance that differs is **`mod4block`**, the
tier's lottery. So the landings did not demonstrably cost a solve; the tier's own coin did.

## 4. `C9` (`jumpreasons`) reaches `veer_axi` from the other side — and its certificate holds

The arm-side reach probe (job `1588570`), run to check that a ships-OFF row was not vacuous, produced an
unrelated and much larger finding. On `veer_axi`, at the same binary and cap, with three same-job controls
timing out:

| | `c hbr` probes | `c hbr` ticks | global ticks | conflicts | verdict |
|---|---|---|---|---|---|
| shipped | 4 | 6,798,135,271 | 6,800,905,567 | 0 | UNKNOWN |
| `TACTIX_JUMP_REASONS=1` | 27,731 | 7,181,987 | 112,109,250 | 42,852 | UNSAT |

**HBR's cost per probe collapses from ~1.7e9 ticks to ~259**, and the instance solves. Reason jumping changes
the binary-reason structure that `congruence_rup_chain` replays, which is the same site the stack samples and
`r9off` both point at — so `C9` and `R9` are two views of one defect, not two independent wins.

**The N2 risk that row carried is discharged on this instance** (job `1588908`, both formats):
`drat-trim s VERIFIED` (61.7 s over 1,891,427 lemmas, 0 RAT lemmas in core) and `cake_lpr s VERIFIED UNSAT`
on a 160.9 MB native-LRAT proof. This was the first tier-scale exercise of the jumped-reason antecedent
expansion (`chain_expand_jumped`), and it holds.

`c9jump` is priced properly in batch 1b, where its proof is checked by the runner rather than by a probe.

## 5. `E3` and `D3` — both answered, both KEEP-SHIPPED, and both cheap because the counters came first

Neither converted a named instance, and both deltas (−0.60, −0.44) are inside `R`-scaled noise. Under their
pre-registered criteria that is KEEP-SHIPPED, and for `E3` a wash resolves that way anyway because the
ceiling is a cadical transliteration.

**What made them cheap is worth keeping.** The tier-wide counters probe proved beforehand that `E3` is
bit-identical on 23 of 28 instances and `D3` on 23 of 28, so both rows cost one arm slot each to answer a
question that would otherwise have looked like a tier-wide null. The morning's 8-instance sample had retired
both for the opposite reason; the whole tier put them back and then answered them.

## 6. Batch 1b — and the reason Tier B at 300 s cannot price a broad scheduling arm

16 arms, in-window noise bar **0.01 PAR-2** (`shipped` 135.55 vs `shipped2` 135.54, both 23/28) — the tightest
replicate this project has measured. And yet:

| arm | PAR-2 | solved | gained | PAR-2 **excluding the two cliff instances** | solved ex-2 |
|---|---|---|---|---|---|
| `shipped` | 135.55 | 23/28 | — | 99.83 | 23/26 |
| `shipped2` | 135.54 | 23/28 | — | 99.81 | 23/26 |
| **`r9off`** | 104.30 | 25/28 | **`veer_axi`** + klieber | **79.46 (−20.36)** | **24/26** |
| **`c9jump`** | 98.59 | 25/28 | **`veer_axi`** + mod4block + klieber, **LOSES `DecompressReader_read_12`** | 98.73 (−1.10) | 23/26 |
| `n1glueoff` | 90.48 | 25/28 | mod4block + klieber | 90.97 | 23/26 |
| `n2tumbleoff` | 106.32 | 25/28 | mod4block + klieber | 95.23 | 23/26 |
| `c6eagoff` | 94.17 | 25/28 | mod4block + klieber | 90.05 | 23/26 |
| `f1touchoff` | 96.07 | 25/28 | mod4block + klieber | 94.75 | 23/26 |
| `f67bandon` | 100.66 | 25/28 | mod4block + klieber | 90.76 | 23/26 |
| `c7randecoff` | 109.67 | 24/28 | mod4block | 94.10 | 23/26 |
| `d1legacy` | 106.98 | 24/28 | mod4block | 85.74 | 23/26 |
| `g3reuseoff` | 107.81 | 24/28 | mod4block | 90.83 | 23/26 |
| `d2oneclock` | 125.97 | 23/28 | — | 89.50 | 23/26 |
| `e1cursoff` | 129.13 | 23/28 | — | 92.90 | 23/26 |
| `g4y1off` | 134.12 | 23/28 | — | 98.28 | 23/26 |

**How many arms solve an instance that `shipped` does not:** `mod4block` **9 of 14**, `mp1-klieber` **7 of 14**,
`veer_axi` **2 of 14** — and those two are `r9off` and `c9jump`. That ratio is the whole finding. An instance
solved by nine unrelated arms is reporting on the 300 s cap, not on the arms.

**The per-instance noise band, measured in-window rather than assumed:** the *bit-identical* pair differs by
**18.4 s on `sted5_0x24204-50`** and 16.4 s on `DivS_862_11`. Against that band, the arms' movers are
overwhelmingly the same handful of instances regardless of mechanism — `sted5` moves −37 s to −248 s under
**thirteen of fourteen arms**, including `g4y1off`, whose entire mechanism is a binary-resolution unit sweep
that fires on 10 of 28 instances and cannot plausibly be worth 37 s there.

**So Tier B at 300 s has four perturbation-sensitive instances — `mod4block`, `mp1-klieber`, `sted5`,
`2013113162201nw` — and they dominate every aggregate on this tier.** Consequences, all of which bind the
rest of phase 4:

1. **A Tier B `#solved` criterion has an effective resolution of about ±2 solves**, so `SHIP-ARM iff
   solved(arm) > solved(shipped)` — the rule pre-registered for every row — fires for 9 of 14 arms and
   discriminates nothing. It was still the right rule to write down: it is what made the failure visible
   instead of arguable.
2. **The broad-and-shallow rows are NOT priced by these two windows** and must go to Tier C, where the cap is
   1200 s and these instances are not at their cliff, or to replicated Tier B draws. Their ex-cliff deltas
   (−1.10 to −14.09 PAR-2) sit inside a band that a bit-identical pair spans on two instances alone.
3. **`mp1-klieber` and `sted5` behave exactly like the may-not-gate set.** Growing that set is a user
   decision and this report does not take it — it records the measurement and flags it.

**`c9jump` carries a cost the aggregate hides.** It loses `DecompressReader_read_12` outright (+299 s to a
timeout), and its proofs are **1026.5 MB against 336.3 MB** with checking at 607.6 s against 238.2 s — a 3×
certificate-size tax that plan §8.3 counts as a first-class metric. It is not a free win, and it is not
shippable on Tier B evidence.

## 7. What these windows do NOT say

- They do not price `C8`, `R10`, `F2`, `E2` or `S-g` — see the pre-registration for why each was excluded.
- They do not establish that disabling `HyperBinaryRes` is a shippable configuration. They establish that
  **HBR's per-probe cost is unbounded on at least one instance**, which is a dosing defect with a named fix,
  and that the effect is mechanism-specific rather than a cliff flip (2 of 14 arms, not 9 of 14).
- **They do not price the broad rows at all** (§6). `C6`, `C7`, `D1`, `D2`, `E1`, `F1`, `F6/F7`, `G3`, `G4`,
  `N1`, `N2` come out of these windows UNDECIDED, not negative and not positive: their whole ex-cliff range is
  inside what a bit-identical pair spans on two instances. They need Tier C or replicated draws.
- `c9jump` is not settled either way: it fixes `veer_axi`, loses `DecompressReader_read_12`, and triples proof
  size.
- Two draws, not a distribution. `mp1-klieber` at 289.9 s against a 300 s cap is not a stable solve, and
  `sted5` at 250.0 s is one bad draw from becoming a fifth cliff instance.

## 8. What to do next, in order

1. **Fix HBR's dose where the work is.** The tier-wide decomposition (job `1588542`, `c hbr` against
   `c meters … global ticks`) says the defect is not "HBR is expensive" but **"one HBR instance is
   indivisible and can cost a whole solve"**:

   | instance | probes | binaries | hbr ticks | **ticks / probe** | ticks / binary | % of solve |
   |---|---|---|---|---|---|---|
   | **`veer_axi…p23_transition`** | **4** | 32,125 | 6,798,135,271 | **1,699,533,817** | 211,615 | **100.0 %** |
   | `summle_X4044…` | 117 | 13,518 | 80,516,544 | 688,175 | 5,956 | 14.1 % |
   | `2013113162201nw` | 4,427 | 17,373 | 235,679,509 | 53,236 | 13,565 | 6.2 % |
   | `PRP_30_38` | 3,324 | 11,624 | 209,540,467 | 63,038 | 18,026 | 40.5 % |
   | `FmlaEquivChain_4_6_6` | 58,353 | 2,564 | 26,669,514 | 457 | 10,401 | 4.7 % |

   **`veer_axi`'s per-probe cost is 2,470× the tier median and 2.5× the next worst.** One root yields ~8,031
   resolvents, and every one of them costs a `congruence_replay_rup` — a full probing propagation plus its
   backtrack — at ~212k ticks each. The entry is dosed at 20 ‰, but its smallest indivisible unit of work is
   ~25 % of the tick budget of an entire 300 s solve, so the dose cannot bind. **That is Sweep's `S-e`
   governor defect exactly** (`2026-08-05-sweep-governor-diagnosis.md`): the ceiling is polled at a boundary
   the work never reaches.

   **The obstacle is in the policy, not the rule, and it should be read before designing the fix.**
   `HyperBinaryRes`'s `EffortSpec` row is `dosed=true, budget_aware=false`
   (`default_policy.cpp:328`), and the policy hands `k.work_budget` only when
   `e.meter && !e.dosed && e.budget_aware` (`:1564`) — **a dosed entry is never given a budget at all.** So
   the ten rules that poll `k.work_budget` inside their loops (`ternary.cpp:247`, `bce.cpp:265`,
   `cce.cpp:386`, `instantiate.cpp:270`, `vivify.cpp:519`, `factor.cpp:511`, `sweep.cpp:880`, …) are all
   non-dosed, and HBR cannot join them without either un-dosing it or letting a dosed entry carry a budget.
   `hyperbinary_try` already polls `kernel_rule_interrupt` in its resolvent loop (`hyperbinary.cpp:209-218`,
   from `288c5f1`) — that is a **termination** poll, not an effort poll, and it is why the instance now
   prints its stats at the cap instead of ignoring SIGTERM.

   **No new constant is needed and none should be invented**: the budget already exists in the ledger, and the
   bug is where it is checked. Note also that a per-resolvent RUP replay is *our* design, not the reference's —
   kissat derives hyper-binary resolvents during the probing propagation itself — so "bound the work" and
   "stop replaying per resolvent" are two different fixes and the second is the one the reference backs.
2. **Add the resolvent-only guard** (`hyperbinary.cpp:163`) so `collect` and `emit` can be told apart; the
   stack samples say `emit`, `r9off` cannot confirm it, and one guard settles it.
3. **Re-home the broad rows to Tier C.** Tier B at 300 s is the wrong instrument for them, which these two
   windows establish at a cost of one afternoon rather than a phase.
4. **Then re-read `c9jump`** on Tier C with its proof-size tax measured, since it is the only other arm that
   touches `veer_axi`.
