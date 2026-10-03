# Internal live parity consequence adapter

## Implemented transition

`parity_live_apply` connects the certified premise collector, bounded GF(2) selector, extension-tree proof engine and kernel namespace reservation. It snapshots exact live large/binary clauses at a quiescent sparse root, using action-local occurrence identities and stable external literals rather than binary-ID sentinels. Its explicit request supplies discovery, elimination, tree-extent and proof-addition limits, a selected consequence, pivot direction and a provenance stream/action identity.

The adapter suppresses a selected relation whose clauses already exist. It preflights the whole derivation and cleanup without kernel mutation, reserves permanent external helper identities, then replays the bounded emission atomically. Counts and final helper identity must agree with preflight. This sequence runs identically with and without an attached proof writer. LRAT/PBP are explicitly unsupported by this internal path. There is no fallback, rollback or global effort budget.

Every derived original-variable unit is retained in the proof and published to the kernel, including useful intermediate units. The final exported binaries/clauses enter ordinary kernel insertion paths, with root propagation drained afterward. A derived contradiction is propagated as UNSAT rather than returning a no-op. Helper clauses are retired, but their identities remain reserved. Proof/output failure is fatal after atomic commitment; interruption and insufficient bounds decline during discovery/preflight.

The streamed record includes request parameters, exact source clauses, selected source-equation occurrence lists, fresh-helper events, typed RUP/RAT additions, deletions naming active addition IDs, and kernel publication. Only active local proof identities are retained in the sink map; retired entries are removed. This is the internal adapter's provenance contract, not yet the complete public controller replay/feedback protocol. The caller must provide a unique action identity and bind the stream to its input/run authority.

## Fixed evidence

The [retained run](../runs/20261003-parity-live-adapter/) builds current kernel and adapter objects against the immutable reference parent. Final pin: `build/pin-parity-live-adapter-2d68c45f-04`. Assertions in `test_parity_live` check:

- insufficient proof allowance returns NoOp without namespace mutation;
- the four-equation cancellation installs both equivalence binaries and reserves helpers without adding search variables;
- repetition does not add duplicate clauses or reserve more helpers;
- a later live extension receives an ID beyond the reserved range;
- continued search propagation uses the equivalence, and the resulting assignment satisfies every live clause while original output width remains seven;
- proof-on/off external maps agree;
- contradictory complete encodings report UNSAT.

The native nonterminal certificate is checked against its actual 16 source clauses plus a disjoint initially unit-free closing gadget beyond every helper identity. Canonical witness-free DSR conversion/DRAT/CakeLPR, forward every-addition DRAT, and every-addition LRAT/CakeLPR pass. Prefix hints never cite gadget premises. An unsupported unit is rejected. A separate provenance audit checks each deletion against its named active addition, finds both published equivalence clauses still present, and confirms no helper literals remain in the adapter's tracked active proof clauses.

These are fixed semantic/proof controls, not real-instance performance evidence. The focused link inherits unrelated objects from the graph-reflection parent, so it is not an all-current-source full Debug pin. Initial pins precede provenance/limit refinements and remain diagnostic only; final hashes identify the tested source.

## Remaining acceptance

This adapter is not registered or selected by DefaultPolicy or the random controller. Next expose it through a shared typed public action with complete replay/raw feedback, action eligibility, intervals and relevant-change scheduling. The mandatory provenance-stream binding also needs executor integration. Add targeted unit/large-clause, root-conflict, interruption, remapping and negative-interface coverage before promotion. Full Debug, current-source Claude-owned both-policy fuzz and enabled B28+C5, Rank1–50, WV100 and WV2–89 regression gates remain pending. No target or baseline campaign was run or newly accepted. The other structural integrations and all 44 misses remain in scope.
