# Example strategies

Each example is a client of [`include/tactix.h`](../include/tactix.h) alone.
[docs/strategies.md](../docs/strategies.md) walks through writing one.

| Example | What it shows |
|---|---|
| `demo1_dpll.c`, `demo2_no_restarts.c` | DPLL and CDCL without restarts from four rules |
| `demo3_scripted_default.c` | The shared public search scheduler |
| `default_strategy/` | The complete default strategy in C (`tactix-default`) |
| `cruxmiter_policy.c`, `chvatal_policy.c` | The paper's openings before the built-in strategy |
| `php_er_policy.cpp` | Extension definitions before ordinary solving |
| `hall_policy.c` | Hidden pigeonholes (MI): a Hall violator refuted by counting |
| `tseitin_policy.c` | XOR encodings (RT): an inconsistent equation subset refuted by parity |
| `vertex_cover_policy.c` | Vertex-cover cardinality (TS): a clique cover refutes the counter |

The last three recognize their structure from literal roles alone, so renamed
variables, flipped signs and reordered clauses do not change the result. They
pass the recognized premises to a startup action, which validates them against
the input and writes a pseudo-Boolean refutation; any other input is solved as
by `tactix`. Each takes `CNF [PROOF]`, and its header records the measured
results:

| Strategy | Family | Verified (strategy / `tactix`) |
|---|---|---|
| `tactix-hall-policy` | MI | 23 / 10 of 23 |
| `tactix-tseitin-policy` | RT | 133 / 87 of 139 |
| `tactix-vertex-cover-policy` | TS | 339 / 331 of 340 |
