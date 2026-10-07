# Rank569 FastEliminate causal gate

Job `1754435` ran the unpatched and patched binaries sequentially on CPU 60
of node002 for 300 seconds each. Both used production defaults plus explicit
native DSR. The source, input, binaries, wrapper, machine topology, arm order,
occupancy and proof hashes are pinned under
`benchmarks/runs/20261007-rank569-fastel-self-strengthen-r7`.

The port has the intended structural effect: startup FastEliminate retires
2,246 variables rather than 2,236, matching the ten previously isolated native
successes. At equal wall and nearly equal user time, the patched arm records
5,350,504 conflicts versus 5,454,975 (-1.915%) and 7,978,064 decisions versus
8,418,022 (-5.226%), but performs 623,943,363 propagations versus 589,084,850
(+5.917%) and writes 593,288,945 proof bytes versus 550,533,210 (+7.766%). One
unrelated four-core allocation left node002 between arms. These fixed-time
prefixes establish a changed search trajectory and work distribution, not a
runtime improvement or a Rank569 closure. Neither prefix is a terminal proof,
so no checker claim attaches to this pair.

Cheap guards were also neutral: Rank263 solved in 0.67/0.66 seconds with the
same 10,957,132-byte DRAT SHA in both arms and both proofs passed independent
forward drat-trim; Rank1004 remained unknown in both 20-second arms with
506,023/506,225 conflicts. These are smoke guards, not cumulative acceptance.

The first full patched candidate, job `1754440`, is invalid infrastructure
evidence. On node013 it aborted after 500.856 seconds without a solver footer,
after writing 924,647,424 DSR bytes; two unrelated jobs on the same node aborted
concurrently. A read-only audit then measured node013 `/tmp` at 430 of 431 GiB
used, with only 1.2 GiB free and an unlimited file-size limit. Preserve that
failure, but do not classify it as a solver result. Immutable rerun `1754535`
uses the same solver binary, job-private shared scratch admitted only above 128
GiB free, and the audited 64-bit no-option DSR elaborator/compressor followed by
independent Trestle. Its terminal result remains pending. The user target is a
solve substantially below 3,359.975 seconds with an independently accepted
proof.
