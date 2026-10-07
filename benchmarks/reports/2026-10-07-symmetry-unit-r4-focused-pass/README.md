# Symmetry-unit admission r4 focused gate: pass

Terminal record for the prepared r4 authority of the
[unit-admission checkpoint](../2026-10-07-symmetry-singleton-unit-admission/README.md).
The archive was submitted unchanged as job **1755545** on 2026-10-07 at 19:04 EDT.

- Inputs: source archive `1249dc7f7b5bd8c3a401d944fa0d74a9aaed080c64e46f8fee685c5a70ae1eee`
  and wrapper `1a1a42367b0b8c928b9f1c2329a8950e0939c47663b3492c72140dd82a787e9e`.
  The three changed code and test files are byte-identical to `d576abaeb`.
- `1755545_0` (Debug): COMPLETED, exit 0, 2:05.
  - All five focused binaries passed.
  - The compound singleton producer printed `c compound singleton unit -1` and `s UNSATISFIABLE`.
  - Forward every-lemma dsr-trim/Trestle reports `s VERIFIED UNSAT`.
  - The invalid witnessed-unit control was rejected with `No UP contradiction for RAT clause 1`.
  - The doc size, section-reference and proof-rule-sync checks reported 0 errors.
- `1755545_1` (ASan/UBSan): COMPLETED, exit 0, 3:42. All five focused binaries passed with no sanitizer report.

Result hashes (`sha256sum` of the retained logs):

```
9a271f2a916b0b6ee6cecb2426f0b3206ef074578be55d9bc94f6f3fe3ab86aa  debug/exit-status
9a271f2a916b0b6ee6cecb2426f0b3206ef074578be55d9bc94f6f3fe3ab86aa  asan/exit-status
c680d3bd38a04c7beee7d54999f8f00b43051f75a53cf2bd170ae0cc5f99eb5f  debug/singleton-proof-check.log
83313b1ebd8030046ec7f456375a3040a120d6354e6d7b125130d7d24ef96d34  debug/invalid-proof-check.log
959efff87ff908dd41a55b2f7b2ea9a87063f8eac22d4844c219d2a508121e17  debug/singleton.dsr
3bebd636b4a2817ede7d5d307f2caaf57b7d0fc2d04ba486d5844c8634c0fb9c  debug/checkers.sha256
```

This qualifies only the focused change. It does not establish full Debug,
fuzz, regression or target acceptance for any candidate.
