# Writing strategies: a guided tour

Tactix features a public interface that allows users to program their own
solving strategies without modifying the internals of the solver. Users can
invoke propagation, decisions, conflict analysis, restarts,
clause-database reduction and dozens of inprocessing and structural reasoning
techniques. We define a *strategy* as a program that decides which rule
to apply next, and with which settings, through the C interface in
[`include/tactix.h`](../include/tactix.h). The solver checks each rule's
soundness precondition and records every step in the proof, so any strategy
produces checkable answers.

This page walks through three strategies of increasing size:

1. [The model](#1-the-model): sessions, rules, results.
2. [DPLL in sixty lines](#2-dpll-in-sixty-lines): `examples/demo1_dpll.c`.
3. [A domain-specific strategy](#3-a-domain-specific-strategy-cruxmiter):
   `examples/cruxmiter_policy.c`, from the paper's case study.
4. [Settings per application](#4-settings-per-application-online-programs):
   online programs, with local search as the example.
5. [Proofs](#5-proofs-what-you-can-and-cannot-break).

### A note on the learning curve

The interface is large. There are 46 rules, many with per-application
settings, plus statistics, configuration setters, online programs, and the
default strategy's own options. That is deliberate: a strategy can express
what the solver's own default strategy does, which means reimplementing
Kissat- and CaDiCaL-style scheduling, symmetry breaking and structural
reasoning, not just turning a few knobs. The price is a learning curve. You
do not need most of the interface to start. The strategies below use a
handful of rules and hand everything else to the default strategy. Grow from
there. The reference documents are
[c-api.md](c-api.md) (the interface, with a step-by-step tutorial in
section 6), [rules.md](rules.md) (every rule, its precondition and proof
steps) and [cli.md](cli.md) (the default strategy's settings).

## 1. The model

A session (`tactix *`) holds a formula, the solver state and an optional
proof. You add clauses (`tactix_add`, or `tactix_read_dimacs`), attach a proof
file (`tactix_proof`), and then either call `tactix_solve`, which runs the
built-in default strategy to completion, or drive the solver yourself:

| Call | Meaning |
|---|---|
| `tactix_rule_count`, `tactix_rule_name` | the rules; look them up by name once |
| `tactix_rules_applicable(s, ids, cap)` | which rules' preconditions hold now |
| `tactix_rule_instances(s, rule, out, cap)` | the choices a rule offers (e.g. which variable to decide), canonical first |
| `tactix_apply(s, rule)` | apply a rule's canonical instance |
| `tactix_apply_instance(s, rule, choice)` | apply a chosen instance |
| `tactix_status(s)` | 10 SAT, 20 UNSAT, 0 undecided |
| `tactix_stat(s, TACTIX_STAT_...)` | counters: conflicts, decisions, merges, ... |
| `tactix_solve(s)` | hand over to the default strategy, from the current state |

`tactix_apply` returns 0 when the rule made progress, 1 when it was a no-op,
2 when propagation found a conflict, 10 or 20 when the formula is decided,
and -1 when the rule's precondition does not hold. The precondition is the
only thing the solver insists on. Within it, any order is sound.

## 2. DPLL in sixty lines

[`examples/demo1_dpll.c`](../examples/demo1_dpll.c) is the classic DPLL loop,
written with four rules:

```c
for (;;) {
  if (tactix_status(s)) return tactix_status(s);
  int ids[32];
  const int n = tactix_rules_applicable(s, ids, 32);
  if (has(ids, n, propagate)) {                    /* unit propagation first */
    if (tactix_apply(s, propagate) == 20) return 20;
    continue;
  }
  if (has(ids, n, conflictanalysis)) {             /* a conflict: back up */
    if (tactix_apply(s, conflictanalysis) == 20) return 20;
    tactix_apply_instance(s, reduce, UINT64_MAX - 1); /* forget what was learned */
    continue;
  }
  tactix_apply(s, decide);                         /* otherwise branch */
}
```

What to notice:

- **The loop is the strategy.** Tactix does not run a search loop behind your
  back. Nothing happens unless the program applies a rule.
- **Rules are found by name** (`rule_id("Propagate")` in the example), so a
  strategy keeps working when rules are added.
- **"No learning" is a choice, not a different solver.** Conflict analysis is
  one rule (it learns a clause and backjumps). DPLL is recovered by
  immediately applying `Reduce` with a victim count that removes every
  learned clause. Swap that line for a reduction schedule and you have CDCL.
  The tutorial in [c-api.md section 6](c-api.md#6-tutorial-building-a-strategy)
  does exactly that, step by step, then adds restarts and inprocessing.
- **The proof is automatic.** Every applied rule writes its proof steps, so
  this toy loop already produces checkable UNSAT proofs.

Build and run it:

```sh
cmake --preset release && cmake --build --preset release --target demo1_dpll
build/release/examples/demo1_dpll        # prints "demo1_dpll OK"
```

## 3. A domain-specific strategy: cruxmiter

The paper's first case study is a family of multiplier-equivalence miters
(`cruxmiter`): two randomly shaped adder trees over the same inputs, and a
miter asking whether their sums can differ. Resolution-based solvers find
these hard, but the formulas contain many gates that compute the same
function. [`examples/cruxmiter_policy.c`](../examples/cruxmiter_policy.c)
(about 250 lines, most of them input and output) exploits that with an
*opening* that runs before the default strategy:

```c
static int solve(struct policy *policy) {
  uint64_t applications = 0;
  int result;
  if (tactix_status(policy->solver) == 20) return 20;
  result = propagate(policy);                    /* root unit propagation */
  if (terminal(result)) return result;
  if (result == 2) return 20;
  result = gate_closure(policy, &applications);  /* Congruence + Substitute rounds */
  if (terminal(result)) return result;
  if (result == 2) return 20;
  result = dose_rule(policy, policy->backbone, 512, &applications);
  if (terminal(result)) return result;
  if (result == 2) return 20;
  return tactix_solve(policy->solver);           /* hand over to the default strategy */
}
```

`dose_rule` applies a rule at most a given number of times, settling each
change with unit propagation before the next application looks at the
formula. `gate_closure` alternates the two rules that do the work on gate
CNFs: `Congruence` finds gates with equal inputs and adds the equivalence of
their outputs, `Substitute` replaces each literal by its representative.
It runs at least two and at most four rounds, and stops early when a round
finds nothing new:

```c
for (round = 0; round < 4; ++round) {
  merges_before = tactix_stat(solver, TACTIX_STAT_CONGRUENCE_MERGES);
  substitutions_before = tactix_stat(solver, TACTIX_STAT_SUBSTITUTE_VARS);
  dose_rule(policy, policy->congruence, 4, applications);
  dose_rule(policy, policy->substitute, 24, applications);
  if (round + 1 >= 2 && /* no new merges and no new substitutions */ ...)
    break;
}
```

Before solving, the program also *configures* the default strategy it hands
over to, through `tactix_set_default_option`: stable mode throughout, and
vivification as the only inprocessing technique, which was the one that paid
off on this family:

```c
static const char *const settings[][2] = {
    {"TACTIX_FORCE_MODE", "stable"},       {"TACTIX_REORDER", "0"},
    {"TACTIX_NO_BVE", "1"},                {"TACTIX_CONFLICT_REUSE", "0"},
    {"TACTIX_SLOT_PERMILLE", "Vivify:200"}, {"TACTIX_NO_FACTOR", "1"},
    {"TACTIX_NO_SWEEP", "1"},              {"TACTIX_SLOT_DISABLE", "Backbone"},
};
for (i = 0; i < sizeof settings / sizeof *settings; ++i)
  tactix_set_default_option(solver, settings[i][0], settings[i][1]);
```

```sh
build/release/examples/tactix-cruxmiter-policy formula.cnf proof   # DRAT proof
```

On the ten size-32 instances, this strategy was about three times faster
than both the default strategy and Kissat 4.0.4, with proofs about a third
the size (paper, Table 4). The pattern carries over to other families: find
the structure, write a short opening that exploits it with a few rules, and
let the default strategy do the rest.
[`examples/chvatal_policy.c`](../examples/chvatal_policy.c), the paper's
second case study, follows the same pattern. It finds counter variables that
the formula's totalizer encodings duplicate, merges them with explicit
substitution-redundancy steps, and then calls `tactix_solve`.

## 4. Settings per application: online programs

`tactix_apply` uses a rule's canonical settings. To give one application its
own settings, such as a step budget, a seed, or an effort limit, a strategy
submits a short *online program*: a sequence of steps, each naming a rule,
its parameters and how often to run it. Local search (`Walk`, a ProbSAT-style
search that saves its best assignment as the phases later decisions follow)
with a budget of one million flips and seed 7:

```c
tactix_online_step step = {0};
step.step_id = 1;
step.rule_id = rule_id("Walk");
step.parameters.kind = TACTIX_ONLINE_PARAMETERS_WALK;
step.parameters.value.walk.step_budget = 1000000;
step.parameters.value.walk.random_seed = 7;
step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
step.repeat_limit = 1;
step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
step.budget = 1;
tactix_online_program program = {.steps = &step, .step_count = 1,
                                 .repeat_mode = TACTIX_ONLINE_REPEAT_ONCE, .repeat_limit = 1};
tactix_online_configure(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET);
tactix_online_result out = {0};
while (!tactix_online_complete(s)) {
  tactix_online_advance(s, &out);
  if (out.status == TACTIX_ONLINE_SAT) return 10;  /* the walk found a model */
}
```

[c-api.md section 5](c-api.md#5-per-invocation-settings-online-programs)
documents the parameters of every rule that has them.

## 5. Proofs: what you can and cannot break

A strategy cannot produce a wrong answer by choosing badly. Every rule checks
its precondition, and a step whose precondition fails returns -1 and changes
nothing. Every applied step is recorded in the proof: pseudo-Boolean (PBP)
by default, or DSR (DRAT plus substitution redundancy) or DRAT on request.
A bad strategy is slow, not unsound. To check an answer:

```sh
tools/checkers/setup.sh                                     # once
my-strategy formula.cnf proof > out.txt
python3 tools/fuzz/checkmodel formula.cnf out.txt           # SAT: the model
python3 tools/bench/proof_oracle.py check formula.cnf proof # UNSAT: the proof
```

What a strategy *is* responsible for is termination and efficiency (see
[c-api.md section 7](c-api.md#7-proofs-and-what-a-strategy-can-and-cannot-break)).

## Where to go next

- [c-api.md](c-api.md): the full interface, with a tutorial that grows a
  plain CDCL loop into a strategy with inprocessing and a custom opening.
- [rules.md](rules.md): every rule, its precondition, effect and proof steps;
  [proof-rules.html](proof-rules.html) is an interactive version.
- [cli.md](cli.md): the default strategy's settings (`TACTIX_*`), which
  `tactix_set_default_option` accepts.
- [`examples/`](../examples/): `demo2_no_restarts.c` (CDCL without
  restarts), `demo3_scripted_default.c` (the default schedule re-implemented
  outside the library), `random3sat_policy.c`, `eevbnn_policy.c`,
  `chvatal_policy.c`, `cruxmiter_policy.c`.
