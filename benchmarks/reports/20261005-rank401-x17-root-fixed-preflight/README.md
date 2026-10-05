# Rank401 X17 root-fixed preflight

This focused preflight tested whether the root-fixed X17 relaxation lets the
native `NaryCspPrefix` producer publish an action on Rank401.  It used exact
source `b6a4b36b1f507f2d6a3668a8295bafc5ab9db3f7`, Release binary SHA-256
`c13578727c8161202bedace441a60d941fd11a923e0274fc1305bf8a25e35766`, and
checker-input SHA-256
`61fc24ce538074e3e1d3a02f8e5286708b0e769d43e866b5d7f0eff7b2787665`.

Slurm validation job `1750571` passed.  Preflight job `1750572` ran the solver
for the requested 60 seconds and ended `UNKNOWN`.  The solver's terminal
counter summary is conclusive:

```text
c controller-rule NaryCspPrefix requests=0 applications=0 effective=0 exhausted=0 stale=0 interrupted=0 work=0 overshoot=0
c controller-producer NaryCspPrefix calls=224 work=1138621224 candidates=0 offers=0
c controller-loop full-restarts=223 partial-restarts=733 configuration-changes=214 maximum-root-actions=0
c nary-csp   0 attempts, 0 applied, 0 no-structure, 0 no-consequence, 0 already-published, 0 invalid, 0 inapplicable, 0 exhausted, 0 interrupted, 0 stale, 0 unsupported-proof
s UNKNOWN
```

The action trace contains 224 requests, all `ResumeSearch`; provenance contains
only the session record.  Thus the relaxed guard admits repeated producer
entry, but the detector publishes no candidate.  The controller never has an
X17 action to select.  The gated 300-second target screen was not submitted,
and independent proof checking was inapplicable because the solver returned
`UNKNOWN` without an effective action.

The Slurm allocation lasted 4:58 because postprocessing tried to inspect a
105,944,320-byte controller trace and a 79,562,052-byte compressed feedback
stream after the solver stopped.  It was cancelled once the terminal counters
were retained.  Failed preflights should write a tiny counter summary before
any full-feedback inspection and skip that inspection when the gate is false.

The next detector change should expose a one-call, bounded producer census:
number of candidate domains, exact domains, relation keys/tuples, and the first
decline or resource-limit reason.  Rank401 currently spends about 5.08 million
producer work units per call and repeats the same zero-candidate outcome.  Do
not expand the target cohort until this census identifies and repairs the
specific recognition boundary.

Evidence hashes are recorded in `report.json`.  The raw campaign is
`benchmarks/runs/20261005-rank401-x17-root-fixed-screen`.
