# RB-SAT DirectCSP causal preflight

This focused preflight selects the two exact LymphoSAT `rbsat` ownership rows
WV100 ranks 93 and 98 (Tier-D ranks 792 and 507) as one DirectCSP cohort.  The
retained modified-LymphoSAT dispatcher log records, for each row, only
`matcher rbsat accepted formula` followed by `solved by rbsat (exit 10)`.
The representative is WV93
`a8608545f9ae0462c7426e71a0e3e634-rbsat-v945c61409g3.cnf.xz`, compressed
SHA-256 `9a231ae7bf89cdaf3266d8798c3c4d8b42e61d262d1d6507cacf6c8eaf8d2ed0`
and decompressed SHA-256
`00f51d50b9841fe5b74a6da44924b80751101050c621f5b935213c3d4dbc7ca`.
All linked Tactix objects came from
`114ba0d7a574929280115b8f0f3954c8d6452010`. No production source changed.

## Causal ladder

The public X13 `DirectCspSupport` detector and action are structurally the
right existing mechanism: signed exact-one domains plus a complete directed
forbidden-pair relation entail one support row.

1. CPU-Q job `1750888` ran an offer-only probe. It found 45 domain candidates,
   reached two ordered-pair candidates and 37 row candidates, rejected no row
   for resources, and returned a first page of 16 exact offers. The complete
   page cost 7,359,226 enumeration work; the domain scan alone cost 791,510.
2. CPU-Q job `1750890` applied the first exact offer. It retained one owner and
   one durable key, added one certified 17-literal support clause, introduced
   no variable, and then invoked the separate propagation action. Propagation
   completed with trail `0 -> 0`: the sound non-unit consequence produced no
   immediate Boolean fact.
3. Continuing with the current DefaultPolicy returned SAT in 186.11 seconds.
   The pinned independent model checker accepted all 61,409 original clauses
   over 945 variables.
4. Same-source plain control job `1750896` returned SAT in 146.86 seconds with
   zero decisions, conflicts, and propagations: the retained one-hot model
   constructor reported one candidate and SAT. Its independently checked
   canonical model SHA-256 is
   `6f163ad5850eb12508c4e9146d26f8ea9fdece8027bdef3d45f3803e1d98d917`,
   exactly the model returned after the support action. The one timing sample
   does not establish a performance regression; it establishes that one
   support row did not alter the chosen model and supplies no evidence that
   the action path can replace the constructor.

The first missing useful-effect rung is therefore after successful certified
publication: one support row is non-unit and creates no propagation. Detection,
offering, application, continuing solve, and external model checking all work.

## Earlier production-controller boundary

The native producer is not usable from its stock controller configuration.
`RandomOnlineConfig::direct_csp_support_reference` is default-constructed with
all zero limits, so the offerer rejects it as an invalid action profile. Even
after installing a valid profile, the producer's stock 100,000 enumeration
allowance is below this representative's 791,510-work domain scan. This is a
configuration boundary, not a detector failure.

The smallest next repair is to install a valid, benchmark-independent bounded
DirectCSP reference profile and a producer page allowance large enough for its
bounded source census. Then exercise the existing shared lifecycle repeatedly:
`DirectCspSupport -> Propagate -> SemanticVariableAdvice`, retaining exact
row keys and stopping by an explicit action/effort allowance before resuming
search. The first follow-up experiment should compare one row with a small
deterministic repeated batch on WV93 and require the original-CNF model checker;
WV98 is the cohort confirmation only after the representative shows actual
propagation or search benefit.

This is a causal preflight, not a target-closure or regression-gate claim. The
existing SAT constructor remains the checked recovery route in these jobs.

## Machine-readable evidence

- `offer-result.json`: exact job `1750888` offer accounting.
- `action-result.json`: exact job `1750890` apply, propagation, solve and model
  check accounting.
- `control-result.json`: exact job `1750896` same-source plain control.
- `SHA256SUMS`: hashes for these files and retained external probe artifacts.
