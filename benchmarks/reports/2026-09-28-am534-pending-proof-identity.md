# AM534: content identity of the two long no-MAB proof checks

The two remaining no-MAB solver phases are UNSAT and have generated complete provisional certificates; their run-local independent checkers remain active. This is a cross-run content comparison, not a substitute for the frozen no-MAB scorer.

| No-MAB row | Current proof SHA-256 / size | Earlier checked identical proof | Checker-route distinction |
| --- | --- | --- | --- |
| Rank 22, `oisc-subrv-sll-nested-8` (`1732161_21`) | `089054c9a2807f7720f59c6387f98ba294d5313a17b951beca0171d2b71ab400`, 790,326,542 bytes | AM487 `results/021.json` verified on the same input | AM487 used the older witness-free-DSR-as-DRAT → drat-trim → CakeLPR route. Current no-MAB canonical dsr-trim route must finish. |
| WV100 Rank 90, `ncc_none_21015...` (`1732212_89`) | `f63206c14c4d7a27d32148defdecebcb7370a145ccd7bdb891064af2853fc7c4`, 401,269,091 bytes | AM507 `results/089.json` verified on the same input | AM507 used the same pinned no-option dsr-trim → dsr-compress → CakeLPR route as the no-MAB job. |

Both pairwise comparisons use identical instance paths, original-input SHA-256 values and proof SHA-256 values from their frozen manifests/results. The relevant pinned dsr-trim, dsr-compress, srcheck and CakeLPR hashes match across the pairs. Thus the no-MAB `ncc` proof has already been independently verified as bytes against the identical original CNF under the same canonical route on a prior source pin; only its run-local completion/scorer record remains pending. Rank 22 has prior independent proof verification but not yet under the current required elaboration route. Neither current job was canceled or relabeled complete. A checker rejection, resource failure or cleanup issue in a current run remains reportable rather than silently overridden by this identity comparison.
