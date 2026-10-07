# Retained graph public action checkpoint

Commit7eedae8d2 preserves Lucky21's monolithic contract and appends scalar26 authentication and27 continuation. Public C++/C, online, replay and Default share one Kernel-owned session and typed executor, reusing GraphLabelingParameters, C tag30 and graph-labeling1. Default feedback names26/27 separately; startup/root-window guards remain and graph stays off by default.

Exact-profile reauthentication skips recognition. Valid changed-profile authentication clears/rebuilds; mismatched continuation and foreign-Kernel sessions decline. Kernel copies start empty; moved state retains its owner guard and requires clear/re-authentication on a new Kernel. Default solve resets its Kernel trajectory. Partial stages publish only phase hints; completed candidates require whole-current-CNF validation. Entry interruption cannot advance the stage cursor.

Frozen source: benchmarks/runs/20261007-graph-public-actions-v3; base4e78714e0bfb8b9df02d4d180e691ffcf5c2a831 plus manifest-owned overlays; SHA25655577b6652f5946ffb2b0e95787b12bf2297cce7a7b65994c54eea9e1e0cfeec. SLURM array1753905 uses cpu-q,16 CPUs/64GiB per Debug/ASan arm, fresh shared scratch cleaned at exit, assertions enabled, -g1 symbols, ASan leak/abort checks and UBSan halt-on-error.

At checkpoint9/10 tests pass in both arms: graph_labeling_actions, graph_labeling_model, instantiate, lucky_terminal, api_n1, api_instances, random_controller_policy, feedback_trace and random_online. The broader lucky trajectory test and downstream independent checkmodel are pending. Root explicitly authorized this reviewable checkpoint to unblock user-directed noL retirement; immutable validation continues. No full-regression, fuzz or performance acceptance is claimed. Doc size/section refs/default-config checks pass.

Predecessor1753891 caught a missing Lucky header, fixed.1753902 compiled all sources but Debug links exhausted compute-local /tmp; its sanitizer was cancelled as obsolete.1753905 moves scratch to the shared repository and limits concurrent links. Preliminary pins are not acceptance evidence.

Inner-stage resumability and complete memory metering remain open. The independent WindowCursor helper exists, but its adapter is deferred behind noL removal. Serialized rule_work_budget already supplies per-call dose independently of retained stage-profile totals. Shared graph/Default/API files were released after this checkpoint.
