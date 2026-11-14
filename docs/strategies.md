# Writing strategies

A *strategy* is a C program that decides which of the solver's techniques
(rules) to apply next, through [`include/tactix.h`](../include/tactix.h).
[c-api.md](c-api.md) introduces the interface; this page walks through two
example strategies, each using a handful of rules, and shows how to compile and
run your own.

## 1. General scheduling shape

A complete strategy normally follows this grammar, where `R` is a
simplification, structural-reasoning, or local-search action:

```text
R* (Search RestartToRoot R*)*
Search ::= Decide (Propagate | ConflictAnalysis | Decide
          | Reduce | Rephase | Reorder | SwitchMode | PartialRestart)*
```

Root work runs only at decision level zero. A full `Restart(0)` ends one search
epoch before more root work; a positive keep-level is a partial restart inside
the current epoch. Conflict analysis and propagation take priority when their
guards hold. `tactix_rules_applicable` checks whether the next grammar-selected
action is legal now; it does not define which sequences a strategy may take.
Every technique in such a strategy is a public rule or policy operation whose
legal boundary the interface exposes.

## 2. DPLL in sixty lines

[`examples/demo1_dpll.c`](../examples/demo1_dpll.c) is the classic DPLL
search (branch, propagate, back up on a conflict) written with four rules:

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

Nothing runs unless the loop applies a rule. Rules are found by name
(`rule_id("Propagate")`), so the program keeps working when rules are added.
`ConflictAnalysis` learns a clause from each conflict: deleting every learned
clause right away with `Reduce` gives DPLL, keeping them gives CDCL. Every
applied rule writes its proof steps, so even this loop produces checkable
proofs.

## 3. A strategy for one family of formulas: cruxmiter

The paper's first case study asks whether two differently built multiplier
circuits can compute different results. These formulas are hard for standard
solvers, but the circuits share many gates that compute the same function.
[`examples/cruxmiter_policy.c`](../examples/cruxmiter_policy.c) exploits this
with a short opening before the built-in strategy:

```c
static int solve(struct policy *policy) {
  ...
  result = propagate(policy);                    /* unit propagation */
  ...
  result = gate_closure(policy, &applications);  /* merge equivalent gates */
  ...
  result = dose_rule(policy, policy->backbone, 512, &applications);
  ...
  return tactix_solve(policy->solver);           /* hand over to the built-in strategy */
}
```

`gate_closure` alternates two rules for two to four rounds, stopping early when
a round finds nothing new: `Congruence` finds gates with the same inputs, so
their outputs are equal, and `Substitute` replaces each such signal by one
representative. `Backbone` then fixes the variables that take one value in
every solution. Before handing over, the program configures the built-in
strategy with `tactix_set_default_option` (eight settings) to stay in one search
mode with `Vivify` as its only simplification technique, for example:

```c
tactix_set_default_option(solver, "TACTIX_FORCE_MODE", "stable");
tactix_set_default_option(solver, "TACTIX_SLOT_PERMILLE", "Vivify:200");
```

On the paper's ten formulas of this family, the strategy was about three times
faster than the built-in strategy and Kissat, with proofs about a third the
size. The paper's second case study,
[`examples/chvatal_policy.c`](../examples/chvatal_policy.c), follows the same
pattern: find a family's structure, exploit it with a short opening of a few
rules, and let the built-in strategy do the rest.

## 4. Choosing Boolean extension variables

Strategies can introduce exact AND/OR/XOR definitions, retain them through
startup elimination, and raise their decision priority before ordinary solve.
See [extended resolution](extended-resolution.md) for the API and the
[PHP strategy](../examples/php_er_policy.cpp).

## 5. Compiling and running your own strategy

Start from an example such as `examples/demo1_dpll.c` (a complete search) or
`examples/cruxmiter_policy.c` (an opening, then the built-in strategy, with
input and output handled). **Inside the Tactix source tree,** put the file in
`examples/`, add two lines to `examples/CMakeLists.txt` and build it:

```cmake
add_executable(my-strategy my_strategy.c)
target_link_libraries(my-strategy PRIVATE tactix_lib)
```

```sh
cmake --preset release
cmake --build --preset release --target my-strategy
build/release/examples/my-strategy formula.cnf proof
```

**Anywhere else,** against an installed Tactix:

```sh
cmake --install build/release --prefix $HOME/.local      # once, from the Tactix tree
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig           # or lib64, depending on the system
cc -O2 -o my-strategy my_strategy.c $(pkg-config --cflags --libs tactix)
./my-strategy formula.cnf proof
```

A CMake project can use `find_package(Tactix)` and link `Tactix::tactix`.
Check the answers as for the solver itself ([cli.md](cli.md)).

## 6. Settings for a single application

`tactix_apply` applies a rule with its default settings. To give one
application its own settings, such as a work budget or a random seed, a
strategy submits a short *online program* with `tactix_online_configure`; the
program format is documented in `include/tactix.h`.

`Walk` offers two algorithms: the ProbSAT break-score walk
(`TACTIX_WALK_ALGORITHM_PROBSAT`) and configuration-checking local search
(`TACTIX_WALK_ALGORITHM_CONFIGURATION_CHECKING`) with weighted
make-minus-break scores, aspiration and clause-weight smoothing, initialized
from the current best, all-negative or random phases. Its online parameters
bound tries, flips and total work. Walk applies only at its consistent root
boundary with search heuristics attached. It leaves the formula unchanged and
publishes a better assignment as search phases; an exact assignment lets the
following search close SAT.

## Where to go next

[`examples/`](../examples/) has more strategies, from CDCL without restarts
(`demo2_no_restarts.c`) to the demo3 search scheduler
(`demo3_scripted_default.c`); [`include/tactix.h`](../include/tactix.h) is the
complete public interface.
