# noL retirement Full Debug supplement

The archived noL-retirement Full Debug run `1754129` remains a failed full
run: 351 of 353 tests passed, while `inventory` lacked an X17 disposition and
`controller_action_space` still counted the new retired registry tombstone as
one of four live exclusions.  Its `x17_retired` lifecycle test passed.  The
original source, build record, CTest log, JUnit, and verdict under
`benchmarks/runs/20261007-nol-retired-1e8295106-full-debug` are unchanged.
The original source archive SHA-256 is
`d73ad86e70597aff8b046e93b24263bdbb4f649af6928f2091c337ed2542f63a`,
the failed CTest log SHA-256 is
`f44a0017db677b06477d8f6d19452e3cdc9d998c7c5b09ad5317e3dd0d7d40a3`,
and the JUnit SHA-256 is
`8713e3bfb7a95c336d45dbec0ffb2b26fb7e1a03eed9349646dca7a3fd180366`.

Supplement job `1754589` extracted that exact archive in fresh node018 scratch
and applied only patch SHA-256
`46c1d647ba48ba519be3ed7f237ca8e672db11695bd7c7f0ea40f2b242e6ca13`:
the implementation inventory records X17 as retired, and the controller test
counts `Retired39` separately while retaining the exact four live exclusions.
The wrapper SHA-256 is
`3c58b2f04c29f719b42e9063f167591ec8dc6a60e4ccc8b6043404a37147444c`.
It configured a new Debug build, built only `test_controller_action_space`, and
ran exactly the two formerly failing CTest entries.  `inventory` passed with
45/45 catalog families dispositioned; `controller_action_space` passed.  The
test binary SHA-256 was
`3903418eb3959962df4e48ff7596a2a5078d90afe0ebbe711306b7bc18e225f6`.

This supplement closes the two stale expectation failures on the archived
production source.  It is not a rerun of the other 351 tests and does not claim
that a current cumulative Full Debug suite has passed.
