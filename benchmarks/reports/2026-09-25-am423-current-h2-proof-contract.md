# AM423: current-source H2 native-DSR proof-contract replay

Date: 2026-09-25. This is a diagnostic, not a checker-route change or N2 acceptance.

## Exact current result

SLURM job `1722271` completed `0:0` in one second on `cpu-dense-hwulab-q` (two CPUs, 8 GiB). The frozen AM369 public C-API replay driver was linked against the latest Release solver objects (`d7043567`; `b91c5652` changed only docs/reports). Driver SHA-256: `8db25626d879770caf6481f76f702f9e7de42b4763782d3c2073068a772912bf`. The original 21-variable, 42-clause CNF SHA-256 is `4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa`. The emitted binary DSR SHA-256 is `1327bb08a0fbde95a662bbb299921d87549546e309f47c9aa54e4b4a46f46889`, **byte-identical** to the historical parent proof. Scripts, inputs-after hashes, solver log and all checker artifacts are in `benchmarks/probes/am423-h2-current/`.

Both checker modes used the same frozen `dsr-trim` SHA-256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`, original-CNF-preserving checker spelling and Trestle SHA-256 `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`.

| Mode | Backward | Forward (`-f`) |
| --- | --- | --- |
| Canonical no option | Reject: `Clause 60 not in watch pointers for literal 22` at addition line 18 | Reject: no UP contradiction for RAT clause 21 at line 15 |
| Diagnostic `--delete-implied-units` | `dsr-trim` verifies; decoded LSR passes Trestle | `dsr-trim` verifies; decoded LSR passes Trestle |

The no-option backward verbose trace records six ignored implied-unit-reason deletions after the first explicit root unit `17`. The later witnessed SR line is therefore checked against a clause population different from the kernel's semantic formula. The local checker source's deletion path corroborates the mechanism: its default flags ignore an implied-unit-reason deletion in forward processing and in the backward watch-population reconstruction. This source checkout is locally modified and is **not** treated as the authority for the pinned binary; the exact frozen-binary trace and paired checker outcomes are the authority.

The focused current Release N1 fixtures `api_instances`, `online_all_actions`, `n1_public_policy`, `decide`, `factor_n1`, `vivify_n1`, and `subsume_n1` pass 7/7. These establish representative reachability only, not every legal action state or N1 completion.

## General-design implication

The no-option checker may retain a clause that Tactix has removed whenever that clause is the checker's current global-UP reason. A future direct-SR witness must be valid against this checker-visible population, or the proof emitter must arrange an independently valid RUP/RAT/SR derivation whose checked population matches the intended transformation. Merely suppressing an otherwise legal API action would violate N1; a fixed H2 deletion mask would not be general. The prior AM387/390/404/407 controls show that neither a simple reorder nor bulk unit publication is a proved general fix, and `--delete-implied-units` changes proof semantics in both directions. The user has been asked which canonical checker contract to target. Until that is settled and fully audited, retain the historical no-option route and keep N2 open. No Tier-D job was submitted.
