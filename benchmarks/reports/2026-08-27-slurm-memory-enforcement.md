# SLURM-only benchmark memory enforcement

Date: 2026-08-27

## Decision

Maintained benchmark and diagnostic runners no longer install `RLIMIT_AS`.
Solver and checker memory is enforced by the SLURM task cgroup.  The allocation
rule is 8 GiB per physical core; on the current SMT-2 nodes this means two
requested logical CPUs per physical core.  In particular, the strict debug
contract is two physical/four logical CPUs with `--mem=16G`, while the targeted
timing contract remains six physical/twelve logical CPUs with exactly
`--mem=48000M`.

This changes experimental infrastructure, not solver semantics.  Frozen
campaign runners and results are not rewritten.

## Cluster evidence

`scontrol show config`, queried at `2026-08-27T00:50:26-04:00`, reported:

- `ProctrackType=proctrack/cgroup` and `TaskPlugin=task/cgroup`;
- `SelectType=select/cons_tres` with `CR_CPU_MEMORY`;
- `ConstrainRAMSpace=yes` and `AllowedRAMSpace=100.0%`;
- `ConstrainSwapSpace=yes` and `AllowedSwapSpace=0.0%`.

Thus a submitted `--mem` request is a real cgroup resident-memory ceiling and
swap cannot mask an overage.  An additional address-space limit is neither
needed nor equivalent.

## Cause of the apparent early OOMs

The frozen rank-1--40 screen
`benchmarks/runs/20260826-2340-tierd-ranks1-40-current-screen/` requested
`--mem=48000M`, but its copied runner also set `RLIMIT_AS=16 GiB`.  Its Tactix
arms on ranks 5, 13, and 14 terminated with `std::bad_alloc` at recorded peak
RSS values of 13,970,768 KiB, 14,189,420 KiB, and 13,342,516 KiB respectively.
Those are not 48,000-MiB cgroup OOMs.  `RLIMIT_AS` counts virtual mappings and a
capacity-growing container can need a new allocation while its old storage is
still live; the allocation can therefore fail while resident high-water is
well below either 16 GiB or 48,000 MiB.

The old screen remains useful evidence about the flawed dual-limit contract,
but it cannot decide current memory competitiveness.  Per project-owner
direction, ranks 1--40 will be rerun from fresh immutable pins under the same
proof-on `-c 12`/`--mem=48000M` timing allocation with no `RLIMIT_AS`.  Every
solver arm remains an independent job and every SAT model or UNSAT proof is
independently checked.  Peak RSS and VmPeak determine whether a separate
`-c 4`/`--mem=16G` hard-memory confirmation is needed.

## Infrastructure changes

- `tools/bench/run.py` retains only its CPU-time backstop and records
  `memory_enforcement=slurm_task_cgroup` for SLURM runs.  Local diagnostics are
  explicitly recorded as having no memory enforcement.
- `tools/bench/frozen_baseline.py` and
  `tools/bench/lymphosat_attribution.py` retain process-group cleanup but no
  longer impose an address-space cap.
- `tools/bench/tactix_tierd.py`, `tools/bench/run.py`, and
  `tools/bench/p4_cvg.py` compute two logical CPUs per physical core and request
  total job memory directly.
- New manifests use `scheduler_memory_gib` and
  `memory_enforcement=slurm_task_cgroup`; the frozen baseline reader accepts
  the legacy field only to interpret old immutable manifests.
- `tests/test_slurm_memory_enforcement.py` rejects any maintained runner that
  installs `RLIMIT_AS` and guards the physical-core allocation policy.

## Validation

- Release CTest: 76/76 passed.
- Focused campaign cleanup, Tier-D pinning, Phase-4 fixture allocation, and
  SLURM memory-enforcement tests passed.
- Python compilation passed for every modified runner.
- Documentation size, section-reference, citation, comment-hygiene,
  configuration, inventory, option, proof-rule, policy-boundary, and statistic
  checks reported zero errors.  Existing documentation-size and living-progress
  warnings remain warnings, not failures.
