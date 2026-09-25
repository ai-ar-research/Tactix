# AM397 causal journal exit-lifetime control

The parent is the frozen AM393 source archive (SHA-256
`dabdb14c8ba8043135f5ec04336c1e16a4f940baf51cd93a82c7fd2642a9dbe9`).
`parent-source/` and `parent-build/` are local ignored extraction/build products;
the archive belongs to the AM393 authority. Both builds use Debug,
`TACTIX_ENABLE_RULE_JOURNAL=ON`, `TACTIX_ENABLE_PROOF_SHADOW=ON`, and
`-fsanitize=address,undefined` with frame pointers. The direct command is:

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
TACTIX_RULE_JOURNAL="$PWD/parent.tsv" \
TACTIX_RULE_JOURNAL_RULE=Condition \
./parent-build/tests/test_api_journal
```

Run from this directory, replacing `parent-build` with the current candidate
binary path and output with `candidate.tsv` for the repaired control. Parent
exit 1 reports a TLS-after-destruction heap-use-after-free in `parent.out`;
candidate exit 0 has no ASan/UBSan error. Both journals SHA-256
`624473c853d6fcc4d32483897a2338c4ab55bf8558f9440bffeeb2b6d8e70637`.
Parent binary SHA-256
`32de8908b24e443fa9f70a6f39539bea502166bdea46fc0553f9c50c3e4478c3`;
candidate binary SHA-256
`344bb85fe6dc89a8d96d364ba39c2fdcbd0a97298825402e0f135f85280f0b93`.

The maintained `tests/test_api_journal.py` additionally checks two-session
recording, replay success and expected rejection of an extra replay record.
This is a focused local microtest, not a solver instance execution or a
campaign verdict. See the [technical report](../../reports/2026-09-25-am397-journal-atexit-lifetime-checkpoint.md).
