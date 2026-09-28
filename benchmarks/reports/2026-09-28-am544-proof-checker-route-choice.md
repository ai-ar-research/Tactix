# AM544: proposed content-declared checker split, not yet adopted

The user asked whether proof checking should adapt between `drat-trim`/CakeLPR and DSR/Trestle. This is a **checker-contract decision**, not a solver inference change. The existing canonical contract, chosen earlier by the user, sends every DSR certificate through pinned no-option `dsr-trim`; it ends at CakeLPR for witness-free LSR or Trestle for witnessed LSR. The submitted AM541 full Tier D bundle `1735296` pins that contract and is held pending the four-winner array. No checker route or active campaign has been changed by this proposal.

The exact alternative would use the solver's final declared route and validate the entire proof's syntax/content before invoking one independent chain:

| Declared and validated certificate | Proposed complete independent chain |
|---|---|
| Complete startup PBP | VeriPB elaboration → CakePB against original CNF |
| Witness-free DSR | Checked DSR-to-DRAT conversion (binary tag rewrite or ASCII copy) → `drat-trim -L` → CakeLPR against original CNF |
| DSR with an SR witness | No-option `dsr-trim` → `dsr-compress -d` for binary LSR → Trestle/srcheck against original CNF |

The route is fixed by the solver's end-of-run declaration, cross-checked with proof content; it does not guess from a checker timeout and does not retry another checker after rejection. A witnessed DSR line is not a DRAT line: `drat-trim` could erase the repeated witness pivot as a duplicate literal. The existing `proof_oracle.py` converter validates the full stream before writing the DRAT form and declines a witnessed line. The startup-PBP chain remains unchanged. Native LRAT/DRAT compatibility modes retain their explicit checkers.

Rank 22 is the concrete reason to consider this change. AM487's 790,326,542-byte original witness-free DSR proof (SHA-256 `089054c9a2807f7720f59c6387f98ba294d5313a17b951beca0171d2b71ab400`) was converted to DRAT and checked by `drat-trim` in 7,250.176 seconds and CakeLPR in 54.086 seconds (7,710.806 seconds total pipeline). AM519 emitted the **identical original DSR proof SHA** but its current no-option `dsr-trim` stage timed out after 15,824 seconds inside a 16,000-second checker window; CakeLPR never ran. A 45,000-second unchanged-canonical diagnostic `1734775_0` is still running. The earlier result establishes independent validity on its own route and a specific performance advantage here, but does not prove every witness-free proof will check faster through DRAT.

Adoption requires a new immutable checker/runner pin and review of the exact route markers and negative controls, followed by public default/random fuzz and affected proof/model regression checks. The already-submitted AM541 array cannot be relabeled as having used this alternative. If the user selects the new contract before AM541 begins, cancel the pending AM541 array and its two dependent guards, prepare and compute-node-validate a new full Tier D bundle with the alternative pin, then resubmit it after the winner array with the same resources and storage guard. The old canonical job/result history remains preserved. Until a choice is explicit, keep AM541 and the Rank diagnostic unchanged.
