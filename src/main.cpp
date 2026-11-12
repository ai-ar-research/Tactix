// Competition frontend (plan §6, compete skill): DIMACS in (plain or
// .xz/.gz via decompressor pipe), `s`/`v` lines on stdout, exit codes
// 10/20/other, proof to argv[2], SIGTERM/SIGINT/SIGXCPU -> flush proof,
// print `s UNKNOWN`, exit cleanly.
#include "frontend/dsr_follow_on.hpp"
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "strategy/feedback_trace.hpp"
#include "strategy/feedback_provenance.hpp"
#include "diagnostics/rule_journal.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp" // BveGate enum for --bve-gates
#include "rules/registry.hpp" // now_nanos + the rule list for `c timing`
#include "strategy/structural_action_sessions.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include <cinttypes>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace tactix;

static volatile sig_atomic_t caught_signal = 0;
static void handle_signal(int) { caught_signal = 1; }

struct CloseFile {
  void operator()(FILE *f) const { std::fclose(f); }
};

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
  char line[4096];
  char *cursor = line;
  *cursor++ = 'v';
  auto flush_line = [&] {
    *cursor++ = '\n';
    fwrite(line, 1, static_cast<size_t>(cursor - line), stdout);
    cursor = line;
    *cursor++ = 'v';
  };
  for (Var v = 0; v < kernel_num_output_vars(k); v++) { // external originals
    const Lit pos = make_lit(v, false);
    const int e = static_cast<int>(v) + 1;
    // Strict > 0, as tactix_value: an unextended value 0 prints as false.
    if (cursor - line + 16 > 4094)
      flush_line();
    *cursor++ = ' ';
    const int value = k.model_values[pos] > 0 ? e : -e;
    const std::to_chars_result converted =
        std::to_chars(cursor, line + 4094, value);
    assert(converted.ec == std::errc{});
    cursor = converted.ptr;
  }
  if (cursor - line + 2 > 4094)
    flush_line();
  *cursor++ = ' ';
  *cursor++ = '0';
  flush_line();
}

// Build provenance only when feedback is requested. File content identities
// permit relocated immutable inputs/builds; destination paths are not choices.
static bool feedback_metadata(const char *input, uint32_t bve_gates,
                              const DefaultStrategyConfig &default_config,
                              std::string &metadata) {
  std::string build_sha, input_sha;
  if (!strategy::controller::feedback_file_sha256("/proc/self/exe", build_sha) ||
      !strategy::controller::feedback_file_sha256(input, input_sha)) return false;
  metadata = "tactix-frontend-feedback-v1\n";
  const auto field = [&](std::string_view name, std::string_view value) {
    metadata += std::string(name) + '=' + std::to_string(value.size()) + ':' +
                std::string(value) + '\n';
  };
  field("build-sha256", build_sha);
  field("input-sha256", input_sha);
  field("policy", "default");
  field("bve-gates", std::to_string(bve_gates));
  field("configuration", default_config.serialize());
  static constexpr const char *environment[] = {
#define TACTIX_FEEDBACK_RUNTIME_ENVIRONMENT
#define TACTIX_FEEDBACK_ENVIRONMENT(name) #name,
#include "strategy/feedback_environment.inc"
#undef TACTIX_FEEDBACK_ENVIRONMENT
#undef TACTIX_FEEDBACK_RUNTIME_ENVIRONMENT
  };
  std::vector<std::pair<std::string, std::string>> values;
  const auto environment_field = [&](const std::string &name) {
    if (const char *value = std::getenv(name.c_str())) values.emplace_back(name, value);
  };
  for (const char *name : environment) environment_field(name);
  // DefaultPolicy's per-rule environment keys are constructed dynamically.
  for (const auto &rule : tactix_rules()) {
    std::string name(rule.name);
    for (char &c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    environment_field("TACTIX_CURSOR_" + name);
    environment_field("TACTIX_ORDER_" + name);
  }
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  for (const auto &[name, value] : values) field(name, value);
  return true;
}

// Reports a frontend failure on stderr, then the `s UNKNOWN` verdict.
[[gnu::format(printf, 1, 2)]] static int fail_unknown(const char *format,
                                                      ...) {
  va_list arguments;
  va_start(arguments, format);
  vfprintf(stderr, format, arguments);
  va_end(arguments);
  printf("s UNKNOWN\n");
  return 1;
}

// Whether `path` and `other` name one file, canonically or by identity.
static bool same_file(const char *path, const char *other) {
  std::error_code a, b, c;
  return (std::filesystem::weakly_canonical(path, a) ==
              std::filesystem::weakly_canonical(other, b) && !a && !b) ||
         std::filesystem::equivalent(path, other, c);
}

// Poses a `uint32_t` kernel knob from a decimal environment value.
static void env_u32(const char *name, uint32_t &knob) {
  if (const char *value = getenv(name))
    knob = (uint32_t)strtoul(value, nullptr, 10);
}

// Poses a boolean kernel knob: any value but a leading '0' enables it.
static void env_flag(const char *name, bool &knob) {
  if (const char *value = getenv(name))
    knob = *value != '0';
}

// Parses an `on`/`off` option value into `out`; false for anything else.
static bool on_off(const char *value, std::optional<bool> &out) {
  if (!strcmp(value, "on")) out = true;
  else if (!strcmp(value, "off")) out = false;
  else return false;
  return true;
}

// `part` as a percentage of `whole`, or 0 when `whole` is 0.
static double percent_of(uint64_t part, uint64_t whole) {
  return whole ? 100.0 * (double)part / (double)whole : 0.0;
}

// One counter of a `c` statistics line, printed as `<value> <label>`.
struct StatCount {
  unsigned long long value;
  const char *label;
};

// Prints a `c` statistics line: `prefix`, then the counters joined by
// `separator`, in one stdio call.
static void stat_line(const char *prefix,
                      std::initializer_list<StatCount> counts,
                      const char *separator = ", ") {
  std::string line = prefix;
  const char *join = "";
  for (const StatCount &count : counts) {
    line += join;
    line += std::to_string(count.value);
    line += ' ';
    line += count.label;
    join = separator;
  }
  line += '\n';
  fputs(line.c_str(), stdout);
}

// Poses a fresh kernel from the command line and the environment: the main
// run's and the DSR follow-on's.
static void pose_kernel(Kernel &kernel, uint32_t bve_gate_mask,
                        bool compact_affine_provenance) {
  kernel.affine_matrix_provenance_compact = compact_affine_provenance;
  kernel_defer_cold_state(kernel);
  kernel.bve_gate_mask = bve_gate_mask;
  // Each env knob poses one `Kernel` field, which documents its default; they
  // are read here so configuration is posed before DefaultPolicy owns it.
  env_u32("TACTIX_BCE_OCC_LIMIT", kernel.bce_occ_limit);
  env_u32("TACTIX_TERNARY_OCC_LIMIT", kernel.ternary_occ_limit);
  env_u32("TACTIX_TERNARY_MAXADD", kernel.ternary_max_add_percent);
  env_flag("TACTIX_BVE_BACKWARD", kernel.bve_backward);
  env_flag("TACTIX_BVE_ON_THE_FLY", kernel.bve_on_the_fly_strengthening);
  if (const char *dt = getenv("TACTIX_BVE_DEF_TICKS"))
    kernel.bve_definition_budget = strtoull(dt, nullptr, 10);
  if (const char *dc = getenv("TACTIX_BVE_DEF_CORES"))
    kernel.bve_definition_cores = std::clamp<uint32_t>(
        static_cast<uint32_t>(strtoul(dc, nullptr, 10)), 1, 100);
  env_u32("TACTIX_BVE_DEF_CORE_MULTIPLIER",
          kernel.bve_definition_core_budget_multiplier);
  env_flag("TACTIX_V4_EARLY_EXIT", kernel.transitive_early_exit);
  env_flag("TACTIX_TRANSITIVE_LIVE_METER", kernel.transitive_live_meter);
  if (const char *meter = getenv("TACTIX_V4_METER"))
    kernel.transitive_budget_checked = !std::strcmp(meter, "checked");
  // =0 also restores the older probe/candidate order (Kernel::vivify_reuse).
  env_flag("TACTIX_VIVIFY_REUSE", kernel.vivify_reuse);
  env_flag("TACTIX_VIVIFY_FLUSH", kernel.vivify_flush);
  env_u32("TACTIX_FACTOR_SIZE", kernel.factor_size);
  // Kissat's `reduction > bound` acceptance threshold (factor.c:939).
  env_u32("TACTIX_FACTOR_BOUND", kernel.factor_bound);
  env_flag("TACTIX_FACTOR_DEMOTE", kernel.factor_demote_fresh);
  env_u32("TACTIX_FACTOR_CAND_ROUNDS", kernel.factor_cand_rounds);
  env_u32("TACTIX_STOP_FUZZ", kernel.stop_fuzz); // see kernel_terminating
  if (getenv("TACTIX_SUBST_DEMOTE"))
    kernel.subst_demote = true; // rank substituted vars last, see Kernel
  // Kissat `eagersubsume` (learn.c): default and maximum 4; 0..4 dose it.
  if (const char *es = getenv("TACTIX_EAGER_SUBSUME")) {
    const unsigned long n = strtoul(es, nullptr, 10);
    kernel.eager_subsume = n > Kernel::EAGER_SUBSUME_SLOTS
                               ? Kernel::EAGER_SUBSUME_SLOTS
                               : (uint32_t)n;
  }
  if (getenv("TACTIX_NO_EXT_REASON_BUMP"))
    kernel.no_ext_reason_bump = true; // arm, see Kernel::no_ext_reason_bump
  // Kissat's one_literal_on_conflict_level shortcut.
  kernel.conflict_reuse = true;
  env_flag("TACTIX_CONFLICT_REUSE", kernel.conflict_reuse);
  // Kissat's `jumpreasons`.
  env_flag("TACTIX_JUMP_REASONS", kernel.jump_reasons);
  // Kissat `promote` and `tumble`.
  env_flag("TACTIX_SHRINK_GLUE", kernel.shrink_glue_promote);
  env_flag("TACTIX_TUMBLE", kernel.tumble);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IOFBF, 1 << 20);
  const bool frontend_time = getenv("TACTIX_FRONTEND_TIME") != nullptr;
  const char *cnf_path = nullptr;
  const char *proof_path = nullptr;
  enum class CliProofFamily { Dsr, Pbp, Automatic };
  // Default: DSR, converted once to PBP where needed (Proof::open_automatic).
  CliProofFamily proof_family = CliProofFamily::Automatic;
  const char *proof_stage_path = nullptr;
  bool binary_proof = true;
  bool quiet = false;
  std::optional<bool> default_affine_search;
  std::optional<DefaultStrategyConfig::AffineSearch::ReasonMode>
      default_affine_reason_mode;
  std::optional<bool> default_graph_labeling;
  std::optional<bool> default_structured_symmetry;
  const char *structural_provenance_path = nullptr;
  bool compact_affine_provenance = false;
  const char *feedback_path = nullptr;
  const char *feedback_replay_path = nullptr;
  uint32_t bve_gate_mask = BVE_GATE_KISSAT_DEFAULT;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--version")) {
      puts("0.2.1");
      return 0;
    } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
      quiet = true;
    } else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) {
      // Progress lines from the default strategy (src/policies/progress.hpp).
      setenv("TACTIX_VERBOSE", "1", 1);
    } else if (!strcmp(a, "--no-binary")) {
      binary_proof = false;
    } else if (!strcmp(a, "--dsr")) {
      proof_family = CliProofFamily::Dsr;
    } else if (!strcmp(a, "--pbp")) {
      proof_family = CliProofFamily::Pbp;
    } else if (!strcmp(a, "--auto-proof")) {
      proof_family = CliProofFamily::Automatic;
    } else if (!strncmp(a, "--proof-stage=", 14) && a[14]) {
      proof_stage_path = a + 14;
    } else if (!strncmp(a, "--default-affine-search=", 24)) {
      if (!on_off(a + 24, default_affine_search))
        return fail_unknown("tactix: --default-affine-search requires on or off\n");
    } else if (!strncmp(a, "--default-affine-reasons=", 25)) {
      using ReasonMode = DefaultStrategyConfig::AffineSearch::ReasonMode;
      if (!strcmp(a + 25, "eager"))
        default_affine_reason_mode = ReasonMode::EagerSourceSupport;
      else if (!strcmp(a + 25, "deferred"))
        default_affine_reason_mode = ReasonMode::Deferred;
      else if (!strcmp(a + 25, "detached-minimal"))
        default_affine_reason_mode = ReasonMode::DetachedMinimal;
      else if (!strcmp(a + 25, "detached-source"))
        default_affine_reason_mode = ReasonMode::DetachedSource;
      else if (!strcmp(a + 25, "lazy-source"))
        default_affine_reason_mode = ReasonMode::LazySource;
      else if (!strcmp(a + 25, "lazy-minimal"))
        default_affine_reason_mode = ReasonMode::LazyMinimal;
      else
        return fail_unknown(
            "tactix: --default-affine-reasons requires eager, deferred, "
            "detached-minimal, detached-source, lazy-source, or "
            "lazy-minimal\n");
    } else if (!strncmp(a, "--default-graph-labeling=", 25)) {
      if (!on_off(a + 25, default_graph_labeling))
        return fail_unknown("tactix: --default-graph-labeling requires on or off\n");
    } else if (!strncmp(a, "--default-structured-symmetry=", 30)) {
      if (!on_off(a + 30, default_structured_symmetry))
        return fail_unknown("tactix: --default-structured-symmetry requires on or off\n");
    } else if (!strncmp(a, "--structural-provenance=", 24)) {
      structural_provenance_path = a + 24;
    } else if (!strcmp(a, "--compact-affine-provenance")) {
      compact_affine_provenance = true;
    } else if (!strncmp(a, "--feedback=", 11)) {
      feedback_path = a + 11;
    } else if (!strncmp(a, "--feedback-replay=", 18)) {
      feedback_replay_path = a + 18;
    } else if (!strncmp(a, "--bve-gates=", 12)) {
      // Comma list of {equiv,andor,ite,xor,def}, or all / none; a disabled
      // gate type falls BVE back to the cross product.
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
            return fail_unknown("tactix: bad --bve-gates spec '%s'\n", spec);
          }
        }
      }
    } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      printf("usage: tactix [-q|-v] [--auto-proof|--dsr|--pbp] "
             "[--no-binary] [--proof-stage=FILE] "
             "[--default-affine-search=on|off] "
             "[--default-affine-reasons=eager|deferred|detached-minimal|detached-source|lazy-source|lazy-minimal] "
             "[--default-graph-labeling=on|off] "
             "[--default-structured-symmetry=on|off] "
             "[--feedback=FILE | --feedback-replay=FILE] "
             "[--structural-provenance=NEW_FILE] [--compact-affine-provenance] "
             "[--bve-gates=all|none|equiv,andor,ite,xor,def] "
             "<cnf[.xz|.gz]> [proof]\n");
      return 0;
    } else if (a[0] == '-' && a[1]) {
      return fail_unknown("tactix: unknown option '%s'\n", a);
    } else if (!cnf_path) {
      cnf_path = a;
    } else if (!proof_path) {
      proof_path = a;
    } else {
      return fail_unknown("tactix: too many arguments\n");
    }
  }

  const ProofFormat proof_format =
      proof_family == CliProofFamily::Pbp ? ProofFormat::PbpStreaming
      : binary_proof                      ? ProofFormat::DsrBinary
                                          : ProofFormat::DsrAscii;
  if (!cnf_path)
    return fail_unknown("tactix: no input file\n");

  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);
