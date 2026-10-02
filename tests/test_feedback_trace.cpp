#undef NDEBUG
#include "strategy/feedback_trace.hpp"
#include "policies/random_controller_policy.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static std::string read(const char *path) {
  std::ifstream in(path);
  return {(std::istreambuf_iterator<char>(in)), {}};
}
static void write(const char *path, const std::string &text) {
  std::ofstream out(path);
  out << text;
  assert(out.good());
}
static Kernel fixture(bool unsat) {
  Kernel k;
  kernel_reserve_vars(k, 2);
  kernel_add_binary_clause(k, internalize(1), internalize(2));
  kernel_add_binary_clause(k, internalize(-1), internalize(2));
  if (unsat) {
    kernel_add_binary_clause(k, internalize(1), internalize(-2));
    kernel_add_binary_clause(k, internalize(-1), internalize(-2));
  }
  return k;
}
static std::string metadata(uint64_t seed, const RandomControllerConfig &config) {
  return "fixture\"\\\n\t seed=" + std::to_string(seed) + " " +
         controller_configuration(config);
}

static void synthetic(FeedbackTrace &trace, bool changed = false) {
  Observation before, after;
  before.cumulative.used_glue[1][127] = 3;
  before.cumulative.rule_calls[0] = 7;
  before.cumulative.sweep_limit_depth = 5;
  after = before;
  // These timestamps change deliberately and must not affect replay.
  after.cumulative.rule_nanos[0] = changed ? 99 : 17;
  after.cumulative.solve_nanos = changed ? 231 : 51;
  trace.session_begin(before);
  ActionFeedback action;
  action.sequence = 1;
  action.identity = "quoted\" identity\n\\";
  action.before = before; action.after = after;
  action.duration_nanos = changed ? 999 : 17;
  action.request.emplace();
  action.request->invocation.rule = "Rephase";
  action.allowance = ActionAllowance{WorkUnit::Applications, 0, {}};
  action.stop = StopReason::Budget;
  action.outcome.invocation.status = InvocationStatus::BudgetExhausted;
  trace.after_action(action);
  SearchIntervalFeedback interval;
  interval.sequence = 1;
  interval.before = before; interval.after = after;
  interval.duration_nanos = changed ? 1000 : 23;
  interval.stop = StopReason::Interrupted;
  trace.search_interval(interval);
  trace.session_end(after, 0, StopReason::Interrupted);
}

static void test_codec(const char *path) {
  const std::string tag = "metadata\"\\\n\t";
  {
    FeedbackTrace writer;
    assert(writer.write(path, tag));
    synthetic(writer);
    assert(writer.terminal() && writer.error().empty());
  }
  const auto original = read(path);
  assert(original.find("\"sweep_limit_depth\":5") != std::string::npos);
  assert(original.find("\"rule_calls\":[7,") != std::string::npos);
  assert(original.find("\"used_glue\":[[") != std::string::npos);
  assert(original.find("rule_nanos") == std::string::npos);
  assert(original.find("solve_nanos") == std::string::npos);
  {
    FeedbackTrace reader;
    assert(reader.replay(path, tag));
    synthetic(reader, true);
    assert(reader.terminal() && reader.error().empty());
  }
  {
    FeedbackTrace reader;
    assert(!reader.replay(path, "different immutable provenance"));
  }
  const auto expect_error = [&](const std::string &corrupt, const char *reason) {
    write(path, corrupt);
    FeedbackTrace reader;
    assert(reader.replay(path, tag));
    synthetic(reader);
    if (reader.error().find(reason) == std::string::npos)
      std::fprintf(stderr, "expected %s; got %s\n", reason, reader.error().c_str());
    assert(reader.error().find(reason) != std::string::npos);
  };
  auto corrupt = original;
  auto position = corrupt.find("\"sweep_limit_depth\":5");
  assert(position != std::string::npos);
  corrupt.replace(position, 21, "\"sweep_limit_depth\":6");
  expect_error(corrupt, "divergence");
  position = original.find("{\"type\":\"elapsed_nanos\"");
  assert(position != std::string::npos);
  corrupt = original;
  const auto end = corrupt.find('\n', position);
  corrupt.replace(position, end - position, "{\"type\":\"elapsed_nanos\",\"value\":-1}");
  expect_error(corrupt, "elapsed value");
  expect_error(original.substr(0, position), "truncated");
  expect_error(original + "{}\n", "after terminal");
  write(path, original);
  {
    FeedbackTrace reader;
    assert(reader.replay(path, tag));
    assert(!reader.replay(path, tag));
  }
  {
    FeedbackTrace invalid;
    invalid.session_end({}, 0, StopReason::Interrupted);
    assert(!invalid.error().empty());
  }
}

