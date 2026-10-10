// Public-API replay reporter (gap plan Step 0; report-only until step 7).
// A reference DefaultPolicy stream on a C-API solver is replayed on a second
// one through tactix.h only: registry instances, decisions, installs, supports,
// the structured-root online steps and the inprocessing slot invocations
// (public_online_encoding.hpp; each must convert back to the native request). The replay stops at the first record
// without a public request or at the first divergence; a census lists every
// blocking entry. Per guidance-publication mode it reports the first H and
// installed-profile divergences and H after each online step; each public
// install must reproduce the reference's profile (asserted). ConflictAnalysis
// replays as the public chrono instance under the record's payload; its
// returned retier schedule must equal the native one (asserted). A lookahead
// pass assumes support transactions without a public request yet.
#undef NDEBUG
// Rename the public alias as api.cpp does: its `tactix` typedef would collide
// with the internal namespace.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "default_action_map.hpp"
#include "public_online_encoding.hpp"

#include "api/api_internal.hpp"
#include "strategy/online_inprocess.hpp"
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

// Installed profiles a client can observe, one serialized entry per family:
// every family DefaultPolicy installs, Walk's guidance and the material
// settings. Index = the family's InprocessParameters alternative.
using Profile = std::vector<std::string>;
constexpr size_t MATERIAL_SETTINGS = std::variant_size_v<strategy::InprocessParameters>;

template <class P> void image(Profile &out, const char *rule, const P &parameters) {
  strategy::OnlineInvocation step;
  step.id = 1;
  step.rule = rule;
  step.parameters = parameters;
  strategy::OnlineStrategyProgram program;
  program.sequence.push_back(std::move(step));
  out[strategy::InprocessParameters(parameters).index()] =
      strategy::serialize_online_strategy_program(program);
}

Profile profile(const Kernel &k) {
  using namespace strategy;
  Profile out(MATERIAL_SETTINGS + 1);
  image(out, "ConflictAnalysis", AnalyzeParameters::from_kernel(k));
  image(out, "Reorder", ReorderParameters::from_kernel(k));
  image(out, "Reduce", ReduceParameters::from_kernel(k));
  WalkParameters walk;
  walk.step_budget = 0;
  walk.even_break_base_milli = k.walk_even_break_base_milli;
  walk.score_curve = k.walk_score_curve;
  image(out, "Walk", walk);
  image(out, "Lucky", ParityModelParameters::from_kernel(k));
  image(out, "Lucky", OnehotModelParameters::from_kernel(k));
  image(out, "Lucky", AffineModelParameters::from_kernel(k));
  image(out, "Lucky", BalancedSignatureModelParameters::from_kernel(k));
  image(out, "Lucky", FermatModelParameters::from_kernel(k));
  image(out, "Lucky", LookupCircuitModelParameters::from_kernel(k));
  image(out, "Lucky", SimonModelParameters::from_kernel(k));
  image(out, "Lucky", StconnProofParameters::from_kernel(k));
  image(out, "Lucky", SignedCardinalityProofParameters::from_kernel(k));
  image(out, "Lucky", ExactCoverModelParameters::from_kernel(k));
  image(out, "Lucky", IndependentSetModelParameters::from_kernel(k));
  image(out, "Lucky", GraphLabelingParameters::from_kernel(k));
  image(out, "Lucky", RamseyStartupParameters::from_kernel(k));
  image(out, "Congruence", k.congruence_parameters);
  image(out, "Instantiate", k.instantiate_parameters);
  image(out, "Backbone", k.backbone_parameters);
  image(out, "BCE", BceParameters::from_kernel(k));
  image(out, "HyperTernaryRes", TernaryParameters::from_kernel(k));
  image(out, "Factor", FactorParameters::from_kernel(k));
  image(out, "TransitiveReduce", TransitiveReduceParameters::from_kernel(k));
  image(out, "Substitute", SubstituteParameters::from_kernel(k));
  image(out, "Vivify", VivifyParameters::from_kernel(k));
  image(out, "BVE", BveParameters::from_kernel(k));
  out[MATERIAL_SETTINGS] = std::string("touch-filter=") +
      (k.touch_filter ? "1" : "0") + ";likely-kept-redundant=" +
      (k.touch_redundant ? "1" : "0");
  return out;
}

// The family an install record writes, as a Profile index.
size_t installed_family(const ActionFeedback &record) {
  if (record.identity == "InstallMaterialSettings") return MATERIAL_SETTINGS;
  return record.native_request->invocation->parameters.index();
}

