# Superseded focused-archive cleanup

At `2026-10-07T18:42:41-04:00`, after jobs `1755490`, `1755498`, and
`1755527` were terminal and their dependency-held clean builds had been
canceled before start, the following ignored, rebuildable tarballs were
deleted. Wrappers, patches, checksums, provenance, and job logs remain in each
campaign directory.

| Campaign | Deleted path | Recorded SHA-256 | Deleted bytes |
|---|---|---|---:|
| r1 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission/pins/base-source.tar.gz` | `99ddedb2b4a2f30add6c9d94c69d17d3c4419e7e0016a4ad21fac488f7997889` | 105,052,603 |
| r1 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission/pins/source.tar.gz` | `bffa012336b679e9410723ee1530fb4c6c700ca172ed20ff3aceefff4fe5d9f1` | not recorded before deletion |
| r2 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission-r2/pins/base-source.tar.gz` | `99ddedb2b4a2f30add6c9d94c69d17d3c4419e7e0016a4ad21fac488f7997889` | 105,052,603 |
| r2 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission-r2/pins/source.tar.gz` | `10b84738065fc40e75d32a3cc4603e7e195e07a6ed343bb3c520df8398e3aa68` | not recorded before deletion |
| r3 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission-r3/pins/base-source.tar.gz` | `cc355afa684cc505e9b7118267e0b6a02ddc1c4411ff598736cbff6654e3ed45` | 105,055,071 |
| r3 | `benchmarks/runs/20261007-symmetry-singleton-unit-admission-r3/pins/source.tar.gz` | `fc0e82850b11b88f9839cdf5c0ec2fddcdffdc40a9418f16ad706747c678ab5a` | not recorded before deletion |

The base byte counts above are reproducible byte-for-byte with `git archive`
from the commits recorded in each `PROVENANCE` file. The three deleted patched
archive byte counts were not captured before cleanup; this record does not
invent them. Their exact source *content* remains reconstructable from the
recorded base commit plus retained `change.patch`. R3 is additionally retained
as the complete 844,052,480-byte archive
`benchmarks/runs/20261007-symmetry-units-clean-source-r3/source.tar`, SHA-256
`615fc5f498e2649e4ef93d6f7340a0e2939d7f7e21f194718a9ae7f38599e1a5`,
which contains the exact focused source plus 408 recorded dependencies.

The current r4 authority was not deleted:
`benchmarks/runs/20261007-symmetry-singleton-unit-admission-r4/pins/source.tar.gz`
is 105,108,800 bytes with SHA-256
`1249dc7f7b5bd8c3a401d944fa0d74a9aaed080c64e46f8fee685c5a70ae1eee`.
