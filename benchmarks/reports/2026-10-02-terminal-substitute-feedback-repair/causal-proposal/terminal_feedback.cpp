#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/random_controller_policy.hpp"
#include "proof/proof.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace tactix;
using namespace tactix::strategy::controller;

struct Receiver final : FeedbackReceiver {
  Kernel &kernel;
  unsigned violations = 0, refutations = 0, ended = 0;
  explicit Receiver(Kernel &k) : kernel(k) {}
  void check(const char *where) {
    if (!kernel.large_watches_connected) {
      ++violations;
      std::printf("UNRESTORED %s\n", where);
    }
  }
  void after_action(const ActionFeedback &record) override {
    check(record.identity.c_str());
    if (record.identity == "Substitute" &&
        record.outcome.invocation.rule_result == ApplyResult::Unsat)
      ++refutations;
    std::printf("ACTION %s result=%d connected=%d\n", record.identity.c_str(),
                int(record.outcome.invocation.rule_result),
                kernel.large_watches_connected);
  }
  void session_end(const Observation &, int verdict, StopReason) override {
    ++ended;
    check("session_end");
    assert(verdict == 20);
  }
  void search_interval(const SearchIntervalFeedback &) override {
    check("search_interval");
  }
};

int main(int argc, char **argv) {
  assert(argc == 5);
  Kernel k;
  Proof proof;
  assert(proof.open(argv[2], ProofFormat::DsrAscii));
  k.proof = &proof;
  proof.bind_variable_map(&k.internal_to_external);
  proof.bind_external_variable_map(&k.external_to_internal);
  FILE *input = std::fopen(argv[1], "rb");
  assert(input && !parse_dimacs(input, k));
  std::fclose(input);
  Receiver receiver(k);
  RandomControllerConfig config;
  config.reuse_search_trail = std::atoi(argv[3]) != 0;
  const bool feedback = std::atoi(argv[4]) != 0;
  RandomControllerPolicy policy(2032000128, config, nullptr, nullptr,
                                feedback ? &receiver : nullptr);
  const int verdict = policy.solve(k);
  std::printf("FINAL verdict=%d connected=%d violations=%u refutations=%u ended=%u route=%s\n",
              verdict, k.large_watches_connected, receiver.violations,
              receiver.refutations, receiver.ended, proof.verification_route());
  if (verdict != 20) return 4;
  if (!k.large_watches_connected || receiver.violations) return 3;
  if (feedback && (receiver.refutations != 1 || receiver.ended != 1)) return 5;
  return 0;
}