// The first differing window of two serialized profiles.
std::string first_difference(const std::string &expected, const std::string &actual) {
  size_t at = 0;
  while (at < expected.size() && at < actual.size() && expected[at] == actual[at]) ++at;
  const size_t from = at > 40 ? at - 40 : 0;
  return "expected ..." + expected.substr(from, 100) + " actual ..." + actual.substr(from, 100);
}

std::string profile_mismatch(const Profile &expected, const Profile &actual) {
  std::string out;
  for (size_t family = 0; family < expected.size(); ++family)
    if (expected[family] != actual[family])
      out += std::string(out.empty() ? "" : ",") + "family" + std::to_string(family);
  return out;
}

int setting_mode(std::string_view value) {
  return value == "inherit" ? TACTIX_ONLINE_SETTING_INHERIT
         : value == "1"     ? TACTIX_ONLINE_SETTING_ENABLE
                            : TACTIX_ONLINE_SETTING_DISABLE;
}

// Replay one permanent install through tactix.h; empty on success.
std::string install(tactix_c_handle *s, const ActionFeedback &record) {
  if (record.identity == "InstallMaterialSettings") {
    const std::string_view text = description(record);
    const size_t touch = text.find("touch-filter="), kept = text.find(";likely-kept-redundant=");
    if (touch != 0 || kept == std::string_view::npos) return "unparsed material settings";
    const auto touch_value = text.substr(13, kept - 13);
    const auto kept_value = text.substr(kept + 23);
    return tactix_install_material_settings(s, setting_mode(touch_value),
                                            setting_mode(kept_value))
               ? "tactix_install_material_settings rejected" : "";
  }
  if (!record.native_request || !record.native_request->invocation) return "no typed profile";
  const auto &invocation = *record.native_request->invocation;
  public_encoding::Encoded encoded;
  if (!public_encoding::encode(invocation.parameters, encoded))
    return "profile has no tactix_online_parameters form";
  const int id = rule_id(invocation.rule);
  if (id < 0) return "unknown rule " + invocation.rule;
  return tactix_install_parameters(s, id, &encoded.parameters)
             ? "tactix_install_parameters rejected " + invocation.rule : "";
}

// A record's retier schedule text (execution::retier_schedule_text) after
// `key`; false when absent or malformed.
bool retier_schedule_field(std::string_view text, std::string_view key,
                           tactix_retier_schedule &out) {
  const size_t at = text.find(key);
  if (at == std::string_view::npos) return false;
  std::string_view rest = text.substr(at + key.size());
  rest = rest.substr(0, rest.find(';'));
  uint64_t v[13];
  for (uint64_t &value : v) {
    const size_t comma = rest.find(',');
    const std::string field(rest.substr(0, comma));
    if (field.empty()) return false;
    value = std::stoull(field);
    rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
  }
  out = {};
  out.enabled = static_cast<int>(v[0]);
  out.next_conflicts = v[1];
  out.interval = v[2];
  out.max_interval = v[3];
  out.growth = static_cast<uint32_t>(v[4]);
  out.tier1_default = static_cast<uint32_t>(v[5]);
  out.tier2_default = static_cast<uint32_t>(v[6]);
  out.tier1_relative_permille = static_cast<uint32_t>(v[7]);
  out.tier2_relative_permille = static_cast<uint32_t>(v[8]);
  out.tier1_by_mode[0] = static_cast<uint32_t>(v[9]);
  out.tier2_by_mode[0] = static_cast<uint32_t>(v[10]);
  out.tier1_by_mode[1] = static_cast<uint32_t>(v[11]);
  out.tier2_by_mode[1] = static_cast<uint32_t>(v[12]);
  return rest.empty();
}

