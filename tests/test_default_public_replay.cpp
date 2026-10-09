// Public-API replay reporter (gap plan Step 0; report-only until step 7).
// A reference DefaultPolicy stream on a C-API-loaded solver is replayed on a
// second one through tactix.h (tactix_apply_instance, tactix_decide), with the
// leading permanent installs applied at load. The replay stops at the first
// record without a public request (default_action_map.hpp) or at the first
// divergence; a census lists every blocking entry of the whole stream.
// Per public guidance-publication mode it also reports the first record after
// which H (saved, score, candidates, active set, target, mode) differs from
// the reference, and the differing fields at the stop point.
#undef NDEBUG
// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "default_action_map.hpp"

#include "api/api_internal.hpp"
#include "policies/heuristics.hpp"
#include "strategy/controller_observation.hpp"

#include <iostream>
#include <map>
#include <variant>

using namespace action_map;

namespace {

tactix_c_handle *load(const Fixture &fixture,
                      int publication = TACTIX_GUIDANCE_DISCARD) {
  tactix_c_handle *s = tactix_init();
  assert(tactix_set_guidance_publication(s, publication) == 0);
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

// H as a client observes it, plus the active set that root and retirement
// publication maintain.
struct Guidance {
  std::vector<int8_t> saved, target;
  std::vector<double> score;
  std::vector<uint8_t> active;
  std::vector<Lit> candidates;
  bool stable = false;
};

Guidance guidance(const Kernel &k) {
  Guidance g;
  if (!k.h) return g;
  g.saved = k.h->saved;
  g.target = k.h->target;
  g.score = k.h->scores.score;
  g.stable = k.h->stable;
  for (Var v = 0; v < k.nvars; ++v) g.active.push_back(heuristics_var_active(k, v));
  g.candidates.resize(k.nvars + 1);
  g.candidates.resize(heuristics_decide_candidates(
      k, g.candidates.data(), static_cast<uint32_t>(g.candidates.size())));
  return g;
}

std::string guidance_mismatch(const Guidance &expected, const Guidance &actual) {
  std::string out;
  auto field = [&](const char *name, bool same) {
    if (!same) out += std::string(out.empty() ? "" : ",") + name;
  };
  field("saved", expected.saved == actual.saved);
  field("score", expected.score == actual.score);
  field("candidates", expected.candidates == actual.candidates);
  field("active", expected.active == actual.active);
  field("target", expected.target == actual.target);
  field("mode", expected.stable == actual.stable);
  return out;
}

// The reference stream with H captured after every record.
struct GuidanceCapture final : FeedbackReceiver {
  const Kernel &kernel;
  std::vector<ActionFeedback> actions;
  std::vector<Guidance> after;
  explicit GuidanceCapture(const Kernel &k) : kernel(k) {}
  void after_action(const ActionFeedback &value) override {
    actions.push_back(value);
    after.push_back(guidance(kernel));
  }
  void search_interval(const SearchIntervalFeedback &) override {}
};

struct Report {
  uint64_t records = 0, loaded = 0, assumed = 0, replayed = 0;
  std::string stop = "end-of-stream";
  std::string first_guidance_divergence = "none"; // record and fields
  std::string guidance_at_stop = "unmeasured";   // differing fields, or empty
  uint64_t guidance_compared = 0, candidates_compared = 0;
};

Report replay(const Fixture &fixture, const GuidanceCapture &reference,
              int publication) {
  const std::vector<ActionFeedback> &records = reference.actions;
  Report report;
  report.records = records.size();
  tactix_c_handle *s = load(fixture, publication);
  Kernel &observed = api_detail::kernel_for_fixed_testing(s);
  // Compare the replayed prefix ending after record `last` with the reference.
  const auto compare = [&](size_t last) {
    const Guidance actual = guidance(observed);
    ++report.guidance_compared;
    report.candidates_compared += actual.candidates.size();
    report.guidance_at_stop = guidance_mismatch(reference.after[last], actual);
    if (!report.guidance_at_stop.empty() && report.first_guidance_divergence == "none")
      report.first_guidance_divergence = "#" + std::to_string(last) + " " +
          records[last].identity + " " + report.guidance_at_stop;
  };
  size_t i = 0;
  for (; i < records.size(); ++i) { // load-time profile
    const Entry *entry = classify(records[i]);
    if (!entry || entry->replay != Replay::LoadProfile) break;
    if (install_at_load(observed, records[i])) ++report.loaded;
    else ++report.assumed;
  }
  for (; i < records.size(); ++i) {
    if (i) compare(i - 1);
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
  if (i == records.size() && i) compare(i - 1);
  tactix_release(s);
  return report;
}

} // namespace

int main() {
  std::map<std::string, uint64_t> blockers;
  uint64_t replayed = 0, records_total = 0;
  std::map<int, uint64_t> guidance_matches;
  uint64_t guidance_compared = 0, candidates_compared = 0;
  uint64_t replayed_fixtures = 0;
  for (const Fixture &fixture : fixtures()) {
    if (fixture.dimacs.empty()) continue;
    ++replayed_fixtures;
    tactix_c_handle *source = load(fixture);
    GuidanceCapture reference(api_detail::kernel_for_fixed_testing(source));
    run(fixture, api_detail::kernel_for_fixed_testing(source), reference);
    tactix_release(source);
    const auto &records = reference.actions;
    for (const ActionFeedback &record : records)
      if (const Entry *entry = classify(record);
          !entry || entry->replay == Replay::None)
        ++blockers[entry ? entry->id : "unclassified"];
    std::string guidance_line;
    Report report;
    for (int publication : {TACTIX_GUIDANCE_DISCARD, TACTIX_GUIDANCE_NATIVE}) {
      const Report mode = replay(fixture, reference, publication);
      // Publication changes H only: the replayed prefix is mode-independent.
      if (publication == TACTIX_GUIDANCE_DISCARD) report = mode;
      assert(mode.replayed == report.replayed && mode.stop == report.stop);
      const char *name = publication == TACTIX_GUIDANCE_NATIVE ? "native" : "discard";
      guidance_line += std::string(" h-") + name + "-at-stop=" +
                       (mode.guidance_at_stop.empty() ? "same" : mode.guidance_at_stop) +
                       " h-" + name + "-first=" + mode.first_guidance_divergence;
      guidance_matches[publication] += mode.guidance_at_stop.empty();
      guidance_compared += mode.guidance_compared;
      candidates_compared += mode.candidates_compared;
    }
    replayed += report.replayed;
    records_total += report.records;
    std::cout << "fixture=" << fixture.name << " records=" << report.records
              << " load-installed=" << report.loaded << " load-assumed="
              << report.assumed << " replayed=" << report.replayed
              << " stop=" << report.stop << guidance_line << '\n';
  }
  std::cout << "replayed " << replayed << " of " << records_total
            << " records; blocking entries over all streams:";
  for (const auto &[id, count] : blockers) std::cout << ' ' << id << '=' << count;
  std::cout << "\nH equal at the replay stop point: discard "
            << guidance_matches[TACTIX_GUIDANCE_DISCARD] << '/' << replayed_fixtures
            << ", native " << guidance_matches[TACTIX_GUIDANCE_NATIVE] << '/'
            << replayed_fixtures << " (" << guidance_compared << " comparisons over "
            << candidates_compared << " candidate literals)\n";
  // Report-only: the replay machinery itself must have executed public calls,
  // and the H comparison must have observed nonempty guidance.
  assert(replayed > 0 && candidates_compared > 0);
  return 0;
}
