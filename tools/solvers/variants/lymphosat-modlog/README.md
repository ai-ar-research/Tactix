# LymphoSAT module-attribution variant (`lymphosat-modlog`)

LymphoSAT (SC2026 winner, `winners/lymphosat-sc2026/`) is a composite solver:
a profiler/dispatcher tries 126 specialized targeted solvers in `order.json`
order and finally a kissat-derived `fallback`. Its stock dispatcher logs
matcher acceptance and failed attempts to stderr but never states which module
produced the final answer, and it does not log the fallback launch at all.

This variant patches only `solver/core/main.cpp` to add two stderr lines:

- `c composite: solved by <name> (exit <10|20>)` on the winning attempt, and
- `c composite: dispatching fallback <name>` when the fallback is launched.

Verdicts, proofs, models, dispatch order, and every nested solver binary are
unchanged; the generated tree differs from the frozen baseline pin
(`benchmarks/baselines/sc24-26-32g-5000s/pins/lymphosat/`) in exactly the
dispatcher source and the rebuilt dispatcher binary.

Regenerate the tree from the repository root:

```sh
tools/solvers/variants/lymphosat-modlog/prepare.py
```

The script refuses to overwrite an existing `tree/` and attests the
two-file-only divergence from the frozen pin.

Consumed by `tools/bench/lymphosat_attribution.py`, which reruns the Tier D
LymphoSAT arm under the frozen 5,000 s / 32 GiB contract and records the
responsible module per instance
(campaign `benchmarks/runs/20260819-tierd-lymphosat-modlog/`).
