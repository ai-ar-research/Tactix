# The Tactix C interface

This guide explains how to drive Tactix from C: how to load a formula, how to
run the built-in strategy, and how to write your own strategy out of the
solver's proof rules. It ends with a tutorial and a reference table of every
public function.

The authoritative contract is the header itself,
[`include/tactix.h`](../include/tactix.h). Its comments define every return
value, argument encoding and structure field; this guide explains how the
pieces fit together and leaves field-by-field detail to the header.
[getting-started.md](getting-started.md) covers building the solver and a
minimal library client; [rules.md](rules.md) describes each rule;
[cli.md](cli.md) describes the command-line programs, including the example
strategies.

## Contents

1. [Overview](#1-overview)
2. [Building and linking](#2-building-and-linking)
3. [Sessions, input, proofs and results](#3-sessions-input-proofs-and-results)
4. [Stepwise strategies](#4-stepwise-strategies)
5. [Per-invocation settings: online programs](#5-per-invocation-settings-online-programs)
6. [Tutorial: building a strategy](#6-tutorial-building-a-strategy)
7. [Proofs and what a strategy can and cannot break](#7-proofs-and-what-a-strategy-can-and-cannot-break)
8. [Errors, threads and stability](#8-errors-threads-and-stability)
9. [Function reference](#9-function-reference)

## 1. Overview

### The model

A **session** (`tactix *`) holds one formula and everything the solver knows
about it: the original clauses, learned and derived clauses, the current
assignment trail, the decision heuristic's state (scores, saved phases, the
current focused or stable mode), the proof being written, and the information
needed to rebuild a model of the original formula after simplification.

A session goes through three stages:

1. **Input.** You add clauses in DIMACS convention and, optionally, attach a
   proof file.
2. **Solving.** The first call that reasons about the formula closes the
   input. From then on the session changes only through **actions**.
3. **Answer.** The session reports SAT (10) or UNSAT (20). After SAT you read
   the model; after UNSAT the proof file holds a refutation.

Every action has a stable numeric id and a name, such as `"Propagate"`,
`"Decide"`, `"ConflictAnalysis"`, `"Restart"`, `"Reduce"`, `"BVE"` or
`"Vivify"`. Actions come in two kinds:

- **Proof rules** change the formula, the learned clauses or the assignment.
  Each application writes the corresponding proof steps (or none, when the
  step needs no justification, such as a decision).
- **Policy operations** change only the decision heuristic's state: switching
  between focused and stable mode, rephasing, reordering the decision queue,
  local-search phase guidance. They write nothing to the proof.

Each action has a **guard**: the condition under which applying it is sound
and meaningful in the current state. For example, `ConflictAnalysis` requires a
recorded conflict, `Propagate` requires pending propagation work, and the
inprocessing rules require decision level 0. `tactix_rules_applicable` lists
the actions whose guard holds now. Any of them may be applied, in any order.

Many actions take an **instance**: a 64-bit payload that selects one concrete
application. For `Decide` the instance is the literal to branch on; for
`Restart` it is the decision level to keep; for `ConflictAnalysis` it is the
backjump level. `tactix_rule_instances` enumerates the legal instances in the
current state, with the rule's own default choice (the **canonical**
instance) first. `TACTIX_INSTANCE_CANONICAL` always selects that default.

Applying an action returns a **result code**: progress, no change, conflict
found, SAT, UNSAT, or "guard does not hold".

### Two ways to use Tactix

- **Built-in strategy.** `tactix_solve` runs the default strategy (a
  reimplementation of the Kissat and CaDiCaL search and inprocessing
  schedules, plus symmetry breaking and structure-specific techniques) until
  it has an answer.
- **Your own strategy.** You write the loop: ask which rules apply, choose one
  and its instance, apply it, observe counters with `tactix_stat`, repeat.
  Strategies are pull-based. The solver never calls back into your code;
  there is no callback registration.

The two combine. A strategy can do custom work first (a structural
preprocessing pass, a short search with its own branching rule) and then call
`tactix_solve` to let the default strategy finish from the current state.
The bundled Chvátal and cruxmiter strategies work this way.

Whatever a strategy does, an UNSAT answer comes with a proof that independent
checkers verify, and a SAT answer comes with a model of the original formula.
Section 7 states precisely what this covers.

## 2. Building and linking

The build produces an object library, `tactix_lib`, which the command-line
solver, the tests and the example strategies all link. Its public include
directory contains `tactix.h`. Strategies are plain C (C99 or later); the
library itself is C++20, so link with the C++ runtime, which CMake does
automatically when you link `tactix_lib`.

The simplest way to build a strategy is the way the bundled examples are built.
[`examples/CMakeLists.txt`](../examples/CMakeLists.txt) declares each one as:

```cmake
add_executable(my-strategy my_strategy.c)
target_link_libraries(my-strategy PRIVATE tactix_lib)
target_include_directories(my-strategy PRIVATE ${CMAKE_SOURCE_DIR}/include)
```

Add your source file to `examples/` with these three lines and rebuild with
the presets from [getting-started.md](getting-started.md):

```sh
cmake --preset release
cmake --build --preset release --parallel
build/release/examples/my-strategy formula.cnf proof
```

From a separate CMake project, add the Tactix source tree as a subdirectory
and link the same target (Tactix needs zlib and CMake 3.25 or newer):

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_strategy C CXX)
add_subdirectory(path/to/Tactix tactix EXCLUDE_FROM_ALL)
add_executable(my-strategy my_strategy.c)
target_link_libraries(my-strategy PRIVATE tactix_lib)
```

`tactix_lib` is an object library on purpose: rules register themselves from
static initializers, and a static archive would let the linker drop rules
that nothing references by name. Link the target, not an archive built from
it.

## 3. Sessions, input, proofs and results

### Creating and releasing a session

```c
tactix *s = tactix_init();
/* ... */
tactix_release(s);
```

`tactix_release` frees the session and closes its proof file. Release the
session before you hand the proof file to a checker.

### Adding clauses

Literals use the DIMACS convention: variable `v` is the positive integer `v`,
its negation is `-v`, and `0` ends a clause. Variables need no declaration.

```c
/* (x1 or not x2) and (x2 or x3) */
const int clauses[] = {1, -2, 0, 2, 3, 0};
for (size_t i = 0; i < sizeof clauses / sizeof *clauses; i++)
  if (tactix_add_checked(s, clauses[i]) != 0)
    /* input already closed */;
```

`tactix_add_checked` returns 0, or -1 without changing anything when the
input is already closed. `tactix_add` is the same call without a return value;
it aborts the process (SIGABRT) on a late addition instead of ignoring it.

`tactix_read_dimacs(s, file)` reads a complete DIMACS CNF stream with the
command-line parser, including its use of the `p cnf` header, and closes the
input. It is legal only on a session that has no input yet. It returns 0, or
-1 either without reading (the session already has input) or after a
malformed stream, in which case the session holds a partial formula and
should be released.

### When the input closes

There is no explicit "end of input" call. The input closes at the first call
that reasons about the formula: `tactix_status`, `tactix_solve`,
`tactix_rules_applicable`, `tactix_rule_instances`,
`tactix_rule_semantic_instances`, any `tactix_apply*` call,
`tactix_decide_candidates`, `tactix_decide`, `tactix_support`, a successful
`tactix_online_configure*`, or `tactix_online_advance`. `tactix_read_dimacs` closes it as well.
Configuration setters (`tactix_set_*`, `tactix_install_*`) and statistics
reads do not.

Adding clauses after the input has closed is not supported in this version:
there is no incremental solving and no assumption interface.

### Attaching a proof

```c
if (tactix_proof(s, "proof.dsr", TACTIX_PROOF_AUTOMATIC) != 0)
  /* late or duplicate attachment, unwritable path, or unknown format */;
```

Attach the proof once, before the input closes. The simplest rule is to call
`tactix_proof` right after `tactix_init`. A session without a proof solves
normally and writes nothing.

| Constant | Value | Output |
|---|---|---|
| `TACTIX_PROOF_DRAT_BINARY` | 0 | DRAT, binary encoding |
| `TACTIX_PROOF_DRAT_ASCII` | 1 | DRAT, text encoding |
| `TACTIX_PROOF_LRAT` | 2 | LRAT with antecedent hints |
| `TACTIX_PROOF_DSR_BINARY` | 3 | DSR (DRAT extended with substitution redundancy), binary; the canonical format |
| `TACTIX_PROOF_DSR_ASCII` | 4 | DSR, text encoding, for inspection |
| `TACTIX_PROOF_AUTOMATIC` | 5 | binary DSR, or a pseudo-Boolean (VeriPB) proof when a complete refutation is found before any other step |
| `TACTIX_PROOF_PBP` | 6 | explicit whole-run pseudo-Boolean proof for VeriPB elaboration and independent CakePB checking |

`TACTIX_PROOF_AUTOMATIC` is what the command-line solver uses by default.
`TACTIX_PROOF_PBP` is additive: the DSR, DRAT and LRAT constants and automatic
selection retain their existing behavior.
[getting-started.md](getting-started.md) explains which checker verifies
each kind of proof and how `tools/bench/proof_oracle.py` picks the route.

With `TACTIX_PROOF_LRAT`, the session replays its input into the proof when
the input closes. In the current implementation this makes
`tactix_read_dimacs` refuse the session (it returns -1); feed the clauses with
`tactix_add_checked` instead.

### Running the default strategy

```c
int result = tactix_solve(s);   /* 10 SAT, 20 UNSAT */
```

`tactix_solve` runs the default strategy from the current state until it has
an answer. If the session is above decision level 0 or holds an unanalyzed
conflict, it first restarts to level 0. Learned clauses, root-level
assignments and simplifications made by earlier actions are kept. The C
interface has no way to interrupt `tactix_solve` or give it a time limit.

The default strategy's settings can be chosen per session:

```c
tactix_set_default_option(s, "TACTIX_FORCE_MODE", "stable");
tactix_set_default_option(s, "TACTIX_SLOT_DISABLE", "Backbone");
```

The names and values are the `TACTIX_*` settings that the command line reads
from the environment (listed in [cli.md](cli.md#environment-variables)); a
session's setting takes precedence over the environment while it solves, and
a `NULL` value removes it. This is how a strategy that hands over to
`tactix_solve` after its own opening tailors the rest of the search; the
cruxmiter strategy in `examples/cruxmiter_policy.c` does so with its
`--default-option=NAME=VALUE` flag.

### Status and model

`tactix_status(s)` returns 0 while the answer is unknown, 10 when the current
formula is satisfied (every active variable assigned at a propagation fixpoint
with no conflict), and 20 when the formula is refuted. A status of 20 also
flushes the proof, so a client that only adds a contradictory input and asks
for the status still gets a complete refutation.

After SAT, `tactix_value(s, lit)` returns `lit` if the literal is true in the
model and `-lit` if it is false. The model is reconstructed for the original
formula: variables removed by elimination, substitution or other
simplifications get values that satisfy every original clause. A variable that
never occurred in the input reads as false (any value works). Before an
answer exists, `tactix_value` also reports unassigned variables as false, so it
cannot be used to test whether a variable is currently assigned.

## 4. Stepwise strategies

### Finding rules by name

Rule ids are stable, but look them up by name rather than hard-coding numbers:

```c
#include <string.h>

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++) {
    const char *candidate = tactix_rule_name(i);
    if (candidate && strcmp(candidate, name) == 0)
      return i;
  }
  return -1;
}
```

`tactix_rule_count()` returns the number of registry slots, `0..count-1`.
`tactix_rule_name(id)` returns the name, or `NULL` for an id out of range.
Slot 39 is retired: it is named `Retired39` and can never be applied.
`tactix_action_kind(id)` returns `TACTIX_ACTION_PROOF_RULE` (0),
`TACTIX_ACTION_POLICY_OP` (1), or -1 for an id out of range or retired.

### Action boundaries

Search actions apply at any decision level. Inprocessing rules apply only at
decision level 0, on the formula simplified by the root-level assignments. A
strategy that wants to inprocess during search therefore first restarts to
level 0 (`Restart` with keep level 0), then applies inprocessing rules, then
resumes search.

| Group | Actions | Boundary |
|---|---|---|
| Search | `Propagate`, `Decide`, `ConflictAnalysis` | any level; `ConflictAnalysis` needs a recorded conflict, `Propagate` needs pending work, `Decide` needs an unassigned variable and no recorded conflict |
| Search management | `Restart`, `Reduce` | any consistent state |
| Heuristic operations | `SwitchMode`, `Rephase`, `Reorder` | any consistent state |
| Clause and variable simplification | `BVE`, `BCE`, `CCE`, `SubsumeForward`, `Strengthen`, `Vivify`, `Instantiate`, `HyperBinaryRes`, `HyperTernaryRes`, `TransitiveReduce`, `Factor`, `Condition` | level 0 |
| Equivalences and probing | `Probe`, `Backbone`, `Congruence`, `Substitute`, `Sweep` | level 0 |
| Symmetry and witnessed redundancy | `SubstitutionRedundancy` | level 0 |
| Model search and phase guidance | `Lucky`, `Walk`, `LocalSearchGuidance` | level 0 |
| Structure-specific reasoning (parity, GF(3), CSP, cardinality and graph encodings) | `ParityConsequence`, `AffineConsequence`, `AffineMatrixInitialize`, `ExactCoverExtension`, `IndependentSetCover`, `SemanticVariableAdvice`, `Gf3AffineConsequence`, `Gf3GaussianConsequence`, `Gf3GeneralRowConsequence`, `DirectCspSupport`, `DirectPhpRefutation`, `TseitinCycleBreaking`, `GraphIntervalEquivalence`, `NaryCspEquality`, `NaryCspEqualityAmo`, `NaryCspEqualityTransitivity` | level 0, plus rule-specific conditions |

The table is a summary. The authoritative answer for the current state is
always `tactix_rules_applicable`, and [rules.md](rules.md) describes each
rule. Several structure-specific rules have no useful canonical application:
they act only on explicit payloads supplied through an online program
(section 5), and some need a provenance stream bound with
`tactix_bind_structural_provenance`.

### Asking what applies

```c
int ids[64];
int n = tactix_rules_applicable(s, ids, 64);   /* ids whose guard holds now */
```

`tactix_rules_applicable` fills the caller's buffer with up to `cap` ids and
returns the count. Every listed action may be applied now. "Applicable" means
the guard holds; an application can still find nothing to do and report a
no-op.

### Instances

```c
uint64_t inst[256];
int k = tactix_rule_instances(s, rule, inst, 256);   /* -1: unknown rule or guard false */
```

`tactix_rule_instances` fills the buffer with up to `cap` payloads, canonical
first, and returns the count. A rule without an instance choice reports
exactly one value, `TACTIX_INSTANCE_CANONICAL`. Enumerated values are a
snapshot: they stay valid until the next call that changes the solver state.

Encodings, from the header:

| Rule | Instance payload |
|---|---|
| `Decide`, `Probe`, `Backbone`, `TransitiveReduce` | a DIMACS literal, encoded with `TACTIX_LIT_INSTANCE(lit)` |
| `BVE` | the enumerated values are `TACTIX_LIT_INSTANCE(v)` for the candidate variables `v` (the cruxmiter example eliminates chosen variables this way) |
| `Restart` | the decision level to keep; 0 is a full restart |
| `Reduce` | the number of learned clauses to delete, ranked by the reduction order; larger values are clamped to all deletable clauses |
| `ConflictAnalysis` | the backjump level, `0..level-1` |
| `Rephase` | 0 best phases, 1 inverted, 2 original |
| `Sweep` | an opaque seed value; enumerate first and apply one of the returned values in the same state |
| `Propagate`, `SwitchMode` | none; only `TACTIX_INSTANCE_CANONICAL` is accepted |
| `Congruence`, `Walk` | currently only the canonical instance |

Always build literal payloads with `TACTIX_LIT_INSTANCE`. It zero-extends the
32-bit literal; a plain cast `(uint64_t)lit` sign-extends `-1` into
`UINT64_MAX`, which is `TACTIX_INSTANCE_CANONICAL`, and would silently give
you the heuristic's choice instead of literal `-1`.

`tactix_rule_semantic_instances` and `tactix_apply_semantic_instance` are a
finer-grained surface for rules whose basic instance groups several choices.
For example, `Instantiate` lists one payload per (clause, literal) pair, and
`Reduce` lists one opaque payload per individually deletable learned clause.
For other rules they behave like the basic calls. One semantic payload is not
enumerated but may be applied: `TACTIX_ANALYZE_TARGET_CHRONO` for
`ConflictAnalysis` chooses the backjump target with chronological
backtracking (keep the trail when the jump would skip more than the installed
number of levels).

### Applying a rule

```c
int r = tactix_apply(s, rule);                         /* canonical instance */
int r = tactix_apply_instance(s, rule, inst[i]);       /* chosen instance */
```

| Return | Meaning |
|---|---|
| 0 | progress: the state changed |
| 1 | no-op: the guard held but there was nothing to do |
| 2 | conflict found (a clause is falsified above level 0); apply `ConflictAnalysis` next |
| 10 | SAT concluded |
| 20 | UNSAT concluded; the proof is flushed |
| -1 | the guard does not hold, the id is unknown or retired, or (for `tactix_apply_instance`) a non-canonical payload was given to a rule without instances |

A -1 is not necessarily an error. `Propagate` returns -1 when propagation is
already at a fixpoint, because its guard requires pending work; the bundled
search loops rely on this. An out-of-range payload on a rule that has
instances is clamped or treated as a no-op, never applied unsoundly.

`tactix_apply_condition(s, clause, n, witness, m)` applies one explicit
`Condition` step: it deletes the live clause `clause` justified by the witness
cube `witness`, and records the cube for model reconstruction (if the deleted
clause is false in a model, the cube's literals are made true). The solver
checks the step before applying it. It returns 0 when the clause was deleted
and -1 when the request is malformed or not justified, in which case nothing
changes.

### Propagation, decisions and conflicts

`Propagate` runs unit propagation to a fixpoint or to the first conflict. A
conflict above level 0 returns 2 and records the conflict; a conflict at level
0 refutes the formula and returns 20.

`ConflictAnalysis` is one atomic step: it analyzes the recorded conflict,
learns a clause (with minimization), and backjumps. Its canonical instance
jumps to the assertion level; other instances choose a level in
`0..level-1`. Its result is 0, or 20 when the empty clause is learned.

`Decide` with the canonical instance takes the decision heuristic's choice
(VMTF in focused mode, scores in stable mode, with saved or target phase).
To branch yourself:

```c
int lits[16];
int n = tactix_decide_candidates(s, lits, 16);   /* unassigned literals, heuristic order */
int pick = n > 0 ? lits[0] : 0;
/* ... replace pick by any unassigned literal you prefer ... */
if (pick && tactix_decide(s, -abs(pick)) != 0)   /* here: always branch negatively */
  /* not applicable: conflict pending, nothing unassigned, or literal assigned */;
```

`tactix_decide_candidates` returns literals with the polarity the heuristic
would choose. You may decide on any unassigned literal, ranked or not.
`tactix_decide(s, lit)` makes the same decision as
`tactix_apply_instance(s, decide_id, TACTIX_LIT_INSTANCE(lit))` and returns 0
or -1; it is a convenience call that skips the per-rule accounting of a
registered application. In stable mode `tactix_decide_candidates` sorts every unassigned
variable by score, so calling it at every decision is expensive on large
formulas; when you only want the heuristic's own pick, apply `Decide`
canonically.

Deciding before propagation has reached a fixpoint is legal, but the usual
loop propagates first and decides only at a fixpoint.

### Restarts, reductions and mode switches

- `Restart` backtracks to a kept level (canonical: 0) and keeps every clause.
  At level 0 a restart changes no assignment.
- `Reduce` deletes learned clauses that are not currently reasons. The
  canonical instance deletes the solver's default fraction; an explicit
  instance names how many. It may also compact clauses at the root.
- `SwitchMode` toggles between focused mode (VMTF, glue-based restarts) and
  stable mode (score-based decisions); `TACTIX_STAT_STABLE` reports the mode.
- `Rephase` resets saved phases (best, inverted or original phases).

These are ordinary actions, not built-in schedules. A strategy that never
applies `Restart` never restarts; [`demo2_no_restarts.c`](../examples/demo2_no_restarts.c)
is exactly that ablation.

The tier boundaries that `Reduce` uses to protect learned clauses can be
recomputed automatically from the glue distribution while conflicts are
analyzed. `tactix_install_retier_schedule` installs that schedule (a struct
`tactix_retier_schedule` with the next deadline, interval growth and per-mode
tier quantiles), and `tactix_read_retier_schedule` reads it back.

### Statistics

`tactix_stat(s, TACTIX_STAT_*)` returns a 64-bit counter, or `UINT64_MAX` for
an unknown id. It allocates nothing and changes nothing, so a strategy can
read counters as often as it likes. The ids fall into four groups:

| Group | Examples | Use |
|---|---|---|
| Search counters | `CONFLICTS`, `DECISIONS`, `PROPAGATIONS`, `TICKS`, `LEARNED`, `RESTARTS`, `REDUCTIONS`, `SWITCHES`, `REPHASED`, `BACKJUMPS` | schedule restarts, reductions and inprocessing by conflict or tick counts |
| Current state | `LEVEL`, `UNASSIGNED`, `STABLE`, `LAST_LEARNED_GLUE` | decision level, unassigned variables, current mode, glue of the last learned clause |
| Formula size | `IRREDUNDANT_CLAUSES`, `BINARY_CLAUSES`, `ROOT_FIXED` | size-scaled intervals; detecting new binaries or new root units |
| Per-technique yield and effort | `BVE_ELIMINATED` with `BVE_RESOLUTIONS`, `VIVIFIED` with `VIVIFY_TICKS`, `PROBE_FAILED` with `PROBE_TICKS`, `SWEEP_EQUIVALENCES` with `SWEEP_TICKS`, `CONGRUENCE_MERGES` with `CONGRUENCE_TICKS`, ... | measure what an inprocessing technique produced and what it cost |

Every inprocessing technique the default strategy schedules exposes such a
pair: a yield counter (clauses removed, variables eliminated, equivalences
found) and an effort counter in that technique's own work unit. Their ratio
is the cost per unit of output, which differs by orders of magnitude between
formulas and is what a strategy needs in order to decide how much effort a
technique deserves. The header lists every id; new ids are only appended.

`tactix_install_tick_attribution(s, TACTIX_STAT_*_TICKS)` additionally
charges the propagation ticks of later applications to one of the probing,
hyper-binary, backbone, vivification or instantiation tick counters, or to
none with `TACTIX_TICK_ATTRIBUTION_NONE`.

### Configuration setters

The `tactix_set_*` calls change rule settings for all later applications.
Every value is sound: settings change which sound applications happen and in
what order, never what a step must prove. They are legal in any state.

| Call | Setting |
|---|---|
| `tactix_set_bve_gates` | which gate types BVE recognizes (`TACTIX_BVE_GATE_*` bitmask: equivalence, AND/OR, if-then-else, XOR, general definitions) |
| `tactix_set_bve_bound` | BVE growth slack: eliminate when resolvents do not exceed occurrences plus the bound |
| `tactix_set_factor_bound`, `tactix_set_factor_composite` | Factor (bounded variable addition) acceptance and composition over extension variables |
| `tactix_set_work_budget` | per-round effort budget for budget-aware rules, in each rule's own unit (Vivify: propagation ticks; Factor: its ticks); 0 is off |
| `tactix_set_vivify_scope`, `tactix_set_vivify_parameters` | which clause band Vivify works on (all, irredundant, or one learned-clause tier) and its reuse, flush and sorting settings |
| `tactix_set_subsume_limits`, `tactix_set_strengthen_limits` | clause-size and occurrence limits of forward subsumption and strengthening |
| `tactix_set_analyze_minimize_depth` | depth limit of learned-clause minimization |
| `tactix_set_congruence_limits`, `tactix_set_congruence_watch_order` | gate arity and count limits of congruence closure; entry ordering |
| `tactix_set_instantiate_limits`, `tactix_set_instantiate_material_threshold` | limits of variable instantiation |
| `tactix_set_backbone_round_limits` | initial and maximum backbone rounds |
| `tactix_set_guidance_publication` | how rule applications update the decision heuristic (below) |

`tactix_config(s, TACTIX_CFG_*)` reads a setting back (`UINT64_MAX` for an
unknown id). Use it to confirm that a setting your strategy depends on is
actually in place.

**Guidance publication.** Inprocessing changes which variables the decision
heuristic should consider: units become fixed, eliminated or substituted
variables disappear, Factor introduces fresh variables. With
`TACTIX_GUIDANCE_DISCARD` (the default) the heuristic is not told: retired
variables stay in its queue and in `tactix_decide_candidates`. With
`TACTIX_GUIDANCE_NATIVE` the heuristic is updated after each application the
way the default strategy updates it. A strategy that mixes inprocessing with
search should set `TACTIX_GUIDANCE_NATIVE`, as both bundled examples do. The
mode affects only the heuristic, never the formula or the proof.

### Installed profiles

`tactix_install_parameters(s, rule, &parameters)` installs a complete,
typed settings profile for one rule family (the same
`tactix_online_parameters` value an online step carries, section 5). It
applies to later canonical applications, to online steps that inherit their
parameters, and to `tactix_solve`. It returns -1, changing nothing, for a
profile that does not match the rule, invalid values, or a family without a
permanent profile.

`tactix_install_work_settings` and `tactix_read_work_settings` set and read
the work budget that budget-aware rules consume (budget, already-spent offset,
whether a zero budget is an explicit zero allowance, whether a rule may stop
mid-round); `fields` names which members to set. `tactix_install_material_settings`
sets two shared filters (`TACTIX_ONLINE_SETTING_INHERIT`, `_DISABLE`,
`_ENABLE`): whether inprocessing restricts itself to variables touched since
its last run, and whether learned clauses likely to survive the next
reduction are marked. `tactix_install_affine_matrix_profile` and
`tactix_read_affine_matrix_result` configure and inspect the persistent GF(2)
(XOR) reasoning session used by `AffineMatrixInitialize`.

### Support transactions

`tactix_support(s, TACTIX_SUPPORT_*, argument)` runs a support transaction:
a lifecycle, storage or heuristic step that the default strategy performs
around rule applications. None is a proof rule. Any unit it derives is
ordinary propagation; the rest changes only storage, heuristic state or
lifecycle. It returns 0 when it changed state, 1 when there was nothing to do,
2 when `SPARSE_COLLECT` left root units for a following `Propagate`, 20 when
the formula is refuted, and -1 (changing nothing) for an unknown support, an
invalid argument or an unmet precondition.

The ones a hand-written strategy is most likely to use:

| Support | Effect |
|---|---|
| `ROOT_PREPARATION` | restart to level 0 without counting a scheduled restart, then propagate at the root (flags `TACTIX_ROOT_PREPARATION_*`) |
| `PREPARATION_RESTART` | backtrack to the level in `argument` without counting a scheduled restart |
| `WARMUP` | run one throw-away assignment in the heuristic's decision order and keep the resulting saved phases (`TACTIX_WARMUP_PHASE_*`) |
| `SPARSE_COLLECT`, `COLLECT_GARBAGE`, `COMPACT_VARIABLES` | remove root-satisfied clauses and false literals, reclaim clause memory, compact variable storage |
| `CLOSE_STARTUP_CERTIFICATE` | end the window in which a complete pre-search refutation may be written as a pseudo-Boolean proof |
| `ENTER_INPROCESS_PHASE`, `OPEN_OCCURRENCE_SESSION`, `LEAVE_INPROCESS_PHASE` | the bracket the default strategy opens around an inprocessing phase: occurrence lists, disconnected watches, cleanup on exit |

The remaining supports (contexts, BVE candidate bookkeeping, retirement
holds, forward-subsumption rounds and others) exist so that the default
strategy can be reproduced exactly through the public interface;
`tests/tactix_public_main.cpp` (built as `tactix-public`) does that and is
the reference for how they compose. The header describes each one.

## 5. Per-invocation settings: online programs

Setters and installed profiles change a rule's settings for every later
application. Often a strategy wants something else: run Vivify now, on
learned clauses only, with an effort limit derived from how much search
happened since the last run; then run it again later on irredundant clauses
with a different limit. **Online programs** give each invocation of a rule
its own settings, effort budget and choice of instances, without disturbing
the installed profile and without restarting or discarding learned state.

### Steps and programs

An online program is a sequence of **steps**. Each step names one rule and
says how to run it:

| Field | Meaning |
|---|---|
| `step_id` | a nonzero id, unique within the program, echoed in results |
| `rule_id` | the rule to run |
| `parameters` | `kind` = `TACTIX_ONLINE_PARAMETERS_INHERIT` to use the installed profile, or the rule's family (`_BVE`, `_VIVIFY`, `_SWEEP`, ...) with its typed settings in `value` |
| `granularity` | `TACTIX_ONLINE_GRANULARITY_CANONICAL`: one canonical application. `_COMPLETE_PASS`: enumerate the rule's current instances and apply each. `_ONE`: exactly one explicit instance. `_BATCH`: a list of explicit instances |
| `instances`, `instance_count` | explicit scalar instances for `_ONE` and `_BATCH` |
| `factor_grids`, `factor_grid_count` | explicit Factor grids (instead of scalar instances) |
| `repeat_mode`, `repeat_limit` | `TACTIX_ONLINE_REPEAT_ONCE`; `_COUNT` (exactly `repeat_limit` times, nonzero); `_FIXPOINT` (until a pass changes nothing; `repeat_limit` 0 means no ceiling); `_SIZE_SCALED_FIXPOINT` (a fixpoint whose round ceiling scales with formula size) |
| `rule_budget_mode`, `rule_budget` | the rule's own effort allowance in its own unit: `TACTIX_ONLINE_RULE_BUDGET_INHERIT`, `_UNLIMITED` (both with `rule_budget` 0), or `_BOUNDED` (zero allowed). The rule checks it at its safe points, so an indivisible step can finish slightly past it |
| `work_unit`, `budget`, `budget_unlimited` | the step's total budget across advances. `work_unit` must be `TACTIX_ONLINE_WORK_BVE_TICKS` for BVE, `_PROPAGATIONS` for Propagate, and `_APPLICATIONS` for every other rule |
| `touch_filter_mode`, `likely_kept_redundant_mode` | per-step overrides of the two material settings (`TACTIX_ONLINE_SETTING_*`; zero inherits) |
| `post_application` | `TACTIX_ONLINE_POST_PROPAGATE` closes every application with propagation inside the same transaction; `_NONE` leaves it to you; `_FORWARD_CLOSURE` is for Strengthen only |

The program adds a `repeat_mode` and `repeat_limit` for the whole sequence.

The typed parameter structures (`tactix_online_vivify_parameters`,
`tactix_online_bve_parameters`, `tactix_online_sweep_parameters`, and so on)
are documented field by field in the header. Do not assume that zero means
"default": for many limits zero means zero work, while fields appended in
later versions use zero to keep earlier behavior. Start from a zeroed struct
and set every field your rule family needs.

### Configure, advance, complete

```c
int tactix_online_configure(tactix *, const tactix_online_program *,
                            uint64_t revision, int reconfigure_mode);
int tactix_online_advance(tactix *, tactix_online_result *);
int tactix_online_complete(tactix *);
uint64_t tactix_online_step_consumed(tactix *, uint64_t step_id);
```

- `tactix_online_configure` validates the program and copies everything it
  needs; your arrays only need to live for the duration of the call. A
  rejected program (-1) changes nothing: the previous program stays and the
  input stays open. A successful configuration closes the input. `revision`
  is your label for this configuration, reported back in each result.
  `TACTIX_ONLINE_RECONFIGURE_RESET` starts from scratch;
  `_RETAIN_BUDGETS` keeps the budget already consumed by steps whose id and
  rule are unchanged.
- `tactix_online_advance` performs at most one atomic rule application and
  describes it in a `tactix_online_result`. It returns -1 only when no program
  is configured; the outcome is in `result.status`. Between advances you may
  apply other rules, read statistics or reconfigure.
- `tactix_online_complete` returns 1 when the program has finished, 0 when it
  has not, and -1 when none is configured.
- `tactix_online_step_consumed` returns the budget a step has used so far.

The result's `status` is one of:

| Status | Meaning |
|---|---|
| `TACTIX_ONLINE_COMPLETED` | an application ran, or the program finished; check `made_rule_progress` |
| `TACTIX_ONLINE_NO_CHANGE` | the application changed nothing |
| `TACTIX_ONLINE_SCOPED_FIXPOINT` | a fixpoint repeat reached its fixpoint |
| `TACTIX_ONLINE_BUDGET_EXHAUSTED` | a budget or repeat ceiling ran out |
| `TACTIX_ONLINE_INAPPLICABLE` | the rule's guard does not hold (for example, not at level 0); nothing was consumed and the program did not move |
| `TACTIX_ONLINE_CONFLICT`, `_SAT`, `_UNSAT` | the application found a conflict, a model, or a refutation |
| `TACTIX_ONLINE_STALE_REQUEST` | explicit instances were invalidated by a formula change; reconfigure |
| `TACTIX_ONLINE_INVALID_REQUEST`, `_RESOURCE_EXHAUSTED`, `_INTERRUPTED`, `_ACCOUNTING_ERROR` | the step could not run as requested |

The result also reports the step and rule ids, the instance applied, the
number of applications and the work in the step's `work_unit`.

Because an inapplicable step does not advance the program, a loop of the form
"advance until complete" must stop on `TACTIX_ONLINE_INAPPLICABLE` (and on the
other non-progress statuses), or it will spin. This helper does that; the
tutorial below reuses it:

```c
/* Advance the configured program until it finishes.
 * Returns 10 or 20 for an answer, 2 for a conflict the caller must analyze,
 * and 0 otherwise. */
static int run_program(tactix *s) {
  tactix_online_result out;
  while (tactix_online_complete(s) == 0) {
    if (tactix_online_advance(s, &out) != 0)
      return 0;
    switch (out.status) {
    case TACTIX_ONLINE_SAT:      return 10;
    case TACTIX_ONLINE_UNSAT:    return 20;
    case TACTIX_ONLINE_CONFLICT: return 2;
    case TACTIX_ONLINE_COMPLETED:
    case TACTIX_ONLINE_NO_CHANGE:
    case TACTIX_ONLINE_SCOPED_FIXPOINT:
      break;                     /* keep going */
    default:
      return 0;                  /* inapplicable, budget, stale, ...: stop this dose */
    }
  }
  return 0;
}
```

### Example: one Vivify dose with its own settings

This sketch runs Vivify once on one clause band, with its own effort limit in
propagation ticks, and propagates after it. It must be called at decision
level 0.

```c
/* Sketch: one Vivify application with per-call settings.
 * scope: 2 = irredundant clauses, 3/4/5 = learned clauses of tier 1/2/3. */
static int vivify_dose(tactix *s, int vivify, unsigned scope,
                       uint64_t ticks, uint64_t revision) {
  tactix_online_step step;
  tactix_online_program program;
  memset(&step, 0, sizeof step);
  step.step_id = 1;
  step.rule_id = vivify;
  step.parameters.kind = TACTIX_ONLINE_PARAMETERS_VIVIFY;
  step.parameters.value.vivify.scope = scope;
  step.parameters.value.vivify.reuse = 1;
  step.parameters.value.vivify.irredundant_sort_ratio = 10; /* must be nonzero */
  step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
  step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  step.repeat_limit = 1;
  step.rule_budget_mode = TACTIX_ONLINE_RULE_BUDGET_BOUNDED;
  step.rule_budget = ticks;                       /* Vivify's unit: propagation ticks */
  step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
  step.budget_unlimited = 1;
  step.post_application = TACTIX_ONLINE_POST_PROPAGATE;

  memset(&program, 0, sizeof program);
  program.steps = &step;
  program.step_count = 1;
  program.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
  program.repeat_limit = 1;

  if (tactix_online_configure(s, &program, revision,
                              TACTIX_ONLINE_RECONFIGURE_RESET) != 0)
    return 0;                                     /* rejected: nothing changed */
  return run_program(s);
}
```

The same pattern works for every rule family: choose the parameter `kind`,
fill its structure, and pick the granularity. A whole inprocessing round is a
program with several steps, for example Probe, then Substitute, then BVE with
a BVE-tick budget, repeated as a sequence until a fixpoint.

For a setting that should hold for every later application rather than one
dose, install it instead: `tactix_install_parameters(s, vivify, &parameters)`
with the same `tactix_online_parameters` value (per-application fields such
as Vivify's band ledger are rejected by an install), or the simpler
`tactix_set_vivify_parameters` and `tactix_set_work_budget`, followed by
`tactix_apply(s, vivify)`.

[`cruxmiter_policy.c`](../examples/cruxmiter_policy.c) has two larger
examples: a graph-symmetry pass of `SubstitutionRedundancy` with its producer,
graph limits and symmetry-chain options, and a deep `Sweep` pass with an
explicit depth, environment size and tick limits. Both use
`TACTIX_ONLINE_GRANULARITY_COMPLETE_PASS` with `TACTIX_ONLINE_POST_PROPAGATE`.

### Explicit structured instances: versioned programs

Some rules take instances that do not fit in 64 bits, such as a clause with a
witness. These are supplied through versioned step types. Each version embeds
the previous one as its `base` member and appends one kind of payload, so a
program of a later version can express everything an earlier one can.

| Function | Step type adds |
|---|---|
| `tactix_online_configure_v2` | substitution-redundancy instances: clause, witness and literal substitution (`tactix_online_sr_instance`) |
| `tactix_online_configure_v3` | `Condition` instances: clause and witness cube |
| `tactix_online_configure_v4` | independent-set covers |
| `tactix_online_configure_v5` | GF(3) equation pairs and direct-CSP support rows |
| `tactix_online_configure_v6` | GF(3) Gaussian-elimination identities |
| `tactix_online_configure_v7` | Tseitin cycle-breaking component identities |
| `tactix_online_configure_v8` | retired slots; both fields must be zero |
| `tactix_online_configure_v9` | certified GF(2) basis rows |
| `tactix_online_configure_v10` | n-ary CSP equality selections |
| `tactix_online_configure_v11` | GF(3) general-row selections |
| `tactix_online_configure_v12` | n-ary CSP equality at-most-one selections |
| `tactix_online_configure_v13` | a Walk objective and admission profile |
| `tactix_online_configure_v14` | direct-CSP guarded-cover selections |
| `tactix_online_configure_v15` | equality-transitivity selections |
| `tactix_online_configure_v16` | a persistent affine-matrix profile |

Use the lowest version that carries what you need. Every version uses the
same `tactix_online_advance` and `tactix_online_complete`.

The Chvátal strategy uses version 2 to merge two variables `a` and `b` that
it has found to be interchangeable. It adds the clause `(a ∨ ¬b)` with witness
`{a}` (and the reverse direction separately) as an explicit
`SubstitutionRedundancy` instance; the rule checks the redundancy condition
itself and writes the witness into the DSR proof:

```c
/* Sketch, adapted from examples/chvatal_policy.c. */
int clause[2] = {a, -b}, witness[1] = {a};
tactix_online_sr_instance instance = {0};
instance.clause = clause;
instance.clause_count = 2;
instance.witness = witness;
instance.witness_count = 1;

tactix_online_step_v2 step;
memset(&step, 0, sizeof step);
step.base.step_id = 1;
step.base.rule_id = rule_id("SubstitutionRedundancy");
step.base.parameters.kind = TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY;
step.base.parameters.value.substitution_redundancy.maximum_validation_ticks =
    UINT64_C(1) << 22;
step.base.granularity = TACTIX_ONLINE_GRANULARITY_ONE;
step.base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
step.base.repeat_limit = 1;
step.base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
step.base.budget_unlimited = 1;
step.sr_instances = &instance;
step.sr_instance_count = 1;

tactix_online_program_v2 program = {&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
tactix_online_result out;
if (tactix_online_configure_v2(s, &program, ++revision,
                               TACTIX_ONLINE_RECONFIGURE_RESET) == 0 &&
    tactix_online_advance(s, &out) == 0 && out.made_rule_progress)
  /* clause added */;
```

A wrong witness is not a soundness problem: the rule rejects the instance and
nothing changes. A formula-changing substitution-redundancy step needs a DSR
proof (`TACTIX_PROOF_DSR_*` or `TACTIX_PROOF_AUTOMATIC`); under other formats
the rule declines.

## 6. Tutorial: building a strategy

The tutorial builds one strategy in three stages. Stage (a) is a complete
program; stages (b) and (c) are sketches that extend it. The bundled examples
named at each stage are complete, tested programs.

### (a) A plain CDCL loop

The loop propagates; on a conflict it analyzes; at a fixpoint it checks for
SAT, then restarts on a Luby schedule, reduces learned clauses periodically,
and decides.

```c
#include "tactix.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); i++) {
    const char *candidate = tactix_rule_name(i);
    if (candidate && strcmp(candidate, name) == 0)
      return i;
  }
  return -1;
}

/* Luby sequence 1, 1, 2, 1, 1, 2, 4, ... for i >= 1. */
static uint64_t luby(uint64_t i) {
  uint64_t k = 1;
  while ((UINT64_C(1) << k) - 1 < i)
    k++;
  if ((UINT64_C(1) << k) - 1 == i)
    return UINT64_C(1) << (k - 1);
  return luby(i - (UINT64_C(1) << (k - 1)) + 1);
}

static int cdcl(tactix *s) {
  const int propagate = rule_id("Propagate");
  const int analyze = rule_id("ConflictAnalysis");
  const int decide = rule_id("Decide");
  const int restart = rule_id("Restart");
  const int reduce = rule_id("Reduce");
  uint64_t restarts = 0, next_restart = 100 * luby(1), next_reduce = 2000;

  for (;;) {
    int r = tactix_apply(s, propagate);       /* -1: already at a fixpoint */
    if (r == 20)
      return 20;
    if (r == 2) {                             /* conflict: learn and backjump */
      if (tactix_apply(s, analyze) == 20)
        return 20;
      continue;
    }
    const int status = tactix_status(s);      /* 10 once everything is assigned */
    if (status)
      return status;

    const uint64_t conflicts = tactix_stat(s, TACTIX_STAT_CONFLICTS);
    if (conflicts >= next_reduce) {
      if (tactix_apply(s, reduce) == 20)
        return 20;
      next_reduce = conflicts + 2000;
    }
    if (conflicts >= next_restart) {
      if (tactix_apply(s, restart) == 20)     /* canonical: back to level 0 */
        return 20;
      next_restart = conflicts + 100 * luby(++restarts + 1);
      continue;
    }
    if (tactix_apply(s, decide) < 0)          /* heuristic's literal */
      return tactix_status(s);
  }
}

/* The DIMACS header's variable count, used only to print the model. */
static int header_variables(FILE *in) {
  char line[4096];
  int variables = 0;
  while (fgets(line, sizeof line, in))
    if (sscanf(line, " p cnf %d", &variables) == 1)
      break;
  rewind(in);
  return variables;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s CNF [PROOF]\n", argv[0]);
    return 1;
  }
  tactix *s = tactix_init();
  if (argc > 2 && tactix_proof(s, argv[2], TACTIX_PROOF_AUTOMATIC) != 0)
    return 1;
  FILE *in = fopen(argv[1], "r");
  if (!in)
    return 1;
  const int variables = header_variables(in);
  if (tactix_read_dimacs(s, in) != 0)
    return 1;
  fclose(in);

  const int result = cdcl(s);
  if (result == 10) {
    puts("s SATISFIABLE");
    printf("v");
    for (int v = 1; v <= variables; v++)
      printf(" %d", tactix_value(s, v));
    printf(" 0\n");
  } else if (result == 20) {
    puts("s UNSATISFIABLE");
  } else {
    puts("s UNKNOWN");
  }
  tactix_release(s);                          /* closes the proof file */
  return result;
}
```

Build it as described in section 2 and check its answers as described in
[getting-started.md](getting-started.md): the proof with
`tools/bench/proof_oracle.py`, the model with `tools/fuzz/checkmodel`.

Things to notice:

- Nothing happens that the loop does not request. No restart, reduction, mode
  switch or inprocessing runs behind the strategy's back.
- The loop never inspects a clause. It observes the solver only through
  return codes and `tactix_stat`.
- The proof needs no attention: every learned clause and deletion is written
  by the rule that produced it.

Related examples: [`demo2_no_restarts.c`](../examples/demo2_no_restarts.c)
is this loop without restarts and reductions;
[`demo1_dpll.c`](../examples/demo1_dpll.c) turns it into DPLL by deleting
learned clauses after every conflict; and
[`demo3_scripted_default.c`](../examples/demo3_scripted_default.c)
reimplements the default strategy's search schedule (glue-based restarts in
focused mode, reluctant doubling in stable mode, mode switching by ticks,
rephasing) outside the library using the same calls.

### (b) Adding one inprocessing rule at restarts

Now let every eighth restart run Vivify at level 0, alternating between
irredundant clauses and tier-2 learned clauses, with an effort limit equal to
a tenth of the propagation ticks spent since the previous dose. This uses
`run_program` and `vivify_dose` from section 5.

First, in `main`, before solving:

```c
tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE);
```

Then replace the restart branch of `cdcl`:

```c
/* Sketch: replaces the restart branch in cdcl() from stage (a).
 * New locals before the loop:
 *   const int vivify = rule_id("Vivify");
 *   uint64_t ticks_at_last_dose = 0, revision = 0; */
if (conflicts >= next_restart) {
  if (tactix_apply(s, restart) == 20)        /* now at level 0 */
    return 20;
  next_restart = conflicts + 100 * luby(++restarts + 1);

  if (restarts % 8 == 0) {
    const uint64_t ticks = tactix_stat(s, TACTIX_STAT_TICKS);
    const uint64_t effort = (ticks - ticks_at_last_dose) / 10;
    const unsigned scope = (restarts / 8) % 2 ? 2 : 4; /* irredundant / tier 2 */
    ticks_at_last_dose = ticks;
    r = vivify_dose(s, vivify, scope, effort, ++revision);
    if (r == 10 || r == 20)
      return r;
    if (r == 2 && tactix_apply(s, analyze) == 20)
      return 20;
  }
  continue;
}
```

Each dose carries its own scope and budget. The installed Vivify profile is
untouched, so a later `tactix_solve` or canonical `tactix_apply(s, vivify)`
still uses the solver's defaults. To see what the doses bought, compare
`TACTIX_STAT_VIVIFIED` (clauses shortened) with `TACTIX_STAT_VIVIFY_TICKS`
(effort) before and after each one.

The same structure takes any root-level rule: replace the Vivify step with a
BVE step under a BVE-tick budget, a Probe pass, or a multi-step program. The
cruxmiter strategy's search profiles restart to level 0 every 10,000
conflicts and run a fixed sequence of structural rules there.

### (c) A custom opening, then the default strategy

The last stage keeps the custom part short and lets the default strategy do
the rest. This opening alternates congruence closure (`Congruence`, which
finds gates with equal inputs) and equivalent-literal substitution
(`Substitute`) at the root for as long as either still finds something, then
calls `tactix_solve`.

```c
/* Sketch, after examples/cruxmiter_policy.c. */
static int opening(tactix *s) {
  const int propagate = rule_id("Propagate");
  const int rules[2] = {rule_id("Congruence"), rule_id("Substitute")};
  for (int round = 0; round < 8; round++) {
    const uint64_t merged = tactix_stat(s, TACTIX_STAT_CONGRUENCE_MERGES);
    const uint64_t substituted = tactix_stat(s, TACTIX_STAT_SUBSTITUTE_VARS);
    for (int i = 0; i < 2; i++) {
      int r = tactix_apply(s, rules[i]);
      if (r == 10 || r == 20)
        return r;
      if (r == 0) {                        /* formula changed: settle it */
        r = tactix_apply(s, propagate);
        if (r == 10 || r == 20)
          return r;
      }
    }
    if (tactix_stat(s, TACTIX_STAT_CONGRUENCE_MERGES) == merged &&
        tactix_stat(s, TACTIX_STAT_SUBSTITUTE_VARS) == substituted)
      break;                               /* neither found anything new */
  }
  return 0;
}

/* in main(), after reading the input: */
tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE);
int result = tactix_status(s);             /* 20 if the input is already refuted */
if (!result)
  result = opening(s);
