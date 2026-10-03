#!/usr/bin/env python3
"""Keep constructor input/algorithm fixed; measure real Kernel budget polling."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
pin = root / "build/pin-graph-meter-calibration-d210d222-01"
pin.mkdir(exist_ok=False)
source = (root / "benchmarks/probes/scratch/graph_constructor_calibration.cpp").read_text()
def replace_once(old, new):
    global source
    assert source.count(old) == 1, old
    source = source.replace(old, new)
replace_once('  const auto begin = Clock::now();', '''  tactix::Kernel budget_kernel;
  volatile sig_atomic_t stop = 0;
  budget_kernel.stop = &stop;
  uint64_t charged = 0, ceiling = 0;
  for (const auto &stage : stages) ceiling += stage.cap;
  const auto begin = Clock::now();''')
replace_once('      const auto charge = [&](uint64_t amount) {', '''      const uint64_t phase_start = charged;
      const auto charge = [&](uint64_t amount) {''')
replace_once('        if (amount > measurement.cap - measurement.work) {', '''        if (tactix::kernel_rule_interrupt(budget_kernel,
              budget_kernel.stats.graph_labeling_interrupted)) return false;
        if (amount > measurement.cap - (charged - phase_start) ||
            amount > ceiling - charged) {''')
replace_once('        measurement.work += amount;', '''        charged += amount;
        budget_kernel.stats.graph_labeling_work += amount;''')
replace_once('      measurement.seconds = std::chrono::duration<double>', '''      measurement.work = charged - phase_start;
      measurement.seconds = std::chrono::duration<double>''')
replace_once('  const double elapsed = ', '''  if (budget_kernel.stats.graph_labeling_work != charged) return 4;
  const double elapsed = ''')
generated = pin / "meter-calibration.cpp"
generated.write_text(source)
reference = root / "winners/lymphosat-sc2026/green/lymphosat/src/solver/solvers/antibandwidth/solver/solver.cpp"
compiler = "/cm/local/apps/gcc/13.1.0/bin/c++"
obj = pin / "meter-calibration.o"
compile_command = [compiler, "-O3", "-DNDEBUG", "-std=c++20", "-fno-exceptions",
    "-fno-rtti", "-I" + str(root / "src"), "-I" + str(root / "include"),
    "-DREFERENCE_GRAPH_SOURCE=" + json.dumps(str(reference)), "-c", str(generated), "-o", str(obj)]
subprocess.run(compile_command, check=True)
parent = root / "build/pin-winner-graph-reflection-113b3472-01/provenance.json"
provenance = json.loads(parent.read_text())
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
assert all(sha(root / name) == digest for name, digest in provenance["source_hashes"].items())
assert all(sha(root / name) == digest for name, digest in provenance["link_inputs"].items())
link = provenance["link_command"]
old_main = str(root / "build/pin-winner-graph-probe-d85b30b4-01/graph-profile-main.cpp.o")
assert link.count(old_main) == 1
link[link.index(old_main)] = str(obj)
binary = pin / "calibrate"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, check=True)
assert all(sha(root / name) == digest for name, digest in provenance["source_hashes"].items())
(pin / "manifest.json").write_text(json.dumps({
    "compile_command": compile_command, "link_command": link,
    "parent_provenance": str(parent.relative_to(root)),
    "bindings": {str(p.relative_to(root)): sha(p) for p in
                 (generated, obj, binary, reference, parent, Path(__file__))},
    "scope": "Real Kernel interruption/cumulative budget/stat accounting; graph input adapters and Boolean/proof work remain excluded. No production source edit.",
}, indent=2) + "\n")
print(sha(binary))
