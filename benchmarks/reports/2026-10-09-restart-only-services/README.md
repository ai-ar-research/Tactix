# Restart-only retained root services (persistent affine search, graph labeling)

User decision (2026-10-09): the persistent affine (XOR / Gauss–Jordan) search service and graph
labeling (Lucky 26/27) must never force a restart; they run only after a natural full restart. The
baseline is c2.3 = `555ecb4b0` (Release `ae4a26e4…`, `build/pin-candidate-c2.3-rel`).

| Commit | Content | Release |
|---|---|---|
| `d453dfbb6` | No forced restarts; affine trigger narrowed to its retained support; Gauss–Jordan (from `f39f59246`); consumption observer removed | `a4e46b78…` (`build/pin-restart-c1-rel`) |
| `d5ea15100` | Kissat conflict-limit + effort schedule for both services | `72f1bede…` (`build/pin-restart-c2-rel`) |
| `b2ff44d6f` | Gauss–Jordan scoped to persistent sessions (fixes a `d453dfbb6` regression, below) | `670bc766…` (pin deleted; campaign copies in `pins/`) |
| `3361d3b51` | Graph labeling's run dose follows its owner, LymphoSAT (main-session N4 decision), not the walk share | `57bdd51c…` = `build/pin-restart-c4-rel` (**c2.4 gate pin**) |

The root-unit visibility fix `3149de667` was already in the tree (c2.1 cherry-picked it; its
patch reverse-applies cleanly), so nothing was ported for it.

## What changed

- **Restart hook.** `keep` comes from the ordinary reuse level only when the restart clock fires.
  When the restart reaches level 0 (`keep == 0`), the hook propagates the root and offers the two
  services; neither requests a restart. Learned-unit backjumps never run them.
- **Affine trigger.** A window runs only if the retained inputs changed since the last run
  (`affine_matrix_search_inputs_changed`, from `ad8434f1d`): a root fact on the admitted support,
  a retired support variable, an owner or premise-lease change, or an unfinished slice. Root facts
  off the support never trigger. The tick timer and the per-propagation
  `observe_affine_consumption` observer, with its reason-ID tables, are gone.
- **Gauss–Jordan (`f39f59246`).** Every sync path of a persistent session (full, sliced, dirty)
  keeps a reduced row-echelon basis. The startup census runs after preprocessing. BVE is not
  offered the admitted support of a persistent session.
- **Schedule (`d5ea15100`).** Both services share `DefaultRootServiceSchedule`:
  - **When:** at `conflicts + kissat_scale_delta(eliminateint · n · log10(n+9)²)` (eliminate's
    NLOG2N limit). The delay halves after a fruitful run and doubles after a fruitless one, and a
    delayed occurrence still advances n. Affine search also waits at an expired limit until its
    inputs change.
  - **How much:** `SET_EFFORT_LIMIT`, a per-mille share of the search ticks since the service's
    last run, floored at mineffort. Affine search gets the sweep share (100‰); graph labeling gets
    the walk share (50‰), capped by its serialized Window dose.
  - **Fruitful** means the run published output. A run cut by its effort limit without output
    defers the verdict.
  - **Removed:** the affine quantum × (delay+1) timer and graph labeling's
    `initial_tick_interval` (1M), `maximum_delay_ticks` (64M) and `fruitlessness_threshold` (4).
- **Scope fix (`b2ff44d6f`).** `MatrixLimits::reduced_row_echelon` is set only for persistent
  sessions. The non-persistent startup root closure (`affine_root_closure`, on by default) keeps
  c2.3's forward-echelon basis.

Plan §9.5 ("At initial/full restart roots only … tick/interval/fruitlessness-delay mechanisms")
already matches this design, so the plan text is unchanged.

## Validation

All runs used SLURM on `cpu-dense-hwulab-q` (8 CPUs, 32 GiB) through `runtime_task`, with
automatic certificates in anonymous scratch. Campaigns are in `benchmarks/runs/20261009-restart-rework-*`.

