// Competition frontend (plan §6, compete skill): DIMACS in (plain or
// .xz/.gz via decompressor pipe), `s`/`v` lines on stdout, exit codes
// 10/20/other, proof to argv[2], SIGTERM/SIGINT/SIGXCPU -> flush proof,
// print `s UNKNOWN`, exit cleanly.
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "kernel/kernel.hpp"
#include "policies/adversarial_policy.hpp"
#include "policies/bve_stress_policy.hpp"
#include "policies/congruence_stress_policy.hpp"
#include "policies/factor_stress_policy.hpp"
#include "policies/probing_stress_policy.hpp"
#include "policies/walk_stress_policy.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_policy.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp" // BveGate enum for --bve-gates
#include "rules/registry.hpp" // now_nanos + the rule list for `c timing`

#include <algorithm>
#include <utility>
#include <vector>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace tactix;

static volatile sig_atomic_t caught_signal = 0;
static void handle_signal(int) { caught_signal = 1; }

static FILE *open_input(const char *path, bool *piped) {
  const size_t n = strlen(path);
  const char *decompressor = nullptr;
  if (n > 3 && !strcmp(path + n - 3, ".xz"))
    decompressor = "xz -dc";
  else if (n > 3 && !strcmp(path + n - 3, ".gz"))
    decompressor = "gzip -dc";
  if (decompressor) {
    // path is single-quoted for the shell; embedded quotes escaped
    std::string cmd(decompressor);
    cmd += " '";
    for (const char *p = path; *p; p++)
      if (*p == '\'')
        cmd += "'\\''";
      else
        cmd += *p;
    cmd += "'";
    *piped = true;
    return popen(cmd.c_str(), "r");
  }
  *piped = false;
  return fopen(path, "r");
}

static void print_model(const Kernel &k) {
  // v lines <= 4096 chars, each terminated by 0 on the last line
  std::string line = "v";
  auto flush_line = [&] {
    fputs(line.c_str(), stdout);
    fputc('\n', stdout);
    line = "v";
  };
  for (Var v = 0; v < kernel_num_output_vars(k); v++) { // extension vars projected out
    const Lit pos = make_lit(v, false);
    const int e = static_cast<int>(v) + 1;
    char buf[16];
    snprintf(buf, sizeof buf, " %d", k.values[pos] >= 0 ? e : -e);
    if (line.size() + strlen(buf) > 4094)
      flush_line();
    line += buf;
  }
  if (line.size() + 2 > 4094)
    flush_line();
  line += " 0";
  flush_line();
}

