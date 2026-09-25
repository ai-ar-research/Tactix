# AM396 current-source full Debug suite

Source is the immutable AM396 archive SHA-256
`7f9c0a3fc8abd95308797168c58fee8addf6b0314dbd1946f666b0bd2a4c9202`.
The runner stages only the exact ignored frozen baseline/campaign fixtures and
AM369 checker binaries required by the full repository tests. It builds with
rule journal and proof shadow enabled, then runs all Debug CTests on hwulab.
Resources: one job, 12 CPUs, 32 GiB, eight-hour wall time, no throttle or node
constraint. This is a correctness suite, not a performance or N1--N4 gate.
Submitted as job `1720277`; terminal CTest artifacts are pending.
