#!/usr/bin/env python3
"""Derive a fresh graph probe from immutable objects and audited dependencies."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / "build/pin-winner-graph-probe-d85b30b4-01"
pin = root / "build/pin-winner-graph-reflection-113b3472-01"
pin.mkdir(exist_ok=False)
old = json.loads((root / "benchmarks/runs/20261003-winner-graph-probe-01/source-hashes.json").read_text())
current = json.loads((root / "benchmarks/runs/20261003-winner-graph-fuzz-handoff/source-hashes.json").read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert all(sha(root / p) == h for p, h in current.items())
changed = {str(root / p) for p, h in old.items() if current.get(p) != h}
affected = []
for depfile in parent.rglob("*.o.d"):
    dependencies = set(shlex.split(depfile.read_text().replace("\\\n", " ")))
    if dependencies & changed:
        affected.append(str(depfile.relative_to(parent))[:-2])
graph_object = "CMakeFiles/tactix_lib.dir/src/rules/lucky/graph_labeling_model.cpp.o"
assert affected == [graph_object], affected
flags = {}
for line in (parent / "CMakeFiles/tactix_lib.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((parent / "CMakeFiles/tactix.dir/link.txt").read_text())
compiler = link[0]
def compile_source(source, obj):
    subprocess.run([compiler, *flags["CXX_DEFINES"], *flags["CXX_INCLUDES"],
                    *flags["CXX_FLAGS"], "-c", str(source), "-o", str(obj)], check=True)
compile_source(root / "src/rules/lucky/graph_labeling_model.cpp", pin / "graph-model.o")
policy_source = (root / "src/policies/default_policy.cpp").read_text()
anchor = '        k, "graph-labeling", completion_);\n'
assert policy_source.count(anchor) == 1
instrumentation = r'''
    const auto &probe_hint = k.local_search_hints[size_t(strategy::LocalSearchKind::GraphLabeling)];
    std::fprintf(stderr, "[graph-probe] result=%d recognized=%llu n=%llu separation=%llu seeds=%llu nodes=%llu repairs=%llu anneal=%llu work=%llu budgets=%llu rejected=%llu sat=%llu hint=%d objective=%llu\n",
        int(graph), (unsigned long long)k.stats.graph_labeling_recognized,
        (unsigned long long)k.stats.graph_labeling_vertices,
        (unsigned long long)k.stats.graph_labeling_separation,
        (unsigned long long)k.stats.graph_labeling_seeds,
        (unsigned long long)k.stats.graph_labeling_nodes,
        (unsigned long long)k.stats.graph_labeling_repairs,
        (unsigned long long)k.stats.graph_labeling_anneal_attempts,
        (unsigned long long)k.stats.graph_labeling_work,
        (unsigned long long)k.stats.graph_labeling_budget,
        (unsigned long long)k.stats.graph_labeling_models_rejected,
        (unsigned long long)k.stats.graph_labeling_sat,
        int(probe_hint.available()), (unsigned long long)probe_hint.objective);
    std::fflush(stderr);
'''
policy = pin / "graph-probe-default-policy.cpp"
policy.write_text(policy_source.replace(anchor, anchor + instrumentation))
compile_source(policy, pin / "graph-probe-default-policy.o")
replacements = {
    graph_object: pin / "graph-model.o",
    "CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o": pin / "graph-probe-default-policy.o",
    "CMakeFiles/tactix.dir/src/main.cpp.o": parent / "graph-profile-main.cpp.o",
}
inputs = {}
for i, token in enumerate(link):
    if token.endswith((".o", ".a")):
        path = replacements.get(token, parent / token)
        inputs[str(path.relative_to(root))] = sha(path)
        link[i] = str(path)
binary = pin / "tactix-graph-profile"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, check=True)
assert all(sha(root / p) == h for p, h in current.items())
(pin / "provenance.json").write_text(json.dumps({
    "parent_pin": str(parent.relative_to(root)),
    "changed_snapshot_paths": sorted(str(Path(p).relative_to(root)) for p in changed),
    "dependency_affected_native_objects": affected,
    "link_inputs": inputs, "link_command": link,
    "probe_policy_sha256": sha(policy), "probe_binary_sha256": sha(binary),
    "builder_sha256": sha(Path(__file__)),
    "source_hashes": current,
    "diagnostic_change": "One constructor-completion stderr record; no new solver stage or hot-path feedback.",
}, indent=2) + "\n")
print(binary, sha(binary))
