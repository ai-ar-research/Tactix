# The C interface

[`include/tactix.h`](../include/tactix.h) lets a C or C++ program use Tactix
as a library: to solve formulas with the built-in strategy, or to control the
solver step by step with a strategy of its own.
[strategies.md](strategies.md) shows how to compile such a program.

## Solving with the built-in strategy

```c
#include "tactix.h"
#include <stdio.h>

int main(int argc, char **argv) {
  tactix *solver = tactix_init();
  tactix_proof(solver, "proof", TACTIX_PROOF_PBP);   /* optional */
  FILE *in = fopen(argv[1], "r");
  if (!in || tactix_read_dimacs(solver, in) != 0) return 1;
  fclose(in);
  int result = tactix_solve(solver);                 /* 10 SAT, 20 UNSAT */
  if (result == 10)
    printf("x1 is %s\n", tactix_value(solver, 1) > 0 ? "true" : "false");
  tactix_release(solver);
  return result;
}
```

A **session** (`tactix *`) holds one formula and the solver's state. Clauses
can also be added one literal at a time with `tactix_add`, as in DIMACS: a
positive or negative variable number, and 0 to end the clause.

## Writing your own strategy

The solver's techniques are available as **rules**: unit propagation,
branching on a variable (`Decide`), learning from a conflict
(`ConflictAnalysis`), restarts, deleting learned clauses (`Reduce`), and
dozens of simplification techniques. A strategy is a loop that, at each step,
asks which rules can be applied, chooses one and applies it:

| Call | What it does |
|---|---|
| `tactix_rule_count`, `tactix_rule_name` | list the rules (find a rule's number by its name once) |
| `tactix_rules_applicable` | the rules that can be applied now |
| `tactix_apply` | apply a rule in its default way |
| `tactix_rule_instances`, `tactix_apply_instance` | list a rule's choices (for example, which variable to branch on) and apply one |
| `tactix_status` | 10 satisfiable, 20 unsatisfiable, 0 not decided yet |
| `tactix_stat` | counters such as conflicts, decisions or simplifications so far |
| `tactix_solve` | hand over to the built-in strategy, from the current state |
| `tactix_set_default_option` | configure the built-in strategy before handing over |

`tactix_apply` returns 0 when the rule changed something, 1 when it changed
nothing, 2 when it found a conflict, 10 or 20 when the formula is decided, and
-1 when the rule cannot be applied in the current state.

The solver does nothing on its own: only the rules the strategy applies run.
A common pattern is a short specialized opening that exploits the structure of
a family of formulas, followed by `tactix_solve`.

## Answers are always checkable

Before applying a rule, the solver checks that the rule is sound in the
current state, and refuses it (returning -1) otherwise. Every applied rule
writes its own steps to the proof. So whatever a strategy does, an
unsatisfiable answer comes with a proof that the checkers accept, and a
satisfiable answer comes with a satisfying assignment. A poor strategy can be
slow or never finish, but it cannot produce a wrong answer.

## More detail

- [strategies.md](strategies.md): example strategies, and how to compile and
  run your own.
- [dev/c-api-reference.md](dev/c-api-reference.md): the complete guide, with
  a tutorial and every function.
- [dev/rules.md](dev/rules.md): every rule, when it applies and what it does.
- [`include/tactix.h`](../include/tactix.h): the exact contract of each
  function.