static void test_actual_execution(const char *path) {
  RandomControllerConfig config;
  config.parameters.rules = {"Rephase", "Propagate"};
  for (const bool unsat : {false, true}) {
    for (const uint64_t seed : {1, 7, 17}) {
      const auto tag = metadata(seed, config);
      Kernel native = fixture(unsat), observed = fixture(unsat);
      const int expected = unsat ? 20 : 10;
      RandomControllerPolicy plain(seed, config);
      assert(plain.solve(native) == expected);
      {
        FeedbackTrace writer;
        assert(writer.write(path, tag));
        RandomControllerPolicy recorded(seed, config, nullptr, nullptr, &writer);
        assert(recorded.solve(observed) == expected);
        assert(writer.error().empty() && writer.terminal());
      }
      assert(serialize_observation(observe_kernel(native)) ==
             serialize_observation(observe_kernel(observed)));
      {
        FeedbackTrace reader;
        assert(reader.replay(path, tag));
        Kernel repeated = fixture(unsat);
        RandomControllerPolicy replayed(seed, config, nullptr, nullptr, &reader);
        assert(replayed.solve(repeated) == expected);
        assert(reader.error().empty() && reader.terminal());
      }
      // Feedback divergence remains out of band and cannot change the verdict.
      auto corrupt = read(path);
      const auto pos = corrupt.find("\"identity\":");
      assert(pos != std::string::npos);
      corrupt.insert(pos + 12, "tampered-");
      write(path, corrupt);
      FeedbackTrace reader;
      assert(reader.replay(path, tag));
      Kernel repeated = fixture(unsat);
      RandomControllerPolicy replayed(seed, config, nullptr, nullptr, &reader);
      assert(replayed.solve(repeated) == expected);
      assert(!reader.error().empty());
      assert(serialize_observation(observe_kernel(native)) ==
             serialize_observation(observe_kernel(repeated)));
    }
  }
}

static Kernel pigeonhole() {
  Kernel k;
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned pigeon, unsigned hole) {
    return make_lit(pigeon * holes + hole, false);
  };
  for (unsigned p = 0; p < pigeons; ++p) {
    Lit clause[holes];
    for (unsigned h = 0; h < holes; ++h) clause[h] = cell(p, h);
    kernel_add_large_clause(k, clause, holes, false, 0);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)));
  }
  return k;
}

static void test_retained_interval_replay(const char *path) {
  uint64_t partial = 0, full = 0;
  RandomControllerConfig config;
  config.parameters.rules = {"Propagate"};
  config.search.restart.initial_interval = 1;
  config.search.restart.reluctant_period = 1;
  for (uint64_t seed = 1; seed <= 16; ++seed) {
    const auto tag = metadata(seed, config);
    uint64_t kept = 0, restarted = 0;
    {
      FeedbackTrace writer;
      assert(writer.write(path, tag));
      Kernel k = pigeonhole();
      RandomControllerPolicy policy(seed, config, nullptr, nullptr, &writer);
      assert(policy.solve(k) == 20);
      assert(writer.error().empty() && writer.terminal());
      kept = policy.coverage().partial_restarts;
      restarted = policy.coverage().full_restarts;
      assert(k.stats.restarts_reused == kept);
    }
    {
      FeedbackTrace reader;
      assert(reader.replay(path, tag));
      Kernel k = pigeonhole();
      RandomControllerPolicy policy(seed, config, nullptr, nullptr, &reader);
      assert(policy.solve(k) == 20);
      assert(reader.error().empty() && reader.terminal());
      assert(policy.coverage().partial_restarts == kept);
      assert(policy.coverage().full_restarts == restarted);
    }
    partial += kept;
    full += restarted;
  }
  assert(partial > 0 && full > 0);
}

int main(int argc, char **argv) {
  if (argc >= 2) {
    FeedbackTrace writer;
    RandomControllerConfig config;
    config.parameters.rules = {"Rephase"};
    assert(writer.write(argv[1], metadata(17, config)));
    Kernel k = fixture(true);
    if (argc == 3 && std::string(argv[2]) == "invalid") {
      RestartController controller(k, {}, &writer);
      ActionRequest wrong;
      wrong.version = 7;
      wrong.sequence = 1;
      wrong.expected = state_version(k);
      wrong.type = ActionType::ResumeSearch;
      wrong.search.branching = static_cast<Branching>(255);
      assert(controller.resume_search(wrong).invocation.status == InvocationStatus::InvalidRequest);
      controller.finish_feedback(0);
      assert(writer.error().empty() && writer.terminal());
      return 0;
    }
    RandomControllerPolicy policy(17, config, nullptr, nullptr, &writer);
    assert(policy.solve(k) == 20);
    assert(writer.error().empty() && writer.terminal());
    return 0;
  }
  char path[] = "/tmp/tactix-policy-feedback-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  test_codec(path);
  test_actual_execution(path);
  test_retained_interval_replay(path);
  unlink(path);
}