**Full Debug.** 358/358 passed on `d453dfbb6` (job 1763241) and 358/358 on `b2ff44d6f`
(job 1763283).

**Identity, 12 rows, 600 s.** Arms: c2.3 `--auto-proof`, new `--auto-proof`, and new
`--auto-proof --default-affine-search=on`. "Identical" means the same verdict, conflicts, decisions
and certificate SHA-256.

| Row | `d453dfbb6` | `d5ea15100` | `b2ff44d6f` (both new arms) |
|---|---|---|---|
| 10 rows without graph authentication (ramsey_4_4_19, mp1-Nb7T42, battleship, oddball, cliquecolouring, 9vliw, crypto-wff, sted5, noL-11-16, b18) | identical | identical | identical |
| lightsout (rank 263) | **UNKNOWN 600 s** (c2.3: UNSAT 0.6 s) | **UNKNOWN 600 s** | identical (UNSAT 0.7 s, cert `6fbc0514…`) |
| abw-K-dwt (rank 991, graph labeling authenticated) | SAT 33.8 s, verified (c2.3: 20.9 s) | UNKNOWN 600 s | UNKNOWN 600 s |

- **lightsout:** `f39f59246` applied Gauss–Jordan to every `AffineMatrix`, including the default
  startup root closure (12k-clause cap) that refutes lightsout. `b2ff44d6f` scopes it to persistent
  sessions.
- **Affine on:** with `--default-affine-search=on`, mp1, battleship, 9vliw, crypto, sted5 and
  noL admit a matrix (1 initialization) but run no restart window. Every row is identical to
  c2.3 except abw.

**Fixed44 closures, 5,000 s, final binary.** Arms: c2.3 in `…-c1-closures`, and `d453dfbb6`,
`d5ea15100` and `b2ff44d6f`.
- **Identical to c2.3 in every arm** (same conflicts, decisions and certificate SHA): REGRandom-K4
  (226), oisc (489), sokoban-p16 (511), crafted num9 (541), openstacks (627), VexRiscv (1075),
  crafted num8 (1129), ramsey_4_4_18 (147) and mp1-Nb7T42 (899). None authenticates graph
  labeling. oisc's certificate is the known never-verifying one; openstacks, VexRiscv, ramsey and
  mp1 are verified. The other checks were still running at handoff, but their certificates are
  byte-identical to c2.3's.
- **snw_16_9_Encpre (497):** times out at 5,000 s in every arm, including c2.3.
- **abw-K-dwt (991):** SAT in **4,826.7 s** with the final binary, model verified. c2.3 takes
  20.9 s and `d453dfbb6` takes 33.8 s.

**abw profile.** Graph labeling follows the same trajectory as c2.3: 22,626,615 nodes, 1,434
repairs, 4.8M anneal attempts and about 5.064e9 work, ending in the same SAT model.
- c2.3 finishes that work after 5.4e7 search ticks. The final binary needs 2.05e11 search ticks
  (104.8M conflicts, 71 slices, effort total 6.48e9).
- The cost is the 50‰ effort share, not a different trajectory or a long pass. Graph work is cheap
  per unit (about 2.7e8 work/s against about 4.2e7 search ticks/s, from these two runs), so 5% of
  ticks gives graph labeling well under 1% of wall time. In c2.3 it took about 19 of abw's 21 s.
- With `d453dfbb6` alone (restart-only, old tick schedule), abw still takes 33.8 s. Waiting for
  natural full restarts costs only 13 s.

**lec_mult, 5,000 s.** Proof checks for all four solved runs, and for c2.3's, fail with dsr-trim
`Ran out of memory on xrealloc()` at 32 GiB: NOT VERIFIED (a 64 GiB recheck is the existing
Tier-D route).