bool same_schedule(const tactix_retier_schedule &a, const tactix_retier_schedule &b) {
  return a.enabled == b.enabled && a.next_conflicts == b.next_conflicts &&
         a.interval == b.interval && a.max_interval == b.max_interval &&
         a.growth == b.growth && a.tier1_default == b.tier1_default &&
         a.tier2_default == b.tier2_default &&
         a.tier1_relative_permille == b.tier1_relative_permille &&
         a.tier2_relative_permille == b.tier2_relative_permille &&
         a.tier1_by_mode[0] == b.tier1_by_mode[0] &&
         a.tier1_by_mode[1] == b.tier1_by_mode[1] &&
         a.tier2_by_mode[0] == b.tier2_by_mode[0] &&
         a.tier2_by_mode[1] == b.tier2_by_mode[1];
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

// The reference stream with H and the installed profile captured after
// every record. Records of a native composite (an inprocessing phase) are
// delivered when it finishes, so their capture is current only if the state
// has not moved since the record: `current` marks the captures to compare.
struct GuidanceCapture final : FeedbackReceiver {
  const Kernel &kernel;
  std::vector<ActionFeedback> actions;
  std::vector<Guidance> after;
  std::vector<Profile> profile_after;
  std::vector<bool> current;
  explicit GuidanceCapture(const Kernel &k) : kernel(k) {}
  void after_action(const ActionFeedback &value) override {
    actions.push_back(value);
    after.push_back(guidance(kernel));
    profile_after.push_back(profile(kernel));
    const auto version = strategy::state_version(kernel);
    current.push_back(mismatch(value.after, strategy::controller::observe_kernel(kernel)).empty() &&
                      version.arena_generation == value.after.state.arena_generation &&
                      version.touch_epoch == value.after.state.touch_epoch &&
                      version.strategy_epoch == value.after.state.strategy_epoch);
  }
  void search_interval(const SearchIntervalFeedback &) override {}
};

struct Report {
  uint64_t records = 0, installed = 0, assumed = 0, replayed = 0;
  uint64_t supports = 0, online_steps = 0, online_h_equal = 0, slot_steps = 0;
  // Online steps that made rule progress, and that grew the root trail.
  uint64_t online_progress = 0, online_roots = 0;
  std::string first_online_h_divergence = "none";
  std::vector<std::string> round_trip_mismatches; // online C form differs
  std::string stop = "end-of-stream";
  std::string first_guidance_divergence = "none"; // record and fields
  std::string guidance_at_stop = "unmeasured";   // differing fields, or empty
  uint64_t guidance_deferred = 0; // comparisons skipped: capture not current
  std::string first_profile_divergence = "none";
  std::vector<std::string> install_mismatches; // installed family differs
  uint64_t guidance_compared = 0, candidates_compared = 0;
  // Public chrono ConflictAnalysis replays: with a nonzero bump payload, with
  // an OTFS retier, and those whose returned schedule differs from native.
  uint64_t analyses = 0, analysis_bumps = 0, analysis_retiers = 0;
  std::vector<std::string> retier_mismatches;
};

// Replay the stream publicly. With `lookahead`, support transactions without
// a public request are assumed instead of stopping the replay, and no H or
// profile comparison is made.
Report replay(const Fixture &fixture, const GuidanceCapture &reference,
              int publication, bool lookahead = false) {
  const std::vector<ActionFeedback> &records = reference.actions;
  Report report;
  report.records = records.size();
  tactix_c_handle *s = load(fixture, publication);
  Kernel &observed = api_detail::kernel_for_fixed_testing(s);
  // The structured-root step configured as a one-step public program, as the
  // native sequence runs each step with its own executor.
  std::string online_key;
  // Compare the replayed prefix ending after record `last` with the reference.
  const auto compare = [&](size_t last) {
    if (lookahead) return;
    if (!reference.current[last]) {
      ++report.guidance_deferred;
      report.guidance_at_stop = "deferred";
      return;
    }
    const Guidance actual = guidance(observed);
    ++report.guidance_compared;
    report.candidates_compared += actual.candidates.size();
    report.guidance_at_stop = guidance_mismatch(reference.after[last], actual);
    if (!report.guidance_at_stop.empty() && report.first_guidance_divergence == "none")
      report.first_guidance_divergence = "#" + std::to_string(last) + " " +
          records[last].identity + " " + report.guidance_at_stop;
    if (report.first_profile_divergence == "none")
      if (const std::string differs =
              profile_mismatch(reference.profile_after[last], profile(observed));
          !differs.empty())
        report.first_profile_divergence = "#" + std::to_string(last) + " " +
            records[last].identity + " " + differs;
  };
  size_t i = 0;
  for (; i < records.size(); ++i) {
    if (i) compare(i - 1);
    const ActionFeedback &record = records[i];
    const Entry *entry = classify(record);
    const std::string where = "#" + std::to_string(i) + " " +
                              (entry ? entry->id : "unclassified") + " (" +
                              record.identity + ")";
    if (!entry || entry->replay == Replay::None) {
      if (lookahead && entry && record.kind == FeedbackKind::Support) {
        ++report.assumed;
        continue;
      }
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
    if (entry->replay == Replay::Support) {
      const int code = tactix_support(s, entry->support, support_argument(*entry, record));
      const bool vacuous = code == -1 && mismatch(record.before, record.after).empty();
      if (code == -1 && !vacuous) {
        report.stop = where + ": tactix_support rejected";
        break;
      }
      if (const std::string differs =
              mismatch(record.after, strategy::controller::observe_kernel(observed));
          !differs.empty()) {
        report.stop = where + ": diverged state " + differs;
        break;
      }
      ++report.supports;
      ++report.replayed;
      continue;
    }
    if (entry->replay == Replay::Online) {
      const auto &invocation = *record.native_request->invocation;
      const std::string_view text = description(record);
      const std::string key = std::string(text.substr(0, text.find(';'))) + ";step=" +
                              std::to_string(invocation.id);
      // A slot occurrence is one online invocation of its own.
      const bool slot = starts(entry->id, "slot-");
      if (slot || key != online_key || tactix_online_complete(s) == 1) {
        public_encoding::EncodedStep encoded;
        if (!public_encoding::encode_step(observed, invocation, rule_id(invocation.rule),
                                          encoded)) {
          report.stop = where + ": step has no tactix_online_step_v2 form";
          break;
        }
        const tactix_online_program_v2 program{&encoded.step, 1,
                                               TACTIX_ONLINE_REPEAT_ONCE, 1};
        if (tactix_online_configure_v2(s, &program, 1, TACTIX_ONLINE_RECONFIGURE_RESET)) {
          report.stop = where + ": tactix_online_configure_v2 rejected";
          break;
        }
        strategy::OnlineStrategyProgram native;
        native.sequence.push_back(invocation);
        const auto *configured = api_detail::online_program_for_fixed_testing(s);
        if (!lookahead && strategy::serialize_online_strategy_program(*configured) !=
                              strategy::serialize_online_strategy_program(native))
          report.round_trip_mismatches.push_back(where + " step=" +
                                                 std::to_string(invocation.id));
        online_key = key;
      }
      tactix_online_result result{};
      if (tactix_online_advance(s, &result)) {
        report.stop = where + ": tactix_online_advance rejected";
        break;
      }
      const auto &expected_invocation = record.outcome.invocation;
      if (result.status != static_cast<int>(expected_invocation.status) ||
          result.applications != expected_invocation.applications) {
        report.stop = where + ": diverged: status " + std::to_string(result.status) +
                      " expected " +
                      std::to_string(static_cast<int>(expected_invocation.status)) +
                      ", applications " + std::to_string(result.applications) +
                      " expected " + std::to_string(expected_invocation.applications);
        break;
      }
      if (const std::string differs =
              mismatch(record.after, strategy::controller::observe_kernel(observed));
          !differs.empty()) {
        report.stop = where + ": diverged state " + differs;
        break;
      }
      ++report.online_steps;
      report.slot_steps += slot;
      report.online_progress += expected_invocation.made_rule_progress;
      report.online_roots += record.after.root_population > record.before.root_population;
      ++report.replayed;
      if (!lookahead) {
        const std::string differs = guidance_mismatch(reference.after[i], guidance(observed));
        report.online_h_equal += differs.empty();
        if (!differs.empty() && report.first_online_h_divergence == "none")
          report.first_online_h_divergence = "#" + std::to_string(i) + " " +
              invocation.rule + " step=" + std::to_string(invocation.id) + " " + differs;
      }
      continue;
    }
    if (entry->replay == Replay::Install) {
      if (const std::string failed = install(s, record); !failed.empty()) {
        report.stop = where + ": " + failed;
        break;
      }
      const size_t family = installed_family(record);
      // The reference observes state when the record is delivered, which a
      // native publication scope (a whole inprocessing phase) defers; the
      // record's own typed profile is what the install must reproduce.
      std::string expected = reference.profile_after[i][family];
      if (family != MATERIAL_SETTINGS) {
        const auto &invocation = *record.native_request->invocation;
        Profile own(MATERIAL_SETTINGS + 1);
        std::visit([&](const auto &p) {
          if constexpr (!std::is_same_v<std::decay_t<decltype(p)>, std::monostate>)
            image(own, invocation.rule.c_str(), p);
        }, invocation.parameters);
        expected = own[family];
      }
      if (!lookahead && profile(observed)[family] != expected)
        report.install_mismatches.push_back(where + ": " +
            first_difference(expected, profile(observed)[family]));
      ++report.installed;
      ++report.replayed;
      continue;
    }
    if (entry->replay == Replay::Analysis) {
      // The native payload: the profile with its bump fields and the retier
      // schedule. Install both, apply the chrono instance, compare the
      // returned schedule, then restore the neutral profile and schedule.
      tactix_retier_schedule before{}, after{}, returned{}, idle{};
      const std::string_view text = description(record);
      if (!record.native_request || !record.native_request->invocation ||
          !retier_schedule_field(text, ";retier=", before) ||
          !retier_schedule_field(text, ";retier-after=", after)) {
        report.stop = where + ": unparsed analysis payload";
        break;
      }
      const auto &invocation = *record.native_request->invocation;
      auto profile = std::get<strategy::AnalyzeParameters>(invocation.parameters);
      public_encoding::Encoded payload, neutral;
      const int id = rule_id("ConflictAnalysis");
      const bool bump = profile.bump_reason_limit != 0;
      if (!public_encoding::encode(invocation.parameters, payload) ||
          tactix_install_parameters(s, id, &payload.parameters) ||
          tactix_read_retier_schedule(s, &idle) ||
          tactix_install_retier_schedule(s, &before)) {
        report.stop = where + ": analysis payload install rejected";
        break;
      }
      const int code = tactix_apply_semantic_instance(s, id, TACTIX_ANALYZE_TARGET_CHRONO);
      const int expected = result_code(record.outcome.invocation.rule_result);
      assert(tactix_read_retier_schedule(s, &returned) == 0);
      profile.bump_reason_limit = 0;
      profile.bump_reason_depth = 1;
      if (!public_encoding::encode(strategy::InprocessParameters(profile), neutral) ||
          tactix_install_parameters(s, id, &neutral.parameters) ||
          tactix_install_retier_schedule(s, &idle)) {
        report.stop = where + ": analysis payload restore rejected";
        break;
      }
      if (code != expected) {
        report.stop = where + ": diverged: result " + std::to_string(code) +
                      " expected " + std::to_string(expected);
        break;
      }
      if (const std::string differs =
              mismatch(record.after, strategy::controller::observe_kernel(observed));
          !differs.empty()) {
        report.stop = where + ": diverged state " + differs;
        break;
      }
      if (!same_schedule(returned, after)) report.retier_mismatches.push_back(where);
      ++report.analyses;
      report.analysis_bumps += bump;
      report.analysis_retiers += !same_schedule(before, after);
      ++report.replayed;
      continue;
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
  std::map<std::string, uint64_t> blockers, lookahead_stops;
  uint64_t replayed = 0, installed = 0, records_total = 0;
  uint64_t lookahead_replayed = 0, lookahead_assumed = 0;
  uint64_t slot_steps = 0, lookahead_slot_steps = 0;
  std::vector<std::string> install_mismatches;
  std::map<int, uint64_t> guidance_matches;
  uint64_t guidance_compared = 0, candidates_compared = 0;
  std::map<int, uint64_t> deferred_stops;
  uint64_t guidance_deferred = 0;
  uint64_t replayed_fixtures = 0;
  uint64_t supports = 0;
  std::map<int, uint64_t> online_steps, online_h_equal;
  uint64_t online_progress = 0, online_roots = 0;
  std::vector<std::string> round_trips;
  uint64_t analyses = 0, analysis_bumps = 0, analysis_retiers = 0;
  uint64_t lookahead_analyses = 0;
  std::vector<std::string> retier_mismatches;
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
      deferred_stops[publication] += mode.guidance_at_stop == "deferred";
      guidance_deferred += mode.guidance_deferred;
      guidance_compared += mode.guidance_compared;
      candidates_compared += mode.candidates_compared;
      for (const auto &where : mode.install_mismatches)
        install_mismatches.push_back(fixture.name + " " + name + " " + where);
      guidance_line += std::string(" profile-") + name + "-first=" +
                       mode.first_profile_divergence;
      online_steps[publication] += mode.online_steps;
      online_h_equal[publication] += mode.online_h_equal;
      if (mode.online_steps)
        guidance_line += std::string(" online-h-") + name + "=" +
                         std::to_string(mode.online_h_equal) + "/" +
                         std::to_string(mode.online_steps) + " online-h-" + name +
                         "-first=" + mode.first_online_h_divergence;
      for (const auto &where : mode.round_trip_mismatches)
        round_trips.push_back(fixture.name + " " + name + " " + where);
    }
    const Report ahead = replay(fixture, reference, TACTIX_GUIDANCE_DISCARD, true);
    analyses += report.analyses;
    analysis_bumps += report.analysis_bumps;
    analysis_retiers += report.analysis_retiers;
    lookahead_analyses += ahead.analyses;
    for (const Report *pass : {static_cast<const Report *>(&report), &ahead})
      for (const auto &where : pass->retier_mismatches)
        retier_mismatches.push_back(fixture.name + " " + where);
    replayed += report.replayed;
    installed += report.installed;
    supports += report.supports;
    online_progress += report.online_progress;
    online_roots += report.online_roots;
    records_total += report.records;
    lookahead_replayed += ahead.replayed;
    lookahead_assumed += ahead.assumed;
    slot_steps += report.slot_steps;
    lookahead_slot_steps += ahead.slot_steps;
    if (ahead.stop != "end-of-stream")
      ++lookahead_stops[ahead.stop.substr(ahead.stop.find(' ') + 1)];
    std::cout << "fixture=" << fixture.name << " records=" << report.records
              << " replayed=" << report.replayed << " installs=" << report.installed
              << " stop=" << report.stop << guidance_line
              << " lookahead-replayed=" << ahead.replayed << " lookahead-assumed="
              << ahead.assumed << " lookahead-stop=" << ahead.stop << '\n';
  }
  std::cout << "replayed " << replayed << " of " << records_total << " records ("
            << installed << " permanent installs, " << supports << " supports, "
            << online_steps[TACTIX_GUIDANCE_DISCARD]
            << " online steps, of which " << slot_steps
            << " inprocessing slots); blocking entries over all streams:";
  for (const auto &[id, count] : blockers) std::cout << ' ' << id << '=' << count;
  std::cout << "\nlookahead (support transactions assumed): replayed "
            << lookahead_replayed << " (" << lookahead_slot_steps
            << " inprocessing slots), assumed " << lookahead_assumed << "; stops:";
  for (const auto &[where, count] : lookahead_stops) std::cout << " [" << where << "]=" << count;
  for (const auto &where : install_mismatches)
    std::cout << "\ninstalled profile differs from the reference: " << where;
  for (const auto &where : round_trips)
    std::cout << "\nonline step C form differs from the native request: " << where;
  std::cout << "\npublic chrono ConflictAnalysis replays: strict " << analyses
            << " (" << analysis_bumps << " with a bump payload, " << analysis_retiers
            << " with an OTFS retier), lookahead " << lookahead_analyses;
  for (const auto &where : retier_mismatches)
    std::cout << "\nreturned retier schedule differs from the native one: " << where;
  std::cout << "\nH equal after structured-root online steps: discard "
            << online_h_equal[TACTIX_GUIDANCE_DISCARD] << '/'
            << online_steps[TACTIX_GUIDANCE_DISCARD] << ", native "
            << online_h_equal[TACTIX_GUIDANCE_NATIVE] << '/'
            << online_steps[TACTIX_GUIDANCE_NATIVE] << " (" << online_progress
            << " steps with rule progress, " << online_roots << " growing the root)";
  std::cout << "\nH equal at the replay stop point: discard "
            << guidance_matches[TACTIX_GUIDANCE_DISCARD] << '/' << replayed_fixtures
            << ", native " << guidance_matches[TACTIX_GUIDANCE_NATIVE] << '/'
            << replayed_fixtures << "; deferred (reference capture not current at the "
            << "stop): discard " << deferred_stops[TACTIX_GUIDANCE_DISCARD] << ", native "
            << deferred_stops[TACTIX_GUIDANCE_NATIVE] << " (" << guidance_compared
            << " comparisons over " << candidates_compared << " candidate literals, "
            << guidance_deferred << " deferred)\n";
  // Report-only, except that the replay machinery itself must have executed
  // public calls (installs, supports and online steps), the H comparison must
  // have observed nonempty guidance, every public install must reproduce the
  // reference's installed profile, and every encoded online step must convert
  // back to exactly the native request.
  std::cout.flush();
  assert(replayed > 0 && installed > 0 && candidates_compared > 0);
  assert(supports > 0 && online_steps[TACTIX_GUIDANCE_DISCARD] > 0);
  assert(install_mismatches.empty() && round_trips.empty());
  assert(retier_mismatches.empty());
  return 0;
}
