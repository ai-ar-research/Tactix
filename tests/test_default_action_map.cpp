// DefaultPolicy action-route ratchet (gap plan Step 0). Every feedback record
// DefaultPolicy emits on the fixtures must be classified by the checked-in map
// in default_action_map.hpp; the internal (Shared/Kernel) entries must equal the
// list for the current refactor step; and each record's observed dispatch must
// agree with its declared route: Public records advance strategy_epoch through
// registry dispatch, internal records naming a registry rule do not.
#undef NDEBUG
#include "default_action_map.hpp"
#include "strategy/policy_completion.hpp"

#include <iostream>
#include <map>
#include <set>

using namespace action_map;

// Internal entries after Step 5 (inprocessing slots as online invocations),
// on top of Step 4b's public phase bracket and Step 6's public chrono
// ConflictAnalysis (its ranked-trail arm is a declared diagnostic). Each later
// step removes the entries it routes publicly.
static const std::set<std::string> EXPECTED_INTERNAL = {
    "propagate-search", "decide",
    "conflict-analysis-ranked-trail", "propagate-root",
    "affine-matrix-initialize", "affine-matrix-search", "slot-fastel-occurrence",
    "install-affine-matrix-parameters", "support-warmup-installed-branching"};

// Entries whose route a refactor step changes must be reached by the fixtures,
// or their dispatch check would be vacuous.
static const std::set<std::string> REQUIRED_OBSERVED = {
    "propagate-search", "decide", "conflict-analysis", "restart-scheduled",
    "resume-search", "reduce", "switch-mode", "reorder", "rephase", "walk",
    "lucky-opportunity", "lucky-graph-session", "local-search-guidance",
    "bce-post-structured", "propagate-root", "slot-registered",
    "slot-bve-candidate", "slot-sweep-batch", "slot-substitute-rounds",
    "structured-root-online", "support-begin-forward-round",
    "install-parameters", "install-walk-guidance", "install-material-settings",
    "close-startup-certificate", "support-root-preparation",
    "support-sparse-collect", "support-compact-random-order",
    "support-compact-variables", "support-reduction-preparation",
    "support-warmup", "support-reset-graph-labeling",
    "support-consume-root-prefix", "support-enter-context",
    "support-leave-context", "materialize-cold-state", "support-phase-entry",
    "support-occurrence-session", "support-phase-exit", "slot-vivify-band",
    "support-forward-closure", "support-reschedule-bve"};

// Slot occurrences the solve fixtures do not schedule (a search-time Vivify
// dose, an Eliminate-pipeline Strengthen pass and its forward closure, a BVE
// growth-bound increase), from one native phase with only the named slots
// enabled, as tests/test_native_policy_feedback.cpp runs it.
static std::vector<ActionFeedback> phase_records(SlotGroup group,
                                                 std::vector<const char *> rules,
                                                 bool vivify_fixture) {
  Kernel k;
  k.stats.conflicts = 1;
  k.stats.search_ticks = 1000000000;
  if (vivify_fixture) {
    kernel_reserve_vars(k, 160);
    for (uint32_t clause = 0; clause < 8; ++clause) {
      Lit lits[20];
      for (uint32_t i = 0; i < 20; ++i) lits[i] = make_lit(clause * 20 + i, true);
      kernel_add_large_clause(k, lits, 20, true, 2, 0);
    }
    k.tier1 = k.tier2 = 2;
  } else {
    pigeonhole(k, 4, 5);
  }
  ScopedHeuristics h(k);
  DefaultStrategyConfig config;
  config.inprocess.minimum_effort_ticks = 1000;
  config.inprocess.phase_rounds = 1;
  config.inprocess.vivify_permille = 1000;
  DefaultPolicy policy(config);
  policy.inprocess().build(k);
  // A Vivify dose poses the focused tier snapshot of the retier schedule.
  RetierSchedule tiers;
  tiers.tier1_by_mode[0] = tiers.tier2_by_mode[0] = 2;
  tiers.tier1_by_mode[1] = tiers.tier2_by_mode[1] = 2;
  if (vivify_fixture) policy.inprocess().set_retier_schedule(&tiers);
  for (auto &entry : policy.inprocess().entries) {
    entry.enabled = false;
    for (const char *rule : rules)
      entry.enabled |= !std::strcmp(tactix_rules()[entry.rule].name, rule);
  }
  Capture capture;
  execution::PolicyCompletion completion(k, &capture,
      [](const Kernel &kernel, const void *owner) {
        return static_cast<const DefaultPolicy *>(owner)->observe(kernel);
      }, &policy);
  completion.begin();
  (void)policy.inprocess().run_phase(k, group, nullptr, &completion);
  completion.finish(0, StopReason::Completed);
  return capture.actions;
}

