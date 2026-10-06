# Aggressive generated-build cleanup

The user explicitly requested aggressive removal of unnecessary old build files.
This sweep removed 237 generated test executables and
843 compiler-intermediate directories, reclaiming
27,417,457,152 allocated bytes (27.42 GB).
Exact paths and sizes are retained in [cleanup.json](cleanup.json). There were
0 skipped candidates.

Active/current-day campaigns, new agent builds and current parent builds were
excluded. Main solver binaries, checker collections, source files and archives,
profiles, manifests, results, logs and frozen reports were preserved. Some old
source snapshots contained copied generated builds; only their generated test
executables and CMake intermediates were removed. Historical CTest suites need
rebuilding from their retained sources before rerunning. Frozen manifests still
record the original tested binary identities; removal is not a new verification.

The standing storage contract is now in DEVELOPMENT_PLAN.md section 8.1: check
allocated repository usage before builds/submissions and at wrap-up; above
700,000,000,000 bytes clean aggressively and recheck before launching more work.
