# Refactor acceptance: behavior and performance preservation

User requirement added after the online-strategy proposal was submitted to
Claude Code for review. The original proposal and review request remain frozen;
this addendum strengthens acceptance without rewriting the reviewed artifact.

For existing configurations, especially shipped DefaultPolicy, the structural
refactor must preserve the pre-change solver's behavior and performance.
New strategy capabilities and expanded random exploration are opt-in until
their explicit configuration is selected. Preserve legacy reproducibility where
the same existing configuration/seed is replayed.

The exact immutable comparison source is recorded before implementation. Match
configuration, input, seed, proof mode, source/build flags and environment. Check
semantic action sequences, instance choices, work counters, verdicts and proof
bytes; normalize only already-justified diagnostic identities. Do not weaken
the comparison to final SAT/UNSAT agreement. Memory/representation metadata may
need separate classification; it is not permission to hide changed solving work.

Matched runtime measurements must show no regression beyond measured noise,
with the required paired repetitions and spread reported. Exact equality of
wall-clock readings is not meaningful; a noisy result is inconclusive, not an
automatic pass. Do not use the separate5% hardcoded-twin overhead allowance as
a permissible5% regression against the pre-refactor implementation. Preserve
the existing memory gates and check for new bookkeeping in runtime builds.

An N1 or soundness repair can conflict with exact legacy behavior. Keep such
semantic repairs isolated from structural moves, preserve their failure
witnesses, and explicitly report the conflict before changing acceptance.
Never preserve a known unsound path merely to reproduce an old trace, or claim
both unchanged behavior and expanded behavior under the identical configuration.

The registered goal's immutable text already requires reference-default behavior
and performance preservation; DEVELOPMENT_PLAN.md and PROGRESS.md now make this
stronger comparison criterion explicit. Claude's original critique must be read
with this addendum before implementation.

A later user decision also fixes the action boundary: inprocessing rules and
root-phase Lucky apply only to the level-zero formula. A strategy that is
currently searching composes an explicit `Restart` before either operation.
The executor does not suspend, rewrite, or replay a non-root decision trail.
This supersedes the proposal's open D-ROOT question without changing the
reviewed proposal artifact.
