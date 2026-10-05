# Native n-ary/domain producer census

Job array `1750492` ran the seven fixed n-ary/domain gap targets for 300 seconds with policy seed 2, only the public `NaryCspPrefix` root family, and only the native `nary-csp` producer. Six rows never requested the rule and recorded no producer entry. Rank 1161 entered the producer once, spent 35,925 work units, found zero candidates and zero offers, and therefore has the exact disposition `no-offer`. No row applied the action.

This campaign is scheduling/profile nonreach, not a negative recognizer result. Its profile omitted structural provenance, which is one fact in the public action guard. The final classifications and per-row counters are frozen in `census.json`.

A single corrected rank401 diagnostic then enabled job-local structural provenance and retained the same pinned diagnostic binary and 30-second cap. Job `1750519` recorded exactly:

```
c controller-startup-availability rule=NaryCspPrefix eligible=0 reason=guard-false discover-entered=0 native-offered=0
```

The action still failed its public guard before native discovery. There was no producer call, candidate census, or offer, so the only valid producer disposition is `producer-not-entered-due-guard`. The 118-byte provenance artifact proves that the corrected run enabled the required output. `rank401-startup.json` binds the binary and input hashes, exact command, stdout hash, proof/provenance hashes, scheduler node, and selected diagnostic lines.

The diagnostic intentionally reports the public availability boundary without bypassing it. The collapsed `guard-false` does not identify which remaining guard fact failed, so this evidence does not justify a seven-row rerun or a recognition change.
