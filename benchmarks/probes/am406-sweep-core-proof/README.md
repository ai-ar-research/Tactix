# AM406 Sweep original-core-unit proof probe

Current-source Debug build from `99b068ff` plus the AM406 white-box regression.
`test_sweep` SHA-256: `bab4da1edfe1c19e02ab34c302b2c04cbe52fe7a44d5bfd8d6608789832567da`.
`tactix` SHA-256: `9e0ef3605cb00d5abb7e53a37e37907eb2680f171e83656500b21f5f847ed123`.

The terminal hwulab job was `1721170` (2 CPUs, 8 GiB, 30-minute limit, 19 seconds elapsed):

```sh
build/am406-sweep-test/tests/test_sweep
tests/sweep_proofs.sh /home/hwu/SAT-solving \
  /home/hwu/SAT-solving/build/am406-sweep-test/tactix \
  /home/hwu/SAT-solving/build/am406-sweep-test/tests/test_subsolver \
  /home/hwu/SAT-solving/build/am406-sweep-test/tests/test_sweep
```

`slurm-1721170.out` is the complete terminal output; SHA-256
`98021229f2636cd7367cfcf5c77d0925e1dced627e7d46e5902e5ad779827dbe`.
The proof script's temporary certificate directory was removed by its exit trap.

Two diagnostic predecessors are retained. Job `1721167` failed because an
original clause already unit *before* environment construction is correctly
omitted; the relevant boundary is a previously imported original becoming unit
during a seed. Job `1721168` passed the new regression but used zero brute-force
oracle rounds, violating that existing oracle's nonempty-environment assertion.
Neither is a solver correctness failure. The terminal job used the default
oracle round count and a posed saved-core row at the actual adapter boundary.
