// Native ConflictAnalysis requests versus the public ANALYZE_TARGET_CHRONO
// instance (gap plan Step 6). Twin kernels load the same random CNF and make
// the same decisions; at every conflict one runs DefaultPolicy's native
// request (per-conflict bump payload by value, chronological target after
// learning, retier schedule carried and returned), the other installs the
// same payload and applies the public semantic instance through registry
// dispatch. Trail, clause arena, analysis counters, posed tiers, retier
// schedule and proof bytes must stay identical, in DRAT and LRAT.
#undef NDEBUG
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include "rules/registry.hpp"
#include "strategy/action_execution.hpp"
#include "strategy/analyze_parameters.hpp"
#include "strategy/conflict_analysis_execution.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace tactix;

namespace {

std::string read_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  // Copy through rdbuf: GCC 13.1's string(InputIt) trips UBSan (PR 109703).
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

std::string random_cnf(std::mt19937_64 &rng, uint32_t vars, uint32_t clauses) {
  std::ostringstream out;
  out << "p cnf " << vars << ' ' << clauses << '\n';
  for (uint32_t c = 0; c < clauses; ++c) {
    const uint32_t width = 3 + (rng() % 4 == 0); // some 4-clauses
    for (uint32_t i = 0; i < width; ++i)
      out << (rng() & 1 ? "" : "-") << 1 + rng() % vars << ' ';
    out << "0\n";
  }
  return out.str();
}

void load(Kernel &k, const std::string &cnf) {
  FILE *input = std::tmpfile();
  assert(input);
  std::fputs(cnf.c_str(), input);
  std::rewind(input);
  assert(!parse_dimacs(input, k));
  std::fclose(input);
}

// Ordered state that analysis, backjump and bumping touch, without timers.
std::string snapshot(const Kernel &k) {
  std::ostringstream out;
  out << k.level << ':' << k.propagate_head << ':' << k.inconsistent << ':'
      << k.conflict_valid << ':' << k.pending.valid << ':' << k.next_clause_id
      << ':' << k.tier1 << ':' << k.tier2;
  for (Lit lit : k.trail) out << ":t" << lit;
  for (Var v = 0; v < k.nvars; ++v) {
    const auto &a = k.assigned[v];
    out << ":v" << int(k.values[make_lit(v, false)]) << ':' << a.level << ':'
        << a.reason << ':' << a.binary;
  }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    out << ":c" << c->id() << ':' << c->garbage() << ':' << c->redundant()
        << ':' << c->glue();
    for (Lit lit : *c) out << ':' << lit;
    ref += clause_actual_words(c);
  }
  const auto &s = k.stats;
  out << ":s" << s.conflicts << ':' << s.learned << ':' << s.learned_units << ':'
      << s.conflict_reused << ':' << s.chronological << ':' << s.retiered << ':'
      << s.otfs_strengthened << ':' << s.reason_bump_rounds;
  for (auto score : k.h->scores.score) out << ":h" << score;
  return out.str();
}

struct Reach {
  uint64_t conflicts = 0, chronological = 0, reused = 0, retiered = 0,
           otfs = 0, bump_rounds = 0, learned = 0, unsat = 0;
};

