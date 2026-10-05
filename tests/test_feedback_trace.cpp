#undef NDEBUG
#include "strategy/feedback_trace.hpp"
#include "policies/random_controller_policy.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unistd.h>

using namespace tactix;
using namespace tactix::strategy;
using namespace tactix::strategy::controller;

static std::string read(const char *path) {
  std::ifstream in(path);
  std::ostringstream bytes;
  bytes << in.rdbuf();
  return bytes.str();
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
  before.cumulative.balanced_signature_forced_ttf = 2;
  LocalSearchCandidateInfo candidate;
  candidate.id = 256;
  candidate.external_variables = 2;
  candidate.known_phases = 1;
  candidate.objective = 3;
  before.local_search_candidates = {candidate};
  before.local_search_last_target_decision_candidate = candidate.id;
  before.local_search_last_target_decision_matched = true;
  before.native_policy_state = "native-clock";
  after = before;
  after.cumulative.sweep_limit_depth = 4;
  after.cumulative.rule_calls[3] = 9;
  after.cumulative.used_glue[1][7] = 11;
  after.native_policy_state.clear();
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
  ActionProducerEffort effort{
      "Gf3AffineConsequence", 6, false, 11, 13, 24, 3, 1};
  effort.have_detector_census = true;
  effort.candidate_domains = 12;
  effort.exact_domains = 3;
  effort.relation_keys = 0;
  effort.tuple_occurrences = 0;
  effort.first_decline_reason = 11;
  action.request->producer_effort.push_back(std::move(effort));
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

static void repeated_services(FeedbackReceiver &receiver, uint64_t count) {
  Observation current;
  receiver.session_begin(current);
  for (uint64_t sequence = 1; sequence <= count; ++sequence) {
    ActionFeedback action;
    action.sequence = sequence;
    action.kind = FeedbackKind::SearchService;
    action.identity = "Decide";
    action.before = current;
    action.after = current;
    action.after.cumulative.decisions = sequence;
    action.outcome.invocation.applications = 1;
    action.outcome.invocation.work = 1;
    action.outcome.invocation.before = action.before.state;
    action.outcome.invocation.after = action.after.state;
    receiver.after_action(action);
    current = action.after;
  }
  receiver.session_end(current, 10, StopReason::Sat);
}

static void write_legacy_services(const char *path, uint64_t count) {
  std::ofstream out(path);
  Observation current;
  out << "{\"type\":\"header\",\"version\":1,\"metadata\":\"legacy\"}\n";
  out << "{\"type\":\"session_begin\",\"observation\":"
      << serialize_observation(current) << "}\n";
  for (uint64_t sequence = 1; sequence <= count; ++sequence) {
    ActionFeedback action;
    action.version = 1;
    action.sequence = sequence;
    action.kind = FeedbackKind::SearchService;
    action.identity = "Decide";
    action.before = current;
    action.after = current;
    action.after.cumulative.decisions = sequence;
    action.outcome.invocation.applications = 1;
    action.outcome.invocation.work = 1;
    action.outcome.invocation.before = action.before.state;
    action.outcome.invocation.after = action.after.state;
    out << serialize_feedback(action) << '\n';
    out << "{\"type\":\"elapsed_nanos\",\"value\":0}\n";
    current = action.after;
  }
  out << "{\"type\":\"session_end\",\"verdict\":10,\"stop\":7,"
         "\"observation\":" << serialize_observation(current) << "}\n";
  assert(out.good());
}

static void test_versions_and_compression(const char *path) {
  const std::string legacy = std::string(path) + ".v1";
  write_legacy_services(legacy.c_str(), 3);
  {
    FeedbackTrace replay;
    assert(replay.replay(legacy.c_str(), "legacy"));
    repeated_services(replay, 3);
    assert(replay.terminal() && replay.error().empty());
  }
  const std::string compressed = std::string(path) + ".jsonl.gz";
  {
    FeedbackTrace writer;
    assert(writer.write(compressed.c_str(), "compressed"));
    repeated_services(writer, 10000);
    assert(writer.terminal() && writer.error().empty());
  }
  assert(std::filesystem::file_size(compressed) < 500000);
  {
    FeedbackTrace replay;
    assert(replay.replay(compressed.c_str(), "compressed"));
    repeated_services(replay, 10000);
    assert(replay.terminal() && replay.error().empty());
  }
  unlink(legacy.c_str());
  unlink(compressed.c_str());
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
  assert(original.find("\"balanced_signature_forced_ttf\":2") !=
         std::string::npos);
  assert(original.find("\"rule_calls\":[7,") != std::string::npos);
  assert(original.find("\"used_glue\":[[") != std::string::npos);
  assert(original.find("\"native_arrays\":{\"rule_calls\":[[3,9]],"
                       "\"used_glue\":[[135,11]]}") != std::string::npos);
  assert(original.find("\"clear\":[\"native_policy_state\"]") !=
         std::string::npos);
  assert(original.find("\"producer_effort\":[{\"family\":"
                       "\"Gf3AffineConsequence\"") != std::string::npos);
  assert(original.find("\"known_phases\":1") != std::string::npos);
  assert(original.find("\"local_search_last_target_decision_candidate\":256") !=
         std::string::npos);
  assert(original.find("\"local_search_last_target_decision_matched\":true") !=
         std::string::npos);
  assert(original.find("\"total_work\":24") != std::string::npos);
  assert(original.find("\"detector_census\":{\"candidate_domains\":12,"
                       "\"exact_domains\":3,\"relation_keys\":0,"
                       "\"tuple_occurrences\":0,"
                       "\"first_decline_reason\":11}") !=
         std::string::npos);
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
  if (argc == 3 && std::string(argv[2]) == "stress") {
    FeedbackTrace writer;
    assert(writer.write(argv[1], "stress"));
    repeated_services(writer, 100000);
    assert(writer.error().empty() && writer.terminal());
    return 0;
  }
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
  test_versions_and_compression(path);
  test_actual_execution(path);
  test_retained_interval_replay(path);
  unlink(path);
}
