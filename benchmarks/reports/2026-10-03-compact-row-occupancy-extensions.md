# Compact row-occupancy extensions using certified interval variables

## Result and distinction

For the previously audited rank-991 graph encoding, every row can be partitioned
into three already-certified absence intervals. Defining one occupancy variable
per row therefore needs only **117 fresh variables, 468 clauses, 1,170 literal
occurrences and 8,424 bytes of text DSR additions**. This is a conservative
extension, not an assertion of the row-cover clauses whose global counting proof
proved expensive. No target solve or performance gain is claimed.

The definitions give CDCL explicit variables for meaningful structural choices.
They are a candidate way to translate theory structure into Boolean reasoning
without dedicated local search. They do not replace the remaining obligation to
derive useful consequences, investigate compact counting, recover the owned
targets and eventually remove the reference constructors.

## Construction and soundness

Let a,b,c be existing literals meaning that their respective disjoint intervals
contain no selected label. The intervals partition one whole row. Introduce fresh
p with clauses (p OR a), (p OR b), (p OR c), and
(NOT p OR NOT a OR NOT b OR NOT c). Thus p means that some position in the row is
selected. Every assignment to old variables has exactly one extension,
p = NOT(a AND b AND c). Reconstructed models project p away.

Each positive-pivot clause is RAT by freshness: no negative p occurs yet. The
last clause is RAT on negative p; each resolvent with an earlier positive-pivot
clause contains a complementary literal pair. No original premise or closing
gadget is needed to establish these definitional proof steps. The row meaning
relies separately on the previously certified original-clause interval meanings.
The stable planned rule statement is in `docs/calculus.md`.

The diagnostic selector minimizes the number of existing interval blocks in an
exact row partition. All 117 selected partitions have three blocks (widths
50,50,17); no 117-literal row definition or global matrix BDD is introduced.
`proposed-definitions.json` records every auxiliary, Boolean clause, semantic
meaning, block map, source defining-clause occurrence, action identity, proof
schema, lifetime and projection obligation. The frozen interval provenance binds
all recursive gate premises. This is still a diagnostic mapping of the audited
input; a native bounded mapping/selection implementation is not yet supplied.

## Checked evidence

`benchmarks/runs/20261003-row-occupancy-extension/` owns static reuse and the
local proof. Exhaustive enumeration verifies a unique extension for all eight
assignments of the three abstract inputs. CakeLPR checks every LRAT addition;
forward DRAT also verifies the definition. Inserting an unjustified occupancy
unit into that local proof fails the RAT check at the intended line. The initial
static admission compared compressed bytes with an uncompressed input hash and
stopped before checking; the corrected hash convention matches the source
recovery tool. This harness correction is retained explicitly.

`benchmarks/runs/20261003-row-occupancy-prefix-check/` owns the target-prefix
validation. Job **1747979** completed. All 468 additions over the actual original
CNF pass explicit all-addition LRAT/CakeLPR (0.168477 seconds) and forward DRAT
(0.391788 seconds). A disjoint initially unit-free contradiction gadget supplies
a terminal suffix. No definition hint references its premises; no backward core
trimming is used to claim prefix validity. A negative proof that attempts an
unjustified empty clause is rejected after parsing the input (RUP fails and no
nonzero RAT pivot exists). These are proof-checking times, not solver times.

## Use in search and limits

A policy may prefer a positive decision on an occupancy variable and prioritize
it through the shared branch interface. This can activate a compact row-cover
condition during that search branch. Such phase/priority advice is not an
inference: do not add p as a permanent unit without its own proof, and do not
hide an assumed row cover behind the definition. Ordinary backtracking and proof
production must remain correct for either branch.

Next build a bounded live definitional-extension path, preserving existing
semantic auxiliaries, source identity and model projection. Study whether normal
CDCL with these variables and explicit policy advice recovers the graph target
with dedicated constructors disabled. No special local-search procedure or
fixed solution assignment is introduced. Acceptance still requires public typed
selection, replay, pure observation, after-action feedback, both-policy fuzz and
the four enabled regression gates. The new definitions are not installed in
production and no target is newly closed.

## Independent validation workstream

The current Ramsey/parity/graph snapshot now has **229 full Debug tests** running
as job **1747977**, with 191 immutable executables, 832 archived source/support
files and 565 verified runtime/archive bindings. The compilation finished; this
is not a completed suite result. Claude CLI session **9899** owns the new
`20261003-ramsey-full-fuzz-handoff`, requiring fresh ordinary default/random legs
and effective Ramsey/X8 coverage with independent proof/model oracles. Read only
its designated public owner report or bounded reproducer. No acceptance is
inferred from a running process or an ordinary lane with the actions disabled.
