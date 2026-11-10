// DefaultPolicy static/hardcoded twin (plan §8.4 correspondence gate). One
// source, two executables over the static and hardcoded action ports. The
// static arm emits one line per correspondence fixture; the hardcoded arm runs
// it and requires identical verdicts, certificate bytes, models and trajectory
// counters. The C default strategy's correspondence with the static arm is
// test_c_default_correspondence.
#undef NDEBUG
#include "default_correspondence.hpp"

#include <iostream>

using namespace correspondence;

namespace {

int solve_policy(tactix_c_handle *s, const DefaultStrategyConfig &config) {
  DefaultPolicy policy(config);
  return policy.solve(api_detail::kernel_for_fixed_testing(s));
}

std::vector<std::string> emit_all() {
  const std::string scratch = scratch_dir();
  std::vector<std::string> out;
  for (const Fixture &fixture : correspondence::fixtures())
    out.push_back(emit(fixture, scratch, solve_policy));
  rmdir(scratch.c_str());
  return out;
}

std::vector<std::string> run_arm(const char *path) {
  FILE *pipe = popen(path, "r");
  assert(pipe);
  std::vector<std::string> out;
  char line[8192];
  while (std::fgets(line, sizeof line, pipe)) {
    std::string text(line);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    if (!text.empty()) out.push_back(text);
  }
  assert(pclose(pipe) == 0);
  return out;
}

} // namespace

int main(int argc, char **argv) {
  const auto actual = emit_all();
  if (argc == 1) {
    for (const std::string &line : actual) std::cout << line << '\n';
    return 0;
  }
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " [<static arm>]\n";
    return 2;
  }
  const auto reference = run_arm(argv[1]);
  assert(!reference.empty() && reference.size() == actual.size());
  int mismatches = 0;
  for (size_t i = 0; i < reference.size(); ++i)
    if (reference[i] != actual[i]) {
      std::cout << "hardcoded differs:\n  static:    " << reference[i]
                << "\n  hardcoded: " << actual[i] << '\n';
      ++mismatches;
    }
  std::cout << "fixtures=" << reference.size() << " mismatches=" << mismatches << '\n'
            << std::flush;
  assert(mismatches == 0);
  return 0;
}
