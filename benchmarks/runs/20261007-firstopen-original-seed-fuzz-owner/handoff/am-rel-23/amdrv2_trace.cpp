// Owner public C v16 driver for the persistent affine-matrix profile.
// Adapted from the ab7ad3012 campaign's amdrv for 1e8295106: theory service is
// restart-root only, so a SearchStep requested above root must decline with no
// mutation and no rule progress (counted as `search-above-root-declined`);
// `--auto-proof` is accepted and ignored (DSR binary output is unchanged).
//
//   amdrv2 [--auto-proof] --scenario=FILE [--dsr [--no-binary] | --drat [--no-binary] | --lrat | --no-proof] CNF [PROOF]
//
// Scenario keys (one per line): `cause C`, `seed N`, `steps N`, `budget N`,
// `stop N`.  Causes:
//   lifecycle   Initialize, then a seeded walk over decide/Propagate/
//               ConflictAnalysis, SearchStep services above root, Restart
//               (trail retraction), root Substitute/BVE/Sweep/Reduce/Probe,
//               RootMaintenance and Refresh.
//   interrupt   the same walk, with a deterministic stop on random services;
//               an interrupted request must not mutate and a fresh retry runs.
//   budget      Initialize under a tiny basis/snapshot/retained allowance:
//               a decline must not mutate; a fresh full Initialize retries.
//   invalid     malformed v16 programs (zero deferred caps, zero retained
//               bytes, admission 2, operation 0/5, legacy semantics) are
//               rejected by configure with no state change; the valid profile
//               is then accepted.
//   mismatch    SearchStep under a profile that differs from the session:
//               Inapplicable and no mutation; the matching profile services.
//   unsupported Initialize/SearchStep under LRAT or without a proof stream.
// All requests use public C (tactix_online_configure_v16 / advance,
// tactix_decide, tactix_apply, tactix_solve, tactix_value).  Each step is
// overwritten right after configure (deep-copy control).  White-box reads of
// the handle's Kernel (first member) observe the formula image and counters;
// the only white-box write is the deterministic stop flag.  A contract
// violation prints `AMDRV-FAIL` and exits 3.
#undef NDEBUG
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/semantic_extension/affine_matrix_search.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace tactix;

namespace {
[[noreturn]] void fail(const std::string &w) {
  std::fprintf(stderr, "AMDRV-FAIL %s\n", w.c_str());
  std::fflush(stderr);
  std::exit(3);
}
#define CHECK(c, w) do { if (!(c)) fail(w); } while (0)

int rule_id(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (std::string(tactix_rule_name(i)) == name) return i;
  fail(std::string("no rule ") + name);
}

std::string image(const Kernel &k) {
  std::ostringstream s;
  s << k.nvars << ':' << k.next_clause_id << ':' << k.arena_generation << ':' << k.touch_epoch
    << ':' << k.level << ':' << k.inconsistent;
  for (auto w : k.arena) s << ":a" << w;
  for (auto l : k.trail) s << ":l" << l;
  if (k.proof) s << ":p" << k.proof->premise_bytes() << ':' << k.proof->premise_literals();
  return s.str();
}

tactix_online_affine_consequence_parameters basis() {
  tactix_online_affine_consequence_parameters b{};
  b.maximum_clauses = 12000; b.maximum_snapshot_literals = 30000; b.maximum_snapshot_work = 1000000;
  b.maximum_groups = 2500; b.maximum_patterns = 10000; b.maximum_elimination_work = 10000000;
  b.maximum_arity = 8; b.maximum_intermediate_width = 68; b.maximum_sources = 160;
  b.maximum_output_width = 4; b.maximum_proof_additions = 1000000; b.maximum_proof_deletions = 1000000;
  b.maximum_journal_bytes = 10000000; b.consequence = 0;
  b.semantics = TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2;
  b.basis_maximum_work = 10000000; b.basis_maximum_intermediate_width = 68;
  b.basis_maximum_sources = 160; b.basis_maximum_premises = 10000; b.basis_maximum_nodes = 10000;
  b.basis_maximum_direct_width = 3; b.maximum_external_extent = 100000;
  b.maximum_session_keys = 10000; b.maximum_session_work = 1000000;
  return b;
}

tactix_online_affine_matrix_initialize_parameters profile(uint32_t op) {
  return {basis(), TACTIX_AFFINE_MATRIX_AUTHENTICATED_SUBIMAGE, op, 100000000, 10000, 1000000};
}

const char *status_name(int s) {
  static const char *n[] = {"Completed", "ScopedFixpoint", "NoChange", "Inapplicable",
                            "BudgetExhausted", "Interrupted", "Conflict", "Sat", "Unsat",
                            "InvalidRequest", "StaleRequest", "ResourceExhausted", "AccountingError"};
  return s >= 0 && s <= 12 ? n[s] : "?";
}

struct Driver {
  tactix_c_handle *h;
  Kernel &k;
  uint64_t revision = 1000;
  std::map<std::string, uint64_t> tally;
  std::string log;
  bool native_interrupted = false, native_exhausted = false;
  bool declined() const { return native_interrupted || native_exhausted; }
  explicit Driver(tactix_c_handle *handle) : h(handle), k(*reinterpret_cast<Kernel *>(handle)) {}

