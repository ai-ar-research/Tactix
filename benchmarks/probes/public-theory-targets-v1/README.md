# Public theory target selections v1

These four diagnostic selections isolate current high-confidence opportunities
for the public GF(3), Tseitin, and direct-PHP actions. They are projections of
the frozen `WV100.tsv` and `WV2-89.tsv` authorities, not new tiers and not
evidence that an action recognizes or solves a row. The saved `d170b4a5`
comparison remains the portfolio authority and must not be rerun.

| Selection | Contract | Status before execution |
|---|---:|---|
| `gf3-wv100.tsv` | 12 rows, 1,200 s | high-confidence one-hot GF(3) SAT owners |
| `tseitin-wv100.tsv` | 3 rows, 1,200 s | high-confidence pure Tseitin UNSAT |
| `tseitin-wv2.tsv` | 6 rows, 600 s | high-confidence pure Tseitin UNSAT |
| `direct-php-wv2.tsv` | 4 rows, 600 s | plausible clique-colouring PHP substructures; require recognition counters |

Do not launch these diagnostics until the Claude-owned structured fuzz report
accepts the corresponding producer/action path. Prepare one immutable campaign
per table with `tools/bench/experimental_tierd.py`, explicit DSR, a 1,800 s
checker cap, and repeatable `--solver-argument=...` values for the exact
controller seed, producer and canonical family. The family configurations are:

```text
Gf3GaussianConsequence: gf3-gaussian
TseitinCycleBreaking: tseitin-cycle
DirectPhpRefutation: no opt-in producer; use native semantic enumeration
```

Every result needs the expected verdict, independent model/proof acceptance,
and nonzero action reach. A timeout, checker censor, producer-only call, or NoOp
does not establish recovery. Keep the WV2 600 s and WV100 1,200 s contracts
separate. Arrays run full width without a throttle or node constraint.
