# Independent-lineage survey — frozen compact record

**Date:** 2026-07-15. **Sources:** Glucose 4.2.1 and CryptoMiniSat, checked
against Tactix's Kissat/CaDiCaL-derived architecture. Current work ownership is
in PROGRESS; this file preserves only lineage evidence.

## Confirmed architecture

| Area | Independent evidence | Disposition |
|---|---|---|
| Arena, watches, propagation | Both lineages use MiniSat-descended region allocation and watched propagation; CMS also stores implicit binaries in watch lists. | Keep Tactix's compact arena and mixed watch layout. |
| Branching and phases | CMS rotates VMTF/VSIDS/random branching and multiple polarity modes. | Tactix's VMTF/EVSIDS split is independently supported; richer rotation is a policy candidate. |
| Conflict analysis | Glucose adds binary-resolution minimization beyond recursive reason minimization. | Candidate owned in PROGRESS, not a claim that current Analyze already subsumes it. |
| Restarts | Glucose combines glue restarts with a blocking-restart test; CMS rotates restart families. | Policy candidates owned in PROGRESS. |
| Reduction | Glucose adapts the reduction interval; CMS has tiering and optional usage/predictor rankings. | Reduction-policy candidates, relevant after the current Reduce divergence closes. |
| Inprocessing registry | CMS dispatches a configurable schedule string of independently budgeted techniques. | Evidence for registry-owned presets; schedule strings are a phase-7 design input. |
| Proof and reconstruction | Glucose's DRUP and CMS's FRAT do not weaken Tactix's native DRAT/LRAT and model-reconstruction requirements. | Keep the stricter N2 contract. |

## Probe and Backbone lineage split

Glucose and CMS both contain binary-implication and failed-literal machinery,
but neither makes Tactix's V1/V2 split redundant. V1 performs full-propagation
failed-literal probing; V2 deliberately confines itself to the binary
implication graph. Keeping two public rules preserves distinct proof effects,
cost currencies, and policy doses. CMS's watch-list implicit subsumption is
also independent evidence for discovering binary subsumers directly from
watches.

## Candidate ownership

The seven candidates formerly owned only by this snapshot now have explicit
homes in PROGRESS:

1. binary-resolution minimization and blocking restarts — phase 4.5;
2. adaptive reduction intervals and usage-count ranking — Reduce policy;
3. branch/restart/polarity rotation, `best_inv`, and schedule strings — phase
   7 scheduler/preset design.

They are candidates, not promised implementations. Each requires a bounded arm
or trace question, correctness/reach gates, cumulative measurement, and an arm
removal condition.

Gauss–Jordan/XOR reasoning remains RFC-level because its proof translation is
non-trivial. ML clause prediction remains a learned-policy possibility, not a
kernel obligation.
