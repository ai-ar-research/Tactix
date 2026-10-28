// DefaultPolicy static/public twin test (plan §5.6 acceptance, §8.4
// correspondence gate; the controller-interface design). One source, three
// executables over the static, hardcoded and public action ports. The public
// executable runs the other two with --emit and requires identical verdicts,
// certificate bytes, models and trajectory counters on every replayable
// action-map fixture, then lists the routes that block a pure public client.
// The complete correspondence is asserted once no internal ratchet entry is
// reached (declared non-actions are not internal).
#undef NDEBUG
#include "default_action_map.hpp"
#include "api/api_internal.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
#include "default_public_port.hpp"
#endif

#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unistd.h>

using namespace action_map;

namespace {

// Port fallbacks: actions whose public request the replay gate reproduces
// (ratchet Public) but which PublicPort still executes statically. Ratcheted
// both ways, like test_default_action_map's internal set.
const std::set<std::string> EXPECTED_PORT_FALLBACKS = {};

// Affine fixtures (structural provenance bound, DSR proof): the startup root
// closure refuting an odd K4 Tseitin system, and the persistent affine search
// (initialization and startup service) on a satisfiable XOR system.
bool affine(const Fixture &fixture) { return starts(fixture.name, "affine-"); }

void without_startup_models(DefaultStrategyConfig &c) {
  for (bool *enabled : {&c.affine_model.enabled, &c.parity_model.enabled,
           &c.onehot_model.enabled, &c.ramsey_startup.enabled,
           &c.signed_cardinality_proof.enabled, &c.balanced_signature_model.enabled,
           &c.exact_cover_model.enabled, &c.independent_set_model.enabled,
           &c.fermat_model.enabled, &c.cerny_model.enabled, &c.lookup_circuit_model.enabled,
           &c.simon_model.enabled, &c.stconn_proof.enabled})
    *enabled = false;
}

std::vector<std::vector<int>> xors(const std::vector<std::pair<std::vector<int32_t>, bool>> &rows) {
  std::vector<std::vector<int>> out;
  for (const auto &[variables, rhs] : rows)
    for (const auto &clause : affine_semantic_extension::clausal_form(variables, rhs))
      out.emplace_back(clause.begin(), clause.end());
  return out;
}

std::vector<Fixture> twin_fixtures() {
  std::vector<Fixture> out;
  for (Fixture &fixture : fixtures())
    if (!fixture.dimacs.empty()) out.push_back(std::move(fixture));
  Fixture closure;
  closure.name = "affine-root-closure-tseitin-k4";
  closure.dimacs = xors({{{1, 2, 3}, true}, {{1, 4, 5}, false}, {{2, 4, 6}, false},
                         {{3, 5, 6}, false}});
  closure.configure = [](DefaultStrategyConfig &c, Kernel &) { without_startup_models(c); };
  out.push_back(closure);
  // tests/test_default_affine_search.cpp's mixed pigeonhole/XOR fixture.
  Fixture search;
  search.name = "affine-search-mixed-pigeons";
  const int pigeons = 6, holes = 5;
  search.dimacs = pigeonhole_dimacs(pigeons, holes);
  for (const auto &clause : xors({{{1, 7, 13}, false}, {{7, 13, 19, 20}, true}}))
    search.dimacs.push_back(clause);
  search.dimacs.push_back({-1, 19});
  const int n = pigeons * holes;
  for (int gate : {n + 1, -(n + 2), n + 5, -(n + 6)})
    for (int a : {n + 3, -(n + 3)})
      for (int b : {n + 4, -(n + 4)}) search.dimacs.push_back({gate, a, b});
  search.configure = [](DefaultStrategyConfig &c, Kernel &) {
    without_startup_models(c);
    assert(c.affine_search.enabled);
    c.affine_root_closure.enabled = false;
    // Keep the XOR sources through preprocessing for the startup
    // initialization (tests/test_default_affine_search.cpp's configuration).
    c.inprocess.program.clear();
    c.inprocess.append_unlisted_rules = false;
    c.inprocess.structured_root_stages.clear();
    c.inprocess.structured_root_bounded_backtracking_stages.clear();
  };
  out.push_back(search);
  return out;
}

// A loaded C session; an affine fixture also binds a provenance stream.
tactix_c_handle *open_session(const Fixture &fixture, const std::string *proof_path,
                              FILE **provenance) {
  tactix_c_handle *s = tactix_init();
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) == 0);
  if (proof_path)
    assert(tactix_proof(s, proof_path->c_str(),
                        affine(fixture) ? TACTIX_PROOF_DSR_ASCII : TACTIX_PROOF_DRAT_ASCII) == 0);
  *provenance = nullptr;
  if (affine(fixture)) {
    *provenance = std::tmpfile();
    assert(*provenance && tactix_bind_structural_provenance(s, *provenance) == 0);
  }
  for (const auto &clause : fixture.dimacs) {
    for (int lit : clause) tactix_add(s, lit);
    tactix_add(s, 0);
  }
  if (fixture.tune) fixture.tune(api_detail::kernel_for_fixed_testing(s));
  (void)tactix_status(s);
  return s;
}

