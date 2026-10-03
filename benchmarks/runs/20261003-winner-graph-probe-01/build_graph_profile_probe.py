#!/usr/bin/env python3
"""Build a diagnostic CLI with a typed DefaultPolicy graph profile.

Preserves the production CLI verbatim except for an explicit configuration
injection. The profile is a serialized single Lucky-21 action, not a second
solver stage. Never run this builder against a submitted immutable pin.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("pin", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
pin = args.pin.resolve()
source = (root / "src/main.cpp").read_text()
anchor = "  DefaultStrategyConfig default_config;\n"
assert source.count(anchor) == 1
injection = r'''
  const char *profile_path = getenv("TACTIX_PROBE_GRAPH_PROFILE");
  if (!profile_path) {
    fprintf(stderr, "probe: TACTIX_PROBE_GRAPH_PROFILE is required\n");
    return 1;
  }
  std::ifstream profile_input(profile_path);
  const std::string profile_source((std::istreambuf_iterator<char>(profile_input)), {});
  strategy::OnlineStrategyProgram profile;
  std::string profile_error;
  if (!profile_input ||
      !strategy::deserialize_online_strategy_program(profile_source, profile, &profile_error) ||
      profile.sequence.size() != 1 || profile.sequence[0].rule != "Lucky" ||
      profile.sequence[0].instances != std::vector<uint64_t>{21} ||
      !std::holds_alternative<strategy::GraphLabelingParameters>(profile.sequence[0].parameters)) {
    fprintf(stderr, "probe: invalid single Lucky-21 profile: %s\n", profile_error.c_str());
    return 1;
  }
  default_config.graph_labeling_model.enabled = true;
  default_config.graph_labeling_model.parameters =
      std::get<strategy::GraphLabelingParameters>(profile.sequence[0].parameters);
'''
generated = pin / "graph-profile-main.cpp"
assert not generated.exists(), "probe build already exists; choose a fresh pin"
generated.write_text(source.replace(anchor, anchor + injection))
flags = {}
for line in (pin / "CMakeFiles/tactix.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((pin / "CMakeFiles/tactix.dir/link.txt").read_text())
obj = pin / "graph-profile-main.cpp.o"
subprocess.run([link[0], *flags["CXX_DEFINES"], *flags["CXX_INCLUDES"],
                *flags["CXX_FLAGS"], "-c", str(generated), "-o", str(obj)], check=True)
assert link.count("CMakeFiles/tactix.dir/src/main.cpp.o") == 1
link[link.index("CMakeFiles/tactix.dir/src/main.cpp.o")] = str(obj)
link[link.index("-o") + 1] = "tactix-graph-profile"
subprocess.run(link, cwd=pin, check=True)
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
(pin / "graph-profile-provenance.json").write_text(json.dumps({
    "production_main_sha256": digest(root / "src/main.cpp"),
    "probe_main_sha256": digest(generated),
    "builder_sha256": digest(Path(__file__)),
    "probe_binary_sha256": digest(pin / "tactix-graph-profile"),
    "link_command": link,
}, indent=2) + "\n")
