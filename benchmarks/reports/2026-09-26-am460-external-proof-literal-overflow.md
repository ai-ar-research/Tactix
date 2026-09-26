# AM460: decline unrepresentable proof-only literals without signed overflow

Date: 2026-09-26. The AM455 Opus owner found a parent-identical internal-API
UBSan failure in `Proof::add_external_rup`. The external DIMACS validation loop
rejects `INT32_MIN`, which has no positive signed-DIMACS counterpart, but
compared each earlier literal with the negation of each later literal before
the later literal reached that guard. With `{1, INT32_MIN}`, the negation had
undefined signed overflow. The proof-only API must decline the clause
atomically; it must not publish a partial proof line.

The complement comparison now widens both operands to `int64_t` before
negation. This preserves the existing accepted literal set, duplicate and
tautology behavior, proof bytes for valid input, and LRAT format refusal.
The serializer regression checks `INT32_MIN` in both positions through RUP
and RAT, both returning false and leaving the DSR stream empty.

Direct old-source negative control: compiled the pinned pre-fix
`7b1c8a5a:src/proof/proof.cpp` with the new `tests/test_proof.cpp` under
`-fsanitize=undefined -fno-sanitize-recover=all`; the test exits nonzero at
`proof.cpp:625` with `negation of -2147483648 cannot be represented`.
Direct new-source check: compiled `src/proof/proof.cpp` and
`tests/test_proof.cpp` with `-fsanitize=address,undefined`, ran with
`ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`, and all proof
serializer cases passed. Leak detection was disabled because this session's
ptrace environment prevents LeakSanitizer startup; ASan and UBSan remained
enabled. No solver run or proof-checker contract changed.

The in-flight AM458 Opus campaign and pinned full Debug job `1727702` use the
preceding `7b1c8a5a` solver source and do not validate this repair. This is a
focused bug fix, not latest-source fuzz, full Debug acceptance, or N2 closure.