int main(int argc, char **argv) {
  const char *cnf_path = nullptr;
  const char *proof_path = nullptr;
  ProofFormat proof_format = ProofFormat::DratBinary;
  bool quiet = false;
  bool random_policy = false;
  bool adversarial_policy = false;
  bool bve_stress_policy = false;
  bool congruence_stress_policy = false;
  bool factor_stress_policy = false;
  bool probing_stress_policy = false;
  bool walk_stress_policy = false;
  bool no_heuristics = false; // run with H detached (unguided; Track H · H5)
  uint32_t bve_gate_mask = ~0u; // BVE gate-type selection (E1d); all gates on
  unsigned long long random_seed = 20260710;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--version")) {
      puts("0.1.0");
      return 0;
    } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
      quiet = true;
    } else if (!strcmp(a, "--no-binary")) {
      proof_format = ProofFormat::DratAscii;
    } else if (!strcmp(a, "--lrat")) {
      proof_format = ProofFormat::Lrat; // native LRAT (plan O4)
    } else if (!strncmp(a, "--random-policy", 15)) {
      // soundness testing mode (plan §8.2 rule-order fuzzing): random
      // legal rule schedule, random branching instances, proofs intact
      random_policy = true;
      if (a[15] == '=')
        random_seed = strtoull(a + 16, nullptr, 10);
    } else if (!strncmp(a, "--adversarial-policy", 20)) {
      // D-LEVEL-1 stress mode: aggressively decides off a propagation
      // fixpoint (the N1 schedule DefaultPolicy never generates)
      adversarial_policy = true;
      if (a[20] == '=')
        random_seed = strtoull(a + 21, nullptr, 10);
    } else if (!strncmp(a, "--bve-stress-policy", 19)) {
      // E1c stress mode: maximizes BVE at the root (gate detection +
      // resolution + reconstruction), the N1 schedule the uniform random
      // policy under-samples (stress-policy skill)
      bve_stress_policy = true;
      if (a[19] == '=')
        random_seed = strtoull(a + 20, nullptr, 10);
    } else if (!strncmp(a, "--probing-stress-policy", 23)) {
      // S3 stress mode: hammers P2's kernel_probing snapshot/restore scope —
      // the probing family in random order at every root, densely interleaved
      // with search that reuses the restored watch lists (stress-policy skill)
      probing_stress_policy = true;
      if (a[23] == '=')
        random_seed = strtoull(a + 24, nullptr, 10);
    } else if (!strncmp(a, "--walk-stress-policy", 20)) {
      // W4 stress mode: S6 Walk at random decision levels, off propagation
      // fixpoints, with tiny/huge budgets and mode flips — the walker is legal
      // anywhere, and DefaultPolicy only ever walks at a quiescent stable root
      // (stress-policy skill)
      walk_stress_policy = true;
      if (a[20] == '=')
        random_seed = strtoull(a + 21, nullptr, 10);
    } else if (!strncmp(a, "--factor-stress-policy", 22)) {
      // S2a Increment-2 stress mode: adversarial FactorMatchPolicy (hash /
      // all-tied / reversed scores, maximizing accept) + Factor maximized at
      // the root — the grid-selection schedules DefaultFactorMatch never
      // steers into (stress-policy skill)
      factor_stress_policy = true;
      if (a[22] == '=')
        random_seed = strtoull(a + 23, nullptr, 10);
    } else if (!strncmp(a, "--congruence-stress-policy", 26)) {
      // C3 stress mode: maximizes V6's newly-legal schedules -- deriving root
      // units, refuting, and re-entering congruence off a fixpoint on the
      // formula its own previous round changed (stress-policy skill)
      congruence_stress_policy = true;
      if (a[26] == '=')
        random_seed = strtoull(a + 27, nullptr, 10);
    } else if (!strcmp(a, "--no-heuristics")) {
      // Track H · H5: detach `H` (an unguided solver — `k.h == nullptr`). Sound
      // but slow; DefaultPolicy needs `H`, so pair with --random-policy (which
      // drives the solve purely through the null-safe rule API).
      no_heuristics = true;
    } else if (!strncmp(a, "--bve-gates=", 12)) {
      // BVE gate-type selection (E1d): comma list of {equiv,andor}, or
      // all / none. A performance knob — every setting is sound (a disabled
      // gate type only falls BVE back to the full cross product).
      const char *spec = a + 12;
      if (!strcmp(spec, "all")) {
        bve_gate_mask = ~0u;
      } else if (!strcmp(spec, "none")) {
        bve_gate_mask = 0u;
      } else {
        bve_gate_mask = 0u;
        for (const char *p = spec; *p;) {
          if (!strncmp(p, "equiv", 5)) {
            bve_gate_mask |= BVE_GATE_EQUIV;
            p += 5;
          } else if (!strncmp(p, "andor", 5)) {
            bve_gate_mask |= BVE_GATE_AND_OR;
            p += 5;
          } else if (*p == ',') {
            p++;
          } else {
            fprintf(stderr, "tactix: bad --bve-gates spec '%s'\n", spec);
            printf("s UNKNOWN\n");
            return 1;
          }
        }
      }
    } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      printf("usage: tactix [-q] [--no-binary] [--lrat] "
             "[--random-policy[=SEED]] [--bve-gates=all|none|equiv,andor] "
             "<cnf[.xz|.gz]> [proof]\n");
      return 0;
    } else if (a[0] == '-' && a[1]) {
      fprintf(stderr, "tactix: unknown option '%s'\n", a);
      printf("s UNKNOWN\n");
      return 1;
    } else if (!cnf_path) {
      cnf_path = a;
    } else if (!proof_path) {
      proof_path = a;
    } else {
      fprintf(stderr, "tactix: too many arguments\n");
      printf("s UNKNOWN\n");
      return 1;
    }
  }
  if (!cnf_path) {
    fprintf(stderr, "tactix: no input file\n");
    printf("s UNKNOWN\n");
    return 1;
  }

  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);
