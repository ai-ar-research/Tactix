# Documentation and cleanup audit — residual record

**Status:** frozen after the Phase 4.19 cleanup. Current actions belong in
`PROGRESS.md`; this record retains only unresolved checker limitations and the
evidence used to decide what could be removed.

## Audit method

The pass read every living authority, enumerated tracked source/scripts/docs/run
artifacts, scanned inbound basename and path references, compared rule catalogs
with registries/checkers, and distinguished executable contracts from dated
evidence. Zero references alone was never treated as proof of dead code.

## Cleanup disposition

- Deleted `s4-factor-dosing-design.md`: zero inbound references and no unique
  contract, evidence, or reopening condition.
- Compacted the landed E2 factorsize and S2a matcher designs from 500 lines to
  79. Calculus/implementation notes own semantics; the designs retain only the
  proof/strategy boundary and validation contract.
- Compacted PROGRESS, closed deviation chronology, and closed Phase-4 plan prose.
- Kept benchmark results and raw diagnostic artifacts when a frozen report,
  regression, or reproducibility path may depend on them. Proof certificates
  from new campaigns are ephemeral after their check metadata is durable.
- Deleted no production source: the scan found optimization debt but no file
  with demonstrated zero runtime/API/test/diagnostic purpose.

## Residual checker limitations

These are tooling improvements, not reasons to distrust a passing check today:

- `check-citations.py` resolves too few qualified/anchored citations; it should
  report a minimum coverage ratio.
- `check-comment-hygiene.py` carries its own large ad-hoc lexer and rationale;
  retain the rules but shrink the implementation or use language-aware parsing.
- `check-config-wiring.py` and `check-effort-specs.py` parse prose/source with
  narrow regexes. Move their executable tables to typed structured data.
- `check-inventory.py` checks token presence in implementation-notes §17 rather
  than comparing plan catalog, registry, calculus, and explicit no-implements.
- `check-progress.py` still infers queue/headline state from prose phrases;
  replace this with explicit machine-readable markers.
- `check-stat-ids.py` needs helper-composed meter and duplicate coverage;
  `check-proof-rule-sync.py` should derive internal exceptions such as V6a.
- `comment-only-diff.sh` should fail closed for new, deleted, or skipped files.

The broad source-structure/test debt discovered by this audit is summarized once
in PROGRESS's active engineering debt. Git history preserves the discarded
line-by-line narrative and prior classifications.
