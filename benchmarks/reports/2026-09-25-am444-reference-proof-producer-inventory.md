# AM444: current proof-producer call-site inventory

Date: 2026-09-25. Read-only source audit; no solver or reference source was
changed. Tactix is `92d3fc73`; local reference heads are Kissat
`8af8e56f`, CaDiCaL `7b99c07f`, SC24 Kissat `1e56df6c`, and
AE-Kissat-MAB `da40eb95`. AM398, AM402, and AM403 own the prior winner,
PB-calculus, and emitter-interface conclusions. This check advances the
source-to-action inventory, not N1/N2 acceptance.

The maintained checks pass on this source: `check-inventory.py` reports
30/30 catalog families disposed; `check-proof-rule-sync.py` reports 25
calculus rules and 26 guide cards with zero mismatches; the rule/policy
boundary checker passes. `check-option-coverage.py` accounts for 161 Kissat
and 257 CaDiCaL options, 418 total. These are structural checks, not proof
or performance oracles.

A bounded scan of direct clausal proof-emitter calls, excluding the serializer
itself, finds 57 calls across 16 Kissat producer files and 60 calls across 13
CaDiCaL producer files. The scope is Kissat's `ADD_*_TO_PROOF`,
`SHRINK_CLAUSE_IN_PROOF`, `DELETE_*_FROM_PROOF` macros and CaDiCaL's direct
`proof->add_derived*`, `delete_clause`, `weaken_*`, and `strengthen` calls.
Representative source-to-action dispositions are:

| Reference producer files | Tactix action/effect |
| --- | --- |
| Kissat `analyze`, `assign`, `propdense`, `internal`, `collect` | M1/M3 root/search derivation, S2 retirement or input normalization |
| Kissat `congruence`, `definition`, `eliminate`, `fastel`, `resolve` | V6/E1 clausal consequences and elimination |
| Kissat `forward`, `strengthen`, `substitute`, `sweep`, `vivify` | C1/C2/V3/V5/C3 |
| CaDiCaL `analyze`, `clause`, `block`, `cover`, `condition` | M3/input and X1/X3/X5 deletion with reconstruction |
| CaDiCaL `congruence`, `decompose`, `definition`, `elim`, `factor` | V6/V3/E1/E2 |
| CaDiCaL `subsume`, `sweep`, `vivify` | C1/C2/V5/C3 |

The clausal event vocabulary remains RUP/RAT/SR/deletion. Kissat/AE-Kissat
emitter identity and CaDiCaL's nonincremental `pol` translation are pinned in
AM398/403. Satsuma's SR-mode cut/orbitopal events map to X7 at this level,
while LymphoSAT/Satsuma PB arithmetic, redundancy and dominance paths are a
real **general live PB-calculus gap** beyond terminal startup PBP (AM402/408).
CaDiCaL incremental assumption/constraint/conclusion events are a separate
deferred O7 protocol, not a missing nonincremental proof inference.

This inventory cannot establish that each reference transformation's complete
preconditions, reconstruction, scheduling, or emitted certificate is matched.
In particular X7 full acceptance remains in progress: the public H2 legal
schedule fails the canonical no-option DSR route (AM436/439/443). The 30/30
and 418/418 checks must not be used as N1/N2/N4 gate passes. The outstanding
producer-by-producer semantic comparison and combined current-source
proof/model/fuzz/benchmark campaign remain required before online-learning
readiness or Tier D can be accepted.