int main(int argc, char **argv) {
  const bool dump = argc > 1 && !std::strcmp(argv[1], "--dump");
  struct Tally {
    uint64_t records = 0, dispatched = 0;
    std::set<std::string> identities;
  };
  std::map<std::string, Tally> tally;
  bool ok = true;
  std::vector<std::pair<std::string, std::vector<ActionFeedback>>> phases;
  phases.emplace_back("phase-vivify", phase_records(SlotGroup::Probe, {"Vivify"}, true));
  phases.emplace_back("phase-eliminate",
                      phase_records(SlotGroup::Eliminate, {"BVE", "Strengthen"}, false));
  const auto account = [&](const std::string &name,
                           const std::vector<ActionFeedback> &records) {
    for (const ActionFeedback &record : records) {
      const Entry *entry = classify(record);
      if (!entry) {
        std::cerr << "unclassified record in " << name << ": kind="
                  << static_cast<int>(record.kind) << " identity=" << record.identity
                  << " instance=" << record.instance << " description="
                  << description(record) << '\n';
        ok = false;
        continue;
      }
      Tally &t = tally[entry->id];
      ++t.records;
      t.dispatched += dispatched(record);
      t.identities.insert(record.identity);
      if (!entry->check_dispatch || !registry_identity(record) || dump) continue;
      // A public step that selects no instance (an empty online pass)
      // performs no application and so no dispatch.
      const bool applied = record.outcome.invocation.applications != 0;
      if (entry->route == Route::Public ? applied && !dispatched(record)
                                        : dispatched(record)) {
        std::cerr << "route mismatch in " << name << ": entry=" << entry->id
                  << " declared=" << route_name(entry->route)
                  << " dispatched=" << dispatched(record) << " identity="
                  << record.identity << " instance=" << record.instance
                  << " description=" << description(record) << '\n';
        ok = false;
      }
    }
  };
  for (const auto &[name, records] : phases) {
    account(name, records);
    if (dump) std::cout << "phase " << name << " records=" << records.size() << '\n';
  }
  for (const Fixture &fixture : fixtures()) {
    Kernel k;
    ScopedHeuristics h(k);
    fixture.load(k);
    Proof proof;
    if (fixture.clausal_proof) {
      assert(proof.open("/dev/null", ProofFormat::DratAscii));
      proof.bind_variable_map(&k.internal_to_external);
      k.proof = &proof;
    }
    const auto records = run(fixture, k);
    account(fixture.name, records);
    if (fixture.clausal_proof) proof.flush();
    if (dump)
      std::cout << "fixture " << fixture.name << " records=" << records.size() << '\n';
  }

  std::set<std::string> internal_ids, declared, all;
  for (const Entry &entry : entries()) {
    assert(all.insert(entry.id).second); // entry ids are unique
    if (internal(entry.route)) internal_ids.insert(entry.id);
    if (entry.route == Route::Declared) declared.insert(entry.id);
    // A Support replay names its tactix_support transaction.
    assert((entry.replay == Replay::Support) == (entry.support != 0));
  }
  uint64_t public_records = 0, internal_records = 0, declared_records = 0;
  for (const Entry &entry : entries()) {
    const auto found = tally.find(entry.id);
    const Tally t = found == tally.end() ? Tally{} : found->second;
    (internal(entry.route) ? internal_records
     : entry.route == Route::Declared ? declared_records
                                      : public_records) += t.records;
    std::cout << route_name(entry.route) << ' '
              << (entry.boundary == Boundary::Interval ? "in" : "x ") << ' '
              << entry.id << " records=" << t.records << " dispatched="
              << t.dispatched << " plan=" << entry.plan;
    if (dump)
      for (const auto &identity : t.identities) std::cout << ' ' << identity;
    std::cout << '\n';
  }
  std::cout << "entries: public=" << all.size() - internal_ids.size() - declared.size()
            << " internal=" << internal_ids.size() << " declared=" << declared.size()
            << "; records: public=" << public_records << " internal="
            << internal_records << " declared=" << declared_records << '\n';
  if (dump) return ok ? 0 : 1;
  for (const auto &id : REQUIRED_OBSERVED)
    if (!tally.count(id)) {
      std::cerr << "fixtures do not reach entry " << id << '\n';
      ok = false;
    }
  if (internal_ids != EXPECTED_INTERNAL) {
    for (const auto &id : internal_ids)
      if (!EXPECTED_INTERNAL.count(id)) std::cerr << "newly internal: " << id << '\n';
    for (const auto &id : EXPECTED_INTERNAL)
      if (!internal_ids.count(id)) std::cerr << "now public (shrink the list): " << id << '\n';
    ok = false;
  }
  assert(ok);
  return 0;
}
