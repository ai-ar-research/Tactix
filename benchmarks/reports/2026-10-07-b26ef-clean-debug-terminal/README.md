# b26ef clean Debug terminal gate

Fresh full Debug job **1755126 completed successfully: 353/353 tests passed,
zero skipped**, with exit status 0 and post-run verification of the archived
source, 284 executable commands and 767 bindings. CTest took 822.70 seconds;
the complete clean build and test allocation took 29 minutes 11 seconds.

The exact candidate is the FirstOpen/original-seed repair at checkpoint
`0a15ecaf1`, corresponding to Release b26efc0f. The source authority is the
immutable ab70cbb5 archive plus the recorded complete ignored fixtures,
reference sources and checker dependencies. `evidence.json` binds the source
archive, final manifest, CTest log, JUnit, exit status and outer Slurm wrapper.
The manifest retains the exact 767 binding records: 290 build artifacts,
65 direct test inputs, 408 packaged dependencies and four campaign files.

The actual outer allocation used cpu-q, eight CPUs, 32 GiB, and CTest -j8.
Its manifest explicitly records that the generated historical cpu-dense header
was run as a shell script, not submitted as a second allocation. Storage
admission and monitoring remained enabled. This is correctness evidence, not
benchmark timing evidence.

The earlier failed startup-support fixture gate remains preserved. This run
includes the explicit optional-handoff fixture repair and does not weaken its
reach assertion. Broad Lucky completed along with every other registered test.

This gate qualifies only the b26ef intermediate source. It does **not** transfer
to the subsequent singleton symmetry-unit admission repair, whose new focused,
Release and full Debug gates have separate source authorities.
