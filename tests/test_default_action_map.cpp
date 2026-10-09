// DefaultPolicy action-route ratchet (gap plan Step 0). Every feedback record
// DefaultPolicy emits on the fixtures must be classified by the checked-in map
// in default_action_map.hpp; the internal (Shared/Kernel) entries must equal the
// list for the current refactor step; and each record's observed dispatch must
// agree with its declared route: Public records advance strategy_epoch through
// registry dispatch, internal records naming a registry rule do not.
#undef NDEBUG
#include "default_action_map.hpp"

#include <iostream>
#include <map>
#include <set>

using namespace action_map;

// Internal entries after Step 3 (permanent installs through
// tactix_install_parameters). Each later step removes the entries it routes
// publicly.
static const std::set<std::string> EXPECTED_INTERNAL = {
    "propagate-search", "decide", "conflict-analysis", "propagate-root",
    "affine-matrix-initialize", "affine-matrix-search", "slot-sweep-batch",
    "slot-substitute-rounds", "slot-vivify-band", "slot-fastel-occurrence",
    "install-affine-matrix-parameters", "support-transaction"};

// Entries whose route a refactor step changes must be reached by the fixtures,
// or their dispatch check would be vacuous.
static const std::set<std::string> REQUIRED_OBSERVED = {
    "propagate-search", "decide", "conflict-analysis", "restart-scheduled",
    "resume-search", "reduce", "switch-mode", "reorder", "rephase", "walk",
    "lucky-opportunity", "lucky-graph-session", "local-search-guidance",
    "bce-post-structured", "propagate-root", "slot-registered",
    "slot-bve-candidate", "structured-root-online",
    "install-parameters", "install-walk-guidance", "install-material-settings",
    "close-startup-certificate", "support-transaction"};

int main(int argc, char **argv) {
  const bool dump = argc > 1 && !std::strcmp(argv[1], "--dump");
  struct Tally {
    uint64_t records = 0, dispatched = 0;
    std::set<std::string> identities;
  };
  std::map<std::string, Tally> tally;
  bool ok = true;
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
    for (const ActionFeedback &record : records) {
      const Entry *entry = classify(record);
      if (!entry) {
        std::cerr << "unclassified record in " << fixture.name << ": kind="
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
        std::cerr << "route mismatch in " << fixture.name << ": entry=" << entry->id
                  << " declared=" << route_name(entry->route)
                  << " dispatched=" << dispatched(record) << " identity="
                  << record.identity << " instance=" << record.instance
                  << " description=" << description(record) << '\n';
        ok = false;
      }
    }
    if (fixture.clausal_proof) proof.flush();
    if (dump)
      std::cout << "fixture " << fixture.name << " records=" << records.size() << '\n';
  }

  std::set<std::string> internal, all;
  for (const Entry &entry : entries()) {
    assert(all.insert(entry.id).second); // entry ids are unique
    if (entry.route != Route::Public) internal.insert(entry.id);
  }
  uint64_t public_records = 0, internal_records = 0;
  for (const Entry &entry : entries()) {
    const auto found = tally.find(entry.id);
    const Tally t = found == tally.end() ? Tally{} : found->second;
    (entry.route == Route::Public ? public_records : internal_records) += t.records;
    std::cout << route_name(entry.route) << ' '
              << (entry.boundary == Boundary::Interval ? "in" : "x ") << ' '
              << entry.id << " records=" << t.records << " dispatched="
              << t.dispatched << " plan=" << entry.plan;
    if (dump)
      for (const auto &identity : t.identities) std::cout << ' ' << identity;
    std::cout << '\n';
  }
  std::cout << "entries: public=" << all.size() - internal.size()
            << " internal=" << internal.size() << "; records: public="
            << public_records << " internal=" << internal_records << '\n';
  if (dump) return ok ? 0 : 1;
  for (const auto &id : REQUIRED_OBSERVED)
    if (!tally.count(id)) {
      std::cerr << "fixtures do not reach entry " << id << '\n';
      ok = false;
    }
  if (internal != EXPECTED_INTERNAL) {
    for (const auto &id : internal)
      if (!EXPECTED_INTERNAL.count(id)) std::cerr << "newly internal: " << id << '\n';
    for (const auto &id : EXPECTED_INTERNAL)
      if (!internal.count(id)) std::cerr << "now public (shrink the list): " << id << '\n';
    ok = false;
  }
  assert(ok);
  return 0;
}
