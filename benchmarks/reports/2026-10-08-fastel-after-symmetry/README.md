# Startup FastEliminate after structured symmetry: mechanism or luck?

Question: does running startup FastEliminate (FE) after structured-root symmetry
breaking hurt search? Candidate c1 skipped FE whenever structured symmetry grew
the root trail. Candidate c2 always runs it. The two sources differ only in that
guard (`default_should_run_fast_elimination_after_structured`).

**Verdict: trajectory luck.** No hypothesis survives as a general mechanism.
Under renaming, both arms solve the same number of copies to within two, and no
row keeps a consistent winner. FE does delete a few symmetry helper binaries and
does change the first decision on noL rows, but neither effect has a consistent
sign. Nothing here supports changing the default.

## Setup

- Arms: `build/pin-candidate-c1-rel/tactix` (c1) and
  `build/pin-candidate-c2-rel/tactix` (c2). Diagnostic pins come from the c2
  source archive plus `symuse-v9.patch` plus `fesym-diagnostic.patch`, with
  `FESYM_MODE` set at compile time:
  - 0 = `on`, c2's behaviour: `build/pin-fesym-on`, `ddd1fc4f…`;
  - 1 = `off`, c1's guard: `pin-fesym-off`, `23d3456d…`;
  - 2 = `frz`, which freezes every variable that occurs in a tagged symmetry
    clause during startup FE: `pin-fesym-frz`, `59e5a961…`.

  The diagnostic patch prints FE's effect on tagged clauses, helper survival
  after each in-search Eliminate phase, and the first 512 decisions. It is a
  test instrument, not a proposal.
- Every solve went through `tools/bench/runtime_task.py`, as SLURM arrays at full
  width on `cpu-dense-hwulab-q`, `-c 8`, 32 GiB, `--auto-proof
  --default-graph-labeling=on`, 600 s. SAT models were checked.
- Run directories:
  - `benchmarks/runs/20261008-fesym-diag` (job 1756891);
  - `-shuffle` (1756849), `-shuffle-ext` (1756918) and `-shuffle-ext2` (1756968);
  - `-mp1-perf` (1756889; profiling exception, direct execution with scratch
    deleted);
  - `-build` (1756848).

  The shuffled inputs were made by the 2026-09-21 recipe, which renames
  variables, flips signs and shuffles clauses (seeds 1–30). They live in each
  campaign's `inputs/`.
- Fidelity check: each diagnostic arm reproduces its candidate's conflict count
  exactly:

  | row | diagnostic arm | candidate | conflicts |
  |---|---|---|---:|
  | noL-11-0 | off | c1 | 10,249,868 |
  | noL-11-10 | off | c1 | 1,434,581 |
  | mp1 | off | c1 | 638,214 |
  | noL-11-6 | on | c2 | 1,645,016 |
  | 6g_6color | on | c2 | 526,238 |

## Profiles: no pass runs long and produces nothing

All three profiled rows are search-bound in both arms, and FE itself takes
milliseconds:

| row | profile run | FE time | `kernel_propagate` share |
|---|---|---:|---|
| noL-11-0 | `20261007-nol110-perf` | 6 ms | 58% (c1) / 59% (c2) |
| noL-11-10 | `20261008-r401-perf` | 6 ms | 59% (c1) / 64% (c2) |
| mp1-Nb7T42 | `20261008-fesym-mp1-perf` | 0.22 s | 35% (c1) / 44% (c2) |

On mp1, c2 runs 14.5k conflicts/s against c1's 11.6k conflicts/s. Every
inprocessing function stays at or below 1.4% (children) in both arms. The gaps
are in the number of conflicts, not in unit cost.

## H1: FE erodes the symmetry-breaking binary layer

| row (original file) | symmetry output | helpers alive before → after FE | FE eliminated (in tagged clauses) | helpers after 1st in-search Eliminate, on / off | result on / off / frz |
|---|---|---|---|---|---|
| noL-11-0 | 1,089 helpers + 3 units | **0 → 0** | 26 (26) | 0 / 0 | TO / 288 s / 286 s |
| noL-11-10 | 1,067 helpers + 21 units | 1,041 → 995 | 27 (27) | 341 / 341 | TO / 40.1 s / 39.6 s |
| noL-11-6 | 1,079 helpers + 13 units | 1,065 → 1,021 | 26 (26) | 349 / 349 | 41.9 s / 320 s / 318 s |
| mp1-Nb7T42 | 1 root unit | — | 28,740 (0) | — | TO / 58.3 s / TO |
| 6g_6color_366_050_04 | 1 root unit | — | 14 (0) | — | 284 s / TO / 280 s |

H1 does not hold:

- **noL-11-0.** The post-structured BCE cleanup deletes 1,084 clauses in both
  arms before FE runs. All helpers are already gone, and FE removes one tagged
  clause (4 → 3). The c1/c2 gap on this row cannot come from symmetry output.
- **mp1, 6g_6color and snw_16_9.** The output is a single root unit, which FE
  cannot touch: 0 eliminated variables occur in tagged clauses. The c1 guard
  merely switched FE off on these rows. Their c1/c2 difference is the plain
  effect of FE, and its sign varies: c1 is better on mp1, while c2 is better on
  6g_6color and snw.
- **noL-11-6 and noL-11-10.** Every FE-eliminated variable is a helper endpoint,
  as expected: a unique-literal AMO pair is blocked on its pivot, and the pivot
  occurs once. FE still removes only 44–46 of about 1,050 helpers (4%). The first
  in-search Eliminate, at about 15k conflicts, takes both arms to the same
  population (341 / 341, 349 / 349).
- **Helper use per conflict** has no FE-consistent direction:

  | row | arm | propagations / conflict | analysis uses / conflict |
  |---|---|---:|---:|
  | noL-11-10 | on | 1.42 | 0.064 |
  | noL-11-10 | off | 1.49 | 0.052 |
  | noL-11-6 | on | 1.48 | 0.061 |
  | noL-11-6 | off | 1.38 | 0.082 |

- **The freeze arm cannot separate H1 from "FE off".** On noL, tagged variables
  cover essentially every active variable (`frozen_skipped` 1,046–1,085), so FE
  declines and `frz` reproduces `off` exactly. On mp1 and 6g_6color it
  reproduces `on`: identical conflicts on 6g, and the same timeout on mp1.

Because H1 does not hold, no fix follows from it.

## H2: FE changes the initial activation order

`decision-prefix.txt` compares the first 512 decisions on the original files.

- **noL rows.** The first decision differs immediately. `off` decides variable
  1419, the queue head, which `on` has eliminated. After FE-eliminated variables
  are filtered out of the `off` sequence, the two prefixes agree for only 10–28
  decisions, and 5–18 of `off`'s first 512 decisions fall on variables that FE
  eliminates.
- **mp1 and 6g_6color.** The first 512 decisions are identical in both arms.

So H2 is real as a perturbation: FE removes queue-head variables on noL. It is
not a direction. Renaming moves those variables, and the shuffle probe below
shows no consistent effect.

## H3: shuffle probe (luck vs mechanism)

PAR-2 below counts a timeout as 1,200 s.

| row | copies | c1 solved | c2 solved | c1 median | c2 median | c1 PAR-2 | c2 PAR-2 | per copy: c1 faster / c2 faster / both TO |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| noL-11-0 | 5 | 1 | 1 | TO | TO | 989 | 1,007 | 1 / 1 / 3 |
| noL-11-10 | 30 | 22 | 20 | 88 s | 133 s | 382 | 476 | 14 / 12 / 4 |
| noL-11-6 | 15 | 13 | 13 | 255 s | 182 s | 378 | 345 | 8 / 6 / 1 |
| mp1-Nb7T42 | 5 | 0 | 0 | TO | TO | 1,200 | 1,200 | 0 / 0 / 5 |

None of the original-file headlines survive renaming:

- **mp1.** c1 solved the original in 55 s against c2's 4,229 s, but solves 0 of
  5 renamed copies. At timeout, c1 reached 7.4–7.8M conflicts and c2 7.2–7.4M.
- **noL-11-0.** c1 solved the original in 277–309 s while c2 failed at 2,400 s.
  On copies the two arms tie at 1/5 each.
- **noL-11-6.** c2's 39.8 s against c1's 304 s becomes 13/15 for each arm.
- **noL-11-10.** The c1 lead on the first 15 copies (12 against 7) reversed on
  seeds 16–30 (10 against 13). Pooled, it is 22 against 20.

The two sibling rows noL-11-6 and noL-11-10 have the same structure, but on the
original files they point in opposite directions.

## Conclusion

There is no general mechanism by which startup FE after structured symmetry
breaking hurts search:

- FE's deterministic footprint on symmetry output is small. It removes 0 helpers
  on noL-11-0 and 4% on noL-11-6/10. That footprint is erased by the first
  in-search Eliminate in both arms.
- FE's effect on the first decisions is a perturbation without a sign.

The rows that motivated the question are single-trajectory draws. Startup FE
stays on, as decided 2026-10-07 (`2026-10-07-fastel-suppression-removal`).

Limitations: 600 s cap, 5 copies for mp1 and noL-11-0 (both arms mostly time
out there), one CPU model.

Files: `evidence.json` (per-run counters and hashes), `shuffle-table.txt`,
`decision-prefix.txt`, `fesym-diagnostic.patch`.
