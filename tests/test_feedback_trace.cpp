#undef NDEBUG
#include "strategy/feedback_trace.hpp"
#include "policies/restart_controller.hpp"
#include "rules/semantic_extension/direct_csp_support_action.hpp"
#include "rules/semantic_extension/nary_csp_equality_action.hpp"
#include "rules/semantic_extension/gf3_affine_action.hpp"
#include "rules/semantic_extension/gf3_gaussian_action.hpp"
#include "rules/semantic_extension/gf3_general_row_action.hpp"
#include "rules/lucky/tseitin_cycle_action.hpp"
#include "strategy/random_online.hpp"
#include "policies/default_policy.hpp"

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
static std::string metadata(uint64_t seed) {
  return "fixture\"\\\n\t seed=" + std::to_string(seed) + " default";
}

// A small UNSAT pigeonhole instance and a profile that reaches CDCL search,
// so a recorded trace contains decisions, conflict analysis, and search
// intervals (as in test_native_policy_feedback).
static Kernel pigeonhole() {
  Kernel k;
  constexpr unsigned pigeons = 5, holes = 4;
  kernel_reserve_vars(k, pigeons * holes);
  auto cell = [](unsigned p, unsigned h) { return make_lit(p * holes + h, false); };
  for (unsigned p = 0; p < pigeons; ++p) {
    std::vector<Lit> clause;
    for (unsigned h = 0; h < holes; ++h) clause.push_back(cell(p, h));
    kernel_add_large_clause(k, clause.data(), holes, false, 0, ++k.next_clause_id);
    for (unsigned q = p + 1; q < pigeons; ++q)
      for (unsigned h = 0; h < holes; ++h)
        kernel_add_binary_clause(k, negate(cell(p, h)), negate(cell(q, h)), ++k.next_clause_id);
  }
  return k;
}

static DefaultStrategyConfig search_profile() {
  DefaultStrategyConfig config;
  config.inprocess.program.clear();
  config.inprocess.append_unlisted_rules = false;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  config.inprocess.structured_root_large_domain_stages.clear();
  config.inprocess.initial_fast_elimination = false;
  config.rephase.late_lucky = false;
  config.rephase.stable_only = false;
  config.rephase.initial_interval = config.rephase.interval = 1;
  config.rephase.rotation = {DefaultRephaseSlot::Walk};
  config.rephase.walk_permille = 1;
  config.mode.initial_conflicts = config.mode.interval = 1;
  config.restart.reluctant_period = 2;
  config.reduction.initial_interval = 5;
  return config;
}

// A real DefaultPolicy solve records, replays, and tolerates a tampered trace:
// feedback divergence stays out of band and never changes the verdict.
static void test_default_execution(const char *path) {
  for (const bool unsat : {false, true}) {
    const auto tag = metadata(unsat ? 20 : 10);
    Kernel native = fixture(unsat), observed = fixture(unsat);
    const int expected = unsat ? 20 : 10;
    assert(DefaultPolicy().solve(native) == expected);
    {
      FeedbackTrace writer;
      assert(writer.write(path, tag));
      DefaultPolicy recorded;
      recorded.set_feedback(&writer);
      assert(recorded.solve(observed) == expected);
      assert(writer.error().empty() && writer.terminal());
    }
    assert(serialize_observation(observe_kernel(native)) ==
           serialize_observation(observe_kernel(observed)));
    {
      FeedbackTrace reader;
      assert(reader.replay(path, tag));
      Kernel repeated = fixture(unsat);
      DefaultPolicy replayed;
      replayed.set_feedback(&reader);
      assert(replayed.solve(repeated) == expected);
      assert(reader.error().empty() && reader.terminal());
    }
    auto corrupt = read(path);
    const auto pos = corrupt.find("\"identity\":\"");
    assert(pos != std::string::npos);
    corrupt.insert(pos + 12, "tampered-");
    write(path, corrupt);
    FeedbackTrace reader;
    assert(reader.replay(path, tag));
    Kernel repeated = fixture(unsat);
    DefaultPolicy replayed;
    replayed.set_feedback(&reader);
    assert(replayed.solve(repeated) == expected);
    assert(!reader.error().empty());
    assert(serialize_observation(observe_kernel(native)) ==
           serialize_observation(observe_kernel(repeated)));
  }
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
    assert(writer.write(argv[1], metadata(17)));
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
    Kernel searched = pigeonhole();
    DefaultPolicy policy(search_profile());
    policy.set_feedback(&writer);
    assert(policy.solve(searched) == 20);
    assert(writer.error().empty() && writer.terminal());
    return 0;
  }
  char path[] = "/tmp/tactix-policy-feedback-XXXXXX";
  const int fd = mkstemp(path);
  assert(fd >= 0);
  close(fd);
  test_codec(path);
  test_versions_and_compression(path);
  test_default_execution(path);
  unlink(path);
}
