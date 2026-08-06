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
#include "policies/reuse_stress_policy.hpp"
#include "policies/sweep_stress_policy.hpp"
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
  bool reuse_stress_policy = false;
  bool sweep_stress_policy = false;
  bool no_heuristics = false; // run with H detached (unguided)
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
      // Stress mode: maximizes BVE at the root (gate detection +
      // resolution + reconstruction), the N1 schedule the uniform random
      // policy under-samples (stress-policy skill)
      bve_stress_policy = true;
      if (a[19] == '=')
        random_seed = strtoull(a + 20, nullptr, 10);
    } else if (!strncmp(a, "--probing-stress-policy", 23)) {
      // Stress mode: hammers the kernel_probing snapshot/restore scope —
      // the probing family in random order at every root, densely interleaved
      // with search that reuses the restored watch lists (stress-policy skill)
      probing_stress_policy = true;
      if (a[23] == '=')
        random_seed = strtoull(a + 24, nullptr, 10);
    } else if (!strncmp(a, "--reuse-stress-policy", 21)) {
      // Stress mode: forces the conflict-reuse shortcut ON, backjumps
      // ALWAYS chronologically (the only way to reach a conflict with one
      // literal at the conflict level), and drops to random levels with a
      // fixpoint invariant check after each — the watch-repair oracle
      // (stress-policy skill)
      reuse_stress_policy = true;
      if (a[21] == '=')
        random_seed = strtoull(a + 22, nullptr, 10);
    } else if (!strncmp(a, "--walk-stress-policy", 20)) {
      // Stress mode: Walk at random decision levels, off propagation
      // fixpoints, with tiny/huge budgets and mode flips — the walker is legal
      // anywhere, and DefaultPolicy only ever walks at a quiescent stable root
      // (stress-policy skill)
      walk_stress_policy = true;
      if (a[20] == '=')
        random_seed = strtoull(a + 21, nullptr, 10);
    } else if (!strncmp(a, "--factor-stress-policy", 22)) {
      // Stress mode: adversarial FactorMatchPolicy (hash /
      // all-tied / reversed scores, maximizing accept) + Factor maximized at
      // the root — the grid-selection schedules DefaultFactorMatch never
      // steers into (stress-policy skill)
      factor_stress_policy = true;
      if (a[22] == '=')
        random_seed = strtoull(a + 23, nullptr, 10);
    } else if (!strncmp(a, "--sweep-stress-policy", 21)) {
      // V5 S-e stress mode: sweeps every root to a fixpoint with no effort
      // budget, interleaving Substitute, so EMISSION is maximized -- and with
      // TACTIX_SWEEP_FORCE_GENERAL=1 every proved equivalence is routed through
      // the general core->chain path, the one the 2026-08-05 N2 breach lived in
      // and that generic fuzz reaches ~8 times in 4380 (stress-policy skill)
      sweep_stress_policy = true;
      if (a[21] == '=')
        random_seed = strtoull(a + 22, nullptr, 10);
    } else if (!strncmp(a, "--congruence-stress-policy", 26)) {
      // Stress mode: maximizes Congruence's newly-legal schedules -- deriving
      // root units, refuting, and re-entering congruence off a fixpoint on the
      // formula its own previous round changed (stress-policy skill)
      congruence_stress_policy = true;
      if (a[26] == '=')
        random_seed = strtoull(a + 27, nullptr, 10);
    } else if (!strcmp(a, "--no-heuristics")) {
      // Detach `H` (an unguided solver — `k.h == nullptr`). Sound
      // but slow; DefaultPolicy needs `H`, so pair with --random-policy (which
      // drives the solve purely through the null-safe rule API).
      no_heuristics = true;
    } else if (!strncmp(a, "--bve-gates=", 12)) {
      // BVE gate-type selection: comma list of {equiv,andor,ite,xor}, or
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
          } else if (!strncmp(p, "ite", 3)) {
            bve_gate_mask |= BVE_GATE_ITE;
            p += 3;
          } else if (!strncmp(p, "xor", 3)) {
            bve_gate_mask |= BVE_GATE_XOR;
            p += 3;
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
             "[--random-policy[=SEED]] "
             "[--bve-gates=all|none|equiv,andor,ite,xor] "
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
  // random_policy, so `--no-heuristics --random-policy --adversarial-policy`
  // would otherwise slip a stress policy (or DefaultPolicy) through with no H.
  if (no_heuristics && (!random_policy || adversarial_policy ||
                        bve_stress_policy || congruence_stress_policy ||
                        sweep_stress_policy)) {
    fprintf(stderr, "tactix: --no-heuristics requires --random-policy alone "
                    "(no DefaultPolicy / stress policy); those need H\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  Kernel kernel;
  Heuristics heuristics; // the session owns H; the kernel holds
                         // only a non-owning pointer. --no-heuristics leaves it
                         // unattached (null) so the whole run is H-free.
  if (!no_heuristics)
    kernel.h = &heuristics;
  kernel.bve_gate_mask = bve_gate_mask;
  // TACTIX_BCE_OCC_LIMIT=<n> (DIAGNOSTIC arm): BCE's candidate-occurrence
  // ceiling. A Kernel CONFIG field, so this only exposes it for an A/B; the
  // shipped value is its default, which is 0 = UNCAPPED. See
  // Kernel::bce_occ_limit for why cadical's 100 is not ported.
  if (const char *ol = getenv("TACTIX_BCE_OCC_LIMIT"))
    kernel.bce_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  // HyperTernaryRes governors (DIAGNOSTIC arms; 0 = unlimited). See
  // Kernel::ternary_occ_limit / ternary_max_add_percent.
  if (const char *ol = getenv("TACTIX_TERNARY_OCC_LIMIT"))
    kernel.ternary_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  if (const char *ma = getenv("TACTIX_TERNARY_MAXADD"))
    kernel.ternary_max_add_percent = (uint32_t)strtoul(ma, nullptr, 10);
  // BVE cost-per-yield ceiling; only consulted under TACTIX_BVE_ADMIT=cost.
  // See Kernel::bve_cost_limit.
  if (const char *cl = getenv("TACTIX_BVE_COST_LIMIT"))
    kernel.bve_cost_limit = (uint32_t)strtoul(cl, nullptr, 10);
  if (getenv("TACTIX_V4_EARLY_EXIT"))
    kernel.transitive_early_exit = true;
  // Track SCHED stage F: the touched-epoch round filters. Default ON;
  // TACTIX_TOUCH_MARKS=0 is the A/B off arm and restores the pre-stage-F
  // candidate sets exactly. Set on the KERNEL so it applies under every policy,
  // --random-policy and the stress policies included.
  if (const char *tm = getenv("TACTIX_TOUCH_MARKS"))
    kernel.touch_filter = (*tm != '0');
  if (const char *sh = getenv("TACTIX_SUBSUME_HOLD"))
    kernel.subsume_holds_elim = (*sh != '0');
  // SCHED-F6: ships OFF (see Kernel::touch_redundant); =1 is the ON arm, and
  // it is a PRE-REQUISITE arm, not a candidate default -- there is no consumer
  // for the redundant band yet.
  if (const char *tr = getenv("TACTIX_TOUCH_REDUNDANT"))
    kernel.touch_redundant = (*tr != '0');
  if (const char *mb = getenv("TACTIX_MIDROUND_BUDGET"))
    kernel.midround_budget = (*mb != '0'); // default OFF, measured negative;
                                           // see Kernel::midround_budget
  if (const char *fs = getenv("TACTIX_FACTOR_SIZE"))
    kernel.factor_size = (uint32_t)strtoul(fs, nullptr, 10);
  if (const char *sp = getenv("TACTIX_FACTOR_SCOPE_PCT"))
    kernel.factor_scope_percent = (uint32_t)strtoul(sp, nullptr, 10);
  // Kernel::factor_bound — kissat's `reduction > bound` acceptance threshold
  // (factor.c:939). Default 0 = "any positive gain applies". This is the PAYOFF
  // axis and it is formula-shape-blind, which is why it is the candidate for
  // making broad admission safe where a scope gate cannot be: Factor's winners
  // and losers sit at the same scope fraction.
  if (const char *fb = getenv("TACTIX_FACTOR_BOUND"))
    kernel.factor_bound = (uint32_t)strtoul(fb, nullptr, 10);
  // SCHED-C5: ships ON (reference parity, ledger SP-FACTOR-2). The hatch is
  // now INVERTED — `=0` turns demotion off for the A/B arm, `=1` (every banked
  // pre-08-05 arm) still means on. See Kernel::factor_demote_fresh.
  if (const char *fd = getenv("TACTIX_FACTOR_DEMOTE"))
    kernel.factor_demote_fresh = (*fd != '0');
  if (const char *cr = getenv("TACTIX_FACTOR_CAND_ROUNDS"))
    kernel.factor_cand_rounds = (uint32_t)strtoul(cr, nullptr, 10);
  if (getenv("TACTIX_NO_EXT_DECISIONS"))
    kernel.no_ext_decisions = true; // diagnostic, see Kernel::no_ext_decisions
  if (const char *sf = getenv("TACTIX_STOP_FUZZ"))
    kernel.stop_fuzz = (uint32_t)strtoul(sf, nullptr, 10); // see kernel_terminating
  if (const char *rt = getenv("TACTIX_RETIRE"))
    kernel.retire_enabled = (*rt != '0'); // retirement ships ON; =0 reverts
  if (getenv("TACTIX_SUBST_DEMOTE"))
    kernel.subst_demote = true; // rank substituted vars last, see Kernel
  if (getenv("TACTIX_SUBST_HAZARD_ROUND"))
    kernel.subst_hazard_round = true; // A/B, see Kernel::subst_hazard_round
  // SCHED-C6: eager subsumption of recently learned clauses ships ON at
  // kissat's own default and maximum (`eagersubsume`=4, learn.c). `=0` is the
  // A/B revert arm; 1..4 dose the register. Read HERE, not in DefaultPolicy,
  // so the knob applies under --random-policy and the stress policies too — a
  // knob the regression bed cannot toggle is not a usable arm.
  if (const char *es = getenv("TACTIX_EAGER_SUBSUME")) {
    const unsigned long n = strtoul(es, nullptr, 10);
    kernel.eager_subsume = n > Kernel::EAGER_SUBSUME_SLOTS
                               ? Kernel::EAGER_SUBSUME_SLOTS
                               : (uint32_t)n;
  }
  if (getenv("TACTIX_NO_EXT_REASON_BUMP"))
    kernel.no_ext_reason_bump = true; // arm, see Kernel::no_ext_reason_bump
  // A/B arm, read HERE rather than in DefaultPolicy so it applies under
  // every policy — the corpus replays run --random-policy, which never enters
  // DefaultPolicy::solve, and a knob the regression bed cannot toggle is not a
  // usable A/B arm.
  if (getenv("TACTIX_CONFLICT_REUSE"))
    kernel.conflict_reuse = true;
  // SCHED / SP-JUMP-1: kissat's `jumpreasons`, default OFF here pending the
  // pre-registered tier A/B. Read HERE for the same reason as the two above —
  // a knob DefaultPolicy owns is one the corpus replays and the stress
  // policies cannot toggle, so it is not a usable arm.
  if (getenv("TACTIX_JUMP_REASONS"))
    kernel.jump_reasons = true;
  if (getenv("TACTIX_CONFLICT_REUSE_NOREPAIR")) // diagnostic; see the field
    kernel.conflict_reuse_no_repair = true;
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
      : reuse_stress_policy
          ? reuse_stress_solve(kernel, random_seed, &caught_signal)
      : factor_stress_policy
          ? factor_stress_solve(kernel, random_seed, &caught_signal)
      : sweep_stress_policy
          ? sweep_stress_solve(kernel, random_seed, &caught_signal)
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
    printf("c input       %llu clauses restated (repeated literals)\n",
           (unsigned long long)kernel.stats.input_restated);
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
    // clause QUALITY: mean size and glue of what we learn, and how much
    // minimize+shrink actually removes — the decomposition a conflict-count
    // gap vs kissat needs (kissat prints the same via -s).
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
    // SCHED-C6 pre-registered readout: an arm of TACTIX_EAGER_SUBSUME whose
    // `tried` is 0 did not test the mechanism, it skipped it.
    printf("c eagersub    %llu subsumed of %llu tried (%llu kept as reason), "
           "register %u\n",
           (unsigned long long)kernel.stats.eager_subsumed,
           (unsigned long long)kernel.stats.eager_tried,
           (unsigned long long)kernel.stats.eager_reason_skipped,
           (unsigned)kernel.eager_subsume);
    printf("c strengthen  %llu rounds, %llu strengthened, %llu subsumed, "
           "%llu flushed\n",
           (unsigned long long)kernel.stats.strengthen_rounds,
           (unsigned long long)kernel.stats.strengthened,
           (unsigned long long)kernel.stats.strengthen_subsumed,
           (unsigned long long)kernel.stats.flushed);
    // `checked` is printed because `vivified` alone is uninterpretable — it is
    // a rate, not a count, and it is NOT comparable to kissat's counter of the
    // same name: kissat's `vivified` is an AGGREGATE (statistics.h:247)
    // incremented for subsumed, PROMOTED (a tier move, not a clause change),
    // shrunken, implied, asym and instantiated, where ours counts only clauses
    // we strengthened. Comparing them needs both denominators.
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
    printf("c substitute  %llu rounds, %llu non-trivial SCCs (%llu lits), "
           "%llu vars substituted, %llu rounds skipped / %llu partial "
           "(%llu lits poisoned) on the hazard check\n",
           (unsigned long long)kernel.stats.substitute_rounds,
           (unsigned long long)kernel.stats.substitute_sccs,
           (unsigned long long)kernel.stats.substitute_scc_lits,
           (unsigned long long)kernel.stats.substitute_vars,
           (unsigned long long)kernel.stats.substitute_hazard_skips,
           (unsigned long long)kernel.stats.substitute_hazard_partial,
           (unsigned long long)kernel.stats.substitute_hazard_lits);
    // Every registered rule gets a stats line: without one, "did this rule
    // even fire on this instance?" is unanswerable from a normal run.
    printf("c bce         %llu rounds, %llu deleted, %llu ticks\n",
           (unsigned long long)kernel.stats.bce_rounds,
           (unsigned long long)kernel.stats.bce_deleted,
           (unsigned long long)kernel.stats.bce_ticks);
    printf("c instantiate %llu rounds, %llu literals removed, %llu probed\n",
           (unsigned long long)kernel.stats.instantiate_rounds,
           (unsigned long long)kernel.stats.instantiated,
           (unsigned long long)kernel.stats.instantiate_checked);
    // Also the reachability witness for Sweep's fuzz legs: `environments 0`
    // means the detector never ran, so any green leg over it is vacuous.
    printf("c sweep       %llu rounds, %llu environments (%llu clauses), "
           "%llu solves, %llu candidates, %llu equivalences (%llu already "
           "binary), %llu env-unsat, %llu budget, %llu ticks\n",
           (unsigned long long)kernel.stats.sweep_rounds,
           (unsigned long long)kernel.stats.sweep_environments,
           (unsigned long long)kernel.stats.sweep_env_clauses,
           (unsigned long long)kernel.stats.sweep_solves,
           (unsigned long long)kernel.stats.sweep_candidates,
           (unsigned long long)kernel.stats.sweep_equivalences,
           (unsigned long long)kernel.stats.sweep_known,
           (unsigned long long)kernel.stats.sweep_env_unsat,
           (unsigned long long)kernel.stats.sweep_budget,
           (unsigned long long)kernel.stats.sweep_ticks);
    // The EMISSION half, reported per path. Detection counters above say the
    // rule ran; only these say a proof line was written, and by which path.
    // Without them a "Sweep is fine" fuzz leg cannot be told from one that
    // never emitted anything -- the vacuous-arm trap that hid the S-e polarity
    // defect (2026-08-05). `not-RUP` is S-d's decline count, i.e. the pairs
    // handed to S-e.
    printf("c sweepemit   %llu binaries, %llu not-rup, %llu general pairs\n",
           (unsigned long long)kernel.stats.sweep_binaries,
           (unsigned long long)kernel.stats.sweep_not_rup,
           (unsigned long long)kernel.stats.sweep_general);
    printf("c ternary     %llu rounds, %llu resolvents, %llu binaries, "
           "%llu ticks\n",
           (unsigned long long)kernel.stats.ternary_rounds,
           (unsigned long long)kernel.stats.ternary_resolvents,
           (unsigned long long)kernel.stats.ternary_binaries,
           (unsigned long long)kernel.stats.ternary_ticks);
    // `stale backjumps` should be 0: a nonzero value means conflicts are being
    // dropped and re-derived, which is otherwise invisible in a tier run.
    printf("c chrono      %llu chronological, %llu stale backjumps\n",
           (unsigned long long)kernel.stats.chronological,
           (unsigned long long)kernel.stats.backjump_stale);
    // "did the conflict-reuse shortcut fire, and did it need the watch
    // repair?" — the second number is what separates the full mechanism from
    // the subset a version that DECLINES instead of repairing could take.
    printf("c reuse       %llu conflicts reused, %llu needed watch repair\n",
           (unsigned long long)kernel.stats.conflict_reused,
           (unsigned long long)kernel.stats.conflict_watch_repaired);
    // Reachability for the TACTIX_JUMP_REASONS arm: an arm whose counter did
    // not move was not tested.
    printf("c jumpreason  %llu binary reasons jumped\n",
           (unsigned long long)kernel.stats.jumped_reasons);
    printf("c inprocess   %llu phases\n",
           (unsigned long long)kernel.stats.inprocess_phases);
    printf("c conggates   %llu and, %llu xor, %llu ite\n",
           (unsigned long long)kernel.stats.congruence_gates_and,
           (unsigned long long)kernel.stats.congruence_gates_xor,
           (unsigned long long)kernel.stats.congruence_gates_ite);
    printf("c congruence  %llu rounds, %llu gates, %llu merges, %llu units, "
           "%llu unjustified, %llu ticks, %llu budget stops\n",
           (unsigned long long)kernel.stats.congruence_rounds,
           (unsigned long long)kernel.stats.congruence_gates,
           (unsigned long long)kernel.stats.congruence_merges,
           (unsigned long long)kernel.stats.congruence_units,
           (unsigned long long)kernel.stats.congruence_unjustified,
           (unsigned long long)kernel.stats.congruence_ticks,
           (unsigned long long)kernel.stats.congruence_budget_stops);
    {
      uint64_t n_sub = 0, n_elim = 0;
      for (Var v = 0; v < kernel.nvars; v++) {
        if (v < kernel.var_status.size() &&
            kernel.var_status[v] == VarStatus::Substituted)
          n_sub++;
        else if (v < kernel.var_status.size() &&
                 kernel.var_status[v] == VarStatus::Eliminated)
          n_elim++;
      }
      printf("c retired     %llu vars (%llu substituted, %llu eliminated) of %u\n",
             (unsigned long long)(n_sub + n_elim), (unsigned long long)n_sub,
             (unsigned long long)n_elim, kernel.nvars);
    }
    printf("c deactivate  %llu deactivations, %llu re-bumps of retired vars\n",
           (unsigned long long)kernel.stats.deactivations,
           (unsigned long long)kernel.stats.reactivations);
    printf("c substdec    %llu decisions on substituted vars (%.2f%% of %llu)\n",
           (unsigned long long)kernel.stats.decisions_substituted,
           kernel.stats.decisions
               ? 100.0 * (double)kernel.stats.decisions_substituted /
                     (double)kernel.stats.decisions
               : 0.0,
           (unsigned long long)kernel.stats.decisions);
    printf("c extension   %u vars added, %llu decisions on them (%.2f%% of "
           "%llu)\n",
           kernel.first_extension_var == UINT32_MAX
               ? 0u
               : kernel.nvars - kernel.first_extension_var,
           (unsigned long long)kernel.stats.decisions_extension,
           kernel.stats.decisions
               ? 100.0 * (double)kernel.stats.decisions_extension /
                     (double)kernel.stats.decisions
               : 0.0,
           (unsigned long long)kernel.stats.decisions);
    printf("c interrupt   %llu rule applications aborted on the stop flag "
           "(%llu stress fires)\n",
           (unsigned long long)kernel.stats.rule_interrupted,
           (unsigned long long)kernel.stats.stop_fuzz_fires);
    printf("c extbumps    %llu of %llu bumps on extension vars (%.2f%%), "
           "%llu reason-side only (%.2f%% of those)\n",
           (unsigned long long)kernel.stats.bumps_extension,
           (unsigned long long)kernel.stats.bumps,
           kernel.stats.bumps ? 100.0 * (double)kernel.stats.bumps_extension /
                                    (double)kernel.stats.bumps
                              : 0.0,
           (unsigned long long)kernel.stats.bumps_ext_reason_only,
           kernel.stats.bumps_extension
               ? 100.0 * (double)kernel.stats.bumps_ext_reason_only /
                     (double)kernel.stats.bumps_extension
               : 0.0);
    // factor_ticks is Factor's dosing currency (and what the first-dose
    // allowance FACTOR_INIT_TICKS caps): printing it is what distinguishes
    // "the allowance bound the opening burst" from "the arm was inert".
    printf("c factor      %llu factorings, %llu large remainders offered, "
           "%llu ticks\n",
           (unsigned long long)kernel.stats.factorings,
           (unsigned long long)kernel.stats.factor_large_remainders,
           (unsigned long long)kernel.stats.factor_ticks);
    // SCHED-C5 pre-registered readout: an arm of TACTIX_FACTOR_DEMOTE whose
    // `demoted` count is 0 was not tested ([[ab-arm-identical-artefacts]]).
    printf("c factor-dem  %llu demotions, %llu fresh vars demoted\n",
           (unsigned long long)kernel.stats.factor_demotions,
           (unsigned long long)kernel.stats.factor_demoted_vars);
    printf("c bve         %llu rounds, %llu eliminated, %llu resolutions\n",
           (unsigned long long)kernel.stats.bve_rounds,
           (unsigned long long)kernel.stats.bve_eliminated,
           (unsigned long long)kernel.stats.bve_resolutions);
    // SP-BVE-GATE-1 pre-registered readout: an arm of --bve-gates whose ite/xor
    // counts are 0 did not test the gate types ([[ab-arm-identical-artefacts]]).
    printf("c bve-gates   %llu equiv, %llu andor, %llu ite, %llu xor; "
           "%llu resolvent pairs saved\n",
           (unsigned long long)kernel.stats.bve_gates_equiv,
           (unsigned long long)kernel.stats.bve_gates_and_or,
           (unsigned long long)kernel.stats.bve_gates_ite,
           (unsigned long long)kernel.stats.bve_gates_xor,
           (unsigned long long)kernel.stats.bve_gate_pairs_saved);
    printf("c restarts    %llu (%llu reused a trail prefix)\n",
           (unsigned long long)kernel.stats.restarts,
           (unsigned long long)kernel.stats.restarts_reused);
    printf("c reductions  %llu (%llu clauses deleted, %llu promoted)\n",
           (unsigned long long)kernel.stats.reductions,
           (unsigned long long)kernel.stats.deleted,
           (unsigned long long)kernel.stats.promoted);
    printf("c tiers       %u/%u live after %llu retiers "
           "(retention band %llu clause-visits dynamic vs %llu static)\n",
           kernel.tier1, kernel.tier2,
           (unsigned long long)kernel.stats.retiered,
           (unsigned long long)kernel.stats.tier2_band_dynamic,
           (unsigned long long)kernel.stats.tier2_band_static);
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
    printf("c randec      %llu sequences, %llu random decisions\n",
           (unsigned long long)kernel.stats.random_sequences,
           (unsigned long long)kernel.stats.random_decisions);
    printf("c subsume     %llu rounds (%llu checked, %llu visits, %llu subsumed)\n",
           (unsigned long long)kernel.stats.forward_subsumptions,
           (unsigned long long)kernel.stats.forward_checked,
           (unsigned long long)kernel.stats.forward_visits,
           (unsigned long long)kernel.stats.forward_subsumed);
    printf("c strengthen  %llu rounds (%llu checked, %llu visits, "
           "%llu strengthened, %llu subsumed)\n",
           (unsigned long long)kernel.stats.strengthen_rounds,
           (unsigned long long)kernel.stats.strengthen_checked,
           (unsigned long long)kernel.stats.strengthen_visits,
           (unsigned long long)kernel.stats.strengthened,
           (unsigned long long)kernel.stats.strengthen_subsumed);
    // Reachability witness for Kernel::midround_budget: rounds cut short by
    // the mid-round work budget. ZERO means the budget never bound, so any run
    // claiming to have exercised it proves nothing.
    printf("c budgetstop  %llu C1 rounds, %llu C2 rounds stopped mid-round\n",
           (unsigned long long)kernel.stats.budget_stops_c1,
           (unsigned long long)kernel.stats.budget_stops_c2);
    // Reachability + NON-WEDGE witness for the touched-epoch filters
    // (Kernel::touched). `marks` must keep growing on a long solve — its
    // producers are on the kernel's clause-creation path, so a stalled counter
    // means the substrate is dead, not quiet. Per rule, `skipped` is what the
    // suppressor bought and `checked` (the `c subsume`/`c strengthen` lines
    // above) is what still got through: BOTH non-zero is the evidence that the
    // filter fires and has not wedged shut. `advances` counts the rounds that
    // completed and therefore moved the watermark; zero advances with non-zero
    // rounds means the filter is inert because every round is being cut.
    printf("c touch       %llu marks, %llu epoch; C1 %llu skipped / %llu "
           "advances / %llu rearmed, C2 %llu skipped / %llu advances / %llu "
           "rearmed\n",
           (unsigned long long)kernel.stats.touch_marks,
           (unsigned long long)kernel.touch_epoch,
           (unsigned long long)kernel.stats.forward_mark_skipped,
           (unsigned long long)kernel.stats.forward_mark_advances,
           (unsigned long long)kernel.stats.forward_rearmed,
           (unsigned long long)kernel.stats.strengthen_mark_skipped,
           (unsigned long long)kernel.stats.strengthen_mark_advances,
           (unsigned long long)kernel.stats.strengthen_rearmed);
    printf("c subhold     %llu elimination rounds held open by C1/C2\n",
           (unsigned long long)kernel.stats.subsume_holds);
    printf("c touch3      BCE %llu skipped / %llu rearmed, X2 %llu skipped / "
           "%llu rearmed, X4 %llu skipped / %llu rearmed\n",
           (unsigned long long)kernel.stats.bce_mark_skipped,
           (unsigned long long)kernel.stats.bce_rearmed,
           (unsigned long long)kernel.stats.ternary_mark_skipped,
           (unsigned long long)kernel.stats.ternary_rearmed,
           (unsigned long long)kernel.stats.instantiate_mark_skipped,
           (unsigned long long)kernel.stats.instantiate_rearmed);
    printf("c touchred    %llu redundant creations in scope (dynamic) vs %llu "
           "(static tier2=6); stamping %s\n",
           (unsigned long long)kernel.stats.touch_redundant_dynamic,
           (unsigned long long)kernel.stats.touch_redundant_static6,
           kernel.touch_redundant ? "ON" : "off");
    printf("c touch4      Factor %llu seeds skipped / %llu rearmed\n",
           (unsigned long long)kernel.stats.factor_mark_skipped,
           (unsigned long long)kernel.stats.factor_rearmed);
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