void close_session(tactix_c_handle *s, FILE *provenance) {
  tactix_release(s);
  if (provenance) std::fclose(provenance);
}

uint64_t fnv(const void *data, size_t size, uint64_t h = 1469598103934665603ull) {
  const auto *p = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

std::string hex(uint64_t value) {
  char out[17];
  std::snprintf(out, sizeof out, "%016" PRIx64, value);
  return out;
}

int max_variable(const Fixture &fixture) {
  int out = 0;
  for (const auto &clause : fixture.dimacs)
    for (int lit : clause) out = std::max(out, lit < 0 ? -lit : lit);
  return out;
}

uint64_t timed_rule_calls(const Kernel &k) {
  uint64_t out = 0;
  for (uint64_t calls : k.stats.rule_calls) out += calls;
  return out;
}

uint64_t trail_hash(const Kernel &k) {
  uint64_t h = fnv(nullptr, 0);
  for (Lit lit : k.trail) h = fnv(&lit, sizeof lit, h);
  return h;
}

// One fixture's observable result on a fresh C session: verdict, certificate
// bytes, model and trajectory counters. Declared counters (rule timing and
// calls, api_decisions, the numeric strategy epoch) are excluded.
std::string emit(const Fixture &fixture, const std::string &scratch) {
  const std::string proof_path = scratch + "/" + fixture.name + ".proof";
  FILE *provenance;
  tactix_c_handle *s =
      open_session(fixture, fixture.clausal_proof ? &proof_path : nullptr, &provenance);
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
  port::PublicPort::session = s;
  port::PublicPort::retirements_held = false;
#endif
  const uint64_t rule_calls_before = timed_rule_calls(k);
  int verdict;
  {
    DefaultStrategyConfig config;
    if (fixture.configure) fixture.configure(config, k);
    DefaultPolicy policy(config);
    verdict = policy.solve(k);
  }
#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
  port::PublicPort::session = nullptr;
  const uint64_t charged = timed_rule_calls(k) - rule_calls_before;
  assert(charged >= port::PublicPort::census.timed_dispatches);
  const uint64_t bypassed = charged - port::PublicPort::census.timed_dispatches;
  port::PublicPort::census.timed_dispatches = 0;
  if (bypassed) port::PublicPort::census.fallbacks["registry-dispatch-not-through-tactix.h"] += bypassed;
#else
  (void)rule_calls_before;
#endif
  std::ostringstream out;
  out << "fixture=" << fixture.name << " verdict=" << verdict;
  std::string model;
  if (verdict == 10)
    for (int v = 1; v <= max_variable(fixture); ++v) model += tactix_value(s, v) > 0 ? '1' : '0';
  out << " model=" << hex(fnv(model.data(), model.size()));
  const Stats &st = k.stats;
  out << " conflicts=" << st.conflicts << " decisions=" << st.decisions
      << " propagations=" << st.propagations << " ticks=" << st.ticks
      << " learned=" << st.learned << " learned-lits=" << st.learned_lits
      << " restarts=" << st.restarts << " reused=" << st.restarts_reused
      << " reductions=" << st.reductions << " switched=" << st.switched
      << " walks=" << st.walks << " roots=" << st.root_assignments
      << " eliminated=" << st.bve_eliminated
      << " affine-init=" << st.affine_matrix_init_applied << '/' << st.affine_matrix_init_attempts
      << " affine-service=" << st.affine_matrix_service_calls
      << " sweep-equivalences=" << st.sweep_equivalences
      << " substituted=" << st.substitute_vars << " level=" << k.level
      << " trail=" << hex(trail_hash(k));
  if (k.h) {
    // The decision structures' exact layout (VMTF links and queue ends, the
    // score heap's array), which removal order changes.
    uint64_t layout = fnv(k.h->scores.stack.data(), k.h->scores.stack.size() * sizeof(uint32_t));
    layout = fnv(k.h->links.data(), k.h->links.size() * sizeof(k.h->links[0]), layout);
    layout = fnv(&k.h->queue_first, sizeof k.h->queue_first, layout);
    layout = fnv(&k.h->queue_last, sizeof k.h->queue_last, layout);
    out << " layout=" << hex(layout);
  }
  if (k.h)
    out << " saved=" << hex(fnv(k.h->saved.data(), k.h->saved.size()))
        << " scores=" << hex(fnv(k.h->scores.score.data(),
                                 k.h->scores.score.size() * sizeof(double)));
  close_session(s, provenance);
  if (fixture.clausal_proof) {
    // rdbuf, not string(istreambuf_iterator): GCC 13.1 PR 109703 under UBSan.
    std::ifstream in(proof_path, std::ios::binary);
    std::ostringstream image;
    image << in.rdbuf();
    const std::string bytes = image.str();
    out << " proof-bytes=" << bytes.size() << " proof=" << hex(fnv(bytes.data(), bytes.size()));
    std::remove(proof_path.c_str());
  }
  return out.str();
}

std::string scratch_dir() {
  const char *base = std::getenv("TMPDIR");
  std::string templ = std::string(base && *base ? base : "/tmp") + "/tactix-twin-XXXXXX";
  std::vector<char> path(templ.begin(), templ.end());
  path.push_back('\0');
  assert(mkdtemp(path.data()));
  return path.data();
}

std::vector<std::string> emit_all() {
  const std::string scratch = scratch_dir();
  std::vector<std::string> out;
  for (const Fixture &fixture : twin_fixtures()) out.push_back(emit(fixture, scratch));
  rmdir(scratch.c_str());
  return out;
}

#if defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
std::vector<std::string> run_arm(const char *path) {
  const std::string command = std::string(path) + " --emit";
  FILE *pipe = popen(command.c_str(), "r");
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

// The ratchet's internal entries the fixture streams reach, from the static
// routes (PublicPort bypassed) with a feedback receiver attached.
std::map<std::string, uint64_t> blocking_entries() {
  std::map<std::string, uint64_t> out;
  port::PublicPort::bypass = true;
  for (const Fixture &fixture : twin_fixtures()) {
    FILE *provenance;
    tactix_c_handle *s = open_session(fixture, nullptr, &provenance);
    for (const ActionFeedback &record : run(fixture, api_detail::kernel_for_fixed_testing(s)))
      if (const Entry *entry = classify(record); !entry || internal(entry->route))
        ++out[entry ? entry->id : "unclassified"];
    close_session(s, provenance);
  }
  port::PublicPort::bypass = false;
  return out;
}

int compare(const char *name, const std::vector<std::string> &reference,
            const std::vector<std::string> &actual) {
  int mismatches = 0;
  if (reference.size() != actual.size()) {
    std::cout << name << ": fixture count " << actual.size() << " vs static "
              << reference.size() << '\n';
    return 1;
  }
  for (size_t i = 0; i < reference.size(); ++i)
    if (reference[i] != actual[i]) {
      std::cout << name << " differs:\n  static: " << reference[i]
                << "\n  " << name << ": " << actual[i] << '\n';
      ++mismatches;
    }
  return mismatches;
}
#endif

} // namespace

int main(int argc, char **argv) {
#if !defined(TACTIX_DEFAULT_POLICY_PUBLIC_PORT)
  // Static and hardcoded arms only emit; the public executable compares.
  (void)argc;
  (void)argv;
  for (const std::string &line : emit_all()) std::cout << line << '\n';
  return 0;
#else
  if (argc != 3) {
    std::cerr << "usage: " << argv[0] << " <static arm> <hardcoded arm>\n";
    return 2;
  }
  const auto reference = run_arm(argv[1]);
  const auto hardcoded = run_arm(argv[2]);
  port::PublicPort::census.clear();
  const auto actual = emit_all();
  assert(!reference.empty());
  for (const std::string &line : reference) std::cout << "static " << line << '\n';
  const int hardcoded_mismatches = compare("hardcoded", reference, hardcoded);
  const int public_mismatches = compare("public", reference, actual);

  const auto &census = port::PublicPort::census;
  uint64_t public_calls = 0, fallbacks = 0, declined = 0;
  std::cout << "public requests:";
  for (const auto &[entry, count] : census.public_calls) {
    std::cout << ' ' << entry << '=' << count;
    public_calls += count;
  }
  std::cout << "\nstatic fallbacks (no public request through the port):";
  for (const auto &[kind, count] : census.fallbacks) {
    std::cout << ' ' << kind << '=' << count;
    fallbacks += count;
  }
  std::cout << "\ndeclined public requests (run statically):";
  for (const auto &[kind, count] : census.declined) {
    std::cout << ' ' << kind << '=' << count;
    declined += count;
  }
  std::cout << "\nrefused no-ops (nothing to apply):";
  for (const auto &[kind, count] : census.vacuous) std::cout << ' ' << kind << '=' << count;
  std::cout << "\ndeclared non-actions and compositions (run statically):";
  for (const auto &[kind, count] : census.declared) std::cout << ' ' << kind << '=' << count;
  const auto blocking = blocking_entries();
  std::cout << "\nratchet internal entries reached:";
  for (const auto &[id, count] : blocking) std::cout << ' ' << id << '=' << count;
  std::cout << "\nfixtures=" << reference.size() << " hardcoded-mismatches="
            << hardcoded_mismatches << " public-mismatches=" << public_mismatches
            << " public-requests=" << public_calls << " fallbacks=" << fallbacks
            << " declined=" << declined << " blocking-entries=" << blocking.size()
            << '\n';
  // Identity holds on every route the port reaches publicly and through the
  // hardcoded twin; the issued public requests must be nonvacuous.
  std::cout << std::flush;
  assert(hardcoded_mismatches == 0 && public_mismatches == 0);
  assert(public_calls > 0);
  bool port_ratchet = declined == 0;
  for (const auto &[kind, count] : census.fallbacks)
    if (!EXPECTED_PORT_FALLBACKS.count(kind)) {
      std::cout << "new port fallback: " << kind << '\n';
      port_ratchet = false;
    }
  for (const auto &kind : EXPECTED_PORT_FALLBACKS)
    if (!census.fallbacks.count(kind)) {
      std::cout << "port fallback now public (shrink the list): " << kind << '\n';
      port_ratchet = false;
    }
  std::cout << std::flush;
  assert(port_ratchet);
  if (blocking.empty() && census.fallbacks.empty()) {
    std::cout << "correspondence: COMPLETE\n";
  } else if (blocking.empty()) {
    std::cout << "correspondence: RATCHET COMPLETE (" << census.fallbacks.size()
              << " fallback kinds still run statically through the port)\n";
  } else {
    std::cout << "correspondence: PARTIAL (" << blocking.size()
              << " ratchet entries without a public request)\n";
  }
  return 0;
#endif
}
