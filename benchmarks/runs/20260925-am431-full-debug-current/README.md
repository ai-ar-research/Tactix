# AM431 current-source full Debug CTest

Immutable `git archive` of commit `c776cc589497eee02ba8f88de5045f51cf504884`,
source tar SHA-256 `19f30629dfae3c83a22ddabf065058a0596d9c738a8141477b3bf9d99d91b785`.
The job builds with rule journal and proof-population shadow enabled and runs
the complete serial Debug CTest suite against the frozen AM369 checker pins.
Resources match the AM400/401 full-suite jobs: one unthrottled hwulab task,
12 CPUs, 32 GiB, eight-hour wall time, no node constraint. This is a
current-source correctness diagnostic, not N1--N4 acceptance or Tier D.

AM400 job 1720786 and AM401 job 1720898 both finished 185/186 with their sole
failure in the old sparse-collection scheduling fixture. AM430 repaired that
stale witness, and the current ASan+UBSan scheduling test passes. This run
checks the entire current suite rather than projecting those historical
results across later solver changes.