  // Returns configure's code; on success the advance result is in *out.
  int run(const tactix_online_affine_matrix_initialize_parameters &p, tactix_online_result *out,
          uint32_t profile_kind = TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1, uint32_t stop = 0) {
    tactix_online_step_v16 step{};
    auto &base = step.base.base.base.base.base.base.base.base.base.base.base.base.base.base.base;
    base.step_id = ++revision;
    base.rule_id = rule_id("AffineMatrixInitialize");
    base.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
    base.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
    base.repeat_limit = 1;
    base.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
    base.budget = 1;
    step.affine_matrix_profile = profile_kind;
    step.affine_matrix = p;
    tactix_online_program_v16 program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
    const int code = tactix_online_configure_v16(h, &program, revision, TACTIX_ONLINE_RECONFIGURE_RESET);
    std::memset(&step, 0xA5, sizeof step);  // configuration owns a deep copy
    if (code != 0) return code;
    if (stop) { k.stop_fuzz = stop; k.stop_fuzz_state = 0x9E3779B97F4A7C15ull ^ revision; }
    tactix_online_result r{};
    CHECK(tactix_online_advance(h, &r) == 0, "tactix_online_advance failed");
    k.stop_fuzz = 0;
    *out = r;
    return 0;
  }

  // One persistent request with the universal contracts.
  tactix_online_result request(uint32_t op, const char *label, uint32_t stop = 0,
                               const tactix_online_affine_matrix_initialize_parameters *custom = nullptr) {
    const auto p = custom ? *custom : profile(op);
    const std::string before = image(k);
    const uint64_t interrupted_before = k.stats.affine_matrix_init_interrupted +
                                        k.stats.affine_matrix_service_interrupted;
    const uint64_t exhausted_before = k.stats.affine_matrix_init_exhausted +
                                      k.stats.affine_matrix_service_exhausted;
    tactix_online_result r{};
    CHECK(run(p, &r, TACTIX_AFFINE_MATRIX_PROFILE_PERSISTENT_V1, stop) == 0,
          std::string("configure_v16 rejected a well-formed profile: ") + label);
    native_interrupted = k.stats.affine_matrix_init_interrupted +
                         k.stats.affine_matrix_service_interrupted != interrupted_before;
    native_exhausted = k.stats.affine_matrix_init_exhausted +
                       k.stats.affine_matrix_service_exhausted != exhausted_before;
    // The public status is recorded verbatim next to the native outcome, so
    // the report can compare them (a native decline may surface as NoChange).
    const std::string tag = std::string(label) + ":" + status_name(r.status) +
        (native_interrupted ? "[native-interrupted]" : native_exhausted ? "[native-exhausted]" : "");
    // ab7ad3012 repair: persistent C16 requests surface the native decline.
    if (native_interrupted)
      CHECK(r.status == TACTIX_ONLINE_INTERRUPTED, "native Interrupted surfaced as " + tag);
    else if (native_exhausted)
      CHECK(r.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED, "native ResourceExhausted surfaced as " + tag);
    ++tally[tag];
    fprintf(stderr, "TRACE request op=%u label=%s status=%s progress=%d level=%u trail=%zu inconsistent=%d session=%d\n",
            op, label, status_name(r.status), (int)r.made_rule_progress, (unsigned)k.level, k.trail.size(),
            (int)k.inconsistent, (int)(k.affine_matrix_search_session && k.affine_matrix_search_session->enabled()));
    CHECK(r.status != TACTIX_ONLINE_INVALID_REQUEST, "well-formed persistent request InvalidRequest: " + tag);
    CHECK(r.status != TACTIX_ONLINE_ACCOUNTING_ERROR, "accounting error: " + tag);
    if (!r.made_rule_progress && (r.status == TACTIX_ONLINE_INAPPLICABLE ||
        r.status == TACTIX_ONLINE_INTERRUPTED || r.status == TACTIX_ONLINE_RESOURCE_EXHAUSTED ||
        r.status == TACTIX_ONLINE_STALE_REQUEST || r.status == TACTIX_ONLINE_NO_CHANGE))
      CHECK(image(k) == before, "declined persistent request mutated the formula: " + tag);
    return r;
  }

