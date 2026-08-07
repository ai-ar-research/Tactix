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
#include "policies/cce_stress_policy.hpp"
#include "policies/sweep_stress_policy.hpp"
#include "policies/walk_stress_policy.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_policy.hpp"
#include "policies/trace.hpp"
#include "policies/stress_registry.hpp" // stress-policy flags added after rev 74
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
  bool cce_stress_policy = false;
  // Stress policies registered in stress_registry.cpp share one parse branch
  // and one dispatch arm; the `--<name>-stress-policy` flags below predate that
  // table. Each is documented by its own header.
  StressSolveFn stress_policy = nullptr;
  bool no_heuristics = false; // run with H detached (unguided)
  const char *trace_path = nullptr;  // --trace=FILE: record the rule sequence
  const char *replay_path = nullptr; // --replay=FILE: apply exactly that one
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
      random_policy = true;
      if (a[15] == '=')
        random_seed = strtoull(a + 16, nullptr, 10);
    } else if (!strncmp(a, "--adversarial-policy", 20)) {
      adversarial_policy = true;
      if (a[20] == '=')
        random_seed = strtoull(a + 21, nullptr, 10);
    } else if (!strncmp(a, "--bve-stress-policy", 19)) {
      bve_stress_policy = true;
      if (a[19] == '=')
        random_seed = strtoull(a + 20, nullptr, 10);
    } else if (!strncmp(a, "--probing-stress-policy", 23)) {
      probing_stress_policy = true;
      if (a[23] == '=')
        random_seed = strtoull(a + 24, nullptr, 10);
    } else if (!strncmp(a, "--reuse-stress-policy", 21)) {
      reuse_stress_policy = true;
      if (a[21] == '=')
        random_seed = strtoull(a + 22, nullptr, 10);
    } else if (!strncmp(a, "--walk-stress-policy", 20)) {
      walk_stress_policy = true;
      if (a[20] == '=')
        random_seed = strtoull(a + 21, nullptr, 10);
    } else if (!strncmp(a, "--factor-stress-policy", 22)) {
      factor_stress_policy = true;
      if (a[22] == '=')
        random_seed = strtoull(a + 23, nullptr, 10);
    } else if (!strncmp(a, "--sweep-stress-policy", 21)) {
      sweep_stress_policy = true;
      if (a[21] == '=')
        random_seed = strtoull(a + 22, nullptr, 10);
    } else if (!strncmp(a, "--cce-stress-policy", 19)) {
      cce_stress_policy = true;
      if (a[19] == '=')
        random_seed = strtoull(a + 20, nullptr, 10);
    } else if (!strncmp(a, "--congruence-stress-policy", 26)) {
      congruence_stress_policy = true;
      if (a[26] == '=')
        random_seed = strtoull(a + 27, nullptr, 10);
    } else if (!strncmp(a, "--trace=", 8)) {
      // Record the (rule, instance) sequence for --replay; see trace.hpp.
      trace_path = a + 8;
    } else if (!strncmp(a, "--replay=", 9)) {
      replay_path = a + 9;
    } else if (!strcmp(a, "--no-heuristics")) {
      // Detach `H` (an unguided solver): sound but slow, and DefaultPolicy
      // needs `H`, so pair with --random-policy.
      no_heuristics = true;
    } else if (!strncmp(a, "--bve-gates=", 12)) {
      // Comma list of {equiv,andor,ite,xor,def}, or all / none. A performance
      // knob: a disabled gate type only falls BVE back to the cross product.
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
          } else if (!strncmp(p, "def", 3)) {
            bve_gate_mask |= BVE_GATE_DEFINITION;
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
    } else if (StressSolveFn fn = stress_policy_flag(a, &random_seed)) {
      stress_policy = fn;
    } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      printf("usage: tactix [-q] [--no-binary] [--lrat] "
             "[--random-policy[=SEED]] "
             "[--bve-gates=all|none|equiv,andor,ite,xor,def] "
             "[--trace=FILE] [--replay=FILE] %s "
             "<cnf[.xz|.gz]> [proof]\n",
             stress_policy_flags());
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

  // Combination checks. A knob that cannot take effect must fail rather than be
  // ignored, since an arm that silently does nothing is the vacuous-experiment
  // failure mode. They gate on the selected policy, not the flag alone, because
  // the dispatch below is a precedence chain with the stress policies above
  // random_policy — the only policy that tolerates a null H and, with replay,
  // the only one expressible as a (rule, instance) sequence (trace.hpp).
  if (trace_path && !random_policy) {
    fprintf(stderr, "tactix: --trace requires --random-policy; DefaultPolicy's "
                    "schedule is not a (rule, instance) sequence (trace.hpp)\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  if (replay_path && (trace_path || random_policy || adversarial_policy ||
                      bve_stress_policy || congruence_stress_policy ||
                      factor_stress_policy || probing_stress_policy ||
                      walk_stress_policy || reuse_stress_policy ||
                      sweep_stress_policy || cce_stress_policy)) {
    fprintf(stderr, "tactix: --replay selects the recorded schedule, so it "
                    "cannot be combined with another policy or with --trace\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  // Must list EVERY stress policy: each needs `H`, and one omitted here runs
  // with `k.h == nullptr`. The set is the same one --replay rejects above, and
  // it drifted apart from it once already.
  if (no_heuristics && (!random_policy || adversarial_policy || stress_policy ||
                        bve_stress_policy || congruence_stress_policy ||
                        factor_stress_policy || probing_stress_policy ||
                        walk_stress_policy || reuse_stress_policy ||
                        sweep_stress_policy || cce_stress_policy)) {
    fprintf(stderr, "tactix: --no-heuristics requires --random-policy alone "
                    "(no DefaultPolicy / stress policy); those need H\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  Kernel kernel;
  Heuristics heuristics; // the session owns H, the kernel holds a non-owning
                         // pointer. --no-heuristics leaves it unattached, so
                         // the whole run is H-free.
  if (!no_heuristics)
    kernel.h = &heuristics;
  kernel.bve_gate_mask = bve_gate_mask;
  // Each env knob below poses one `Kernel` config field; that field documents
  // its default and soundness class. They are read here rather than in
  // DefaultPolicy so they apply under every policy — the regression bed replays
  // under --random-policy, and a knob it cannot toggle is not a usable A/B arm.
  if (const char *ol = getenv("TACTIX_BCE_OCC_LIMIT"))
    kernel.bce_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  if (const char *ol = getenv("TACTIX_TERNARY_OCC_LIMIT"))
    kernel.ternary_occ_limit = (uint32_t)strtoul(ol, nullptr, 10);
  if (const char *ma = getenv("TACTIX_TERNARY_MAXADD"))
    kernel.ternary_max_add_percent = (uint32_t)strtoul(ma, nullptr, 10);
  // Consulted only under TACTIX_BVE_ADMIT=cost.
  if (const char *cl = getenv("TACTIX_BVE_COST_LIMIT"))
    kernel.bve_cost_limit = (uint32_t)strtoul(cl, nullptr, 10);
  if (const char *dt = getenv("TACTIX_BVE_DEF_TICKS"))
    kernel.bve_definition_budget = strtoull(dt, nullptr, 10);
  if (getenv("TACTIX_V4_EARLY_EXIT"))
    kernel.transitive_early_exit = true;
  if (const char *lm = getenv("TACTIX_TRANSITIVE_LIVE_METER"))
    kernel.transitive_live_meter = (*lm != '0');
  // Touched-epoch round filters ship ON; =0 restores unfiltered candidates.
  if (const char *tm = getenv("TACTIX_TOUCH_MARKS"))
    kernel.touch_filter = (*tm != '0');
  if (const char *sh = getenv("TACTIX_SUBSUME_HOLD"))
    kernel.subsume_holds_elim = (*sh != '0');
  // Vivify's cross-candidate trail reuse ships ON; =0 also restores the older
  // probe/candidate order, which cannot be separated (Kernel::vivify_reuse).
  if (const char *vr = getenv("TACTIX_VIVIFY_REUSE"))
    kernel.vivify_reuse = (*vr != '0');
  // Ships OFF: a prerequisite arm, not a candidate default, since the redundant
  // band has no consumer yet (Kernel::touch_redundant).
  if (const char *tr = getenv("TACTIX_TOUCH_REDUNDANT"))
    kernel.touch_redundant = (*tr != '0');
  // SubsumeForward's shadow arm; the `c c1shadow` line below is its output.
  if (const char *ss = getenv("TACTIX_SUBSUME_SHADOW"))
    kernel.subsume_shadow = (*ss != '0');
  if (const char *mb = getenv("TACTIX_MIDROUND_BUDGET"))
    kernel.midround_budget = (*mb != '0'); // default OFF, measured negative;
                                           // see Kernel::midround_budget
  if (const char *fs = getenv("TACTIX_FACTOR_SIZE"))
    kernel.factor_size = (uint32_t)strtoul(fs, nullptr, 10);
  if (const char *sp = getenv("TACTIX_FACTOR_SCOPE_PCT"))
    kernel.factor_scope_percent = (uint32_t)strtoul(sp, nullptr, 10);
  // Kissat's `reduction > bound` acceptance threshold (factor.c:939), default 0
  // accepting any positive gain. Formula-shape-blind, which is why it and not a
  // scope gate is the candidate for making broad admission safe: Factor's
  // winners and losers sit at the same scope fraction.
  if (const char *fb = getenv("TACTIX_FACTOR_BOUND"))
    kernel.factor_bound = (uint32_t)strtoul(fb, nullptr, 10);
  // Fresh-variable demotion ships ON (reference parity); `=0` is the off arm.
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
  // Eager subsumption of recent learned clauses ships ON at kissat's default
  // and maximum (`eagersubsume`=4, learn.c); `=0` reverts, 1..4 dose it.
  if (const char *es = getenv("TACTIX_EAGER_SUBSUME")) {
    const unsigned long n = strtoul(es, nullptr, 10);
    kernel.eager_subsume = n > Kernel::EAGER_SUBSUME_SLOTS
                               ? Kernel::EAGER_SUBSUME_SLOTS
                               : (uint32_t)n;
  }
  if (getenv("TACTIX_NO_EXT_REASON_BUMP"))
    kernel.no_ext_reason_bump = true; // arm, see Kernel::no_ext_reason_bump
  if (getenv("TACTIX_CONFLICT_REUSE"))
    kernel.conflict_reuse = true;
  // Kissat's `jumpreasons`, default OFF pending its pre-registered tier A/B.
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

  tactix::RuleTrace trace;
  if (trace_path && !trace.open(trace_path, "random", random_seed)) {
    fprintf(stderr, "tactix: cannot write trace '%s'\n", trace_path);
    printf("s UNKNOWN\n");
    return 1;
  }
  const uint64_t solve_t0 = tactix::now_nanos();
  const int res =
      replay_path
          ? replay_solve(kernel, replay_path, &caught_signal)
      : stress_policy ? stress_policy(kernel, random_seed, &caught_signal)
      : probing_stress_policy
          ? probing_stress_solve(kernel, random_seed, &caught_signal)
      : walk_stress_policy
          ? walk_stress_solve(kernel, random_seed, &caught_signal)
      : reuse_stress_policy
          ? reuse_stress_solve(kernel, random_seed, &caught_signal)
      : factor_stress_policy
          ? factor_stress_solve(kernel, random_seed, &caught_signal)
      : sweep_stress_policy
          ? sweep_stress_solve(kernel, random_seed, &caught_signal)
      : cce_stress_policy
          ? cce_stress_solve(kernel, random_seed, &caught_signal)
      : congruence_stress_policy
          ? congruence_stress_solve(kernel, random_seed, &caught_signal)
      : bve_stress_policy
          ? bve_stress_solve(kernel, random_seed, &caught_signal)
      : adversarial_policy
          ? adversarial_solve(kernel, random_seed, &caught_signal)
      : random_policy
          ? random_solve(kernel, random_seed, &caught_signal,
                         trace_path ? &trace : nullptr)
          : default_solve(kernel, &caught_signal);
  kernel.stats.solve_nanos = tactix::now_nanos() - solve_t0;
  trace.close();
  if (res == -1) { // a replay that could not be replayed: see trace.cpp
    proof.flush(); // N2: whatever WAS derived is still on disk, as on any exit
    printf("s UNKNOWN\n");
    fflush(stdout);
    return 1;
  }

  proof.flush();
  if (!quiet) {
    // Every registered rule gets a stats line, and most pair a yield with a
    // reachability witness: a zero on the witness means the mechanism never
    // ran, so an A/B arm reading zero was skipped, not tested. Only counters
    // whose reading is non-obvious carry a comment below.
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
    // `vivified` here counts strengthened clauses only, so it is not comparable
    // to kissat's counter of the same name, an aggregate (statistics.h:247)
    // over subsumed, promoted, shrunken, implied, asym and instantiated.
    printf("c vivify      %llu rounds, %llu vivified, %llu deleted, %llu checked\n",
           (unsigned long long)kernel.stats.vivify_rounds,
           (unsigned long long)kernel.stats.vivified,
           (unsigned long long)kernel.stats.vivify_deleted,
           (unsigned long long)kernel.stats.vivify_checked);
    // Vivify trail reuse is inert three independent ways, one number each
    // (docs/design/2026-08-05-vivify-trail-reuse.md §4).
    printf("c vivify2     %llu reused, %llu probes, %llu full, %llu tier3, "
           "%llu prio, %llu rearm, %llu polls, %llu term\n",
           (unsigned long long)kernel.stats.vivify_reused,
           (unsigned long long)kernel.stats.vivify_probes,
           (unsigned long long)kernel.stats.vivify_reuse_full,
           (unsigned long long)kernel.stats.vivify_tier3,
           (unsigned long long)kernel.stats.vivify_prioritized,
           (unsigned long long)kernel.stats.vivify_reprioritized,
           (unsigned long long)kernel.stats.vivify_polls,
           (unsigned long long)kernel.stats.vivify_terminated);
    printf("c probe       %llu rounds, %llu probed, %llu failed\n",
           (unsigned long long)kernel.stats.probe_rounds,
           (unsigned long long)kernel.stats.probed,
           (unsigned long long)kernel.stats.probe_failed);
    printf("c backbone    %llu rounds, %llu probed, %llu failed\n",
           (unsigned long long)kernel.stats.backbone_rounds,
           (unsigned long long)kernel.stats.backbone_probed,
           (unsigned long long)kernel.stats.backbone_failed);
    printf("c transitive  %llu rounds, %llu checked, %llu deleted, "
           "%llu failed, %llu ticks, %llu polls, %llu terminated\n",
           (unsigned long long)kernel.stats.transitive_rounds,
           (unsigned long long)kernel.stats.transitive_checked,
           (unsigned long long)kernel.stats.transitive_deleted,
           (unsigned long long)kernel.stats.transitive_failed,
           (unsigned long long)kernel.stats.transitive_ticks,
           (unsigned long long)kernel.stats.transitive_polls,
           (unsigned long long)kernel.stats.transitive_terminated);
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
    printf("c bce         %llu rounds, %llu deleted, %llu ticks\n",
           (unsigned long long)kernel.stats.bce_rounds,
           (unsigned long long)kernel.stats.bce_deleted,
           (unsigned long long)kernel.stats.bce_ticks);
    // With `cla 0` no covered literal was added, so nothing BCE does not
    // already do was exercised; `blocked 0` means no witness was pushed.
    printf("c cce         %llu rounds, %llu tried, %llu deleted "
           "(%llu blocked / %llu ata), %llu ala, %llu cla, %llu ticks\n",
           (unsigned long long)kernel.stats.cce_rounds,
           (unsigned long long)kernel.stats.cce_tried,
           (unsigned long long)kernel.stats.cce_deleted,
           (unsigned long long)kernel.stats.cce_blocked,
           (unsigned long long)kernel.stats.cce_ata,
           (unsigned long long)kernel.stats.cce_ala,
           (unsigned long long)kernel.stats.cce_cla,
           (unsigned long long)kernel.stats.cce_ticks);
    // Its touched-epoch consumer, same contract as `c touch` below.
    printf("c cce2        %llu skipped / %llu advances / %llu rearmed\n",
           (unsigned long long)kernel.stats.cce_mark_skipped,
           (unsigned long long)kernel.stats.cce_mark_advances,
           (unsigned long long)kernel.stats.cce_rearmed);
    printf("c instantiate %llu rounds, %llu literals removed, %llu probed\n",
           (unsigned long long)kernel.stats.instantiate_rounds,
           (unsigned long long)kernel.stats.instantiated,
           (unsigned long long)kernel.stats.instantiate_checked);
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
    // Environment shape and candidate-solve outcomes, separating the several
    // mechanisms behind `equivalences 0`; then emission per path, since only
    // those say a proof line was written. `not-rup` is S-d's decline count.
    printf("c sweepdiag   %llu env-lits, %llu root-false (%llu stripped, "
           "%llu FREE), %llu units, %llu cand-seeded, %llu refined, "
           "%llu dir-sat, %llu dir-unsat\n",
           (unsigned long long)kernel.stats.sweep_env_lits,
           (unsigned long long)kernel.stats.sweep_env_false,
           (unsigned long long)kernel.stats.sweep_env_stripped,
           (unsigned long long)kernel.stats.sweep_env_freefalse,
           (unsigned long long)kernel.stats.sweep_env_units,
           (unsigned long long)kernel.stats.sweep_cand_seeded,
           (unsigned long long)kernel.stats.sweep_cand_refined,
           (unsigned long long)kernel.stats.sweep_dir_sat,
           (unsigned long long)kernel.stats.sweep_dir_unsat);
    printf("c sweepcost   %llu dedup ticks, %llu sub-solver ticks; closure "
           "%llu/%llu occurrences, %llu/%llu vars whole\n",
           (unsigned long long)kernel.stats.sweep_ticks_dedup,
           (unsigned long long)kernel.stats.sweep_ticks_sub,
           (unsigned long long)kernel.stats.sweep_occ_env,
           (unsigned long long)kernel.stats.sweep_occ_f,
           (unsigned long long)kernel.stats.sweep_vars_closed,
           (unsigned long long)kernel.stats.sweep_vars_total);
    printf("c sweepemit   %llu binaries, %llu not-rup, %llu general pairs, "
           "%llu strip units cited\n",
           (unsigned long long)kernel.stats.sweep_binaries,
           (unsigned long long)kernel.stats.sweep_not_rup,
           (unsigned long long)kernel.stats.sweep_general,
           (unsigned long long)kernel.stats.sweep_strip_cited);
    printf("c ternary     %llu rounds, %llu resolvents, %llu binaries, "
           "%llu ticks\n",
           (unsigned long long)kernel.stats.ternary_rounds,
           (unsigned long long)kernel.stats.ternary_resolvents,
           (unsigned long long)kernel.stats.ternary_binaries,
           (unsigned long long)kernel.stats.ternary_ticks);
    // A resolvent is emitted only if the RUP replay re-derives it, so binaries
    // 0 with declines > 0 means the rule ran and refused.
    printf("c hbr         %llu rounds, %llu probes, %llu binaries, "
           "%llu subsumed, %llu declined, %llu ticks\n",
           (unsigned long long)kernel.stats.hbr_rounds,
           (unsigned long long)kernel.stats.hbr_probes,
           (unsigned long long)kernel.stats.hbr_binaries,
           (unsigned long long)kernel.stats.hbr_subsumed,
           (unsigned long long)kernel.stats.hbr_declined,
           (unsigned long long)kernel.stats.hbr_ticks);
    // `stale backjumps` should be 0: nonzero means conflicts are dropped and
    // re-derived, which is otherwise invisible in a tier run.
    printf("c chrono      %llu chronological, %llu stale backjumps\n",
           (unsigned long long)kernel.stats.chronological,
           (unsigned long long)kernel.stats.backjump_stale);
    // The second number separates the full mechanism from the subset a version
    // that declines instead of repairing would reach.
    printf("c reuse       %llu conflicts reused, %llu needed watch repair\n",
           (unsigned long long)kernel.stats.conflict_reused,
           (unsigned long long)kernel.stats.conflict_watch_repaired);
    printf("c jumpreason  %llu binary reasons jumped\n",
           (unsigned long long)kernel.stats.jumped_reasons);
    // What a binary-first watch order would be worth: `ordered` over `scans` is
    // the reachability, `large` over `words` the ceiling on an early break.
    printf("c binprop     %llu scans, %llu flagged, %llu words, %llu skipped, "
           "%llu residual-ordered, %llu residual-large\n",
           (unsigned long long)kernel.stats.binprop_scans,
           (unsigned long long)kernel.stats.binprop_flagged,
           (unsigned long long)kernel.stats.binprop_words,
           (unsigned long long)kernel.stats.binprop_skipped,
           (unsigned long long)kernel.stats.binprop_ordered,
           (unsigned long long)kernel.stats.binprop_large);
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
    printf("c congy1      %llu found, %llu units\n",
           (unsigned long long)kernel.stats.congruence_y1_found,
           (unsigned long long)kernel.stats.congruence_y1_units);
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
    // factor_ticks is Factor's dosing currency and what FACTOR_INIT_TICKS caps,
    // so it separates a bound opening burst from an inert arm.
    printf("c factor      %llu factorings, %llu large remainders offered, "
           "%llu ticks\n",
           (unsigned long long)kernel.stats.factorings,
           (unsigned long long)kernel.stats.factor_large_remainders,
           (unsigned long long)kernel.stats.factor_ticks);
    printf("c factor-dem  %llu demotions, %llu fresh vars demoted\n",
           (unsigned long long)kernel.stats.factor_demotions,
           (unsigned long long)kernel.stats.factor_demoted_vars);
    printf("c bve         %llu rounds, %llu eliminated, %llu resolutions\n",
           (unsigned long long)kernel.stats.bve_rounds,
           (unsigned long long)kernel.stats.bve_eliminated,
           (unsigned long long)kernel.stats.bve_resolutions);
    printf("c bve-gates   %llu equiv, %llu andor, %llu ite, %llu xor, "
           "%llu definition; %llu resolvent pairs saved\n",
           (unsigned long long)kernel.stats.bve_gates_equiv,
           (unsigned long long)kernel.stats.bve_gates_and_or,
           (unsigned long long)kernel.stats.bve_gates_ite,
           (unsigned long long)kernel.stats.bve_gates_xor,
           (unsigned long long)kernel.stats.bve_gates_definition,
           (unsigned long long)kernel.stats.bve_gate_pairs_saved);
    // `checked` proves the sub-solve path runs, `definition` above that it
    // finds something, and `budget` whether TACTIX_BVE_DEF_TICKS binds.
    printf("c bve-def     %llu checked, %llu ticks, %llu hit the tick "
           "ceiling\n",
           (unsigned long long)kernel.stats.bve_definitions_checked,
           (unsigned long long)kernel.stats.bve_definition_ticks,
           (unsigned long long)kernel.stats.bve_definition_budget);
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
    printf("c redscan     %llu scans, %llu arena words walked, %llu "
           "skippable by a first_reducible watermark (%.2f%%)\n",
           (unsigned long long)kernel.stats.reduce_scans,
           (unsigned long long)kernel.stats.reduce_scan_words,
           (unsigned long long)kernel.stats.reduce_skippable_words,
           kernel.stats.reduce_scan_words
               ? 100.0 * (double)kernel.stats.reduce_skippable_words /
                     (double)kernel.stats.reduce_scan_words
               : 0.0);
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
    printf("c budgetstop  %llu C1 rounds, %llu C2 rounds stopped mid-round\n",
           (unsigned long long)kernel.stats.budget_stops_c1,
           (unsigned long long)kernel.stats.budget_stops_c2);
    // Diagnostic, zero unless TACTIX_SUBSUME_SHADOW=1. `observed` is what
    // SubsumeForward declined to delete; absorption by any rule other than
    // Strengthen is not counted, so `residual` overstates its unique yield
    // rather than flattering it.
    printf("c c1shadow    %llu observed, %llu deleted + %llu rewritten by C2, "
           "%llu residual\n",
           (unsigned long long)kernel.stats.c1_shadow_observed,
           (unsigned long long)kernel.stats.c1_shadow_deleted,
           (unsigned long long)kernel.stats.c1_shadow_rewritten,
           (unsigned long long)(kernel.stats.c1_shadow_observed -
                                kernel.stats.c1_shadow_deleted -
                                kernel.stats.c1_shadow_rewritten));
    // Non-wedge witness for the touched-epoch filters (Kernel::touched):
    // `marks` must keep growing on a long solve, `skipped` and `checked` (the
    // `c subsume` / `c strengthen` lines) must both be non-zero, and `advances`
    // 0 with rounds > 0 says every round is being cut.
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
    // Which rule honoured the interrupt: `c interrupt` above sums over every
    // polling rule, so it cannot show a particular round is interruptible.
    printf("c interrupt3  BCE %llu, X2 %llu, X4 %llu aborts\n",
           (unsigned long long)kernel.stats.bce_interrupted,
           (unsigned long long)kernel.stats.ternary_interrupted,
           (unsigned long long)kernel.stats.instantiate_interrupted);
    // TODO: fold this and `c interrupt3` into one line once Vivify's share
    // lands; they are separate only because the shares landed separately.
    printf("c interrupt2  C1 %llu, C2 %llu aborts\n",
           (unsigned long long)kernel.stats.subsume_interrupted,
           (unsigned long long)kernel.stats.strengthen_interrupted);
    printf("c touchred    %llu redundant creations in scope (dynamic) vs %llu "
           "(static tier2=6), %llu band marks; band %s\n",
           (unsigned long long)kernel.stats.touch_redundant_dynamic,
           (unsigned long long)kernel.stats.touch_redundant_static6,
           (unsigned long long)kernel.stats.touch_kept_marks,
           kernel.touch_redundant ? "ON" : "off");
    // A pair because `checked` alone cannot distinguish a scope that admits
    // nothing from a filter that admits everything.
    printf("c f7band      C2 %llu band candidates checked / %llu skipped, "
           "%llu subsumed / %llu strengthened\n",
           (unsigned long long)kernel.stats.strengthen_red_checked,
           (unsigned long long)kernel.stats.strengthen_red_skipped,
           (unsigned long long)kernel.stats.strengthen_red_subsumed,
           (unsigned long long)kernel.stats.strengthen_red_strengthened);
    printf("c touch4      Factor %llu seeds skipped / %llu rearmed\n",
           (unsigned long long)kernel.stats.factor_mark_skipped,
           (unsigned long long)kernel.stats.factor_rearmed);
    // --- `c timing`: where the wall clock actually goes ---------------------
    // Rules are charged individually by the registry dispatch wrapper, inside
    // the slot and out. The slot line is the whole inprocessing phase, so slot
    // minus the rules in it is scaffolding, and search is the residual solve
    // minus slot: the per-conflict path is deliberately untimed, a clock there
    // being exactly the hot-path cost N3 cannot pay.
    {
      const double total_ms = kernel.stats.solve_nanos / 1e6;
      const double slot_ms = kernel.stats.inprocess_nanos / 1e6;
      auto pct = [&](double ms) { return total_ms > 0 ? 100.0 * ms / total_ms : 0.0; };
      printf("c timing      %.0f ms solve\n", total_ms);
      printf("c timing      %.0f ms (%.1f%%) inprocessing slot, %.0f ms (%.1f%%) "
             "search+scaffolding outside it\n",
             slot_ms, pct(slot_ms), total_ms - slot_ms, pct(total_ms - slot_ms));
      // Per rule, biggest first; only rules that actually ran.
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
