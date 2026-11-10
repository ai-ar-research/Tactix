#pragma once

// The DefaultPolicy correspondence fixtures and their observable signature
// (plan §5.6 acceptance, §8.4 correspondence gate): verdict, model,
// certificate bytes and trajectory counters of one fixture solved on a fresh
// C session. Declared counters (rule timing and calls, api_decisions, the
// numeric strategy epoch) are excluded. Shared by the static/hardcoded/public
// port twin and the C strategy's correspondence test.
#include "default_action_map.hpp"
#include "api/api_internal.hpp"
#include "rules/semantic_extension/affine_image.hpp"

#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <unistd.h>

namespace correspondence {

using namespace action_map;

// Affine fixtures (structural provenance bound, DSR proof): the startup root
// closure refuting an odd K4 Tseitin system, and the persistent affine search
// (initialization and startup service) on a satisfiable XOR system.
inline bool affine(const Fixture &fixture) { return starts(fixture.name, "affine-"); }

inline void without_startup_models(DefaultStrategyConfig &c) {
  for (bool *enabled : {&c.affine_model.enabled, &c.parity_model.enabled,
           &c.onehot_model.enabled, &c.ramsey_startup.enabled,
           &c.signed_cardinality_proof.enabled, &c.balanced_signature_model.enabled,
           &c.exact_cover_model.enabled, &c.independent_set_model.enabled,
           &c.fermat_model.enabled, &c.cerny_model.enabled, &c.lookup_circuit_model.enabled,
           &c.simon_model.enabled, &c.stconn_proof.enabled})
    *enabled = false;
}

inline std::vector<std::vector<int>> xors(
    const std::vector<std::pair<std::vector<int32_t>, bool>> &rows) {
  std::vector<std::vector<int>> out;
  for (const auto &[variables, rhs] : rows)
    for (const auto &clause : affine_semantic_extension::clausal_form(variables, rhs))
      out.emplace_back(clause.begin(), clause.end());
  return out;
}

inline std::vector<Fixture> fixtures() {
  std::vector<Fixture> out;
  for (Fixture &fixture : action_map::fixtures())
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

// A fixture's strategy configuration. The structured-root SR program
// dispatches its steps internally; the correspondence covers the rest of the
// schedule.
inline DefaultStrategyConfig configuration(const Fixture &fixture, Kernel &k) {
  DefaultStrategyConfig config;
  config.inprocess.structured_root_stages.clear();
  config.inprocess.structured_root_bounded_backtracking_stages.clear();
  if (fixture.configure) fixture.configure(config, k);
  return config;
}

// A loaded C session; an affine fixture also binds a provenance stream.
inline tactix_c_handle *open_session(const Fixture &fixture, const std::string *proof_path,
                                     FILE **provenance) {
  tactix_c_handle *s = tactix_init();
  if (fixture.deferred) assert(tactix_defer_startup_state(s) == 0);
  assert(tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) == 0);
  if (proof_path)
    assert(tactix_proof(s, proof_path->c_str(), TACTIX_PROOF_DSR_ASCII) == 0);
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
  if (fixture.session) fixture.session(s);
  return s;
}

inline void close_session(tactix_c_handle *s, FILE *provenance) {
  tactix_release(s);
  if (provenance) std::fclose(provenance);
}

inline uint64_t fnv(const void *data, size_t size, uint64_t h = 1469598103934665603ull) {
  const auto *p = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

inline std::string hex(uint64_t value) {
  char out[17];
  std::snprintf(out, sizeof out, "%016" PRIx64, value);
  return out;
}

inline int max_variable(const Fixture &fixture) {
  int out = 0;
  for (const auto &clause : fixture.dimacs)
    for (int lit : clause) out = std::max(out, lit < 0 ? -lit : lit);
  return out;
}

inline uint64_t trail_hash(const Kernel &k) {
  uint64_t h = fnv(nullptr, 0);
  for (Lit lit : k.trail) h = fnv(&lit, sizeof lit, h);
  return h;
}

// Solves `fixture` on a fresh C session with `solve(session, configuration)`
// and returns its observable signature.
using Solver = std::function<int(tactix_c_handle *, const DefaultStrategyConfig &)>;

inline std::string emit(const Fixture &fixture, const std::string &scratch, const Solver &solve) {
  const std::string proof_path = scratch + "/" + fixture.name + ".proof";
  FILE *provenance;
  tactix_c_handle *s =
      open_session(fixture, fixture.clausal_proof ? &proof_path : nullptr, &provenance);
  Kernel &k = api_detail::kernel_for_fixed_testing(s);
  const int verdict = solve(s, configuration(fixture, k));
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
  // A deferred startup's own H leaves with DefaultPolicy's solve; a client's
  // session keeps the one it attached.
  if (k.h && !fixture.deferred) {
    // The decision structures' exact layout (VMTF links and queue ends, the
    // score heap's array), which removal order changes.
    uint64_t layout = fnv(k.h->scores.stack.data(), k.h->scores.stack.size() * sizeof(uint32_t));
    layout = fnv(k.h->links.data(), k.h->links.size() * sizeof(k.h->links[0]), layout);
    layout = fnv(&k.h->queue_first, sizeof k.h->queue_first, layout);
    layout = fnv(&k.h->queue_last, sizeof k.h->queue_last, layout);
    out << " layout=" << hex(layout);
    out << " saved=" << hex(fnv(k.h->saved.data(), k.h->saved.size()))
        << " scores=" << hex(fnv(k.h->scores.score.data(),
                                 k.h->scores.score.size() * sizeof(double)));
  }
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

inline std::string scratch_dir() {
  const char *base = std::getenv("TMPDIR");
  std::string templ = std::string(base && *base ? base : "/tmp") + "/tactix-twin-XXXXXX";
  std::vector<char> path(templ.begin(), templ.end());
  path.push_back('\0');
  assert(mkdtemp(path.data()));
  return path.data();
}

} // namespace correspondence
