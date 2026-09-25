# AM394 immutable full Debug CTest launch

Source: AM393 commit `c58c1f68e5e8245f9b2900ccc750811d273ce0fd`, immutable archive `../20260925-am393-late-factor-journal/source.tar` SHA-256 `dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9`. Frozen native/compatibility checkers: AM369 owner `pins/`. The job builds from its own extraction and runs the full Debug suite with rule journaling and proof shadow enabled. It is correctness regression evidence only, not a benchmark or N1--N4 acceptance claim.

Resources: `cpu-dense-hwulab-q`, 12 CPUs, 32 GiB, eight-hour wall time; one job, no array throttle or node constraint. Output and terminal result are in `out/`, `configure.log`, `build.log`, `ctest.log`, and `solver.sha256`. Do not mutate the source/archive or build while the job is active.

Job `1720161` was canceled after 4:46 because `git archive` omitted ignored
frozen benchmark fixtures required by the suite. The early setup failures are
not solver verdicts. The immutable retry with exact fixture staging is
`../20260925-am394-full-debug-ctest-v2/` (job `1720222`); preserve these logs
to explain why the first attempt was not accepted.