#ifdef SIGXCPU
  signal(SIGXCPU, handle_signal);
#endif

  if ((feedback_path && feedback_replay_path) ||
      (feedback_path && !*feedback_path) ||
      (feedback_replay_path && !*feedback_replay_path))
    return fail_unknown(
        "tactix: choose one nonempty feedback recording or replay path\n");
  // An output artifact is truncated when opened: it must not name another one.
  if (structural_provenance_path)
    for (const char *other : {cnf_path, proof_path, feedback_path, feedback_replay_path})
      if (other && same_file(structural_provenance_path, other)) {
        fprintf(stderr, "tactix: structural provenance aliases another artifact\n");
        return 1;
      }
  if (proof_stage_path && (!proof_path ||
                           proof_family != CliProofFamily::Automatic))
    return fail_unknown("tactix: --proof-stage needs an automatic proof\n");
  if (proof_stage_path)
    for (const char *other : {cnf_path, proof_path, feedback_path, feedback_replay_path,
                              structural_provenance_path})
      if (other && same_file(proof_stage_path, other))
        return fail_unknown("tactix: proof stage aliases another artifact\n");
  // Replay input must likewise survive opening the requested proof output.
  if (const char *record = feedback_path ? feedback_path : feedback_replay_path)
    for (const char *other : {cnf_path, proof_path})
      if (other && same_file(record, other))
        return fail_unknown("tactix: feedback path aliases input or proof\n");
  Kernel kernel;
  StructuralActionSessions structural_action_sessions(kernel);
  pose_kernel(kernel, bve_gate_mask, compact_affine_provenance);
  // Structural actions record into a bound journal: a named one is created
  // exclusively and outlives the policy; otherwise the discard journal.
  std::unique_ptr<FILE, CloseFile> structural_records;
  if (structural_provenance_path) {
    std::string input_sha;
    if (!strategy::controller::feedback_file_sha256(cnf_path, input_sha)) {
      fprintf(stderr, "tactix: cannot identify structural provenance input\n");
      return 1;
    }
    structural_records.reset(std::fopen(structural_provenance_path, "wx"));
    if (!structural_records) {
      fprintf(stderr, "tactix: cannot create structural provenance stream\n");
      return 1;
    }
    std::fprintf(structural_records.get(),
        "{\"kind\":\"session\",\"version\":1,\"input_file_sha256\":\"%s\"}\n", input_sha.c_str());
    std::fflush(structural_records.get());
    if (std::ferror(structural_records.get())) return 1;
    kernel.structural_provenance = structural_records.get();
  } else {
    kernel.structural_provenance = StructuralJournal::discard();
  }
  Proof proof;
  // --no-binary spells DSR in ASCII; PBP is textual. N2: a run without a proof
  // file answers to the same writer as a --pbp run, with nothing to write.
  kernel.startup_certificate_open = true;
  const bool opened =
      !proof_path ? proof.open_nowhere()
      : proof_family == CliProofFamily::Automatic
          ? proof.open_automatic(proof_path, binary_proof, proof_stage_path)
          : proof.open(proof_path, proof_format);
  if (!opened)
    return fail_unknown("tactix: cannot write proof to '%s'\n",
                        proof_path ? proof_path : "/dev/null");
  kernel.proof = &proof;
  proof.bind_variable_map(&kernel.internal_to_external);
  proof.bind_external_variable_map(&kernel.external_to_internal);

  bool piped = false;
  FILE *in = open_input(cnf_path, &piped);
  if (!in)
    return fail_unknown("tactix: cannot read '%s'\n", cnf_path);
  const uint64_t parse_t0 = frontend_time ? tactix::now_nanos() : 0;
  const char *err = parse_dimacs(in, kernel);
  const uint64_t parse_nanos =
      frontend_time ? tactix::now_nanos() - parse_t0 : 0;
  // A failed decompressor leaves a truncated formula that must not be solved.
  const int close_status = piped ? pclose(in) : fclose(in);
  if (!err && piped && close_status != 0) {
    fail_unknown("tactix: decompressing '%s' failed\n", cnf_path);
    fflush(stdout);
    return 1;
  }
  if (err) {
    fail_unknown("tactix: parse error: %s\n", err);
    fflush(stdout);
    return 1;
  }

  const uint64_t solve_t0 = tactix::now_nanos();
  DefaultStrategyConfig default_config;
  if (default_affine_search) default_config.affine_search.enabled = *default_affine_search;
  if (default_affine_reason_mode)
    default_config.affine_search.reason_mode = *default_affine_reason_mode;
  if (default_graph_labeling)
    default_config.graph_labeling_model.enabled = *default_graph_labeling;
  if (default_structured_symmetry)
    default_config.inprocess.structured_root_symmetry_enabled =
        *default_structured_symmetry;
  DefaultPolicy default_policy(default_config);
  std::optional<strategy::controller::FeedbackTrace> feedback;
  std::string feedback_setup_error;
  if (feedback_path || feedback_replay_path) {
    std::string metadata;
    if (!feedback_metadata(cnf_path, bve_gate_mask, default_config, metadata)) {
      feedback_setup_error = "cannot read immutable feedback build/input provenance";
    } else {
      metadata += std::string("structural-provenance=1:") +
          (structural_provenance_path ? "1\n" : "0\n");
      if (compact_affine_provenance)
        metadata += "compact-affine-provenance=1:1\n";
      feedback.emplace();
      // Opening/provenance errors stay in the receiver: a rejected header must
      // not steer the policy or suppress its certificate or verdict.
      if (feedback_path) (void)feedback->write(feedback_path, metadata);
      else (void)feedback->replay(feedback_replay_path, metadata);
      default_policy.set_feedback(&*feedback);
    }
  }
  const int res = default_policy.solve(kernel, &caught_signal);
  kernel.stats.solve_nanos = tactix::now_nanos() - solve_t0;
  rule_journal_close();

  proof.flush();
  const bool feedback_failed = !feedback_setup_error.empty() ||
      (feedback && (!feedback->error().empty() || !feedback->terminal()));
  if (feedback_failed)
    fprintf(stderr, "tactix: feedback verification: %s\n",
            !feedback_setup_error.empty() ? feedback_setup_error.c_str() :
            feedback->error().empty() ? "incomplete session" : feedback->error().c_str());
  if (!quiet) {
    // Every registered rule gets a stats line; most pair a yield with a
    // reachability witness, whose zero means the mechanism never ran.
    using ull = unsigned long long;
    const Stats &s = kernel.stats;
    printf("c tactix 0.2.1\n");
    {
      const auto &affine = default_policy.affine_search_diagnostics();
      // The affine search block prints only when enabled or a counter moved.
#define AS_(name) {#name, affine.name}
#define AM_(name) {#name, s.affine_matrix_##name}
#define AI_(name) {#name, s.affine_matrix_init_##name}
      const std::pair<const char *, uint64_t> affine_head[] = {
        {"enabled", default_config.affine_search.enabled}, AS_(initializations),
        AI_(normalization_rup_checks), AI_(shortening_additions), AI_(maintenance_service_work),
        AI_(discovery_recognized_groups), AI_(discovery_skipped_groups),
        AI_(discovery_recognizer_work), AS_(refreshes), AS_(unchanged_refreshes),
        AS_(empty_maintenance_skips), AS_(unchanged_census_skips), AS_(usefulness_resumptions),
        AS_(paused_search_skips), AS_(root_services), AS_(search_services), AS_(reasons_published),
        AS_(implications_consumed), AS_(reasons_deferred), AS_(reasons_materialized),
        AM_(analyze_handles), AM_(analyze_materializations), AM_(analyze_vectors_constructed),
        AM_(analyze_literals_constructed), AS_(reasons_detached), AS_(materialization_attempts),
        AS_(materialization_inapplicable), AS_(materialization_stale), AS_(materialization_invalid),
        AS_(materialization_exhausted), AS_(materialization_interrupted),
        AS_(materialization_unsupported_proof), AS_(materialization_retained_authentications),
        AS_(materialization_last_audit_stage), AS_(admitted_sources), AS_(active_sources),
      };
      const std::pair<const char *, uint64_t> affine_tail[] = {
        AS_(retained_basis_rank), AS_(retained_support_variables), AS_(rank_before_last_action),
        AS_(rank_after_last_action), AS_(support_before_last_action),
        AS_(support_after_last_action), AS_(recognition_rank_gains), AS_(recognition_rank_losses),
        AS_(maintenance_rank_gains), AS_(maintenance_rank_losses), AS_(service_rank_losses),
        AS_(isolated_projection_rank_losses), AS_(native_session_disables),
        AS_(optional_failure_disables), AS_(service_inapplicable), AS_(service_interrupted),
        AS_(service_unsupported_proof), AS_(service_stale), AS_(service_invalid),
        AS_(last_service_status), AS_(last_terminal_status), AS_(last_terminal_audit_stage),
        AS_(last_terminal_transaction_stage), AS_(last_terminal_synchronization_stage),
        AS_(paused_basis_rank), AS_(paused_support_variables), AS_(last_survival_action),
        AS_(last_disable_cause),
        {"retained_bytes", structural_action_sessions.affine_matrix_search.retained_bytes()},
        AS_(retired_sources), AS_(projected_variables), AS_(initialization_work), AS_(service_work),
        AS_(incremental_capture_calls), AS_(unchanged_support_calls), AS_(cached_event_calls),
        AS_(restart_runs), AS_(unchanged_skips), AS_(trigger_work), AS_(delayed_runs),
        AS_(fruitful_runs), AS_(fruitless_runs), AS_(run_conflict_limit), AS_(delay_current),
        AS_(run_effort_last), AS_(run_effort_total), AS_(run_work), AS_(run_steps),
        AS_(pending_slices), AS_(resumed_slices), AS_(completed_slice_steps), AS_(slice_work),
        AS_(slice_cumulative_work), AS_(minimum_slice_work), AS_(capture_delta_literals),
        AS_(premise_maintenance_work), AS_(alias_work), AS_(alias_proof_additions),
        AS_(proof_additions), AS_(proof_deletions), AS_(overhead_nanos), AS_(synchronization_nanos),
        AS_(publication_nanos), AS_(resource_declines), AS_(usefulness_disabled),
      };
      bool affine_active = false;
      for (const auto &metric : affine_head) affine_active |= metric.second != 0;
      for (const auto &metric : affine_tail) affine_active |= metric.second != 0;
      if (affine_active)
        for (const auto &[name, value] : affine_head)
          printf("c affine-search %s=%llu\n", name, (ull)value);
      const auto &graph = default_policy.graph_labeling_diagnostics();
#define GS_(name) {#name, graph.name}
#define GL_(name) {#name, s.graph_labeling_##name}
      const std::pair<const char *, uint64_t> graph_metrics[] = {
        {"enabled", default_config.graph_labeling_model.enabled}, GS_(authentication_attempts),
        GS_(authentication_successes), GS_(authentication_exhausted),
        GS_(authentication_no_structure), GS_(authentication_interrupted), GS_(authentication_work),
        GS_(slices), GS_(pending_slices), GS_(completed_trajectories), GS_(useful_hints), GS_(runs),
        GS_(delayed_runs), GS_(fruitful_runs), GS_(fruitless_runs), GS_(run_conflict_limit),
        GS_(delay_current), GL_(recognized), GL_(vertices), GL_(separation), GL_(work), GL_(nodes),
        GL_(repairs), GL_(anneal_attempts), GL_(sat), {"search_ticks", s.search_ticks},
      };
#undef AS_
#undef AM_
#undef AI_
#undef GS_
#undef GL_
      for (const auto &[name, value] : graph_metrics)
        printf("c graph-schedule %s=%llu\n", name, (ull)value);
      if (affine_active) {
        printf("c affine-survival gauges=last-native-boundary rank=unconditioned-coefficient-rank "
               "sources=retained-equations pause=last-pause "
               "action-codes=1:recognition,2:maintenance,3:root-service,4:search-service "
               "disable-codes=1:native-clear,2:optional-terminal-failure\n");
        printf("c affine-service status-codes=0:disabled,1:no-event,2:duplicate,3:applied,"
               "4:inapplicable,5:resource-exhausted,6:interrupted,7:unsupported-proof,"
               "8:stale,9:invalid,10:pending audit-codes=0:none,1:prepared-contract,"
               "2:proof-support,3:applicability,4:kernel-snapshot,5:trail-or-event,"
               "6:authentication-or-sources,7:source-identity,8:reason-order,9:pbp-plan,"
               "10:matrix-authentication,11:selected-sources,12:resume-sources,"
               "13:transaction-plan\n");
        printf("c affine-transaction failure-stage-codes=0:none,1:request,"
               "2:coordinate-frame,3:dense-mapping,4:cache-population,"
               "5:certificate-planning,6:certificate-semantics,"
               "7:proof-population\n");
        printf("c affine-synchronization failure-stage-codes=0:none,1:request,"
               "2:trail-order,3:trail-support,4:simplex-preparation,"
               "5:cached-assignment,6:deferred-reason,7:source-identity\n");
        for (const auto &[name, value] : affine_tail)
          printf("c affine-search %s=%llu\n", name, (ull)value);
        // Rare: printed only when nonzero.
        if (s.affine_matrix_analyze_retractions)
          printf("c affine-search analyze_retractions=%llu\n",
                 (ull)s.affine_matrix_analyze_retractions);
      }
    }
    stat_line("c count-pb    ", {{s.counting_calls, "calls"}, {s.counting_stars, "stars"},
                                 {s.counting_unsat, "UNSAT"}, {s.counting_work, "work"},
                                 {s.counting_interrupted, "interrupted"},
                                 {s.counting_additions, "additions"}});
    stat_line("c input       ", {{s.input_restated, "clauses restated (normalized literals)"}});
    printf("c decisions   %llu (%llu via API)\n", (ull)s.decisions, (ull)s.api_decisions);
    printf("c conflicts   %llu\n", (ull)s.conflicts);
    printf("c propagations %llu (%llu search, %llu inprocess)\n", (ull)s.propagations,
           (ull)(s.propagations - s.inprocess_propagations), (ull)s.inprocess_propagations);
    printf("c learned     %llu (%llu units)\n", (ull)s.learned, (ull)s.learned_units);
    if (s.learned) {
      printf("c learnedsize %.2f avg lits, %.2f avg glue\n",
             (double)s.learned_lits / (double)s.learned,
             (double)s.learned_glue_sum / (double)s.learned);
      const double kept = (double)s.learned_lits;
      const double removed = (double)(s.minimized + s.shrunken);
      printf("c reduction   %.1f%% of resolved lits removed by minimize+shrink\n",
             100.0 * removed / (kept + removed));
    }
    stat_line("c minimized   ", {{s.minimized, "lits"}});
    stat_line("c min-depth   ", {{s.minimize_depth_cutoffs, "cutoffs"}});
    stat_line("c shrunken    ", {{s.shrunken, "lits"}});
    printf("c reasonbump  %llu vars in %llu rounds (%llu depth2, %llu aborted)\n",
           (ull)s.reason_bumped, (ull)s.reason_bump_rounds, (ull)s.reason_bump_depth2,
           (ull)s.reason_bump_aborted);
    printf("c otfs        %llu strengthened (%llu conflicts subsumed)\n", (ull)s.otfs_strengthened,
           (ull)s.otfs_subsumed);
    printf("c eagersub    %llu subsumed of %llu tried (%llu kept as reason), "
           "register %u\n", (ull)s.eager_subsumed, (ull)s.eager_tried, (ull)s.eager_reason_skipped,
           (unsigned)kernel.eager_subsume);
    // Reachability readouts for TACTIX_SHRINK_GLUE / TACTIX_TUMBLE.
    printf("c shrinkglue  %llu promoted (clamp %s)\n", (ull)s.shrink_promoted,
           kernel.shrink_glue_promote ? "size-1" : "size");
    printf("c tumble      %llu vars seeded (%s)\n", (ull)s.tumble_seeded,
           kernel.tumble ? "activation order" : "off");
    // Kissat's warmup counters: the readout of the sweep that seeds Walk.
    stat_line("c warmup      ", {{s.warmups, "warmups"}, {s.warming_decisions, "decisions"},
                                 {s.warming_propagations, "propagations"}});
    stat_line("c denseocc    ", {{s.dense_slice_moves, "slice moves"},
                                 {s.dense_defrags, "defrags"}});
    stat_line("c sparsecol   ", {{s.sparse_collections, "runs"}, {s.sparse_collected, "clauses"}});
    // `vivified` counts strengthened clauses only; kissat's (statistics.h:247)
    // is an aggregate of the `c vivify3` counters.
    stat_line("c vivify      ", {{s.vivify_rounds, "rounds"}, {s.vivified, "vivified"},
                                 {s.vivify_deleted, "deleted"}, {s.vivify_flushed, "flushed"},
                                 {s.vivify_checked, "checked"}});
    stat_line("c vivify2     ", {{s.vivify_reused, "reused"}, {s.vivify_probes, "probes"},
                                 {s.vivify_reuse_full, "full"}, {s.vivify_tier3, "tier3"},
                                 {s.vivify_prioritized, "prio"}, {s.vivify_reprioritized, "rearm"},
                                 {s.vivify_polls, "polls"}, {s.vivify_terminated, "term"}});
    stat_line("c vivify3     ", {{s.vivify_subsumed, "subsumed"}, {s.vivify_promoted, "promoted"},
                                 {s.vivify_shrunken, "shrunken"}, {s.vivify_implied, "implied"},
                                 {s.vivify_asymmetric, "asym"},
                                 {s.vivify_instantiated, "instantiated"}, {s.vivify_noop, "noop"}});
    stat_line("c probe       ", {{s.probe_rounds, "rounds"}, {s.probed, "probed"},
                                 {s.probe_failed, "failed"}});
    stat_line("c backbone    ", {{s.backbone_rounds, "rounds"}, {s.backbone_probed, "probed"},
                                 {s.backbone_failed, "failed"}});
    stat_line("c transitive  ", {{s.transitive_rounds, "rounds"}, {s.transitive_checked, "checked"},
                                 {s.transitive_deleted, "deleted"}, {s.transitive_units, "units"},
                                 {s.transitive_ticks, "ticks"}, {s.transitive_polls, "polls"},
                                 {s.transitive_terminated, "terminated"}});
    printf("c substitute  %llu rounds, %llu ticks, %llu non-trivial SCCs (%llu lits), "
           "%llu vars substituted, %llu rounds skipped / %llu partial "
           "(%llu lits poisoned) on the hazard check\n", (ull)s.substitute_rounds,
           (ull)s.substitute_ticks, (ull)s.substitute_sccs, (ull)s.substitute_scc_lits,
           (ull)s.substitute_vars, (ull)s.substitute_hazard_skips, (ull)s.substitute_hazard_partial,
           (ull)s.substitute_hazard_lits);
    stat_line("c bce         ", {{s.bce_rounds, "rounds"}, {s.bce_deleted, "deleted"},
                                 {s.bce_ticks, "ticks"}});
    // `cla 0`: no covered literal was added; `blocked 0`: no witness pushed.
    printf("c cce         %llu rounds, %llu tried, %llu deleted "
           "(%llu blocked / %llu ata), %llu ala, %llu cla, %llu ticks\n", (ull)s.cce_rounds,
           (ull)s.cce_tried, (ull)s.cce_deleted, (ull)s.cce_blocked, (ull)s.cce_ata, (ull)s.cce_ala,
           (ull)s.cce_cla, (ull)s.cce_ticks);
    stat_line("c cce2        ", {{s.cce_mark_skipped, "skipped"}, {s.cce_mark_advances, "advances"},
                                 {s.cce_rearmed, "rearmed"}}, " / ");
    stat_line("c instantiate ", {{s.instantiate_rounds, "rounds"},
                                 {s.instantiated, "literals removed"},
                                 {s.instantiate_checked, "probed"}});
    printf("c sweep       %llu rounds, %llu swept, %llu environments (%llu clauses), "
           "%llu solves, %llu candidates, %llu equivalences, %llu units "
           "(%llu already binary), %llu env-unsat, %llu budget, %llu ticks\n", (ull)s.sweep_rounds,
           (ull)s.sweep_swept, (ull)s.sweep_environments, (ull)s.sweep_env_clauses,
           (ull)s.sweep_solves, (ull)s.sweep_candidates, (ull)s.sweep_equivalences,
           (ull)s.sweep_units, (ull)s.sweep_known, (ull)s.sweep_env_unsat, (ull)s.sweep_budget,
           (ull)s.sweep_ticks);
    // Environment shape and candidate-solve outcomes behind `equivalences 0`,
    // then emission per path; `not-rup` is S-d's decline count.
    printf("c sweepdiag   %llu env-lits, %llu root-false (%llu stripped, "
           "%llu FREE), %llu units, %llu cand-seeded, %llu refined, "
           "%llu dir-sat, %llu dir-unsat\n", (ull)s.sweep_env_lits, (ull)s.sweep_env_false,
           (ull)s.sweep_env_stripped, (ull)s.sweep_env_freefalse, (ull)s.sweep_env_units,
           (ull)s.sweep_cand_seeded, (ull)s.sweep_cand_refined, (ull)s.sweep_dir_sat,
           (ull)s.sweep_dir_unsat);
    stat_line("c sweepflip   ", {{s.sweep_flip_attempts, "attempts"},
                                 {s.sweep_flipped, "successful"}});
    stat_line("c sweepback   ", {{s.sweep_backbone_candidates, "candidates"},
                                 {s.sweep_backbone_flipped, "flipped"}});
    printf("c sweepcost   %llu dedup ticks, %llu sub-solver ticks, "
           "%llu representation words; closure "
           "%llu/%llu occurrences, %llu/%llu vars whole\n", (ull)s.sweep_ticks_dedup,
           (ull)s.sweep_ticks_sub, (ull)s.sweep_ticks_repr, (ull)s.sweep_occ_env,
           (ull)s.sweep_occ_f, (ull)s.sweep_vars_closed, (ull)s.sweep_vars_total);
    stat_line("c sweepemit   ", {{s.sweep_binaries, "binaries"}, {s.sweep_not_rup, "not-rup"},
                                 {s.sweep_general, "general pairs"}});
    printf("c sweepsched  %llu scheduled, %llu rescheduled, %llu resumed, "
           "%llu completed, %zu tail; limits %u vars, %u clauses, depth %u\n",
           (ull)s.sweep_scheduled, (ull)s.sweep_rescheduled, (ull)s.sweep_resumed,
           (ull)s.sweep_completed, static_cast<size_t>(kernel.sweep_live), s.sweep_limit_vars,
           s.sweep_limit_clauses, s.sweep_limit_depth);
    stat_line("c ternary     ", {{s.ternary_rounds, "rounds"}, {s.ternary_resolvents, "resolvents"},
                                 {s.ternary_binaries, "binaries"}, {s.ternary_ticks, "ticks"}});
    // A resolvent is emitted only if its RUP replay re-derives it.
    stat_line("c hbr         ", {{s.hbr_rounds, "rounds"}, {s.hbr_probes, "probes"},
                                 {s.hbr_binaries, "binaries"}, {s.hbr_subsumed, "subsumed"},
                                 {s.hbr_declined, "declined"}, {s.hbr_ticks, "ticks"}});
    // `stale backjumps` should be 0; `re-arms` counts clause additions that
    // landed behind the propagation cursor.
    stat_line("c chrono      ", {{s.chronological, "chronological"},
                                 {s.backjump_stale, "stale backjumps"},
                                 {s.propagation_rearms, "propagation re-arms"}});
    stat_line("c reuse       ", {{s.conflict_reused, "conflicts reused"},
                                 {s.conflict_watch_repaired, "needed watch repair"}});
    printf("c jumpreason  %llu binary reasons jumped, %llu classifications, "
           "%llu big-binary; gate %s\n", (ull)s.jumped_reasons, (ull)s.jump_reason_classifications,
           (ull)s.jump_reason_bigbig, kernel.jump_reasons_bigbig ? "on" : "off");
    // What a binary-first watch order would be worth.
    stat_line("c binprop     ", {{s.binprop_scans, "scans"}, {s.binprop_flagged, "flagged"},
                                 {s.binprop_words, "words"}, {s.binprop_skipped, "skipped"},
                                 {s.binprop_ordered, "residual-ordered"},
                                 {s.binprop_large, "residual-large"}});
    stat_line("c inprocess   ", {{s.inprocess_phases, "phases"}});
    stat_line("c conggates   ", {{s.congruence_gates_and, "and"}, {s.congruence_gates_xor, "xor"},
                                 {s.congruence_gates_ite, "ite"}});
    stat_line("c congruence  ", {{s.congruence_rounds, "rounds"},
                                 {s.congruence_binary_rebuilds, "binary rebuilds"},
                                 {s.congruence_gates, "gates"}, {s.congruence_merges, "merges"},
                                 {s.congruence_units, "units"},
                                 {s.congruence_unjustified, "unjustified"},
                                 {s.congruence_ticks, "ticks"},
                                 {s.congruence_budget_stops, "budget stops"}});
    stat_line("c congy1      ", {{s.congruence_y1_found, "found"}, {s.congruence_y1_units, "units"},
                                 {s.congruence_binary_equivalences, "binary equivalences"},
                                 {s.congruence_y1_ticks, "ticks"}});
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
             (ull)(n_sub + n_elim), (ull)n_sub, (ull)n_elim, kernel.nvars);
    }
    printf("c compact     %llu passes removed %llu internal variables\n",
           (ull)s.variable_compactions, (ull)s.compacted_variables);
    stat_line("c deactivate  ", {{s.deactivations, "deactivations"},
                                 {s.deactivation_repeats, "repeated reports"},
                                 {s.reactivations, "re-bumps of retired vars"}});
    printf("c substdec    %llu decisions on substituted vars (%.2f%% of %llu)\n",
           (ull)s.decisions_substituted, percent_of(s.decisions_substituted, s.decisions),
           (ull)s.decisions);
    printf("c extension   %u vars added, %llu decisions on them (%.2f%% of "
           "%llu)\n",
           kernel.first_extension_var == UINT32_MAX ? 0u : static_cast<unsigned>(std::count_if(
               kernel.internal_to_external.begin() + kernel.first_extension_var,
               kernel.internal_to_external.end(),
               [&](Var external) { return external >= kernel.original_external_vars; })),
           (ull)s.decisions_extension,
           percent_of(s.decisions_extension, s.decisions),
           (ull)s.decisions);
    printf("c interrupt   %llu rule applications aborted on the stop flag "
           "(%llu stress fires)\n", (ull)s.rule_interrupted, (ull)s.stop_fuzz_fires);
    printf("c extbumps    %llu of %llu bumps on extension vars (%.2f%%), "
           "%llu reason-side only (%.2f%% of those)\n", (ull)s.bumps_extension, (ull)s.bumps,
           percent_of(s.bumps_extension, s.bumps), (ull)s.bumps_ext_reason_only,
           percent_of(s.bumps_ext_reason_only, s.bumps_extension));
    stat_line("c factor      ", {{s.factorings, "factorings"},
                                 {s.factor_large_remainders, "large remainders offered"},
                                 {s.factor_ticks, "ticks"}});
    stat_line("c factor-rem  ", {{s.factor_remainders_materialized, "materialized"},
                                 {s.factor_remainders_reused, "reused"}});
    stat_line("c factor-bnd  ",
              {{s.factor_rejected_by_bound, "grids rejected by the coupled bound"}});
    stat_line("c factor-dem  ", {{s.factor_demotions, "demotions"},
                                 {s.factor_demoted_vars, "fresh vars demoted"}});
    stat_line("c sr          ",
              {{s.sr_applications, "applications"}, {s.sr_applied, "applied"},
               {s.sr_validation_ticks, "validation ticks"},
               {s.sr_witness_literals, "witness literals"},
               {s.sr_unique_literal_amo_binaries, "unique-literal AMO binaries"}});
    stat_line("c sr-life     ",
              {{s.sr_post_amo_nonhelper_applied, "post-AMO nonhelper consequences"},
               {s.sr_post_amo_graph_root_units, "graph root units"}});
  printf("c sr-admit    %llu nonforcing cut batches declined, %llu cuts; "
         "%llu singleton forcing batches declined, %llu rows\n",
         (ull)s.sr_nonforcing_cut_batches_declined, (ull)s.sr_nonforcing_cuts_declined,
         (ull)s.sr_singleton_forcing_batches_declined, (ull)s.sr_singleton_forcing_rows_declined);
    // Residual search handoff (off by default) prints when enabled or run.
    if (default_config.inprocess.structured_root_residual_search_handoff ||
        s.residual_handoff_calls ||
        s.residual_handoff_variables ||
        s.residual_handoff_nanos)
      printf("c handoff     %llu residual seeds, %llu variables, %.3f seconds\n",
             (ull)s.residual_handoff_calls, (ull)s.residual_handoff_variables,
             s.residual_handoff_nanos / 1e9);
    stat_line("c bve         ", {{s.bve_rounds, "rounds"}, {s.bve_eliminated, "eliminated"},
                                 {s.bve_resolutions, "resolutions"}});
    stat_line("c bve-otf     ", {{s.bve_otf_strengthened, "strengthened"},
                                 {s.bve_otf_subsumed, "subsumed"}});
    printf("c fastel      %llu checks, %llu candidates / %llu active, "
           "%llu discovery ticks, %llu budget stops, %llu minority declines, "
           "%llu root-suppressed, %llu extensions protected, %llu eliminated, "
           "%llu resolutions, "
           "%.3f seconds\n", (ull)s.fastel_rounds, (ull)s.fastel_candidates, (ull)s.fastel_active,
           (ull)s.fastel_discovery_ticks, (ull)s.fastel_budget_stops, (ull)s.fastel_declined,
           (ull)s.fastel_root_suppressed, (ull)s.fastel_extensions_protected,
           (ull)s.fastel_eliminated, (ull)s.fastel_resolutions, s.fastel_nanos / 1e9);
    stat_line("c bve-bwd     ", {{s.bve_bwd_queued, "subsumers"}, {s.bve_bwd_subsumed, "subsumed"},
                                 {s.bve_bwd_strengthened, "strengthened"},
                                 {s.bve_bwd_units, "units"}});
    printf("c bve-gates   %llu equiv, %llu andor, %llu ite, %llu xor, "
           "%llu definition; %llu resolvent pairs saved\n", (ull)s.bve_gates_equiv,
           (ull)s.bve_gates_and_or, (ull)s.bve_gates_ite, (ull)s.bve_gates_xor,
           (ull)s.bve_gates_definition, (ull)s.bve_gate_pairs_saved);
    stat_line("c bve-def     ", {{s.bve_definitions_checked, "checked"},
                                 {s.bve_definition_ticks, "ticks"},
                                 {s.bve_definition_budget, "hit the tick ceiling"},
                                 {s.bve_definition_units, "units"}});
    printf("c restarts    %llu (%llu reused a trail prefix)\n", (ull)s.restarts,
           (ull)s.restarts_reused);
    printf("c reductions  %llu (%llu clauses deleted, %llu promoted)\n", (ull)s.reductions,
           (ull)s.deleted, (ull)s.promoted);
    printf("c tiers       %u/%u live after %llu retiers "
           "(retention band %llu clause-visits dynamic vs %llu static)\n", kernel.tier1,
           kernel.tier2, (ull)s.retiered, (ull)s.tier2_band_dynamic, (ull)s.tier2_band_static);
    printf("c redscan     %llu scans, %llu arena words walked, %llu "
           "skippable by a first_reducible watermark (%.2f%%)\n", (ull)s.reduce_scans,
           (ull)s.reduce_scan_words, (ull)s.reduce_skippable_words,
           percent_of(s.reduce_skippable_words, s.reduce_scan_words));
    printf("c mode        %s after %llu switches (%llu ticks, "
           "%llu target phases saved)\n",
           !kernel.h ? "unguided" : kernel.h->stable ? "stable" : "focused", (ull)s.switched,
           (ull)s.ticks, (ull)s.target_saved);
    printf("c rephased    %llu (%llu best phases saved)\n", (ull)s.rephased, (ull)s.best_saved);
    stat_line("c reordered   ", {{s.reorder_eligible, "eligible"},
                                 {s.reordered, "decision re-rankings"}});
    printf("c lucky       %llu early, %llu late (%llu SAT), %llu probes\n",
           (ull)s.lucky_early_calls, (ull)s.lucky_late_calls, (ull)s.lucky_late_sat,
           (ull)s.lucky_probes);
    printf("c parity     %llu calls, %llu equations, rank %llu, "
           "%llu candidates, %llu SAT, %llu interrupted\n", (ull)s.parity_model_calls,
           (ull)s.parity_model_equations, (ull)s.parity_model_rank, (ull)s.parity_model_candidates,
           (ull)s.parity_model_sat, (ull)s.parity_model_interrupted);
    stat_line("c parity-ref ", {{s.parity_refutation_probe_declines, "probe declines"},
                                {s.parity_refutation_recognized, "recognized"},
                                {s.parity_refutation_unsat, "UNSAT"},
                                {s.parity_refutation_proof_additions, "proof additions"},
                                {s.parity_refutation_proof_deletions, "proof deletions"}});
    stat_line("c theory-act ", {{s.parity_consequence_attempts, "parity"},
                                {s.affine_consequence_attempts, "affine"},
                                {s.exact_cover_extension_attempts, "exact-cover"},
                                {s.independent_set_cover_attempts, "independent-set"},
                                {s.semantic_variable_advice_attempts, "advice attempts"}});
    stat_line("c directphp  ", {{s.direct_php_attempts, "attempts"},
                                {s.direct_php_applied, "applied"},
                                {s.direct_php_not_recognized, "not-recognized"},
                                {s.direct_php_resource_exhausted, "resource-exhausted"},
                                {s.direct_php_interrupted, "interrupted"},
                                {s.direct_php_stale, "stale"},
                                {s.direct_php_replay_mismatch, "replay-mismatch"}});
    stat_line("c gf3-gauss  ", {{s.gf3_gaussian_attempts, "attempts"},
                                {s.gf3_gaussian_applied, "applied"},
                                {s.gf3_gaussian_no_structure, "no-structure"},
                                {s.gf3_gaussian_no_supported_outcome, "no-outcome"},
                                {s.gf3_gaussian_exhausted, "exhausted"},
                                {s.gf3_gaussian_interrupted, "interrupted"},
                                {s.gf3_gaussian_stale, "stale"},
                                {s.gf3_gaussian_unsupported_proof, "unsupported-proof"}});
    stat_line("c gf3-row    ", {{s.gf3_general_row_attempts, "attempts"},
                                {s.gf3_general_row_applied, "applied"},
                                {s.gf3_general_row_no_structure, "no-structure"},
                                {s.gf3_general_row_no_consequence, "no-consequence"},
                                {s.gf3_general_row_already_published, "already-published"},
                                {s.gf3_general_row_exhausted, "exhausted"},
                                {s.gf3_general_row_interrupted, "interrupted"},
                                {s.gf3_general_row_stale, "stale"},
                                {s.gf3_general_row_unsupported_proof, "unsupported-proof"},
                                {s.gf3_general_row_installed_clauses, "clauses"}});
    stat_line("c card-search ", {{s.cardinality_search_attempts, "attempts"},
                                 {s.cardinality_search_applied, "applied"}});
    stat_line("c tseitin-cycle ", {{s.tseitin_cycle_attempts, "attempts"},
                                   {s.tseitin_cycle_applied, "applied"},
                                   {s.tseitin_cycle_no_structure, "no-structure"},
                                   {s.tseitin_cycle_exhausted, "exhausted"},
                                   {s.tseitin_cycle_interrupted, "interrupted"},
                                   {s.tseitin_cycle_stale, "stale"},
                                   {s.tseitin_cycle_unsupported_proof, "unsupported-proof"}});
    stat_line("c nary-eq    ", {{s.nary_csp_equality_attempts, "attempts"},
                                {s.nary_csp_equality_applied, "applied"},
                                {s.nary_csp_equality_no_structure, "no-structure"},
                                {s.nary_csp_equality_no_consequence, "no-consequence"},
                                {s.nary_csp_equality_already_published, "already-published"},
                                {s.nary_csp_equality_invalid, "invalid"},
                                {s.nary_csp_equality_inapplicable, "inapplicable"},
                                {s.nary_csp_equality_resource_exhausted, "exhausted"},
                                {s.nary_csp_equality_interrupted, "interrupted"},
                                {s.nary_csp_equality_stale, "stale"},
                                {s.nary_csp_equality_unsupported_proof, "unsupported-proof"},
                                {s.nary_csp_equality_introduced_variables, "introduced"}});
    stat_line("c nary-eq-amo ", {{s.nary_csp_equality_amo_attempts, "attempts"},
                                 {s.nary_csp_equality_amo_applied, "applied"},
                                 {s.nary_csp_equality_amo_missing_equality, "missing-equality"},
                                 {s.nary_csp_equality_amo_stale, "stale"},
                                 {s.nary_csp_equality_amo_duplicate, "duplicate"},
                                 {s.nary_csp_equality_amo_declined, "declined"},
                                 {s.nary_csp_equality_amo_binary_clauses, "clauses"}});
    stat_line("c affine-matrix-init ",
              {{s.affine_matrix_init_attempts, "attempts"},
               {s.affine_matrix_init_applied, "applied"},
               {s.affine_matrix_init_applied_partial, "partial"},
               {s.affine_matrix_init_incomplete_image, "incomplete-image"},
               {s.affine_matrix_init_incomplete_matrix, "incomplete-matrix"},
               {s.affine_matrix_init_exhausted, "exhausted"},
               {s.affine_matrix_init_interrupted, "interrupted"},
               {s.affine_matrix_init_normalization_rup_checks, "normalization-RUP-checks"},
               {s.affine_matrix_init_shortening_additions, "shortening-additions"},
               {s.affine_matrix_init_maintenance_service_work, "maintenance-service-work"}});
    stat_line("c affine_discovery ",
              {{s.affine_matrix_init_discovery_recognized_groups, "recognized-groups"},
               {s.affine_matrix_init_discovery_skipped_groups, "unchanged-groups"},
               {s.affine_matrix_init_discovery_recognizer_work, "recognizer-work"}});
    stat_line("c affine     ", {{s.affine_model_calls, "calls"},
                                {s.affine_model_equations, "equations"},
                                {s.affine_model_projection, "coordinates"},
                                {s.affine_model_entries, "entries"},
                                {s.affine_model_candidates, "candidates"},
                                {s.affine_model_sat, "SAT"}, {s.affine_model_work, "work"},
                                {s.affine_model_budget, "budgets"},
                                {s.affine_model_unavailable, "unavailable"},
                                {s.affine_model_interrupted, "interrupted"}});
    printf("c onehot     %llu calls, %llu domains, %llu equations, rank %llu, "
           "%llu candidates, %llu SAT, %llu interrupted\n", (ull)s.onehot_model_calls,
           (ull)s.onehot_model_domains, (ull)s.onehot_model_equations, (ull)s.onehot_model_rank,
           (ull)s.onehot_model_candidates, (ull)s.onehot_model_sat,
           (ull)s.onehot_model_interrupted);
    printf("c directcsp  %llu calls, %llu domains, %llu edges, %llu restarts, "
           "%llu infeasible, %llu work, best %llu, %llu SAT\n", (ull)s.direct_csp_model_calls,
           (ull)s.direct_csp_model_domains, (ull)s.direct_csp_model_edges,
           (ull)s.direct_csp_model_restarts, (ull)s.direct_csp_model_infeasible,
           (ull)s.direct_csp_model_work,
           (ull)(s.direct_csp_model_best == UINT64_MAX ? 0 : s.direct_csp_model_best),
           (ull)s.direct_csp_model_sat);
    stat_line("c exact-one  ", {{s.exact_one_refutation_calls, "calls"},
                                {s.exact_one_refutation_recognized, "recognized"},
                                {s.exact_one_refutation_equations, "equations"},
                                {s.exact_one_refutation_unsat, "UNSAT"},
                                {s.exact_one_refutation_interrupted, "interrupted"},
                                {s.exact_one_refutation_proof_additions, "proof additions"},
                                {s.exact_one_refutation_proof_deletions, "proof deletions"}});
    stat_line("c balance    ", {{s.balanced_signature_calls, "calls"},
                                {s.balanced_signature_recognized, "recognized"},
                                {s.balanced_signature_forced_ttf, "forced TTF"},
                                {s.balanced_signature_candidates, "candidates"},
                                {s.balanced_signature_restarts, "restarts"},
                                {s.balanced_signature_steps, "steps"},
                                {s.balanced_signature_completion_rounds, "completion rounds"},
                                {s.balanced_signature_sat, "SAT"},
                                {s.balanced_signature_interrupted, "interrupted"}});
    stat_line("c fermat     ", {{s.fermat_model_calls, "calls"},
                                {s.fermat_model_recognized, "recognized"},
                                {s.fermat_model_candidates, "candidates"},
                                {s.fermat_model_factor_iterations, "factor iterations"},
                                {s.fermat_model_completion_rounds, "completion rounds"},
                                {s.fermat_model_sat, "SAT"},
                                {s.fermat_model_interrupted, "interrupted"}});
    stat_line("c cerny      ", {{s.cerny_model_calls, "calls"},
                                {s.cerny_model_recognized, "recognized"},
                                {s.cerny_model_states, "states"}, {s.cerny_model_length, "length"},
                                {s.cerny_model_sat, "SAT"},
                                {s.cerny_model_interrupted, "interrupted"}});
    stat_line("c lookup     ",
              {{s.lookup_circuit_model_calls, "calls"},
               {s.lookup_circuit_model_recognized, "recognized"},
               {s.lookup_circuit_model_gates, "gates"}, {s.lookup_circuit_model_support, "support"},
               {s.lookup_circuit_model_vector_evaluations, "vector gate evaluations"},
               {s.lookup_circuit_model_csp_work, "CSP work"}, {s.lookup_circuit_model_sat, "SAT"},
               {s.lookup_circuit_model_interrupted, "interrupted"}});
    stat_line("c simon      ", {{s.simon_model_calls, "calls"},
                                {s.simon_model_recognized, "recognized"},
                                {s.simon_model_rounds, "rounds"},
                                {s.simon_model_candidates, "candidates"},
                                {s.simon_model_sat, "SAT"},
                                {s.simon_model_interrupted, "interrupted"}});
    printf("c stconn     %llu calls, %llu recognized, width %llu, "
           "%llu columns, %llu RUP clauses, %llu budget stops, %llu UNSAT, "
           "%llu interrupted\n", (ull)s.stconn_proof_calls, (ull)s.stconn_proof_recognized,
           (ull)s.stconn_proof_width, (ull)s.stconn_proof_columns, (ull)s.stconn_proof_rup_clauses,
           (ull)s.stconn_proof_budget_exhausted, (ull)s.stconn_proof_unsat,
           (ull)s.stconn_proof_interrupted);
    stat_line("c signedcard ", {{s.signed_cardinality_proof_calls, "calls"},
                                {s.signed_cardinality_proof_recognized, "recognized"},
                                {s.signed_cardinality_proof_groups, "groups"},
                                {s.signed_cardinality_proof_bdd_nodes, "BDD nodes"},
                                {s.signed_cardinality_proof_additions, "proof additions"},
                                {s.signed_cardinality_proof_budget_exhausted, "budget stops"},
                                {s.signed_cardinality_proof_unsat, "UNSAT"},
                                {s.signed_cardinality_proof_interrupted, "interrupted"}});
    stat_line("c exactcover ", {{s.exact_cover_model_calls, "calls"},
                                {s.exact_cover_model_recognized, "recognized"},
                                {s.exact_cover_model_blocks, "blocks"},
                                {s.exact_cover_model_nodes, "nodes"},
                                {s.exact_cover_model_restarts, "restarts"},
                                {s.exact_cover_model_budget_exhausted, "budget stops"},
                                {s.exact_cover_model_sat, "SAT"},
                                {s.exact_cover_model_interrupted, "interrupted"}});
    printf("c indepset   %llu calls, %llu recognized, %llu vertices, %llu target, "
           "%llu greedy, %llu found, %llu steps, %llu completion rounds, "
           "%llu SAT, %llu interrupted; %llu stable cores, %llu covers, "
           "%llu authenticated resolvents\n", (ull)s.independent_set_model_calls,
           (ull)s.independent_set_model_recognized, (ull)s.independent_set_model_vertices,
           (ull)s.independent_set_model_target, (ull)s.independent_set_model_greedy_size,
           (ull)s.independent_set_model_size, (ull)s.independent_set_model_steps,
           (ull)s.independent_set_model_completion_rounds, (ull)s.independent_set_model_sat,
           (ull)s.independent_set_model_interrupted, (ull)s.independent_set_stable_cores,
           (ull)s.independent_set_stable_cover_clauses, (ull)s.independent_set_stable_resolvents);
    stat_line("c graphlabel ", {{s.graph_labeling_calls, "calls"},
                                {s.graph_labeling_recognized, "recognized"},
                                {s.graph_labeling_vertices, "vertices"},
                                {s.graph_labeling_separation, "separation"},
                                {s.graph_labeling_seeds, "seeds"},
                                {s.graph_labeling_nodes, "nodes"},
                                {s.graph_labeling_repairs, "repairs"},
                                {s.graph_labeling_anneal_attempts, "anneal attempts"},
                                {s.graph_labeling_work, "work"},
                                {s.graph_labeling_budget, "budget stops"},
                                {s.graph_labeling_interrupted, "interrupted"},
                                {s.graph_labeling_models_rejected, "models rejected"},
                                {s.graph_labeling_sat, "SAT"}});
    printf("c graphinterv %llu calls, %llu recognized, %llu vertices, "
           "%llu gates, %llu intervals, %llu groups, %llu selected, "
           "%llu work, %llu source clauses/%llu literals/%llu bytes, "
           "%llu resource stops/%llu interrupted, (%llu RUP ticks), %llu RUP rejected, "
           "%llu duplicates, %llu additions, %llu proof literals\n",
           (ull)s.graph_interval_equivalence_calls, (ull)s.graph_interval_equivalence_recognized,
           (ull)s.graph_interval_equivalence_vertices, (ull)s.graph_interval_equivalence_gates,
           (ull)s.graph_interval_equivalence_intervals, (ull)s.graph_interval_equivalence_groups,
           (ull)s.graph_interval_equivalence_selected, (ull)s.graph_interval_equivalence_work,
           (ull)s.graph_interval_equivalence_source_clauses,
           (ull)s.graph_interval_equivalence_source_literals,
           (ull)s.graph_interval_equivalence_source_workspace,
           (ull)s.graph_interval_equivalence_resource_exhausted,
           (ull)s.graph_interval_equivalence_interrupted,
           (ull)s.graph_interval_equivalence_rup_ticks,
           (ull)s.graph_interval_equivalence_rup_rejected,
           (ull)s.graph_interval_equivalence_duplicates,
           (ull)s.graph_interval_equivalence_published,
           (ull)s.graph_interval_equivalence_proof_literal_occurrences);
    stat_line("c ramsey     ", {{s.ramsey_startup_attempts, "attempts"},
                                {s.ramsey_startup_recognized, "recognized"},
                                {s.ramsey_startup_recognition_work, "recognition work"},
                                {s.ramsey_startup_certificate_work, "certificate work"},
                                {s.ramsey_startup_nodes, "nodes"},
                                {s.ramsey_startup_additions, "additions"},
                                {s.ramsey_startup_deletions, "deletions"},
                                {s.ramsey_startup_helpers, "helpers"},
                                {s.ramsey_startup_exhausted, "budget stops"},
                                {s.ramsey_startup_unsat, "UNSAT"},
                                {s.ramsey_startup_interrupted, "interrupted"}});
    printf("c localsearch %llu hints offered (%llu phases, %llu replacements), "
           "%llu applied / %llu target installs from %llu attempts "
           "(%llu eligible, %llu changed, "
           "%llu matching, %llu replaced)\n", (ull)s.local_search_hints_offered,
           (ull)s.local_search_hint_phases_offered, (ull)s.local_search_hints_replaced,
           (ull)s.local_search_hints_applied, (ull)s.local_search_target_installs,
           (ull)s.local_search_target_install_attempts, (ull)s.local_search_target_eligible_phases,
           (ull)s.local_search_target_changed_phases, (ull)s.local_search_target_matching_phases,
           (ull)s.local_search_target_replaced_phases);
    printf("c localphase %llu decisions (%llu matched, %llu overridden), "
           "%llu native / %llu rephase / %llu walk overwrites\n",
           (ull)s.local_search_target_decisions, (ull)s.local_search_target_decision_matches,
           (ull)s.local_search_target_decision_overrides,
           (ull)s.local_search_target_overwritten_native,
           (ull)s.local_search_target_overwritten_rephase,
           (ull)s.local_search_target_overwritten_walk);
    printf("c walk        %llu walks (%llu improved, %llu flips, %llu steps)\n", (ull)s.walks,
           (ull)s.walk_improved, (ull)s.walk_flips, (ull)s.walk_steps);
    printf("c walkseed    %llu imports / %llu phases (%llu improved, %llu exported), %llu steps, "
           "%llu budget stops, %llu interrupted, %llu stale hints\n", (ull)s.walk_seed_imports,
           (ull)s.walk_seed_imported_phases, (ull)s.walk_seed_improved, (ull)s.walk_seed_exported,
           (ull)s.walk_seed_steps, (ull)s.walk_seed_budget_exhausted, (ull)s.walk_seed_interrupted,
           (ull)s.local_search_hints_stale);
    stat_line("c randec      ", {{s.random_sequences, "sequences"},
                                 {s.random_decisions, "random decisions"}});
    printf("c subsume     %llu rounds (%llu checked, %llu visits, %llu subsumed)\n",
           (ull)s.forward_subsumptions, (ull)s.forward_checked, (ull)s.forward_visits,
           (ull)s.forward_subsumed);
    printf("c strengthen  %llu rounds (%llu checked, %llu visits, "
           "%llu strengthened, %llu subsumed)\n", (ull)s.strengthen_rounds,
           (ull)s.strengthen_checked, (ull)s.strengthen_visits, (ull)s.strengthened,
           (ull)s.strengthen_subsumed);
    stat_line("c budgetstop  ", {{s.budget_stops_c1, "C1 rounds"},
                                 {s.budget_stops_c2, "C2 rounds stopped mid-round"}});
    // Witness for the touched-epoch filters (Kernel::touched).
    printf("c touch       %llu marks, %llu epoch; C1 %llu skipped / %llu "
           "advances / %llu rearmed, C2 %llu skipped / %llu advances / %llu "
           "rearmed\n", (ull)s.touch_marks, (ull)kernel.touch_epoch, (ull)s.forward_mark_skipped,
           (ull)s.forward_mark_advances, (ull)s.forward_rearmed, (ull)s.strengthen_mark_skipped,
           (ull)s.strengthen_mark_advances, (ull)s.strengthen_rearmed);
    stat_line("c subhold     ", {{s.subsume_holds, "elimination rounds held open by C1/C2"}});
    printf("c touch3      BCE %llu skipped / %llu rearmed, X2 %llu skipped / "
           "%llu rearmed, X4 %llu skipped / %llu rearmed\n", (ull)s.bce_mark_skipped,
           (ull)s.bce_rearmed, (ull)s.ternary_mark_skipped, (ull)s.ternary_rearmed,
           (ull)s.instantiate_mark_skipped, (ull)s.instantiate_rearmed);
    // Which rule honoured the interrupt (`c interrupt` sums over all).
    printf("c interrupt3  BCE %llu, X2 %llu, X4 %llu aborts\n", (ull)s.bce_interrupted,
           (ull)s.ternary_interrupted, (ull)s.instantiate_interrupted);
    printf("c interrupt2  C1 %llu, C2 %llu aborts\n", (ull)s.subsume_interrupted,
           (ull)s.strengthen_interrupted);
    printf("c touchred    %llu redundant creations in scope (dynamic) vs %llu "
           "(static tier2=6), %llu band marks; band %s\n", (ull)s.touch_redundant_dynamic,
           (ull)s.touch_redundant_static6, (ull)s.touch_kept_marks,
           kernel.touch_redundant ? "ON" : "off");
    printf("c f7band      C2 %llu band candidates checked / %llu skipped, "
           "%llu subsumed / %llu strengthened\n", (ull)s.strengthen_red_checked,
           (ull)s.strengthen_red_skipped, (ull)s.strengthen_red_subsumed,
           (ull)s.strengthen_red_strengthened);
    stat_line("c touch4      Factor ", {{s.factor_mark_skipped, "seeds skipped"},
                                        {s.factor_rearmed, "rearmed"}}, " / ");
    // `c timing`: rules are charged by the registry dispatch wrapper; the slot
    // is the whole inprocessing phase. The per-conflict path is untimed.
    {
      const double total_ms = s.solve_nanos / 1e6;
      const double slot_ms = s.inprocess_nanos / 1e6;
      auto pct = [&](double ms) { return total_ms > 0 ? 100.0 * ms / total_ms : 0.0; };
      printf("c timing      %.0f ms solve\n", total_ms);
      printf("c timing      %.0f ms (%.1f%%) inprocessing slot, %.0f ms (%.1f%%) "
             "search+scaffolding outside it\n", slot_ms, pct(slot_ms), total_ms - slot_ms,
             pct(total_ms - slot_ms));
      printf("c phasecost   structured %.0f ms, entry %.0f ms, dense-connect %.0f ms, "
             "slot-loop %.0f ms, exit %.0f ms\n", s.structured_root_nanos / 1e6,
             s.inprocess_entry_nanos / 1e6, s.inprocess_dense_connect_nanos / 1e6,
             s.inprocess_slot_loop_nanos / 1e6, s.inprocess_exit_nanos / 1e6);
      // Per rule, biggest first; only rules that actually ran.
      const auto &rules = tactix::tactix_rules();
      std::vector<std::pair<uint64_t, size_t>> ranked;
      double charged_ms = 0;
      for (size_t i = 0; i < rules.size() && i < tactix::Stats::MAX_TIMED_RULES; i++)
        if (s.rule_calls[i]) {
          ranked.push_back({s.rule_nanos[i], i});
          charged_ms += s.rule_nanos[i] / 1e6;
        }
      std::sort(ranked.rbegin(), ranked.rend());
      for (const auto &e : ranked)
        printf("c timing        %-16s %9.0f ms (%5.1f%%) in %llu applications\n",
               rules[e.second].name, e.first / 1e6, pct(e.first / 1e6),
               (ull)s.rule_calls[e.second]);
      printf("c timing        %-16s %9.0f ms (%5.1f%%)\n", "(unattributed)", slot_ms - charged_ms,
             pct(slot_ms - charged_ms));
    }
    {
      if (std::getenv("TACTIX_STRATEGY_CONFIG"))
        printf("c strategy-config %s\n", default_policy.strategy_config_snapshot().c_str());
      if (std::getenv("TACTIX_SLOT_STATS"))
        default_policy.inprocess().report(kernel);
    }
    // Why the automatic proof has its format: `dsr-selected`, `pending`, or the
    // PBP trigger.
    if (proof_path && proof.automatic()) {
      printf("c auto-proof    %s\n", automatic_proof_reason_name(proof.automatic_reason()));
      // One proof at a time: whether the run paid the conversion to PBP.
      const tactix::LazyPbpReport &lazy = proof.lazy_pbp_report();
      printf("c auto-proof-switch %s dsr-bytes %" PRIu64 " pbp-bytes %" PRIu64
             " nanos=%" PRIu64 "\n", lazy.state, lazy.dsr_bytes, lazy.pbp_bytes, lazy.nanos);
    }
  }
  // An explicit DSR proof cannot spell the startup counting refutation: a
  // follow-on solve without it writes the proof, or the run has no verdict.
  int verdict = res;
  bool follow_on_failed = false;
  if (res == 20 && proof.dsr_follow_on_pending()) {
    if (!quiet)
      printf("c follow-on   startup-count-pb\n");
    fflush(stdout);
    const DsrFollowOn follow_on = run_dsr_follow_on(
        kernel, proof, default_config,
        [&](Kernel &k) {
          pose_kernel(k, bve_gate_mask, compact_affine_provenance);
        },
        &caught_signal);
    if (!quiet) {
      stat_line("c follow-on   ",
                {{static_cast<unsigned long long>(follow_on.result), "result"},
                 {follow_on.conflicts, "conflicts"},
                 {follow_on.decisions, "decisions"},
                 {follow_on.propagations, "propagations"},
                 {follow_on.sr, "sr applications"}});
      printf("c follow-on   %.0f ms\n", follow_on.nanos / 1e6);
    }
    if (follow_on.result != 20) {
      verdict = 0;
      // Only a signal may end the follow-on without an answer.
      follow_on_failed = follow_on.result != 0 || !caught_signal;
      if (follow_on_failed)
        fprintf(stderr, "tactix: the DSR follow-on solve returned %d\n",
                follow_on.result);
    }
  }
  if (res == 10) {
    const uint64_t extend_t0 = frontend_time ? tactix::now_nanos() : 0;
    kernel_extend_model(kernel); // rebuild the model through rho (plan §4)
    const uint64_t extend_nanos =
        frontend_time ? tactix::now_nanos() - extend_t0 : 0;
    printf("s SATISFIABLE\n");
    const uint64_t print_t0 = frontend_time ? tactix::now_nanos() : 0;
    print_model(kernel);
    if (frontend_time) {
      fflush(stdout); // keep stderr diagnostics out of a buffered model token
      fprintf(stderr,
              "[frontend-time] ms: parse %.0f extend-model %.0f "
              "print-model %.0f\n",
              parse_nanos / 1e6, extend_nanos / 1e6,
              (tactix::now_nanos() - print_t0) / 1e6);
    }
  } else if (verdict == 20) {
    if (proof_path)
      printf("c proof-route %s\n", proof.verification_route());
    printf("s UNSATISFIABLE\n");
  } else {
    printf("s UNKNOWN\n");
  }
  fflush(stdout);
  if (feedback_failed) return 1; // Solver verdict and certificate remain intact.
  if (follow_on_failed) return 1;
  return res == 10 ? 10 : verdict == 20 ? 20 : 0;
}
