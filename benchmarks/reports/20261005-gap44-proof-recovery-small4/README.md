# Four-row proof recovery under the 30-minute checker cap

Job array `1750469` regenerated ranks 291, 541, 744, and 1129 from current pinned source `9b3508b51fe22c4b13c644564321d0fd4baa7ec5`. Every solver output is byte-for-byte identical to the corresponding `d170b4a5` Tier-D certificate; solve behavior therefore did not change materially.

No row verified within the current 1,800-second whole-pipeline checker cap. Rank 291 completed `dsr-trim` in 679.356 seconds and `dsr-compress` in 10.781 seconds, then exhausted the remaining allowance in independent checking. The witness-free route for ranks 541, 744, and 1129 exhausted the allowance during route/conversion work before an external checker stage completed. All four failed proofs were retained with an exact SHA-256, `artifact_deleted=false`, and `cleanup_pending=true`.

Rank 744 separately qualifies for the fixed 44-target under the older 14,400-second protocol through two independently verified, byte-identical certificates documented in `../20261005-gap44-certificate-identity-audit/`. It still fails the current 1,800-second regression cap.

A checker-only canonical DSR retry is limited to ranks 541 and 1129. The concrete hypothesis is that native `dsr-trim` avoids the witness-free conversion that consumed the standard route's full allowance; rank 291 shows that `dsr-trim` can elaborate a larger 424.5 MB proof in 679 seconds. Immutable validation job `1750515` passed, and checker-only job `1750516` was submitted. No solver is invoked. `report.json` binds every source result, certificate, retained artifact, stage, and campaign manifest hash.
