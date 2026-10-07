# Storage and aggressive disposable-object cleanup — 2026-10-07

The repository apparent-size scan completed at 746,019,287,694 bytes, triggering a cleanup audit under the user's 700 GB instruction. A separate completed allocated-block scan totals approximately 339 GB over the principal directories (see raw output); file lengths and allocated blocks differ materially here. Do not claim the apparent number is actual quota consumption or a fresh post-cleanup disk measurement.

Removed exactly 2,177 unbound compiler `.o` outputs: 19,202,388,664 apparent bytes and 3,984,900,608 allocated bytes. Both exact removal inventories, directory restrictions, inbound-reference audits and active-job map are frozen here. The removed paths were checked absent afterward. Bound control objects and all current XOR source/object/build dependencies were excluded. Four retired full-Debug builds were already terminal; their bound executables, manifests, archived sources, logs and compiler/CMake metadata remain intact. No active job was cancelled and no evidence was rewritten.

This cleanup removes regenerable compiler outputs only. Latest cumulative builds may continue increasing usage; development must continue periodic measurements and further audited cleanup if actual usage exceeds 700 GB. Preserved artifacts are not assumed disposable merely because their parent jobs finished.
