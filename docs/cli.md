# Command-line reference

This is the complete reference for the `tactix` command-line solver and the
example strategy programs built with it. For building, a first solve and
checking proofs, start with [getting-started.md](getting-started.md). For
writing your own strategy, see [c-api.md](c-api.md).

- [Synopsis](#synopsis)
- [Options](#options)
- [Rejected combinations](#rejected-combinations)
- [Output](#output)
- [Exit codes](#exit-codes)
- [Signals and limits](#signals-and-limits)
- [Proof output](#proof-output)
- [Input](#input)
- [Environment variables](#environment-variables)
- [Example strategy programs](#example-strategy-programs)
- [SAT Competition entry points](#sat-competition-entry-points)

## Synopsis

```
tactix [options] <cnf[.gz|.xz]> [proof]
```

| Argument | Meaning |
|---|---|
| `cnf` | The input formula in DIMACS CNF. A name ending in `.gz` or `.xz` is decompressed on the fly (see [Input](#input)). Required. |
| `proof` | Where to write the proof. Optional. Without it, no proof is written. |

Options and arguments can appear in any order. The first non-option argument
is the formula, the second is the proof path; a third is an error. A lone `-`
is treated as a file name, so standard input is not supported.

`tactix -h` (or `--help`) prints a usage summary and exits with code 0.
`tactix --version` prints the version (`0.1.0`) and exits with code 0.

## Options

Options marked **research** select alternative policies or record data for
experiments and testing. They are not needed for ordinary solving, and their
details may change between releases.

| Option | Use | Section |
|---|---|---|
| `-q`, `--quiet` | normal | [Output control](#output-control) |
| `-v`, `--verbose` | normal | [Output control](#output-control) |
| `--version`, `-h`, `--help` | normal | [Synopsis](#synopsis) |
| `--auto-proof`, `--dsr`, `--drat`, `--lrat`, `--no-binary` | normal | [Proof format](#proof-format) |
| `--default-affine-search=on\|off` | normal | [Default strategy toggles](#default-strategy-toggles) |
| `--default-graph-labeling=on\|off` | normal | [Default strategy toggles](#default-strategy-toggles) |
| `--default-structured-symmetry=on\|off` | normal | [Default strategy toggles](#default-strategy-toggles) |
| `--bve-gates=...` | normal (tuning) | [Variable elimination gates](#variable-elimination-gates) |
| `--random-policy[=SEED]`, `--stress-policy[=SEED]`, `--no-heuristics` | research | [Randomized policies](#randomized-policies) |
| `--controller-*` | research | [Random controller](#random-controller) |
| `--trace`, `--replay`, `--feedback`, `--feedback-replay`, `--structural-provenance` | research | [Recording and replay](#recording-and-replay) |

Options that take a value use the form `--option=VALUE`; a separate argument
(`--option VALUE`) is not accepted. An unknown option is an error.

### Output control

**`-q`, `--quiet`.** Suppresses the statistics block printed after solving
(every `c` line from `c tactix 0.1.0` to the timing lines). The `s` and `v`
lines, the `c proof-route` line and error messages are still printed. The
research policies print a few `c` lines of their own that `-q` does not
suppress.

**`-v`, `--verbose`.** Prints progress while the default strategy searches:
a header, then a line at most about once per second at restarts and clause
database reductions, and a last line when it stops.

```
c progress: seconds conflicts decisions restarts reductions mode fixed-variables irredundant-clauses (of 570 variables)
c *     0.00            0             0        0      0 focused        1        526
c r     1.01        65451        154813     2893     21 stable        20        788
c r     2.01       126150        272111     4803     33 focused       20        836
...
c R    13.15       723232       1365990    26035    106 stable        20        972
...
c 0   186.19      7682404      11558820   200895    510 focused      116        950
```

The first column says what triggered the line: `*` start, `r` restart, `R`
reduction, and at the end `1` (SAT), `0` (UNSAT) or `?` (stopped). The other
columns are elapsed seconds; conflicts, decisions, restarts and reductions so
far; the search mode; variables fixed at the root; and live irredundant
clauses of three or more literals. `-v` only adds output: the search, the
answer and the proof are the same as without it. Through the C interface, set
it per session with `tactix_set_default_option(s, "TACTIX_VERBOSE", "1")`.

### Proof format

These options choose the format of the proof written to the `proof`
argument. Without a proof path they have no visible effect (see
[Proof output](#proof-output) for one exception).

| Option | Effect |
|---|---|
| `--auto-proof` | Default. Binary DSR; a complete refutation found before search is written as a pseudo-Boolean (PBP) proof instead. |
| `--dsr` | Always DSR. |
| `--drat` | DRAT. |
| `--lrat` | LRAT with hints. |
| `--no-binary` | Write the text variant of DSR or DRAT instead of binary. |

`--auto-proof`, `--dsr`, `--drat` and `--lrat` select one format family; if
several are given, the last one wins. `--no-binary` combines with any of them.
The resulting format is:

| Flags | Proof written |
|---|---|
| none, or `--auto-proof` | binary DSR, or PBP |
| `--no-binary` (alone or with `--auto-proof`) | text DSR (never PBP) |
| `--dsr` | binary DSR |
| `--dsr --no-binary` | text DSR |
| `--drat` | binary DRAT |
| `--drat --no-binary` | text DRAT |
| `--lrat` | text LRAT (`--no-binary` has no effect) |

DRAT and LRAT cannot express substitution-redundancy steps. With `--drat` or
`--lrat`, techniques that need them (symmetry breaking and some structured
techniques) are declined, so the solver can take a different path than with
the default format.

### Default strategy toggles

These switch optional services of the built-in default strategy on or off.
Each requires the value `on` or `off`; anything else is an error. They are
rejected together with any other policy (see
[Rejected combinations](#rejected-combinations)).

| Option | Default | Controls |
|---|---|---|
| `--default-affine-search=on\|off` | `off` | Gauss–Jordan elimination over XOR (GF(2)) constraints recognized in the formula, maintained during search in the style of CryptoMiniSat. Its statistics appear as `c affine-search` lines when it is enabled or active. |
| `--default-graph-labeling=on\|off` | `on` | A model-construction routine for graph-labeling encodings, run at the initial root and at later full restarts. Statistics: `c graph-schedule`. |
| `--default-structured-symmetry=on\|off` | `on` | The structured startup programs that use substitution-redundancy steps (symmetry breaking). With `off`, the other startup stages still run. |

### Variable elimination gates

**`--bve-gates=all|none|LIST`.** Chooses which gate definitions bounded
variable elimination (BVE) looks for. `LIST` is a comma-separated subset of:

| Name | Gate |
|---|---|
| `equiv` | equivalence `x = ¬y` |
| `andor` | AND/OR gates |
| `ite` | if-then-else |
| `xor` | parity (XOR) |
| `def` | general definitions found with an embedded solver |

Default: `equiv,andor,ite,def` (Kissat's gate set; `xor` off). Every setting
is sound: a disabled gate type only makes BVE fall back to the full set of
resolvents. An unknown name is an error. The default strategy uses this
setting; other policies may choose their own gate set per invocation.

### Randomized policies

**Research.** These replace the default strategy with randomized schedules
over the same proof rules. They exist to test soundness: every UNSAT answer
still carries a checkable proof and every SAT answer a valid model, but they
are much slower than the default strategy.

| Option | Effect |
|---|---|
| `--random-policy[=SEED]` | Generates random sequences of applicable rules with random parameters and applies them until SAT or UNSAT. Deterministic for a given seed. Default seed: `20260710`. |
| `--stress-policy[=SEED]` | The same randomized scheduler, used for targeted stress runs: set `TACTIX_STRESS_RULES` (see [Environment variables](#environment-variables)) to favour particular rules. Cannot record `--trace`. |
| `--no-heuristics` | Runs without the heuristic state (variable scores, saved phases). Requires `--random-policy` and no `--stress-policy`. Not listed in `--help`. |

If both `--random-policy` and `--stress-policy` are given, `--stress-policy`
runs.

### Random controller

**Research.** `--controller-policy` selects a seeded random controller. At the
root, after each full restart, it chooses inprocessing and structured actions
and a search configuration (branching, restart and phase heuristics), then
resumes search. It is the reference user of the online configuration
interface that external, for example learned, controllers program against.
[controller-interface.md](controller-interface.md) specifies the action space,
the file formats and the transcript.

| Option | Default | Effect |
|---|---|---|
| `--controller-policy[=SEED]` | seed `20260710` | Selects the controller. |
| `--controller-effort=N` | `10000000` | Native work ceiling for one action. `0` is allowed and means zero work. |
| `--controller-restart-period=N` | `1024` | Base period of the reluctant (Luby) restart schedule. `N` must be positive. |
| `--controller-trail-retention=ranked-prefix\|full` | `ranked-prefix` | `ranked-prefix` keeps a valid prefix of the trail on restart; `full` always restarts to the root. |
| `--controller-search-configurations=all\|LIST` | `all` | Restricts the search configurations the controller may choose. `LIST` is comma-separated `branching/restart/phase` names (below). Repeated options accumulate; duplicates and unknown names are errors. |
| `--controller-families=all\|LIST` | `all` | Restricts the root actions to the listed rule names (comma-separated). `Decide`, `ConflictAnalysis`, `Restart` and `SwitchMode` are not root actions and are rejected. |
| `--controller-actions=FILE` | none | Reads a serialized strategy program and offers its steps, with their parameters, as the action collection. Cannot be combined with `--controller-families`. |
| `--controller-native-producers=all\|none\|LIST` | `none` | Enables the controller's own discovery of structured actions. `LIST` is comma-separated: `gf2-affine`, `gf3-affine`, `direct-csp`, `nary-csp-equality`, `nary-csp-equality-amo`, `gf3-gaussian`, `gf3-general-row`, `tseitin-cycle`. |
| `--controller-startup-prefix=FILE` | none | An ordered list of actions to run once at startup before random selection begins. |
| `--controller-gf2-affine-profile=FILE` | none | Limits and allowances for the `gf2-affine` producer. Requires that producer to be enabled and `AffineConsequence` to be an allowed action. |
| `--controller-direct-csp-profile=FILE` | none | Limits and allowances for the `direct-csp` producer. |
| `--controller-guarded-cover-profile=FILE` | none | Profile for guarded-cover discovery. Must be given together with the next option. |
| `--controller-guarded-cover-limits=FILE` | none | Limits for guarded-cover discovery. |
| `--controller-trace=FILE` | none | Records a transcript of the controller's configuration, requests and outcomes. Requires `--controller-policy`. |
| `--controller-replay=FILE` | none | Replays a transcript and verifies every deterministic event and the final answer. Selects the controller by itself (the seed comes from the transcript); cannot be combined with `--controller-policy` or `--controller-trace`. Use the same build, input and controller settings as the recorded run. |

Search configuration names have the form `branching/restart/phase`, for
example `evsids/reluctant/target`:

| Component | Choices |
|---|---|
| branching | `vmtf`, `evsids` |
| restart | `adaptive-glue`, `reluctant` |
| phase | `saved`, `target`, `positive`, `negative`, `focused-rotation` |

All `--controller-*` settings other than `--controller-policy` and
`--controller-replay` require one of those two. A file that fails to parse or
validate is an error, reported on stderr before solving starts.

### Recording and replay

**Research.** These options record what a policy did, so that a run can be
reproduced or analysed.

| Option | Effect |
|---|---|
| `--trace=FILE` | Records every program `--random-policy` applies, and the final answer. Requires `--random-policy`. |
| `--replay=FILE` | Applies exactly the programs recorded by `--trace` and checks that the run ends with the recorded answer. If the trace is malformed or the run diverges, prints `s UNKNOWN` and exits with code 1. Cannot be combined with another policy or with `--trace`. |
| `--feedback=FILE` | Records the per-action feedback stream of the default strategy or the random controller. |
| `--feedback-replay=FILE` | Re-runs the same policy, seed and configuration and verifies the regenerated feedback stream against `FILE`. |
| `--structural-provenance=NEW_FILE` | Writes the provenance records of structured techniques to `NEW_FILE` as JSON lines. |

Feedback files start with provenance metadata: SHA-256 hashes of the running
executable and the input file, the policy, seed, configuration, proof mode,
BVE gate mask, and the values of solver environment variables that are set.
`--feedback` and `--feedback-replay` are mutually exclusive, need a non-empty
path, and work only with the default strategy or the random controller. A
feedback error does not change the solver's answer or proof: the run finishes
normally, then the error is reported on stderr and the exit code is 1.
`tools/bench/inspect_policy_feedback.py` validates and summarizes a feedback
file.

`--structural-provenance` creates `NEW_FILE` exclusively, so the file must not
exist. Its first line is a session record with the SHA-256 hash of the input.
Without this option the records go to an anonymous temporary file. Neither the
feedback path nor the provenance path may name the same file as the input,
the proof or a transcript.

## Rejected combinations

The solver checks option combinations after reading all arguments. A setting
that could not take effect is an error rather than being ignored. Each of
these prints a message on stderr and exits with code 1:

| Combination | Reason |
|---|---|
| A `--default-*` toggle with `--random-policy`, `--stress-policy`, `--controller-policy`, `--controller-replay`, `--replay` or `--no-heuristics` | The toggles configure only the default strategy. |
| `--feedback` together with `--feedback-replay` | Choose one. |
| `--feedback` or `--feedback-replay` with `--random-policy`, `--stress-policy`, `--replay` or `--no-heuristics`, or with an empty path | Feedback exists only for the default strategy and the controller. |
| A feedback path that names the input, proof or a transcript file | It would overwrite that file. |
| `--structural-provenance` naming any other input or output file | It would overwrite that file. |
| The controller with `--random-policy`, `--stress-policy`, `--replay`, `--no-heuristics` or `--trace` | One policy at a time; `--trace` is for `--random-policy` only. |
| `--controller-replay` with `--controller-policy` or `--controller-trace` | Replay selects the controller and its seed itself. |
| `--controller-trace` without `--controller-policy` | Nothing to record. |
| Any other `--controller-*` setting without `--controller-policy` or `--controller-replay` | The setting would be ignored. |
| `--controller-actions` with `--controller-families` | Both select the action collection. |
| Only one of `--controller-guarded-cover-profile` and `--controller-guarded-cover-limits` | Both are needed. |
| `--trace` without `--random-policy` | Only the random policy's schedule can be recorded. |
| `--replay` with `--trace`, `--random-policy` or `--stress-policy` | Replay runs the recorded schedule. |
| `--no-heuristics` without `--random-policy`, or with `--stress-policy` | Other policies need the heuristic state. |

When several policies are selected and no check applies, the precedence is
`--replay`, then the controller, then `--stress-policy`, then
`--random-policy`, then the default strategy.

## Output

The solver writes to standard output in the SAT Competition format. Error
messages, prefixed `tactix:`, go to standard error.

### Answer and model

The last line of the answer is one of:

| Line | Meaning |
|---|---|
| `s SATISFIABLE` | Followed by the model in `v` lines. |
| `s UNSATISFIABLE` | The formula has no model. With a proof path, preceded by `c proof-route`. |
| `s UNKNOWN` | Interrupted, or an error before or during solving. |

The model gives a value for every variable of the input formula, positive for
true and negative for false, as space-separated literals on lines starting
with `v`. Each line stays under 4096 characters, and the last line ends with
`0`. Variables the solver introduced itself are not printed.

### Statistics

Unless `-q` is given, the solver prints a statistics block after solving and
before the `s` line. It starts with `c tactix 0.1.0`. Each line is `c`, a short
keyword, and counters. The block covers:

- **Search:** decisions, conflicts, propagations, learned clauses and their
  size and glue, minimization, restarts, clause-database reductions, mode
  switches, rephasing.
- **Inprocessing:** one or more lines per technique, for example `c vivify`,
  `c probe`, `c backbone`, `c sweep`, `c bve`, `c factor`, `c congruence`,
  `c substitute`, `c subsume`, plus `c retired` (substituted and eliminated
  variables).
- **Structured techniques:** symmetry breaking (`c sr`), parity and affine
  reasoning, counting arguments (`c count-pb`), and the recognizers for
  specific problem families. A recognizer that never ran shows zeros.
- **Local search:** `c walk`, `c warmup` and related lines.
- **Default strategy services:** `c graph-schedule`, and `c affine-search`
  when affine search is enabled or active. Printed only for the default
  strategy.
- **Timing:** `c timing` gives total solve time, the share spent in
  inprocessing, and one line per rule that ran, largest first; `c phasecost`
  breaks the inprocessing time down further.

Most lines pair a result count with a count of attempts, so a line of zeros
means the technique did not run. The set of lines and their wording are
diagnostic output and may change between releases; scripts should rely only
on the `s`, `v` and `c proof-route` lines.

### Proof route

On UNSAT with a proof path, the solver prints one line just before
`s UNSATISFIABLE`:

```
c proof-route <route>
```

| Route | Proof |
|---|---|
| `dsr-cake_lpr` | DSR without substitution witnesses (checked as DRAT) |
| `dsr-trestle` | DSR with substitution witnesses |
| `pbp` | pseudo-Boolean proof |
| `drat` | DRAT (`--drat`) |
| `lrat` | LRAT (`--lrat`) |

The line is printed even with `-q`. [getting-started.md](getting-started.md)
explains which checkers each route uses.

## Exit codes

| Code | Meaning |
|---|---|
| 10 | Satisfiable. |
| 20 | Unsatisfiable. |
| 0 | Unknown: interrupted by a signal, or the policy stopped without an answer. Also the exit code of `--help` and `--version`. |
| 1 | Error: unknown option, bad option value, rejected combination, no input file, too many arguments, unreadable input, parse error, proof file that cannot be created, a `--replay` that diverged, or a feedback error (in that last case the answer, model and proof are still written). |

Most errors also print `s UNKNOWN` on standard output. Some print only the
message on stderr: invalid values of `--controller-families`,
`--controller-native-producers`, `--controller-search-configurations`,
`--controller-trail-retention`, `--controller-restart-period` and
`--controller-effort`, and `--structural-provenance` errors.

An invalid value in a `TACTIX_ORDER_*` environment variable, or a strategy
configuration that fails validation, aborts the process (SIGABRT).

## Signals and limits

SIGINT, SIGTERM and SIGXCPU (where the platform defines it) stop the solver
at the next safe point. It then flushes the proof written so far, prints the
statistics (unless `-q`), prints `s UNKNOWN` and exits with code 0.

There are no time or memory limit options. Use the environment, for example
`timeout` (SIGTERM) or `ulimit -t` (SIGXCPU).

## Proof output

- A proof is written only when a proof path is given. Without it, no proof
  file is created.
- The proof file is created (and truncated) before the input is read. On SAT
  or UNKNOWN its contents are not meaningful.
- Binary DRAT is drat-trim's binary format. Binary DSR is a separate binary
  format; `tools/bench/proof_oracle.py` reads it and converts it as needed
  for the checkers. LRAT and PBP are always text.
- The automatic mode can switch to PBP only when the whole refutation is
  found before search (parity and counting arguments). Once any clausal proof
  step is written, the proof stays DSR.
- The `c proof-route` line tells which checker pipeline applies.
  [getting-started.md](getting-started.md) describes the routes and the
  `tools/bench/proof_oracle.py check` script that runs them. A `--drat` proof
  can also be checked directly with drat-trim, and an `--lrat` proof with
  cake_lpr.

One effect does not depend on the proof path: PBP is only possible in the
automatic binary mode, so `--no-binary`, `--dsr`, `--drat` and `--lrat` disable
the startup techniques that would produce one, even when no proof is written.

## Input

The input is DIMACS CNF. The `p cnf <variables> <clauses>` header is read for
sizing but its counts are not enforced. Comment lines start with `c`. A
missing final `0` on the last clause is accepted. A malformed literal is a
parse error (exit code 1).

Compressed input is detected by the file name alone (case-sensitive):

| Suffix | Decompressed with |
|---|---|
| `.gz` | `gzip -dc` |
| `.xz` | `xz -dc` |

The decompressor runs as a separate process and must be on the `PATH`. Its
exit status is not checked: if it is missing or fails, the solver reads a
truncated or empty formula. Other compression formats are not supported.

The example strategy programs below read plain DIMACS only.

## Environment variables

The solver reads a number of `TACTIX_*` environment variables. They are
**developer and diagnostic settings, not a supported interface**: names,
values and defaults may change without notice, and values are not validated
(a non-numeric value usually reads as 0). Unless noted, a variable affects
only the default strategy. Variables marked "set" take effect when defined,
whatever their value; boolean variables read `0` as off and anything else as
on.

### Search

| Variable | Value | Default | Effect |
|---|---|---|---|
| `TACTIX_FORCE_MODE` | `focused` or `stable` | automatic | Stay in one search mode instead of alternating. |
| `TACTIX_CHRONO_LEVELS` | N | 100 | Backjumps longer than N levels backtrack chronologically. |
| `TACTIX_NO_BUMP_REASONS` | set | off | Disable bumping of reason-side variables. |
| `TACTIX_BUMP_REASONS_LIMIT` | N | 10 | Limit for reason-side bumping. |
| `TACTIX_BUMP_REASONS_RATE` | number | 10.0 | Rate for reason-side bumping. |
| `TACTIX_NO_EXT_REASON_BUMP` | set | off | Do not bump reasons on solver-introduced variables. |
| `TACTIX_CONFLICT_REUSE` | 0/1 | 1 | Reuse the conflict clause when it has one literal on the conflict level. |
| `TACTIX_JUMP_REASONS` | 0/1 | 1 | Jump over binary reasons during analysis. |
| `TACTIX_SHRINK_GLUE` | 0/1 | 1 | Glue promotion during clause shrinking. |
| `TACTIX_TUMBLE` | 0/1 | 1 | Initial variable order seeded by activation order. |
| `TACTIX_EAGER_SUBSUME` | 0–4 | 4 | Number of recent learned clauses checked for eager subsumption. |
| `TACTIX_RANDEC` | 0/1 | 1 | Random decision sequences in focused mode. |
| `TACTIX_REORDER` | 0, 1 or 2 | 2 | Variable reordering: off, stable mode only, both modes. |
| `TACTIX_REPHASE_BOTH_MODES` | set | off | Rephase in both modes (default: stable mode only). |
| `TACTIX_WALK_PERMILLE` | N | 50 | Local-search (walk) effort, in per mille. |
| `TACTIX_LUCKY_LATE` | 0/1 | 1 | Retry the cheap "lucky" assignment checks later in the run. |

### Clause database

| Variable | Value | Default | Effect |
|---|---|---|---|
| `TACTIX_REDUCE_CADENCE` | `sqrt` or `arith` | `sqrt` | Reduction interval growth. |
| `TACTIX_REDUCE_RAMP` | 0/1 | follows cadence | Ramp the fraction of clauses removed. |
| `TACTIX_NO_DYNAMIC_TIERS` | set | off | Fixed clause tiers instead of dynamic ones. |
| `TACTIX_NO_COMPACT` | set | off | Disable variable compaction. |

### Inprocessing schedule

| Variable | Value | Default | Effect |
|---|---|---|---|
| `TACTIX_NO_BVE`, `TACTIX_NO_FACTOR`, `TACTIX_NO_SWEEP` | set | off | Disable bounded variable elimination, factoring, or SAT sweeping. |
| `TACTIX_PREPROCESS_ONLY` | set | off | Run inprocessing only before the first conflict. |
| `TACTIX_SLOT_ENABLE`, `TACTIX_SLOT_DISABLE` | `Rule,Rule,...` | none | Enable or disable steps of the inprocessing program by rule name. |
| `TACTIX_SLOT_PERMILLE` | `Rule:N,...` | per rule | Effort of a step, in per mille. |
| `TACTIX_SLOT_MAX_APPLIES` | `Rule:N,...` | per rule | Maximum applications of a step. |
| `TACTIX_CURSOR_<RULE>` | 0/1 | per rule | Use a persistent candidate cursor for the step. `<RULE>` is the rule name in upper case. |
| `TACTIX_ORDER_<RULE>` | `native`, `reverse`, `random`, `occ-asc`, `occ-desc`, `negocc-desc`, `heuristic` | per rule | Candidate order for the step. Any other value aborts. |
| `TACTIX_ARENA_ORDER` | 0/1 | 0 | CaDiCaL-style variable and phase ordering instead of Kissat's. |
| `TACTIX_PREPROCESS_FOLLOWUPS`, `TACTIX_NO_CLOSURE_DELAY`, `TACTIX_SLOT_CLOCKS`, `TACTIX_SLOT_WATERMARK`, `TACTIX_SUBSUME_HOLD`, `TACTIX_TOUCH_MARKS`, `TACTIX_TOUCH_REDUNDANT`, `TACTIX_FACTOR_COUPLE` | 0/1 or set | — | Internal scheduler switches; see `configure_from_env` in `src/policies/default_policy.cpp`. |

Rule names are those printed in the `c timing` lines (for example `BVE`,
`Vivify`, `Sweep`).

### Technique parameters

| Variable | Value | Default | Effect |
|---|---|---|---|
| `TACTIX_BVE_RAMP_MAX` | N | 16 | Largest clause-growth bound BVE ramps up to. |
| `TACTIX_BVE_BACKWARD` | 0/1 | 0 | Backward subsumption during BVE. |
| `TACTIX_BVE_DEF_TICKS` | N | 1000000 | Work budget for finding general definitions. |
| `TACTIX_BVE_DEF_CORES` | 1–100 | 2 | Number of unsatisfiable cores tried per definition. |
| `TACTIX_BVE_DEF_CORE_MULTIPLIER` | N | 10 | Budget multiplier per core. |
| `TACTIX_BCE_OCC_LIMIT` | N | 0 (no limit) | Occurrence limit for blocked-clause elimination. |
| `TACTIX_TERNARY_OCC_LIMIT` | N | 100 | Occurrence limit for hyper-ternary resolution. |
| `TACTIX_TERNARY_MAXADD` | N | 1000 | Maximum clauses added by hyper-ternary resolution, in percent. |
| `TACTIX_FACTOR_SIZE` | N | 5 | Maximum clause size for factoring. |
| `TACTIX_FACTOR_BOUND` | N | 0 | A factoring is accepted only if it removes more than N clauses. |
| `TACTIX_FACTOR_DEMOTE` | 0/1 | 1 | Rank variables introduced by factoring last. |
| `TACTIX_FACTOR_CAND_ROUNDS` | N | 2 | Candidate rounds per factoring call. |
| `TACTIX_FACTOR_INIT_TICKS` | N | 700000000 | Effort of the first factoring round. |
| `TACTIX_V4_EARLY_EXIT`, `TACTIX_TRANSITIVE_LIVE_METER` | 0/1 | 1 | Transitive-reduction budget details. |
| `TACTIX_V4_METER` | `checked` | unchecked | Transitive-reduction budget meter. |
| `TACTIX_VIVIFY_REUSE` | 0/1 | 1 | Reuse the trail between vivification candidates. |
| `TACTIX_VIVIFY_FLUSH` | 0/1 | 0 | Flush during vivification. |
| `TACTIX_SUBST_DEMOTE` | set | off | Rank substituted variables last. |
| `TACTIX_SWEEP_DEPTH`, `TACTIX_SWEEP_VARS`, `TACTIX_SWEEP_CLAUSES` | N | 2, 256, 1024 | SAT-sweeping environment depth and size. |
| `TACTIX_SWEEP_CANDIDATES`, `TACTIX_SWEEP_SEEDS`, `TACTIX_SWEEP_TICKS`, `TACTIX_SWEEP_ROUND_TICKS`, `TACTIX_SWEEP_FLIP_ROUNDS` | N | 0, 0, 0, 0, 1 | SAT-sweeping candidate, seed, effort and flip limits. |
| `TACTIX_SWEEP_STRIP`, `TACTIX_SWEEP_PARTITION`, `TACTIX_SWEEP_RESUME`, `TACTIX_SWEEP_CLOSURE`, `TACTIX_SWEEP_FORCE_GENERAL` | 0/1 | 1, 1, 1, 0, 0 | SAT-sweeping switches. |

Some of these (`TACTIX_BCE_OCC_LIMIT`, `TACTIX_TERNARY_*`, the
`TACTIX_BVE_DEF_*` and `TACTIX_BVE_BACKWARD`, `TACTIX_FACTOR_*` except
`INIT_TICKS` and `COUPLE`, `TACTIX_V4_*`, `TACTIX_TRANSITIVE_LIVE_METER`,
`TACTIX_VIVIFY_*`, `TACTIX_SUBST_DEMOTE`, `TACTIX_EAGER_SUBSUME`,
`TACTIX_NO_EXT_REASON_BUMP`, `TACTIX_CONFLICT_REUSE`, `TACTIX_JUMP_REASONS`,
`TACTIX_SHRINK_GLUE`, `TACTIX_TUMBLE`) also set the solver-wide defaults used
by every policy.

### Diagnostic output and testing

| Variable | Value | Effect |
|---|---|---|
| `TACTIX_FRONTEND_TIME` | set | On SAT, print parse, model-extension and model-printing times on stderr. |
| `TACTIX_STRATEGY_CONFIG` | set | Print the default strategy's full configuration as a `c strategy-config` line. |
| `TACTIX_SLOT_STATS` | set | Print a per-step report of the inprocessing scheduler. |
| `TACTIX_SLOT_TRACE` | set | Trace inprocessing steps on stderr. |
| `TACTIX_SR_TRACE` | `1` or `2` | Trace symmetry breaking on stderr; `2` is more detailed. |
| `TACTIX_TRACE` | set | Trace each rule application of the randomized policies on stderr. |
| `TACTIX_STRESS_RULES` | `Rule,Rule,...` | Randomized policies favour these rules. |
| `TACTIX_STRESS_NORMAL_EVERY` | N (default 8) | With `TACTIX_STRESS_RULES`, every N-th program is an ordinary random one. |
| `TACTIX_STOP_FUZZ` | N | Testing only: make stop checks fire at random (about one in N), to exercise interruption paths. |
| `TACTIX_EVENT_TRACE` | FILE | Event trace. Only in builds configured with `-DTACTIX_ENABLE_EVENT_TRACE=ON`. |
| `TACTIX_RULE_JOURNAL` | FILE | Rule journal. Only in builds configured with `-DTACTIX_ENABLE_RULE_JOURNAL=ON`. |

The source also reads many more narrowly targeted trace variables (names
ending in `_TRACE`, and `SAT_*` variables that select a conflict or variable
for those traces). They exist for debugging individual techniques and are not
documented here.

## Example strategy programs

These programs are built with the solver (`cmake --build --preset release`)
and drive it through the C interface ([c-api.md](c-api.md)). All three follow
the same output conventions as `tactix` (`s`/`v` lines, exit codes 10, 20 and
0) and read plain DIMACS only. The sources are in [../examples/](../examples/)
and `tests/`.

### tactix-public

```
build/release/tests/tactix-public [--auto-proof] <cnf> [proof]
```

The default strategy with every operation issued through `tactix.h`. Proofs
are always in the automatic format; `--auto-proof` is accepted and ignored,
and any other option is an error. It prints `c proof-route` on UNSAT with a
proof path, a short statistics block and `c public-port` counts of interface
requests. It handles SIGINT, SIGTERM and SIGXCPU like `tactix`. It has no
`--help`.

### tactix-cruxmiter-policy

```
build/release/examples/tactix-cruxmiter-policy [--profile=NAME] [--default-option=NAME=VALUE]... [--drat] <cnf> [proof]
```

A strategy for multiplier-equivalence miters. In the default profile, `full`,
it repeats hyper-binary resolution, congruence closure and equivalent-literal
substitution (at most two rounds), then applies vivification, strengthening,
subsumption, BVE and transitive reduction, a final congruence and
substitution pass, and hands the formula to the default strategy.

- The proof is always binary DRAT. `--drat` is accepted for runner
  compatibility and changes nothing. No `c proof-route` line is printed.
- `--profile=NAME` selects an alternative experimental schedule. Accepted
  names: `full` (default), `search`, `positive`, `saved`, `gate-search`,
  `backbone-search`, `backbone-close-search`, `taco-warmup`, `input-ranked`,
  `input-saved`, `input-ascending`, `input-descending`, `gate`, `binary`,
  `backbone`, `backbone-negative`, `backbone-positive`, `backbone-fixpoint`,
  `backbone-first`, `backbone-close`, `backbone-heavy`, `symmetry-backbone`,
  `backbone-symmetry`, `backbone-symmetry-scan`, `symmetry-sandwich`, `sweep`,
  `deep-sweep`, `bdd-bve`, `kissat-order`, `kissat-order-negative`. The
  `input-*` profiles require the size-32 cruxmiter formula (570 variables).
- `--default-option=NAME=VALUE` (repeatable) sets a `TACTIX_*` setting of the
  default strategy that the program hands over to, through
  `tactix_set_default_option` in the C interface. The paper's configuration is
  `--profile=backbone` with `TACTIX_FORCE_MODE=stable`, `TACTIX_REORDER=0`,
  `TACTIX_NO_BVE=1`, `TACTIX_CONFLICT_REUSE=0`,
  `TACTIX_SLOT_PERMILLE=Vivify:200`, `TACTIX_NO_FACTOR=1`, `TACTIX_NO_SWEEP=1`
  and `TACTIX_SLOT_DISABLE=Backbone` (see `paper/run/experiments.py`).
- A line `c cruxmiter-policy profile=...` records the profile in use.
- Schedule details can be adjusted with `TACTIX_CRUX_*` environment variables
  (`TACTIX_CRUX_RELUCTANT_PERIOD`, `TACTIX_CRUX_REDUCE_INTERVAL`,
  `TACTIX_CRUX_INPROCESS_INTERVAL`, `TACTIX_CRUX_FOCUS_CONFLICTS`,
  `TACTIX_CRUX_WARMUP_CONFLICTS`, `TACTIX_CRUX_CS_ROUNDS`,
  `TACTIX_CRUX_CS_FORCE`, `TACTIX_CRUX_SWEEP_DEPTH`,
  `TACTIX_CRUX_INPUT_ORDER`, `TACTIX_CRUX_BVE_INPUT_ORDER`,
  `TACTIX_CRUX_BVE_CUTOFF`, `TACTIX_CRUX_BVE_PROGRESS`); see the source.
- It installs no signal handlers, so an interrupt ends the process without an
  `s` line.

### tactix-chvatal-policy

```
build/release/examples/tactix-chvatal-policy [--twins|--default] [--dsr|--drat] <cnf> [proof]
```

A strategy for Chvátal-conjecture subproblems. It finds counter variables that
the formula's totalizers duplicate ("twins"), merges each pair with
substitution-redundancy steps, collapses the resulting equivalences, and then
runs the default strategy.

| Option | Effect |
|---|---|
| `--twins` | Default. Merge twins first. |
| `--default` | Skip twin merging and run the default strategy directly. |
| `--dsr` | Default. Binary DSR proof. |
| `--drat` | Binary DRAT proof. The merge steps cannot be written in DRAT, so the solver declines them. |
| `--forward` | Not in the usage line. Adds only one direction of each twin equivalence; for experiments. |

With a DSR proof, it prints `c proof-route` after the `s UNSATISFIABLE` line
(by scanning the finished proof). It prints `c chvatal-twins` lines with the
number of twin pairs found and accepted. The environment variables
`CHVATAL_DEBUG`, `CHVATAL_ONE_COLLAPSE`, `CHVATAL_NO_SUBSUME` and
`CHVATAL_MERGE_ONLY` (stop after merging, without an answer) are for
experiments. It installs no signal handlers.

`examples/` also contains smaller strategies (`tactix-crux-fallback-policy`,
`tactix-eevbnn-policy`, `tactix-random3sat-policy` and the `demo*` programs);
see their sources for usage. [../examples/cnf/](../examples/cnf/) has small
test formulas.

## SAT Competition entry points

Two scripts in `tools/compete/` follow the SAT Competition conventions; both
work from any directory.

| Script | Usage | Does |
|---|---|---|
| `build.sh` | `tools/compete/build.sh` | `cmake --preset release` and `cmake --build --preset release --parallel`. |
| `run.sh` | `tools/compete/run.sh INSTANCE OUTPUT_DIRECTORY` | Creates `OUTPUT_DIRECTORY` and runs `build/release/tactix INSTANCE OUTPUT_DIRECTORY/proof.out` with default options. |

`run.sh` exits with code 2 if it does not get exactly two arguments.
Otherwise it replaces itself with the solver, so its output and exit code are
the solver's. The proof uses the default automatic format.