| Rank | c2.3 | Final, service off | Final, affine on |
|---|---|---|---|
| 569 CvW_12x12 | UNSAT 3,597 s | UNSAT 3,456 s, identical conflicts/decisions/cert | UNSAT 2,705 s; rank 293 retained, 4 restart runs, 0 published reasons, delay 16 |
| 1004 DvW_12x12 | UNKNOWN 5,000 s (also Tier D c2.3) | UNSAT 4,971 s* | UNSAT 3,986 s; rank 298 retained, 5 restart runs, 0 published reasons, 34 delayed occurrences, delay 32 |

\* The code path matches c2.3: no graph authentication, and the root closure is forward-echelon. c2.3
was cut at 5,000.0 s, so identity cannot be shown from a timeout.

With the service on, Gauss–Jordan keeps rank 293/298 for the whole solve (the purged report saw
it decay to 0). But every run is fruitless: there are no published reasons, and outside the
refresh maintenance only one Gaussian step runs in the whole solve. The affine-on speedups therefore come from admission side effects (census placement,
BVE skipping the support), not from Gaussian output.

## Owner-dosed graph labeling (`3361d3b51`)

The main session decided (N4) that graph labeling's run dose must follow its owner. LymphoSAT's
dispatcher runs the matched antibandwidth solver to completion before its CDCL fallback
(`winners/lymphosat-sc2026/green/lymphosat/src/solver/core/main.cpp`,
`solvers/antibandwidth/solver/meta.json`).
- The first run is in the initial root window.
- Each run continues the trajectory slice by slice within the serialized stage doses until it
  completes, finds a model or is interrupted.
- Later runs stay at natural full restarts under the Eliminate conflict limit, with the delay
  doubling after a fruitless run (no model) and halving after a fruitful one.
- The 50‰ walk share is removed. Affine search keeps the Kissat sweep share.

**Validation (Release `57bdd51c…`):**
- **Full Debug:** 358/358 (job 1763339).
- **Identity (12 rows × 3 arms):** the 10 rows without graph labeling plus lightsout are identical
  to c2.3 in both new arms (verdict, conflicts, decisions, certificate SHA), with graph runs = 0.
- **abw-K-dwt:** SAT in **21.3 s** (affine-on arm 21.6 s; c2.3 21.1 s), model verified.
  - One graph-labeling run of 56 slices before search does about 5.064e9 work and finds the
    model with 0 conflicts. c2.3 needed 8,935 conflicts, because its slices sat between forced
    restarts.
  - The certificate differs from c2.3 (`1026936d…`) because the model is now found before search.
- **Closures:** not rerun. Graph labeling authenticates only abw among the fixed44 closure rows
  (`authentication_successes=0` on every other row in the earlier campaigns), so the change cannot
  reach them.

## lec_mult 64 GiB proof rechecks

These use `tools/bench/experimental_tierd.py`, the Tier-D home-certificate route: hwulab, 8 CPUs,
64 GiB, 5,000 s solve, 28,800 s check, long64-fixed dsr-trim, then Trestle. Certificates are kept
on the home quota under the governor. Campaigns are `benchmarks/runs/20261009-restart-rework-lec64-*`.

| Arm | 569 | 1004 |
|---|---|---|
| c2.3 | UNSAT 3,575 s, cert `9dd8775c…` (same as before): verified | (unsolved by c2.3) |
| `3361d3b51` default | UNSAT 3,573 s, cert `9dd8775c…` (identical to c2.3): verified | UNKNOWN at 5,000 s this time (4,971 s before); the 7,200 s re-solve (`…-lec64-new1004`) gives UNSAT 5,423 s: verified |
| `3361d3b51` affine on | UNSAT 2,732 s, cert `f7689d01…` (identical to the `b2ff44d6f` run): verified | UNSAT 4,102 s, cert `a827670c…` (identical to the `b2ff44d6f` run): verified |

## Open for the main session

- **Gates.** The four §8.4 gates (candidate c2.4, pin `build/pin-restart-c4-rel`, commit
  `3361d3b51`) have not been run.
