# AM397: rule-journal exit lifetime

Date: 2026-09-25. Parent: immutable AM393 source archive, SHA-256
`dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9`.
This is a causal diagnostic-support repair, not combined fuzz/full-suite
acceptance, a proof-rule change, N1--N4 promotion, or a performance result.

The AM393 Opus owner's smoke found that `open_journal` registered the public
`rule_journal_close` as an `atexit` callback. Its final assignment to the
`thread_local TransactionState` could run after that object's destructor.
The frozen-parent Debug ASan+UBSan public API journal fixture exits 1 with
`AddressSanitizer: heap-use-after-free` at `rule_journal_close` (parent line
631); the trace shows `__call_tls_dtors` freed a transaction string first.
The parent binary SHA-256 is
`32de8908b24e443fa9f70a6f39539bea502166bdea46fc0553f9c50c3e4478c3`.

The exit callback now checks replay exhaustion and closes the process-wide
streams without touching TLS. An explicit `rule_journal_close` retains its
transaction reset. This separation is independent of formula and journal
contents; it preserves repeated public API solve sessions and the existing
replay contract. The existing two-session public fixture is intentionally left
without explicit close, documenting the exit-lifetime boundary.

The candidate Debug ASan+UBSan binary SHA-256 is
`344bb85fe6dc89a8d96d364ba39c2fdcbd0a97298825402e0f135f85280f0b93`.
The same direct fixture exits 0 with two `Condition` records. Parent and
candidate journal files are byte-identical (SHA-256
`624473c853d6fcc4d32483897a2338c4ab55bf8558f9440bffeeb2b6d8e70637`).
With ASan leak detection both off and on, `test_api_journal.py` passes its
record/sequence, replay, and extra-record rejection controls. Focused
`rule_journal`, `api_journal`, and `substitute_publication` CTests pass 3/3 in
current Debug and current ASan builds. `git diff --check` is clean;
`check-doc-size.py` has zero errors and six pre-existing capacity warnings.
The causal artifacts and reproduction instructions are in
[the AM397 run](../runs/20260925-am397-journal-atexit/README.md).

AM393's counted Opus campaign remains live on its original pin and cannot
validate this candidate. AM396 Factor fuzz and current-source full Debug
validation remain live; a fresh immutable AM397 affected campaign and full
Debug source pin are due. H2's historical no-option checker route and the
post-finalization `tactix_add` contract remain unresolved. No Tier D run was
submitted.
