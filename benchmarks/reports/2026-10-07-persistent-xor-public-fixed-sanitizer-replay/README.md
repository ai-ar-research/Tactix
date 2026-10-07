# Frozen public XOR sanitizer and replay supplement

The immutable public-fixed source tree SHA256 `8fb900e5a0f2f17c62d172405e8d9025a35413ef0acd86b1497dd2ac70107f63` passes seven focused ASan/UBSan/leak tests with halt-on-error and leak detection enabled, including the original-model and independent DSR/DRAT proof oracle wrappers. The three maintained flat-payload, restart-controller and restart-reduce compatibility tests also pass.

This supplements the 11-check Debug report without changing that frozen artifact. It does not accept the subsequent incremental assignment-capture source, new-candidate full Debug/fuzz/regressions, or rank 569 performance. No additional instance is independently recovered by these microtests.
