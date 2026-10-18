# Typed controller controls (historical `controller-v5` fixture name)

These are exact caller-owned controller requests, not generic discovery hints.
`gf3.actions` selects two authenticated equation pairs; `direct-csp.actions`
selects two rows over the fixed domains `{1,2,3}` and `{4,5,6,7}`. Both pin
their complete semantic and resource parameters. The current trace records
action-space v8; the directory and CLI flag retain their historical names.

`controller_v5.py` generates matching inputs. GF(3) recognition requires the
whole source to be a complete forbidden-tuple affine encoding, so its SAT cases
vary equation right-hand sides and its UNSAT cases use a minimally inconsistent
three-equation affine cycle whose first selected pair still publishes a new
consequence. Appending arbitrary disjoint clauses would make that recognition
ambiguous and is deliberately excluded. Direct CSP authenticates the selected
domains and relation locally, so its surrounding Boolean component can be
mutated on fresh variables. No mutation creates a root unit: both public action
guards require an assignment-free root snapshot.

Every counted case must report a nonzero `applications` and `effective` source
counter for its selected family before its SAT model or UNSAT DSR proof counts.
The campaign also checks seeded trace identity and record/replay proof, model,
and counter identity on one SAT fixture per family.