if (!result)
  result = tactix_solve(s);                /* default strategy from here */
```

`tactix_solve` continues from the state the opening left: the substituted
variables are gone, their equivalences are in the proof, and the model it
reports is reconstructed for the original formula. The opening starts at
level 0, where both rules apply; at level 0 a propagation conflict refutes
the formula, so `Propagate` returns 20 rather than 2.

Two complete strategies follow this pattern:

- [`chvatal_policy.c`](../examples/chvatal_policy.c) finds variables that the
  formula's totalizer encodings duplicate, merges them with explicit
  `SubstitutionRedundancy` instances (section 5), collapses the equivalences
  with `Substitute`, `SubsumeForward` and garbage collection, and then calls
  `tactix_solve`.
- [`cruxmiter_policy.c`](../examples/cruxmiter_policy.c), in its default
  profile, runs a structural root pipeline (congruence and substitution
  rounds, hyper-binary resolution, vivification, strengthening, subsumption,
  bounded BVE, transitive reduction) and then calls `tactix_solve`. Its other
  profiles keep the search loop for themselves, with their own branching rule
  and restart schedule.

A note on proof formats: with `TACTIX_PROOF_AUTOMATIC`, a few complete
refutations that the default strategy can find before any other step (parity
and counting arguments) are written as pseudo-Boolean proofs. An opening that
applies rules first closes that window, and the proof is DSR throughout.

## 7. Proofs and what a strategy can and cannot break

### What the solver guarantees

The guarantees hold for every strategy, including one that applies rules at
random:

- **Every UNSAT answer is certified.** Each rule application writes the proof
  steps that justify it, in the attached format. A strategy never writes proof
  lines and cannot omit them.
- **Every application is sound or refused.** The guard is checked on every
  call. Explicit instances that carry a justification (a substitution-redundancy
  clause and witness, a `Condition` clause and cube, a Factor grid) are
  validated before anything changes; an invalid one is rejected. An
  out-of-range scalar payload is clamped or treated as a no-op.
- **Settings are sound.** Setters, installed profiles and online parameters
  choose which sound applications happen and in what order. No value relaxes
  what a step must prove.
- **Policy operations and support transactions write no proof.** They change
  only heuristic state, storage or lifecycle; any unit they derive is ordinary
  propagation.
- **Every SAT answer has a model of the original formula.** A SAT result from
  `tactix_status`, a rule, or `tactix_solve` means the current formula is
  satisfied; `tactix_value` then reconstructs the values of eliminated,
  substituted and otherwise removed variables.

Some rules support only some proof formats. Formula-changing
substitution-redundancy steps need DSR; several structure-specific rules
decline under LRAT or pseudo-Boolean output. A rule that cannot express its
step in the attached format declines instead of writing an unverifiable
step. Choose `TACTIX_PROOF_AUTOMATIC` or `TACTIX_PROOF_DSR_BINARY` unless you
need a specific format.

The proof file is complete when an action returns 20 (it is flushed at that
point) and is closed by `tactix_release`.

### What a strategy is responsible for

- **Termination and completeness.** The solver does not prevent a strategy
  from looping: deciding nothing, restarting before every conflict, deleting
  every learned clause, or alternating two rules that undo each other's
  effect. A strategy that ends its loop without an answer simply returns
  "unknown".
- **Cost.** Unlimited budgets on expensive rules, very frequent inprocessing,
  or exhaustive instance enumeration on large formulas can make a strategy
  arbitrarily slow. Bound rule effort with budgets and watch the per-technique
  yield and effort counters.
- **Proof size and check time.** Every step is in the proof. A strategy that
  produces many redundant clauses or eliminations produces a larger proof that
  takes longer to check.
- **Heuristic hygiene.** With the default `TACTIX_GUIDANCE_DISCARD`,
  inprocessing does not update the decision heuristic. Set
  `TACTIX_GUIDANCE_NATIVE` when the strategy mixes inprocessing with search.

## 8. Errors, threads and stability

### Return conventions

| Kind of call | Success | Failure |
|---|---|---|
| `int` calls in general | 0 (or a count, or a result code) | -1, with no change to the session |
| `tactix_apply*`, `tactix_decide` | 0, 1, 2, 10, 20 as above | -1: guard false, unknown or retired id, invalid argument |
| `tactix_support` | 0, 1, 2, 20 | -1: unknown support, invalid argument, unmet precondition |
| `uint64_t` readers (`tactix_stat`, `tactix_config`, `tactix_online_step_consumed`) | the value | `UINT64_MAX` for an unknown id (or no program) |
| `tactix_rule_name` | the name | `NULL` for an id out of range |
| `void` setters | | none reported; every value is accepted, except that `tactix_set_vivify_scope` ignores a scope above 5 |
| `tactix_add` | | aborts the process on a late addition |
| `tactix_online_advance` | 0, outcome in `result.status` | -1: no program configured |

Configuration calls are transactional: a rejected `tactix_online_configure*`,
`tactix_install_*` or `tactix_set_vivify_parameters` call leaves the previous
settings in place. A malformed stream passed to `tactix_read_dimacs` is the
one failure that leaves the session unusable.

The fields `rule_result`, `budget_status` and `work_unit` of
`tactix_online_result` use internal encodings that the header does not
specify; rely on `status`, `made_rule_progress`, `applications` and `work`.

### Threads

The header makes no thread-safety guarantee. Use a session from one thread at
a time, and do not call into a session while another call on it is running.

### Stability

The interface is version 1 and frozen; later changes are additive only. In
practice:

- Existing functions keep their signatures and meanings, and existing
  constants keep their values.
- New functions, statistics ids, configuration ids, support ids and rule
  families are appended. Rule ids are never renumbered; a retired rule keeps
  its slot (slot 39).
- Structures change only by appending fields or by adding a new versioned
  type (`tactix_online_step_v2`, `..._v3`, ...). Appended fields are defined
  so that a zero value keeps the earlier behavior, which is why the examples
  zero every structure with `memset` before filling it.

Look rules up by name, treat unknown statistics as `UINT64_MAX`, and zero
your structures, and a strategy written against this version keeps compiling
and behaving the same against later ones.

## 9. Function reference

All 69 public functions, grouped by area. `TACTIX_LIT_INSTANCE(lit)` is a
macro, not a function.

**Session and input**

| Function | Description |
|---|---|
| `tactix_init` | Create a session. |
| `tactix_release` | Free a session and close its proof file. |
| `tactix_add` | Add a literal or end a clause (0); aborts after input has closed. |
| `tactix_add_checked` | As `tactix_add`, but return -1 after input has closed. |
| `tactix_read_dimacs` | Read a complete DIMACS CNF stream and close the input. |
| `tactix_proof` | Attach a proof file in a `TACTIX_PROOF_*` format, before input closes. |
| `tactix_bind_structural_provenance` | Bind a writable stream for provenance records of structural rules (`NULL` unbinds). |

**Solving and results**

| Function | Description |
|---|---|
| `tactix_solve` | Run the default strategy from the current state to an answer: 10 or 20. |
| `tactix_status` | 0 unknown, 10 satisfied, 20 refuted. |
| `tactix_value` | Model value of a literal after SAT: the literal if true, its negation if false. |
| `tactix_stat` | Read a `TACTIX_STAT_*` counter. |

**Rule discovery**

| Function | Description |
|---|---|
| `tactix_rule_count` | Number of rule registry slots. |
| `tactix_rule_name` | Name of a rule id. |
| `tactix_action_kind` | Proof rule or policy operation. |
| `tactix_rules_applicable` | Ids of the rules whose guard holds now. |
| `tactix_rule_instances` | Instance payloads of a rule, canonical first. |
| `tactix_rule_semantic_instances` | Finer-grained instance payloads of a rule. |

**Applying actions**

| Function | Description |
|---|---|
| `tactix_apply` | Apply a rule's canonical instance. |
| `tactix_apply_instance` | Apply a chosen instance. |
| `tactix_apply_semantic_instance` | Apply a fine-grained instance. |
| `tactix_apply_condition` | Delete one clause justified by an explicit witness cube (`Condition`). |
| `tactix_decide_candidates` | Unassigned literals in the decision heuristic's order. |
| `tactix_decide` | Branch on any unassigned literal. |

**Configuration**

| Function | Description |
|---|---|
| `tactix_set_bve_gates` | Gate types BVE may recognize. |
| `tactix_set_bve_bound` | BVE clause-growth bound. |
| `tactix_set_factor_bound` | Factor acceptance bound. |
| `tactix_set_factor_composite` | Whether Factor composes over extension variables. |
| `tactix_set_work_budget` | Per-round budget of budget-aware rules. |
| `tactix_set_vivify_scope` | Clause band Vivify works on. |
| `tactix_set_vivify_parameters` | Complete Vivify settings (scope, reuse, flush, sort ratio). |
| `tactix_set_subsume_limits` | Forward subsumption size and occurrence limits. |
| `tactix_set_strengthen_limits` | Strengthening size and occurrence limits. |
| `tactix_set_analyze_minimize_depth` | Learned-clause minimization depth. |
| `tactix_set_congruence_limits` | Congruence closure gate limits. |
| `tactix_set_congruence_watch_order` | Congruence entry ordering. |
| `tactix_set_instantiate_limits` | Instantiation limits. |
| `tactix_set_instantiate_material_threshold` | Instantiation new-material threshold. |
| `tactix_set_backbone_round_limits` | Backbone round limits. |
| `tactix_set_guidance_publication` | Whether applications update the decision heuristic (`DISCARD` or `NATIVE`). |
| `tactix_config` | Read a `TACTIX_CFG_*` setting back. |

**Installed profiles and work settings**

| Function | Description |
|---|---|
| `tactix_install_parameters` | Install a typed settings profile for a rule family. |
| `tactix_install_material_settings` | Set the touched-variable filter and likely-kept marking. |
| `tactix_install_work_settings` | Set budget, offset, bounded and mid-round work settings. |
| `tactix_read_work_settings` | Read the work settings back. |
| `tactix_install_tick_attribution` | Charge later propagation ticks to a technique's tick counter. |
| `tactix_install_retier_schedule` | Install the learned-clause tier recomputation schedule. |
| `tactix_read_retier_schedule` | Read that schedule back. |
| `tactix_install_affine_matrix_profile` | Install the persistent GF(2) matrix profile. |
| `tactix_read_affine_matrix_result` | Read the last GF(2) matrix initialization result and session state. |

**Online programs**

| Function | Description |
|---|---|
| `tactix_online_configure` | Configure a program of scalar-instance steps. |
| `tactix_online_configure_v2` | ... with substitution-redundancy instances. |
| `tactix_online_configure_v3` | ... with `Condition` instances. |
| `tactix_online_configure_v4` | ... with independent-set covers. |
| `tactix_online_configure_v5` | ... with GF(3) equation pairs and direct-CSP support rows. |
| `tactix_online_configure_v6` | ... with GF(3) Gaussian identities. |
| `tactix_online_configure_v7` | ... with Tseitin cycle identities. |
| `tactix_online_configure_v8` | ... with the retired v8 slots (must be zero). |
| `tactix_online_configure_v9` | ... with GF(2) basis rows. |
| `tactix_online_configure_v10` | ... with n-ary CSP equality selections. |
| `tactix_online_configure_v11` | ... with GF(3) general-row selections. |
| `tactix_online_configure_v12` | ... with equality at-most-one selections. |
| `tactix_online_configure_v13` | ... with a Walk profile. |
| `tactix_online_configure_v14` | ... with direct-CSP guarded-cover selections. |
| `tactix_online_configure_v15` | ... with equality-transitivity selections. |
| `tactix_online_configure_v16` | ... with a persistent affine-matrix profile. |
| `tactix_online_advance` | Perform at most one atomic application of the program. |
| `tactix_online_complete` | Whether the configured program has finished. |
| `tactix_online_step_consumed` | Budget consumed so far by a step. |

**Support transactions**

| Function | Description |
|---|---|
| `tactix_support` | Run a `TACTIX_SUPPORT_*` lifecycle, storage or heuristic transaction. |