#ifdef SIGXCPU
  signal(SIGXCPU, handle_signal);
#endif

  // Only RandomPolicy drives the solve purely through the null-safe rule API and
  // tolerates a null H. Gate on the SELECTED policy, not just the flag: the
  // dispatch below is a precedence chain where the stress policies sit ABOVE
  // random_policy, so `--no-heuristics --random-policy --adversarial-policy` would
  // otherwise slip a stress policy (or DefaultPolicy) through with no H (H5).
  if (no_heuristics && (!random_policy || adversarial_policy ||
                        bve_stress_policy || congruence_stress_policy)) {
    fprintf(stderr, "tactix: --no-heuristics requires --random-policy alone "
                    "(no DefaultPolicy / stress policy); those need H\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  Kernel kernel;
  Heuristics heuristics; // the session owns H (Track H · H7); the kernel holds
                         // only a non-owning pointer. --no-heuristics leaves it
                         // unattached (null) so the whole run is H-free.
  if (!no_heuristics)
    kernel.h = &heuristics;
  kernel.bve_gate_mask = bve_gate_mask;
  // TACTIX_BCE_OCC_LIMIT=<n> (DIAGNOSTIC arm; 0 = uncapped, the pre-2026-07-29
  // behaviour): X1 BCE's candidate-occurrence ceiling. A Kernel CONFIG field, so
  // this only exposes it for an A/B — the shipped value is its default (100,
  // cadical's blockocclim). See Kernel::bce_occ_limit.
  if (const char *ol = getenv("TACTIX_BCE_OCC_LIMIT"))
    kernel.bce_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  // X2 governors (DIAGNOSTIC arms; 0 = unlimited, the pre-2026-07-30 behaviour).
  // See Kernel::ternary_occ_limit / ternary_max_add_percent.
  if (const char *ol = getenv("TACTIX_TERNARY_OCC_LIMIT"))
    kernel.ternary_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  if (const char *ma = getenv("TACTIX_TERNARY_MAXADD"))
    kernel.ternary_max_add_percent = (uint32_t)strtoul(ma, nullptr, 10);
  // BVE cost-per-yield ceiling; only consulted under TACTIX_BVE_ADMIT=cost.
  // See Kernel::bve_cost_limit.
  if (const char *cl = getenv("TACTIX_BVE_COST_LIMIT"))
    kernel.bve_cost_limit = (uint32_t)strtoul(cl, nullptr, 10);
  // TB4b(A) A/B arm, read HERE rather than in DefaultPolicy so it applies under
  // every policy — the corpus replays run --random-policy, which never enters
  // DefaultPolicy::solve, and a knob the regression bed cannot toggle is not a
  // usable A/B arm.
  if (getenv("TACTIX_CONFLICT_REUSE"))
    kernel.conflict_reuse = true;
  Proof proof;
  if (proof_path) {
    if (!proof.open(proof_path, proof_format)) {
      fprintf(stderr, "tactix: cannot write proof to '%s'\n", proof_path);
      printf("s UNKNOWN\n");
      return 1;
    }
    kernel.proof = &proof;
  }

  bool piped = false;
  FILE *in = open_input(cnf_path, &piped);
  if (!in) {
    fprintf(stderr, "tactix: cannot read '%s'\n", cnf_path);
    printf("s UNKNOWN\n");
    return 1;
  }
  const char *err = parse_dimacs(in, kernel);
  if (piped)
    pclose(in);
  else
    fclose(in);
  if (err) {
    fprintf(stderr, "tactix: parse error: %s\n", err);
    printf("s UNKNOWN\n");
    fflush(stdout);
    return 1;
  }

  const uint64_t solve_t0 = tactix::now_nanos();
  const int res =
      probing_stress_policy
          ? probing_stress_solve(kernel, random_seed, &caught_signal)
      : walk_stress_policy
          ? walk_stress_solve(kernel, random_seed, &caught_signal)
      : factor_stress_policy
          ? factor_stress_solve(kernel, random_seed, &caught_signal)
      : congruence_stress_policy
          ? congruence_stress_solve(kernel, random_seed, &caught_signal)
      : bve_stress_policy
          ? bve_stress_solve(kernel, random_seed, &caught_signal)
      : adversarial_policy
          ? adversarial_solve(kernel, random_seed, &caught_signal)
      : random_policy ? random_solve(kernel, random_seed, &caught_signal)
                      : default_solve(kernel, &caught_signal);
  kernel.stats.solve_nanos = tactix::now_nanos() - solve_t0;

  proof.flush();
  if (!quiet) {
    printf("c tactix 0.1.0\n");
    printf("c decisions   %llu (%llu via API)\n",
           (unsigned long long)kernel.stats.decisions,
           (unsigned long long)kernel.stats.api_decisions);
    printf("c conflicts   %llu\n", (unsigned long long)kernel.stats.conflicts);
    printf("c propagations %llu (%llu search, %llu inprocess)\n",
           (unsigned long long)kernel.stats.propagations,
           (unsigned long long)(kernel.stats.propagations -
                                kernel.stats.inprocess_propagations),
           (unsigned long long)kernel.stats.inprocess_propagations);
    printf("c learned     %llu (%llu units)\n",
           (unsigned long long)kernel.stats.learned,
           (unsigned long long)kernel.stats.learned_units);
    // clause QUALITY (2026-07-28): mean size and glue of what we learn, and
    // how much minimize+shrink actually removes — the decomposition the
    // conflict-count gap vs kissat needs (kissat prints the same via -s).
    if (kernel.stats.learned) {
      printf("c learnedsize %.2f avg lits, %.2f avg glue\n",
             (double)kernel.stats.learned_lits / (double)kernel.stats.learned,
             (double)kernel.stats.learned_glue_sum /
                 (double)kernel.stats.learned);
      const double kept = (double)kernel.stats.learned_lits;
      const double removed =
          (double)(kernel.stats.minimized + kernel.stats.shrunken);
      printf("c reduction   %.1f%% of resolved lits removed by minimize+shrink\n",
             100.0 * removed / (kept + removed));
    }
    printf("c minimized   %llu lits\n",
           (unsigned long long)kernel.stats.minimized);
    printf("c shrunken    %llu lits\n",
           (unsigned long long)kernel.stats.shrunken);
    printf("c reasonbump  %llu vars in %llu rounds (%llu aborted)\n",
           (unsigned long long)kernel.stats.reason_bumped,
           (unsigned long long)kernel.stats.reason_bump_rounds,
           (unsigned long long)kernel.stats.reason_bump_aborted);
    printf("c otfs        %llu strengthened (%llu conflicts subsumed)\n",
           (unsigned long long)kernel.stats.otfs_strengthened,
           (unsigned long long)kernel.stats.otfs_subsumed);
    printf("c strengthen  %llu rounds, %llu strengthened, %llu subsumed, "
           "%llu flushed\n",
           (unsigned long long)kernel.stats.strengthen_rounds,
           (unsigned long long)kernel.stats.strengthened,
           (unsigned long long)kernel.stats.strengthen_subsumed,
           (unsigned long long)kernel.stats.flushed);
    // `checked` is printed because `vivified` alone is uninterpretable — it is a
    // rate, not a count. Added 2026-07-29 while chasing a mod4block "46 vs
    // kissat's 47,526" gap that turned out NOT to be like-for-like: kissat's
    // `vivified` is an AGGREGATE (statistics.h:247, a % of checks) incremented
    // for subsumed, PROMOTED (a tier move, not a clause change), shrunken,
    // implied, asym and instantiated, where ours counts only clauses we
    // strengthened. Comparing the two needs both denominators — the L14 trap,
    // and the reason this counter now ships.
    printf("c vivify      %llu rounds, %llu vivified, %llu deleted, %llu checked\n",
           (unsigned long long)kernel.stats.vivify_rounds,
           (unsigned long long)kernel.stats.vivified,
           (unsigned long long)kernel.stats.vivify_deleted,
           (unsigned long long)kernel.stats.vivify_checked);
    printf("c probe       %llu rounds, %llu probed, %llu failed\n",
           (unsigned long long)kernel.stats.probe_rounds,
           (unsigned long long)kernel.stats.probed,
           (unsigned long long)kernel.stats.probe_failed);
    printf("c backbone    %llu rounds, %llu probed, %llu failed\n",
           (unsigned long long)kernel.stats.backbone_rounds,
           (unsigned long long)kernel.stats.backbone_probed,
           (unsigned long long)kernel.stats.backbone_failed);
    printf("c transitive  %llu rounds, %llu checked, %llu deleted, "
           "%llu failed, %llu ticks\n",
           (unsigned long long)kernel.stats.transitive_rounds,
           (unsigned long long)kernel.stats.transitive_checked,
           (unsigned long long)kernel.stats.transitive_deleted,
           (unsigned long long)kernel.stats.transitive_failed,
           (unsigned long long)kernel.stats.transitive_ticks);
    printf("c substitute  %llu rounds\n",
           (unsigned long long)kernel.stats.substitute_rounds);
    // Track R families: registered and N1-reachable, not scheduled by
    // DefaultPolicy yet — without these lines "did the new rule even fire?"
    // is unanswerable from a normal run (the same gap V6/E2 once had).
    printf("c bce         %llu rounds, %llu deleted, %llu ticks\n",
           (unsigned long long)kernel.stats.bce_rounds,
           (unsigned long long)kernel.stats.bce_deleted,
           (unsigned long long)kernel.stats.bce_ticks);
    printf("c instantiate %llu rounds, %llu literals removed, %llu probed\n",
           (unsigned long long)kernel.stats.instantiate_rounds,
           (unsigned long long)kernel.stats.instantiated,
           (unsigned long long)kernel.stats.instantiate_checked);
    printf("c ternary     %llu rounds, %llu resolvents, %llu binaries, "
           "%llu ticks\n",
           (unsigned long long)kernel.stats.ternary_rounds,
           (unsigned long long)kernel.stats.ternary_resolvents,
           (unsigned long long)kernel.stats.ternary_binaries,
           (unsigned long long)kernel.stats.ternary_ticks);
    // V6 + E2 ship ON by default but had no stats line, so "did congruence /
    // Factor even fire on this instance?" was unanswerable from a normal run.
    printf("c chrono      %llu chronological, %llu stale backjumps\n",
           (unsigned long long)kernel.stats.chronological,
           (unsigned long long)kernel.stats.backjump_stale);
    printf("c inprocess   %llu phases\n",
           (unsigned long long)kernel.stats.inprocess_phases);
    printf("c congruence  %llu rounds, %llu units, %llu unjustified\n",
           (unsigned long long)kernel.stats.congruence_rounds,
           (unsigned long long)kernel.stats.congruence_units,
           (unsigned long long)kernel.stats.congruence_unjustified);
    printf("c factor      %llu factorings\n",
           (unsigned long long)kernel.stats.factorings);
    printf("c bve         %llu rounds, %llu eliminated, %llu resolutions\n",
           (unsigned long long)kernel.stats.bve_rounds,
           (unsigned long long)kernel.stats.bve_eliminated,
           (unsigned long long)kernel.stats.bve_resolutions);
    printf("c restarts    %llu (%llu reused a trail prefix)\n",
           (unsigned long long)kernel.stats.restarts,
           (unsigned long long)kernel.stats.restarts_reused);
    printf("c reductions  %llu (%llu clauses deleted, %llu promoted)\n",
           (unsigned long long)kernel.stats.reductions,
           (unsigned long long)kernel.stats.deleted,
           (unsigned long long)kernel.stats.promoted);
    printf("c mode        %s after %llu switches (%llu ticks, "
           "%llu target phases saved)\n",
           !kernel.h ? "unguided" : kernel.h->stable ? "stable" : "focused",
           (unsigned long long)kernel.stats.switched,
           (unsigned long long)kernel.stats.ticks,
           (unsigned long long)kernel.stats.target_saved);
    printf("c rephased    %llu (%llu best phases saved)\n",
           (unsigned long long)kernel.stats.rephased,
           (unsigned long long)kernel.stats.best_saved);
    printf("c reordered   %llu decision re-rankings\n",
           (unsigned long long)kernel.stats.reordered);
    printf("c lucky       %llu probes\n",
           (unsigned long long)kernel.stats.lucky_probes);
    printf("c walk        %llu walks (%llu improved, %llu flips, %llu steps)\n",
           (unsigned long long)kernel.stats.walks,
           (unsigned long long)kernel.stats.walk_improved,
           (unsigned long long)kernel.stats.walk_flips,
           (unsigned long long)kernel.stats.walk_steps);
    printf("c subsume     %llu rounds (%llu checked, %llu subsumed)\n",
           (unsigned long long)kernel.stats.forward_subsumptions,
           (unsigned long long)kernel.stats.forward_checked,
           (unsigned long long)kernel.stats.forward_subsumed);
    // --- `c timing`: where the wall clock actually goes ---------------------
    // Rules are charged individually by the registry dispatch wrapper; the SLOT
    // line is the whole inprocessing phase, so (slot − Σ rules in it) is the
    // scaffolding (occurrence index, garbage collection, scheduling). SEARCH is
    // DERIVED as solve − slot, because the per-conflict path is deliberately
    // untimed (a clock there is exactly the hot-path cost N3 cannot pay), and
    // it is the residual that matters anyway. Rules dispatched OUTSIDE the slot
    // (the C API, the fuzz/stress policies) are charged too, so under those
    // policies "search" is the residual after them as well.
    {
      const double total_ms = kernel.stats.solve_nanos / 1e6;
      const double slot_ms = kernel.stats.inprocess_nanos / 1e6;
      auto pct = [&](double ms) { return total_ms > 0 ? 100.0 * ms / total_ms : 0.0; };
      printf("c timing      %.0f ms solve\n", total_ms);
      printf("c timing      %.0f ms (%.1f%%) inprocessing slot, %.0f ms (%.1f%%) "
             "search+scaffolding outside it\n",
             slot_ms, pct(slot_ms), total_ms - slot_ms, pct(total_ms - slot_ms));
      // per-rule, biggest first; only rules that actually ran
      const auto &rules = tactix::tactix_rules();
      std::vector<std::pair<uint64_t, size_t>> ranked;
      double charged_ms = 0;
      for (size_t i = 0; i < rules.size() && i < tactix::Stats::MAX_TIMED_RULES; i++)
        if (kernel.stats.rule_calls[i]) {
          ranked.push_back({kernel.stats.rule_nanos[i], i});
          charged_ms += kernel.stats.rule_nanos[i] / 1e6;
        }
      std::sort(ranked.rbegin(), ranked.rend());
      for (const auto &e : ranked)
        printf("c timing        %-16s %9.0f ms (%5.1f%%) in %llu applications\n",
               rules[e.second].name, e.first / 1e6, pct(e.first / 1e6),
               (unsigned long long)kernel.stats.rule_calls[e.second]);
      printf("c timing        %-16s %9.0f ms (%5.1f%%)\n", "(unattributed)",
             slot_ms - charged_ms, pct(slot_ms - charged_ms));
    }
  }
  if (res == 10) {
    kernel_extend_model(kernel); // rebuild the model through rho (plan §4)
    printf("s SATISFIABLE\n");
    print_model(kernel);
  } else if (res == 20) {
    printf("s UNSATISFIABLE\n");
  } else {
    printf("s UNKNOWN\n");
  }
  fflush(stdout);
  return res == 10 ? 10 : res == 20 ? 20 : 0;
}
