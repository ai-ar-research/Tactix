# AM480 committed-source public DSR replays

Source: pushed commit `454e1cbb` (bounded-spool native DSR serializer). Release public C-API driver `benchmarks/probes/scratch/am478-rank12-paired/variant/xdrv-am479`, SHA-256 `b8cea41fd5b612e84d9f59a38c96b887f632fc0d21bfd1ad692fc5558cd2b959`, linked from the pinned AM479 Release objects. Pinned checker binaries are AM369 copies: `dsr-trim` SHA-256 `3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`, Trestle `srcheck` SHA-256 `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`. `dsr-compress -d` expands binary LSR before Trestle. The final checker CNF is clause/literal-preserving canonical spelling of the original input; the solver reads the original CNF. No `--delete-implied-units` or other checker option is used.

SLURM jobs on `cpu-dense-hwulab-q`, 2 CPUs/8 GiB, full-width arrays without throttle or node constraint:

| job | public cases | proof modes | tasks | verified no-option DSR→LSR→Trestle chains |
|---|---|---|---:|---:|
| `1729949` | D1 reduced, D1 original, H2 original | DSR binary/ASCII | 3/3 | 12/12 |
| `1729988` | D2 reduced, D2 original | DSR binary/ASCII/AUTO | 2/2 | 12/12 |

Each mode verified under no-option backward/core trimming and additional `-f` forward checking. Terminal statuses and proof SHA-256 hashes are in `results/`; the launch commands and exact corpus locations are in `scripts/`. Every `dsr-trim` log contains `s VERIFIED UNSAT`, and every Trestle log contains `s VERIFIED UNSAT`. Proof bytes and decoded LSR intermediates were not retained. These are the fixed public reproducer controls, not a broad fuzz acceptance or complete N2 gate. The independent synthetic D2F internal-API failures remain unresolved pending Opus AM473's terminal public-reach report. Full Debug CTest `1729972` is separately submitted on the same commit and was pending at this checkpoint.
