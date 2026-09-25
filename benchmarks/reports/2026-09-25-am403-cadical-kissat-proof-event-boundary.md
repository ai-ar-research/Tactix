# AM403: CaDiCaL/Kissat proof-event boundary audit

Date: 2026-09-25. Read-only review of pinned CaDiCaL 3.0.0 and the local
SC24 Kissat/AE-Kissat-MAB source against Tactix `ac624a12`. This closes the
*emitter-interface* comparison left by AM398/402, not the producer-by-producer
semantic, N1 reachability, or N2 certificate audit.

## Source boundary

CaDiCaL's `Proof` dispatch sends derived-clause, deletion, restoration
metadata, strengthening, and incremental query events to `Tracer`
(`cadical/src/proof.cpp:556-674`). The interface makes a nonincremental
derived clause either witnessless or RAT-pivot witnessed
(`cadical/src/tracer.hpp:29-78`); its separate assumption/constraint/conclusion
events are specifically incremental (`tracer.hpp:104-148`).

The VeriPB serializer does **not** show a further general solver inference:
`VeripbTracer::add_derived_clause` selects one of three renderings of that
same clause event (`cadical/src/veripbtracer.cpp:323-335`): `red` for a RAT
pivot and its simple witness (`:193-215`), `pol` for an antecedent resolution
chain (`:218-253`), or PB `rup` without antecedents (`:255-271`). It can emit
`core id`/`delc` under checked-deletion mode (`:281-310`), and its
`weaken_minus` bookkeeping tracks a clause that may be restored
(`:364-378`; `cadical/src/proof.cpp:396-419`). Those are proof-format,
deletion-checking, and model-restoration semantics, not an uncovered
nonincremental PB arithmetic rule in CaDiCaL's producer. A literal search of
the pinned `cadical/src/*.cpp` finds the `red`, `pol`, `rup`, and PB header
output in this serializer only. This does **not** prove the incremental
protocol is implemented in Tactix; it remains deferred by plan §5.1/O7.

The complete `proof.c` in SC24 Kissat and AE-Kissat-MAB is byte-identical
(SHA-256 `dd393d3767b53cb3c0fb26a6dba3e4ad4dd21460b8dba018f942e128003dedda`,
reconfirming AM398). Its exported `proof.h:23-37` and
`proof.c:287-403` provide binary/clause/unit/empty additions, one shrink
operation implemented as add-before-delete, and deletions. The binary/text
serializer writes an addition or `d` deletion (`proof.c:287-314`); RAT is
encoded by the first added literal and inferred by its checker rather than a
new output command. Tactix's typed RUP/RAT/delete event vocabulary
(`src/proof/proof.hpp:72-106`) covers these clausal proof primitives. This is
an emitter-interface comparison only; it does not prove every Kissat
transformation is correctly or competitively implemented.

## What this does and does not establish

The current 30/30 inventory checker reports that every declared catalog
family has a *disposition*. Its own docstring explicitly says it does not
verify registration, actual reachability, or the truth of those dispositions
(`tools/docs/check-inventory.py:2-18`). The current inventory still marks X7
full acceptance in progress (`docs/implementation-notes.md:428`), while X5's
explicit cube action is implemented but its CaDiCaL producer is default-off
(`:426`, §16). Neither that green checker nor the source interface review can
support a combined N1/N2 verdict.

The AM402 winner finding remains distinct: listed LymphoSAT family paths and
Satsuma's VeriPB mode emit general PB arithmetic/redundancy/dominance
derivations, whereas Tactix's PB emitters are limited to terminal Lucky 19/20
startup refutations. CaDiCaL's `pol` serialization of a clausal chain should
not be counted as an additional missing semantic rule. The next work is a
source-to-action audit of actual CaDiCaL/Kissat producers, then independent
public-instance and certificate verification on the combined current pin.

At review time AM400/401 full Debug jobs `1720786`/`1720898` were running;
AM400/401 affected Opus owners had no terminal acceptance verdict. H2 and
post-finalization public additions still obstruct N1/N2. No Tier-D run was
submitted.
