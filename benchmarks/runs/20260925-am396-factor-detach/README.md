# AM396 parent-failing Factor regression

The new `tests/test_factor.cpp` object from current Debug source (SHA-256
`8ccb35803010d1bf55a4276ad16b88524c79ea28ff7032a4f5e6d6d1bcf8005d`)
was linked read-only against immutable AM393 parent Debug objects from the
running AM394 v2 full-suite build. Its resulting `test_factor-parent` binary
(SHA-256 `7a9f45261b951bd8af3b31cc494d0713f689f455f637cd97ff8e2368b9235344`)
exits 134 at the first new assertion: the freshly created Factor extension is
absent from `compact_import_order` after H detachment. The current Release,
Debug, and ASan+UBSan `factor` CTests pass. The parent link did not edit the
AM394 source, build objects, or job artifacts.
