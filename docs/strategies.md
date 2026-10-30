# Writing strategies

A *strategy* is a C program that decides which of the solver's techniques
(rules) to apply next, through [`include/tactix.h`](../include/tactix.h).
[c-api.md](c-api.md) introduces the interface; this page walks through two
example strategies and shows how to compile and run your own.

The interface is large: 46 rules, many with their own settings. You
do not need most of it to start. Both examples use a handful of rules and
hand the rest of the work to the built-in strategy.

## 1. General scheduling shape

A complete strategy normally follows this grammar, where `R` is a
simplification, structural-reasoning, or local-search action:

```text
R* (Search RestartToRoot R*)*
Search ::= Decide (Propagate | ConflictAnalysis | Decide
          | Reduce | Rephase | Reorder | SwitchMode | PartialRestart)*
```

Root work runs only at decision level zero.  A full `Restart(0)` ends one
search epoch before more root work; a positive keep-level is a partial restart
inside the current epoch.  Conflict analysis and propagation take priority
when their guards hold.  `tactix_rules_applicable` checks whether the next
grammar-selected action is legal in the current state; it does not define the
strategy's possible sequences.  Every technique in such a strategy is a
public rule or policy operation, with its legal boundary exposed by the same
interface.

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

- **The loop is the strategy.** Nothing runs unless the program applies a
  rule.
- **Rules are found by name** (`rule_id("Propagate")` in the example), so the
  program keeps working when rules are added.
- **Learning is a choice.** `ConflictAnalysis` learns a clause from each
  conflict. Deleting every learned clause right away with `Reduce` gives DPLL;
  keeping them gives conflict-driven clause learning (CDCL).
- **The proof is automatic.** Every applied rule writes its proof steps, so
  even this loop produces checkable proofs.

## 3. A strategy for one family of formulas: cruxmiter

The paper's first case study is a family of formulas that ask whether two
differently built multiplier circuits can compute different results. They are
hard for standard solvers, but the two circuits contain many gates that
compute the same function.
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

`gate_closure` alternates two rules for two to four rounds: `Congruence`
finds gates with the same inputs, so their outputs are equal, and
`Substitute` replaces each such signal by a single representative. It stops
early when a round finds nothing new. `Backbone` then fixes the variables that
must take one value in every solution.

Before handing over, the program also configures the built-in strategy with
`tactix_set_default_option`. For this family it stays in one search mode and
uses `Vivify` as its only simplification technique (eight settings in all),
for example:

```c
tactix_set_default_option(solver, "TACTIX_FORCE_MODE", "stable");
tactix_set_default_option(solver, "TACTIX_SLOT_PERMILLE", "Vivify:200");
```

On the paper's ten formulas of this family, the strategy was about three
times faster than the built-in strategy and Kissat, with proofs about a third
the size. The pattern carries over: find the structure of a family, write a
short opening that exploits it with a few rules, and let the built-in strategy
do the rest. The paper's second case study,
[`examples/chvatal_policy.c`](../examples/chvatal_policy.c), follows it too.

## 4. Compiling and running your own strategy

Start from an example, for instance `examples/demo1_dpll.c` (a complete
search) or `examples/cruxmiter_policy.c` (an opening, then the built-in
strategy, with input and output handled).

**Inside the Tactix source tree.** Put the file in `examples/`, add two lines
to `examples/CMakeLists.txt`:

```cmake
add_executable(my-strategy my_strategy.c)
target_link_libraries(my-strategy PRIVATE tactix_lib)
```

and build it:

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

(A CMake project can use `find_package(Tactix)` and link `Tactix::tactix`
instead; see [dev/install.md](dev/install.md).)

Check the answers as for the solver itself ([cli.md](cli.md)):

```sh
python3 tools/bench/proof_oracle.py check formula.cnf proof   # unsatisfiable
python3 tools/fuzz/checkmodel formula.cnf out.txt             # satisfiable, with the output saved in out.txt
```

A strategy cannot produce a wrong answer by choosing badly: the solver
refuses a rule that is not sound in the current state, and records every
applied rule in the proof. A poor strategy is slow, or never finishes.

## 5. Settings for a single application

`tactix_apply` applies a rule with its default settings. To give one
application its own settings, such as a work budget or a random seed, a
strategy submits a short *online program* with `tactix_online_configure`; the
program format is documented in `include/tactix.h`.

`Walk` offers two public algorithms. `TACTIX_WALK_ALGORITHM_PROBSAT` is the
ordinary break-score walk. `TACTIX_WALK_ALGORITHM_CONFIGURATION_CHECKING`
uses weighted make-minus-break scores, configuration checking, aspiration and
clause-weight smoothing; its initialization may use current best phases, all
negative phases, or random phases. The online Walk parameters bound its tries,
flips and total work. Walk is applicable only at its declared consistent root
boundary with search heuristics attached, and does not rewrite the formula: it
publishes a better assignment as search phases, while an exact assignment lets
the following search close SAT.
Configuration checking derives a flipped variable's neighbors lazily from the
clause-occurrence table; it does not materialize the quadratic primal-graph
clique of a wide clause.
This makes it suitable as a bounded opening or between search epochs without
creating a private strategy mechanism.

## Where to go next

- [`examples/`](../examples/): more strategies, from CDCL without restarts
  (`demo2_no_restarts.c`) to the built-in schedule written outside the library
  (`demo3_scripted_default.c`).
- [`include/tactix.h`](../include/tactix.h): the complete public interface and
  per-invocation online-program format.
