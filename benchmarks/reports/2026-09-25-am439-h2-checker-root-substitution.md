# AM439: H2's checker-retained root conflicts with the SR witness

Date: 2026-09-25. Checker-only diagnosis on AM436's frozen 16-clause original
CNF (`6a1328d1...`) and 314-byte native DSR (`d2012329...`), with frozen
AM369 no-option `dsr-trim` (`3b5c7737...`). No solver, checker, proof emitter,
public action, canonical route, or active campaign was changed.

The frozen forward checker at `-f -V` reports its failure at addition ordinal
12, which is physical ASCII proof line 26, candidate `20 -13`:

```
Error: [line 12] No UP contradiction for RAT clause 15.
failing RAT clause: [15] 19 -18 0
RAT clause under substitution: FF -18 0
current substitution: -13 -19 20 (14 -> -15) (15 -> 14)
unit 19 due to clause [15] 19 -18 0
```

The checker-visible formula immediately before that physical line, obtained by
feeding the first 25 ASCII proof lines to frozen `dsr-trim -f
--emit-valid-formula-to /dev/stderr`, still contains original input clause 15
`-18 19`. It has explicit proof units `17`, `18`, `16`, and `-12`; hence its
global UP derives `19` through that retained reason. Tactix's emitted witness
maps `19` to false because its live formula has retired the reason. The
checker fails the corresponding SR obligation. This identifies the *exact*
state mismatch, not merely a later watch-pointer symptom.

Checker-only controls on AM437's archived ASCII representation sharpen it:
omitting mapping `19 -> -19` only from physical line 26 moves the first
failure to physical line 31, whose witness maps `19 -> 15`; omitting that
pair and the line-36 `19 -> -19` pair moves failure again to physical line
41, candidate `20 -19`. Each changed proof still rejects under the frozen
no-option forward checker. These variants are not proposed solver proofs:
they demonstrate that changing one witness or publishing one unit merely
shifts the checker-visible obligation. The separate AM437 explicit-unit
variants turn the retained reason into an undeletable true-unit obligation.

Reproduction (the archived baseline is an exact ASCII spelling of the binary
proof; only this spelling is used in the `sed` controls):

```
P=benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/dsr-trim
C=benchmarks/probes/am436-h2-shrink/minimal.cnf
A=benchmarks/probes/am437-h2-unit-publication/artifacts.tar.gz
$P -f -V $C benchmarks/probes/am436-h2-shrink/minimal.dsr /dev/null
tar -xOzf $A ./baseline.dsr | head -n 25 | \
  $P -f --emit-valid-formula-to /dev/stderr $C /dev/stdin /dev/null
tar -xOzf $A ./baseline.dsr | \
  sed '26s/ 19 -19 0/ 0/' | $P -f -V $C /dev/stdin /dev/null
```

This is evidence against a local reorder or one-witness patch, not proof of
impossibility of every alternative derivation. A general no-option repair
would need to represent the public SR transformation against the checker's
retained formula (possibly via a different derivation), without declining an
otherwise legal public X7 instance. An audited alternative checker contract
remains a separate user choice; AM404/407 prove its option mode is not
monotone, so silently switching is not justified. H2/N2 remain open.

At this checkpoint, the AM401, AM412 and AM414 Claude/Opus owner reports are
terminal affected-scope PASS; AM401's broader frozen CLI campaign is NOT MET
for coverage/resource reasons, not a newly reported correctness failure.
Current-source full Debug job 1723391 and paired AES checker jobs 1723443_0/1
were still running. These scoped reports and partial jobs do not prove N1--N4
or authorize the deferred Tier-D submission.