  bool root() const { return k.level == 0; }
  // Current-trail free original variables (white-box read; tactix_value is
  // model access after SAT only).
  std::vector<int> free_vars(int nvars) const {
    std::vector<int> out;
    for (int v = 1; v <= nvars; ++v) {
      const Lit lit = kernel_internalize(k, v);
      if (lit == INVALID_LIT || kernel_var_retired(k, var_of(lit)) || k.value(lit)) continue;
      out.push_back(v);
    }
    return out;
  }
  bool done() const { return k.inconsistent || tactix_status(h) == 20 || tactix_status(h) == 10; }
};

}  // namespace

int main(int argc, char **argv) {
  const char *scenario = nullptr, *cnf = nullptr, *proof = nullptr;
  int format = TACTIX_PROOF_DSR_BINARY;
  bool ascii = false, no_proof = false;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--auto-proof")) continue;
    if (!std::strncmp(argv[i], "--scenario=", 11)) scenario = argv[i] + 11;
    else if (!std::strcmp(argv[i], "--dsr")) format = TACTIX_PROOF_DSR_BINARY;
    else if (!std::strcmp(argv[i], "--drat")) format = TACTIX_PROOF_DRAT_BINARY;
    else if (!std::strcmp(argv[i], "--lrat")) format = TACTIX_PROOF_LRAT;
    else if (!std::strcmp(argv[i], "--no-binary")) ascii = true;
    else if (!std::strcmp(argv[i], "--no-proof")) no_proof = true;
    else if (!cnf) cnf = argv[i];
    else proof = argv[i];
  }
  if (ascii && format == TACTIX_PROOF_DSR_BINARY) format = TACTIX_PROOF_DSR_ASCII;
  if (ascii && format == TACTIX_PROOF_DRAT_BINARY) format = TACTIX_PROOF_DRAT_ASCII;
  CHECK(scenario && cnf, "usage");
  std::ifstream sin(scenario);
  std::string key, cause = "lifecycle";
  uint64_t seed = 1, steps = 40, budget = 3, stop = 4;
  while (sin >> key) {
    if (key == "cause") sin >> cause;
    else if (key == "seed") sin >> seed;
    else if (key == "steps") sin >> steps;
    else if (key == "budget") sin >> budget;
    else if (key == "stop") sin >> stop;
    else fail("unknown key " + key);
  }
  std::mt19937_64 rng(seed);
  tactix_c_handle *h = tactix_init();
  FILE *records = std::tmpfile();
  CHECK(h && records, "init");
  if (proof && !no_proof) CHECK(tactix_proof(h, proof, format) == 0, "tactix_proof");
  int nvars = 0;
  {
    std::ifstream in(cnf);
    std::string t;
    while (in >> t) {
      if (t == "p") { std::string f; int m; in >> f >> nvars >> m; continue; }
      if (t == "c") { std::getline(in, t); continue; }
      tactix_add(h, std::stoi(t));
    }
  }
  CHECK(tactix_bind_structural_provenance(h, records) == 0, "bind provenance");
  Driver d(h);
  Kernel &k = d.k;
  if (d.done()) {
    std::printf("c amdrv-skip input decided before any request\ns UNKNOWN\n");
    return 0;
  }
  const int propagate = rule_id("Propagate"), analyze = rule_id("ConflictAnalysis"),
            restart = rule_id("Restart");
  const int root_rules[] = {rule_id("Substitute"), rule_id("BVE"), rule_id("Sweep"),
                            rule_id("Reduce"), rule_id("Probe")};
  const char *root_names[] = {"Substitute", "BVE", "Sweep", "Reduce", "Probe"};
  std::string summary = "cause=" + cause;
  uint64_t retractions = 0, namespaces = 0, retired = 0, searched = 0, consumed_reasons = 0;

  if (cause == "invalid") {
    struct Bad { const char *name; tactix_online_affine_matrix_initialize_parameters p; uint32_t kind; };
    std::vector<Bad> bad;
    auto p = profile(1); p.maximum_deferred_deletions = 0; bad.push_back({"deferred-deletions", p, 1});
    p = profile(1); p.maximum_deferred_literal_work = 0; bad.push_back({"deferred-work", p, 1});
    p = profile(1); p.maximum_retained_bytes = 0; bad.push_back({"retained", p, 1});
    p = profile(1); p.admission = 2; bad.push_back({"admission", p, 1});
    p = profile(1); p.operation = 0; bad.push_back({"operation0", p, 1});
    p = profile(1); p.operation = 5; bad.push_back({"operation5", p, 1});
    p = profile(1); p.basis.semantics = 0; bad.push_back({"semantics", p, 1});
    p = profile(1); bad.push_back({"profile-kind", p, 7});
    for (const auto &b : bad) {
      const std::string before = image(k);
      tactix_online_result r{};
      const int code = d.run(b.p, &r, b.kind);
      ++d.tally[std::string("invalid-") + b.name + (code ? ":rejected" : ":accepted")];
      CHECK(code == -1, std::string("malformed v16 profile accepted: ") + b.name);
      CHECK(image(k) == before, std::string("rejected v16 profile mutated: ") + b.name);
    }
    d.request(1, "init-after-invalid");
  } else if (cause == "unsupported") {
    d.request(1, "init-unsupported");
    if (!proof || no_proof)
      CHECK(d.tally.count("init-unsupported:Inapplicable"), "proof-free persistent Initialize applied");
    if (!d.done()) d.request(4, "search-unsupported");
  } else if (cause == "budget") {
    auto p = profile(1);
    switch (rng() % 4) {
      case 0: p.basis.basis_maximum_work = budget; summary += " cap=basis"; break;
      case 1: p.basis.maximum_snapshot_work = budget; summary += " cap=snapshot"; break;
      case 2: p.maximum_retained_bytes = budget; summary += " cap=retained"; break;
      default: p.basis.maximum_clauses = 1 + budget % 4; summary += " cap=clauses"; break;
    }
    const auto first = d.request(1, "init-capped", 0, &p);
    summary += std::string(" first=") + status_name(first.status);
    if (first.status != TACTIX_ONLINE_COMPLETED || d.declined()) {
      const auto retry = d.request(1, "init-retry");
      summary += std::string(" retry=") + status_name(retry.status);
    }
  } else if (cause == "mismatch") {
    const auto init = d.request(1, "init");
    if (init.status == TACTIX_ONLINE_COMPLETED && !d.done()) {
      for (int i = 0; i < 8 && !d.done(); ++i) {
        const int pre = tactix_apply(h, propagate);
        if (pre == 2) { tactix_apply(h, analyze); continue; }
        if (pre == 20 || pre == 10) break;
        const std::vector<int> free = d.free_vars(nvars);
        if (free.empty()) break;
        const int v = free[rng() % free.size()];
        if (tactix_decide(h, (rng() & 1) ? v : -v) != 0) { ++d.tally["decide-declined"]; break; }
        if (tactix_apply(h, propagate) == 2) { tactix_apply(h, analyze); continue; }
        auto other = profile(4); ++other.maximum_retained_bytes;
        const auto mis = d.request(4, "search-mismatch", 0, &other);
        CHECK(mis.status != TACTIX_ONLINE_COMPLETED || !mis.made_rule_progress,
              "SearchStep serviced a session under a different profile");
        const bool above_root = k.level != 0;
        const std::string pre_image = image(k);
        const auto ok = d.request(4, "search-match");
        if (above_root) {
          CHECK(!ok.made_rule_progress && image(k) == pre_image,
                std::string("SearchStep above root serviced: ") + status_name(ok.status));
          ++d.tally["search-above-root-declined"];
        }
        if (ok.made_rule_progress && tactix_apply(h, propagate) == 2) tactix_apply(h, analyze);
      }
    }
  } else {
    // lifecycle / interrupt
    const bool interrupt = cause == "interrupt";
    const auto init = d.request(1, "init", interrupt && rng() % 2 ? uint32_t(stop) : 0);
    if (init.status == TACTIX_ONLINE_INTERRUPTED || d.declined()) d.request(1, "init-retry");
    for (uint64_t s = 0; s < steps && !d.done(); ++s) {
      const uint32_t st = interrupt && rng() % 3 == 0 ? uint32_t(stop) : 0;
      const uint64_t move = rng() % 10;
      if (move <= 4) {
        // decide and propagate, then a SearchStep at the new fixpoint
        int pr = tactix_apply(h, propagate);
        if (pr == 2) { if (tactix_apply(h, analyze) == 20) break; continue; }
        if (pr == 20 || pr == 10) break;
        const std::vector<int> free = d.free_vars(nvars);
        if (free.empty()) break;
        const int v = free[rng() % free.size()];
        if (tactix_decide(h, (rng() & 1) ? v : -v) != 0) { ++d.tally["decide-declined"]; continue; }
        pr = tactix_apply(h, propagate);
        if (pr == 2) { if (tactix_apply(h, analyze) == 20) break; continue; }
        if (pr == 20 || pr == 10) break;
        const bool above_root = k.level != 0;
        const std::string pre_image = image(k);
        auto r = d.request(4, above_root ? "search" : "search-root", st);
        ++searched;
        if (above_root) {
          CHECK(!r.made_rule_progress && image(k) == pre_image,
                std::string("SearchStep above root serviced: ") + status_name(r.status));
          ++d.tally["search-above-root-declined"];
        }
        if (r.status == TACTIX_ONLINE_INTERRUPTED || d.native_interrupted) r = d.request(4, "search-retry");
        if (r.made_rule_progress) {
          pr = tactix_apply(h, propagate);
          if (pr == 2) { ++consumed_reasons; if (tactix_apply(h, analyze) == 20) break; }
          if (pr == 20 || pr == 10) break;
        }
      } else if (move <= 6) {
        if (!k.level) continue;
        const uint64_t before = k.affine_trail_retraction_epoch;
        const int r = tactix_apply(h, restart);
        retractions += k.affine_trail_retraction_epoch != before;
        if (r == 20) break;
        if (tactix_apply(h, propagate) == 20) break;
      } else {
        if (k.level) {
          const uint64_t before = k.affine_trail_retraction_epoch;
          if (tactix_apply(h, restart) == 20) break;
          retractions += k.affine_trail_retraction_epoch != before;
          if (tactix_apply(h, propagate) == 20) break;
        }
        const size_t which = rng() % 5;
        const uint64_t ns = k.affine_semantic_namespace_epoch;
        uint64_t retired_before = 0;
        for (Var v = 0; v < k.nvars; ++v) retired_before += kernel_var_retired(k, v);
        const int r = tactix_apply(h, root_rules[which]);
        fprintf(stderr, "TRACE root-rule %s -> %d\n", root_names[which], r);
        ++d.tally[std::string("root-") + root_names[which] + ":" + std::to_string(r)];
        uint64_t retired_after = 0;
        for (Var v = 0; v < k.nvars; ++v) retired_after += kernel_var_retired(k, v);
        retired += retired_after - retired_before;
        namespaces += k.affine_semantic_namespace_epoch != ns;
        if (r == 20 || r == 10 || d.done()) break;
        if (tactix_apply(h, propagate) == 20) break;
        const uint32_t op = rng() % 2 ? 3 : 2;
        const auto m = d.request(op, op == 3 ? "rootmaint" : "refresh", st);
        if (m.status == TACTIX_ONLINE_INTERRUPTED || d.native_interrupted)
          d.request(op, op == 3 ? "rootmaint-retry" : "refresh-retry");
        if (m.made_rule_progress && tactix_apply(h, propagate) == 20) break;
      }
    }
  }
  const auto *session = k.affine_matrix_search_session;
  summary += " session=" + std::to_string(session && session->enabled()) +
             " persistent=" + std::to_string(session && session->enabled() && session->persistent()) +
             " keys=" + std::to_string(session ? session->published_keys() : 0) +
             " searched=" + std::to_string(searched) + " retractions=" + std::to_string(retractions) +
             " namespaces=" + std::to_string(namespaces) + " retired=" + std::to_string(retired) +
             " consumed=" + std::to_string(consumed_reasons);
  const auto &st = k.stats;
  summary += " init_attempts=" + std::to_string(st.affine_matrix_init_attempts) +
             " init_applied=" + std::to_string(st.affine_matrix_init_applied) +
             " init_partial=" + std::to_string(st.affine_matrix_init_applied_partial) +
             " init_exhausted=" + std::to_string(st.affine_matrix_init_exhausted) +
             " init_interrupted=" + std::to_string(st.affine_matrix_init_interrupted) +
             " retention_work=" + std::to_string(st.affine_matrix_init_premise_retention_work) +
             " root_unit_work=" + std::to_string(st.affine_matrix_init_root_unit_work) +
             " alias_work=" + std::to_string(st.affine_matrix_alias_work) +
             " alias_additions=" + std::to_string(st.affine_matrix_alias_proof_additions) +
             " svc_calls=" + std::to_string(st.affine_matrix_service_calls) +
             " svc_applied=" + std::to_string(st.affine_matrix_service_applied) +
             " svc_no_event=" + std::to_string(st.affine_matrix_service_no_event) +
             " svc_lifecycle_work=" + std::to_string(st.affine_matrix_service_lifecycle_work) +
             " svc_additions=" + std::to_string(st.affine_matrix_service_proof_additions) +
             " stop_fires=" + std::to_string(st.stop_fuzz_fires);
  if (k.proof)
    summary += " premise_bytes=" + std::to_string(k.proof->premise_bytes()) +
               " deferred=" + std::to_string(k.proof->premise_deletions_deferred());
  for (const auto &[tag, n] : d.tally) summary += " " + tag + "=" + std::to_string(n);
  std::printf("c amdrv %s\n", summary.c_str());
  if (no_proof || !proof) {
    // A proof-free run is a decline-contract row only; it certifies nothing.
    std::printf("c amdrv-skip proof-free contract row\ns UNKNOWN\n");
    tactix_release(h);
    std::fclose(records);
    return 0;
  }
  if (k.level && !k.inconsistent) tactix_apply(h, restart);
  const int res = tactix_solve(h);
  if (res == 10) {
    std::printf("s SATISFIABLE\n");
    std::string line = "v";
    for (int v = 1; v <= nvars; ++v) {
      line += ' ' + std::to_string(tactix_value(h, v) > 0 ? v : -v);
      if (line.size() > 3000) { std::printf("%s\n", line.c_str()); line = "v"; }
    }
    std::printf("%s 0\n", line.c_str());
  } else std::printf(res == 20 ? "s UNSATISFIABLE\n" : "s UNKNOWN\n");
  tactix_release(h);
  std::fclose(records);
  return res == 10 ? 10 : res == 20 ? 20 : 0;
}
