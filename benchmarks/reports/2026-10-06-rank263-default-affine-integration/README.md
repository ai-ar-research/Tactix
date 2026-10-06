# Rank 263 DefaultPolicy affine integration

Commit `3676cff1c95eed187455fc327321d7e0e34c82d3` integrates the
certified affine root-closure mechanism into the ordinary DefaultPolicy.
DefaultPolicy attempts it after initial root propagation and before
formula-changing startup passes.  Admission requires the recognized affine
image to cover every live irredundant clause exactly and matrix construction to
finish completely; otherwise the pass makes no formula or proof change.

CPU-Q build job `1751029` built an immutable Release binary with SHA-256
`88347e1c2a2835cefe6a92a9371d1203f412e64291ed16217c13b087865ec8aa`.
Paired job `1751030` ran that binary through the unmodified DefaultPolicy on
Tier-D rank 263, once with DSR and once without proof output.  Both arms returned
UNSAT at root with zero decisions, one conflict and 577 propagations.  The DSR
arm took 0.72 seconds and the no-proof arm 0.76 seconds.  The DSR is 4,991,343
bytes; its SHA-256 is
`6fbc0514e27aa0ea7b8a7d7253a0b6b3c3cd312132d6ec1e60da5bff9259e666`.
It is byte-identical to the earlier opt-in causal proof.

The raw feedback records a complete-image initialization followed by one
affine-matrix conflict event.  Initialization consumed 175,317 measured work;
the service record consumed 25,050,074 aggregate affine-matrix work.  The
retained DSR-arm provenance is 43,203,878 bytes and has SHA-256
`e6bcd33a568bb59f2bcd4f71c3e49f1b7b2722713dc1752f875796a8d5639031`.

CPU-Q job `1751032` independently checked the new proof with the repository's
witnessed DSR/Trestle pipeline and the every-lemma audit.  It returned
`s VERIFIED UNSAT` in 46.01 seconds with 131,652 KiB peak RSS, below the
30-minute checker cap.

This closes the Rank-263 integration boundary: its measured CryptoMiniSat-like
requirement is now part of DefaultPolicy rather than an opt-in controller
profile.  It does not establish a general incremental XOR engine.  Component
partitioning, useful-work scheduling, watched or touched rows, local pivot
repair and efficient backtrack state remain open for affine reasoning that must
continue during CDCL.  Full Debug, both-policy fuzz and the four regression
campaigns also remain required before broader acceptance.