void run(uint64_t seed, ProofFormat format, Reach &reach) {
  std::mt19937_64 rng(seed);
  // Around and above the 3-SAT threshold: both verdicts occur.
  const uint32_t vars = 30 + rng() % 40;
  const std::string cnf = random_cnf(rng, vars, vars * (42 + rng() % 20) / 10);
  const std::string stem = "/tmp/tactix-analysis-target-" +
                           std::to_string(getpid()) + "-" + std::to_string(seed);
  const std::string native_path = stem + "-native", public_path = stem + "-public";
  {
    Kernel native, published;
    Proof native_proof, public_proof;
    assert(native_proof.open(native_path.c_str(), format));
    assert(public_proof.open(public_path.c_str(), format));
    native.proof = &native_proof;
    published.proof = &public_proof;
    native_proof.bind_variable_map(&native.internal_to_external);
    public_proof.bind_variable_map(&published.internal_to_external);
    load(native, cnf);
    load(published, cnf);
    ScopedHeuristics native_h(native), public_h(published);
    // Registry dispatch closes the startup certificate; close both alike.
    execution::execute_action(native, execution::CloseStartupCertificateRequest{});
    execution::execute_action(published, execution::CloseStartupCertificateRequest{});
    const bool stable = rng() & 1;
    const bool reuse = rng() % 4 != 0;
    const uint32_t chrono_choices[] = {0, 1, 2, 100};
    const uint32_t chrono = chrono_choices[rng() % 4];
    for (Kernel *k : {&native, &published}) {
      k->h->stable = stable;
      strategy::AnalyzeParameters profile = strategy::AnalyzeParameters::from_kernel(*k);
      profile.conflict_reuse = reuse;
      profile.chronological_levels = chrono;
      strategy::install_rule_parameters(*k, profile);
    }
    // The native side owns its schedule; the public side installs the same.
    RetierSchedule schedule;
    schedule.enabled = rng() % 4 != 0;
    schedule.interval = 1 + rng() % 3;
    schedule.next_conflicts = schedule.interval;
    schedule.max_interval = 64;
    schedule.growth = 2;
    published.retier_schedule = schedule;
    const RuleVTable &analysis = tactix_rules().at(tactix_rule_id("ConflictAnalysis"));
    assert(analysis.apply_semantic_instance);
    for (uint32_t step = 0; step < 20000 && native.stats.conflicts < 1500; ++step) {
      const ApplyResult propagated = propagate_apply(native);
      assert(propagate_apply(published) == propagated);
      if (propagated == ApplyResult::Unsat) break;
      if (propagated == ApplyResult::Conflict) {
        execution::NativeConflictAnalysisRequest request;
        const uint32_t limits[] = {0, 0, 2, 10};
        request.bump_reason_limit = limits[rng() % 4];
        request.bump_reason_depth = 1 + rng() % 2;
        request.target_mode = AnalysisTarget::Chrono;
        request.retier_schedule = schedule;
        const auto result = execution::execute_action(
            native, request, [](auto &&apply) { return apply(); },
            [](ApplyResult) -> uint32_t { return UINT32_MAX; });
        schedule = result.retier_schedule;
        assert(native.bump_reason_limit == 0 && native.bump_reason_depth == 1);

        strategy::AnalyzeParameters payload = strategy::AnalyzeParameters::from_kernel(published);
        payload.bump_reason_limit = request.bump_reason_limit;
        payload.bump_reason_depth = request.bump_reason_depth;
        strategy::install_rule_parameters(published, payload);
        const ApplyResult applied = apply_rule_semantic_instance(
            analysis, published, ANALYZE_TARGET_CHRONO);
        payload.bump_reason_limit = 0;
        payload.bump_reason_depth = 1;
        strategy::install_rule_parameters(published, payload);

        assert(applied == result.result);
        assert(published.retier_schedule == schedule);
        if (snapshot(native) != snapshot(published)) {
          std::fprintf(stderr, "seed=%llu format=%d diverged at conflict %llu\n",
                       static_cast<unsigned long long>(seed), static_cast<int>(format),
                       static_cast<unsigned long long>(native.stats.conflicts));
          assert(false);
        }
        if (result.result == ApplyResult::Unsat) {
          ++reach.unsat;
          break;
        }
        continue;
      }
      if (native.unassigned_active == 0) break;
      Var pick = 0;
      for (uint32_t probe = rng() % native.nvars;; probe = (probe + 1) % native.nvars)
        if (!native.values[make_lit(probe, false)] && !kernel_var_retired(native, probe)) {
          pick = probe;
          break;
        }
      const Lit decision = make_lit(pick, rng() & 1);
      kernel_assign_decision(native, decision);
      kernel_assign_decision(published, decision);
    }
    assert(snapshot(native) == snapshot(published));
    reach.conflicts += native.stats.conflicts;
    reach.chronological += native.stats.chronological;
    reach.reused += native.stats.conflict_reused;
    reach.retiered += native.stats.retiered;
    reach.otfs += native.stats.otfs_strengthened;
    reach.bump_rounds += native.stats.reason_bump_rounds;
    reach.learned += native.stats.learned;
    native_proof.flush();
    public_proof.flush();
  }
  assert(read_file(native_path) == read_file(public_path));
  assert(std::remove(native_path.c_str()) == 0);
  assert(std::remove(public_path.c_str()) == 0);
}

} // namespace

int main() {
  Reach reach;
  uint32_t runs = 0;
  for (uint64_t seed = 1; seed <= 40; ++seed)
    for (ProofFormat format : {ProofFormat::DratAscii, ProofFormat::Lrat}) {
      run(seed, format, reach);
      ++runs;
    }
  std::printf("PASS: %u twin runs, %llu conflicts, %llu learned, %llu chronological, "
              "%llu reused, %llu retiers, %llu OTFS, %llu reason-bump rounds, "
              "%llu refutations\n",
              runs, static_cast<unsigned long long>(reach.conflicts),
              static_cast<unsigned long long>(reach.learned),
              static_cast<unsigned long long>(reach.chronological),
              static_cast<unsigned long long>(reach.reused),
              static_cast<unsigned long long>(reach.retiered),
              static_cast<unsigned long long>(reach.otfs),
              static_cast<unsigned long long>(reach.bump_rounds),
              static_cast<unsigned long long>(reach.unsat));
  std::fflush(stdout);
  // Every compared path must be reached, or the equality is vacuous.
  assert(reach.chronological && reach.reused && reach.retiered && reach.otfs &&
         reach.bump_rounds && reach.unsat);
  return 0;
}
