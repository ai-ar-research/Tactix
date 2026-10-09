// Public-API replay reporter (gap plan Step 0; report-only until step 7).
// A reference DefaultPolicy run on a C-API-loaded solver produces a feedback
// stream; a second solver loaded the same way replays it through tactix.h
// (tactix_apply_instance, tactix_decide) record by record, comparing the
// resulting state with the record's observation. The leading permanent
// parameter installs are applied as load-time configuration. The replay stops
// at the first record without a public request (default_action_map.hpp) or at
// the first divergence and reports why; a census lists every blocking entry of
// the whole stream, so later steps show the replayable prefix growing.
#undef NDEBUG
// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "default_action_map.hpp"

#include "api/api_internal.hpp"
#include "strategy/controller_observation.hpp"

#include <iostream>
#include <map>
#include <variant>

using namespace action_map;

namespace {

tactix_c_handle *load(const Fixture &fixture) {
  tactix_c_handle *s = tactix_init();
  for (const auto &clause : fixture.dimacs) {
    for (int lit : clause) tactix_add(s, lit);
    tactix_add(s, 0);
  }
  if (fixture.tune) fixture.tune(api_detail::kernel_for_fixed_testing(s));
  (void)tactix_status(s); // completes input
  return s;
}

int rule_id(const std::string &name) {
  for (int id = 0; id < tactix_rule_count(); ++id)
    if (const char *rule = tactix_rule_name(id); rule && name == rule) return id;
  return -1;
}

int result_code(ApplyResult result) {
  switch (result) {
  case ApplyResult::Ok: return 0;
  case ApplyResult::NoOp: return 1;
  case ApplyResult::Conflict: return 2;
  case ApplyResult::Sat: return 10;
  case ApplyResult::Unsat: return 20;
  }
  return -2;
}

// Apply a recorded permanent profile while loading the replay solver.
// Guidance-only and material installs are assumed: their typed description
// is wider than what they write (InstallWalkGuidance inherits the budget).
bool install_at_load(Kernel &k, const ActionFeedback &record) {
  if (record.identity != "InstallRuleParameters" || !record.native_request ||
      !record.native_request->invocation)
    return false;
  bool installed = false;
  std::visit([&](const auto &parameters) {
    if constexpr (requires { strategy::install_rule_parameters(k, parameters); }) {
      strategy::install_rule_parameters(k, parameters);
      installed = true;
    }
  }, record.native_request->invocation->parameters);
  return installed;
}

std::string mismatch(const Observation &expected, const Observation &actual) {
  std::string out;
  auto field = [&](const char *name, uint64_t a, uint64_t b) {
    if (a != b)
      out += std::string(out.empty() ? "" : ",") + name + "=" + std::to_string(a) +
             "/" + std::to_string(b);
  };
  field("level", expected.level, actual.level);
  field("trail", expected.trail_assignments, actual.trail_assignments);
  field("root", expected.root_population, actual.root_population);
  field("irredundant", expected.irredundant_clauses, actual.irredundant_clauses);
  field("inconsistent", expected.inconsistent, actual.inconsistent);
  field("conflict", expected.conflict, actual.conflict);
  return out;
}

struct Report {
  uint64_t records = 0, loaded = 0, assumed = 0, replayed = 0;
  std::string stop = "end-of-stream";
};

Report replay(const Fixture &fixture, const std::vector<ActionFeedback> &records) {
  Report report;
  report.records = records.size();
  tactix_c_handle *s = load(fixture);
  Kernel &observed = api_detail::kernel_for_fixed_testing(s);
  size_t i = 0;
  for (; i < records.size(); ++i) { // load-time profile
    const Entry *entry = classify(records[i]);
    if (!entry || entry->replay != Replay::LoadProfile) break;
    if (install_at_load(observed, records[i])) ++report.loaded;
    else ++report.assumed;
  }
  for (; i < records.size(); ++i) {
    const ActionFeedback &record = records[i];
    const Entry *entry = classify(record);
    const std::string where = "#" + std::to_string(i) + " " +
                              (entry ? entry->id : "unclassified") + " (" +
                              record.identity + ")";
    if (!entry || entry->replay == Replay::None || entry->replay == Replay::LoadProfile) {
      report.stop = where + ": no public request: " +
                    (entry ? entry->public_route : "unmapped");
      break;
    }
    if (entry->replay == Replay::Marker) continue;
    // Unrecorded native writes between records surface as a pre-state drift.
    if (const std::string drift =
            mismatch(record.before, strategy::controller::observe_kernel(observed));
        !drift.empty()) {
      report.stop = where + ": diverged before the record " + drift;
      break;
    }
    int code;
    int expected = result_code(record.outcome.invocation.rule_result);
    if (entry->replay == Replay::Decide) {
      code = tactix_decide(s, static_cast<int32_t>(static_cast<uint32_t>(record.instance)));
      expected = record.outcome.invocation.rule_result == ApplyResult::Ok ? 0 : -1;
    } else {
      const int id = rule_id(record.identity);
      assert(id >= 0);
      code = tactix_apply_instance(s, id, record.instance);
    }
    // A native no-op (for instance Propagate at a fixpoint) is declined by
    // the public guard; the same unchanged state is an equivalent replay.
    const bool vacuous = code == -1 && mismatch(record.before, record.after).empty();
    if (code != expected && !vacuous) {
      report.stop = where + ": diverged: result " + std::to_string(code) +
                    " expected " + std::to_string(expected);
      break;
    }
    const std::string differs =
        mismatch(record.after, strategy::controller::observe_kernel(observed));
    if (!differs.empty()) {
      report.stop = where + ": diverged state " + differs;
      break;
    }
    ++report.replayed;
  }
  tactix_release(s);
  return report;
}

} // namespace

int main() {
  std::map<std::string, uint64_t> blockers;
  uint64_t replayed = 0, records_total = 0;
  for (const Fixture &fixture : fixtures()) {
    if (fixture.dimacs.empty()) continue;
    tactix_c_handle *reference = load(fixture);
    const auto records = run(fixture, api_detail::kernel_for_fixed_testing(reference));
    tactix_release(reference);
    for (const ActionFeedback &record : records)
      if (const Entry *entry = classify(record);
          !entry || entry->replay == Replay::None)
        ++blockers[entry ? entry->id : "unclassified"];
    const Report report = replay(fixture, records);
    replayed += report.replayed;
    records_total += report.records;
    std::cout << "fixture=" << fixture.name << " records=" << report.records
              << " load-installed=" << report.loaded << " load-assumed="
              << report.assumed << " replayed=" << report.replayed
              << " stop=" << report.stop << '\n';
  }
  std::cout << "replayed " << replayed << " of " << records_total
            << " records; blocking entries over all streams:";
  for (const auto &[id, count] : blockers) std::cout << ' ' << id << '=' << count;
  std::cout << '\n';
  // Report-only: the replay machinery itself must have executed public calls.
  assert(replayed > 0);
  return 0;
}
