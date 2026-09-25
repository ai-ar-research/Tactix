# AM394 full Debug CTest retry

Source is the immutable AM393 archive SHA-256
`dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9`.
The first job (`1720161`) was canceled after setup failures caused by ignored
frozen test fixtures absent from `git archive`; it was not a solver verdict.
This fresh run stages symlinks to only the named frozen baseline/campaign
fixtures and frozen AM369 checker executables, then builds and runs all Debug
CTests on hwulab. The staged fixtures are read-only authorities and are not
part of the source pin. See `run.sbatch` for exact commands and resources.
