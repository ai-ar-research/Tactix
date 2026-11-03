// Competition frontend (plan §6, compete skill): DIMACS in (plain or
// .xz/.gz via decompressor pipe), `s`/`v` lines on stdout, exit codes
// 10/20/other, proof to argv[2], SIGTERM/SIGINT/SIGXCPU -> flush proof,
// print `s UNKNOWN`, exit cleanly.
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
    // Strict > 0, agreeing with tactix_value: after kernel_extend_model no
    // variable is value-0, and if extension were ever skipped a 0 must print
    // as false, not fabricate a positive phase the API would deny.
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
                              ProofFormat format, bool have_proof,
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
  field("proof-format", std::to_string(static_cast<unsigned>(format)));
  field("proof-enabled", have_proof ? "1" : "0");
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
// `separator`, then the newline. The line is written with one stdio call,
// as the single printf it replaces was.
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

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IOFBF, 1 << 20);
  const bool frontend_time = getenv("TACTIX_FRONTEND_TIME") != nullptr;
  const char *cnf_path = nullptr;
  const char *proof_path = nullptr;
  enum class CliProofFamily { Dsr, Drat, Lrat, Pbp, Automatic };
  // Automatic selection is the public default: the solver runs exactly as
  // under --pbp and keeps whole-run PBP only when a PB-native step fires;
  // otherwise its native-DSR copy becomes the proof (plan §5).
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
    } else if (!strcmp(a, "--drat")) {
      proof_family = CliProofFamily::Drat;
    } else if (!strcmp(a, "--lrat")) {
      proof_family = CliProofFamily::Lrat; // native LRAT compatibility
    } else if (!strcmp(a, "--pbp")) {
      proof_family = CliProofFamily::Pbp;
    } else if (!strcmp(a, "--auto-proof")) {
      proof_family = CliProofFamily::Automatic;
    } else if (!strncmp(a, "--proof-stage=", 14) && a[14]) {
      proof_stage_path = a + 14;
    } else if (!strncmp(a, "--default-affine-search=", 24)) {
      if (!strcmp(a + 24, "on")) default_affine_search = true;
      else if (!strcmp(a + 24, "off")) default_affine_search = false;
      else
        return fail_unknown(
            "tactix: --default-affine-search requires on or off\n");
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
      if (!strcmp(a + 25, "on")) default_graph_labeling = true;
      else if (!strcmp(a + 25, "off")) default_graph_labeling = false;
      else
        return fail_unknown(
            "tactix: --default-graph-labeling requires on or off\n");
    } else if (!strncmp(a, "--default-structured-symmetry=", 30)) {
      if (!strcmp(a + 30, "on")) default_structured_symmetry = true;
      else if (!strcmp(a + 30, "off")) default_structured_symmetry = false;
      else
        return fail_unknown(
            "tactix: --default-structured-symmetry requires on or off\n");
    } else if (!strncmp(a, "--structural-provenance=", 24)) {
      structural_provenance_path = a + 24;
    } else if (!strcmp(a, "--compact-affine-provenance")) {
      compact_affine_provenance = true;
    } else if (!strncmp(a, "--feedback=", 11)) {
      feedback_path = a + 11;
    } else if (!strncmp(a, "--feedback-replay=", 18)) {
      feedback_replay_path = a + 18;
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
            return fail_unknown("tactix: bad --bve-gates spec '%s'\n", spec);
          }
        }
      }
    } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      printf("usage: tactix [-q|-v] [--auto-proof|--dsr|--drat|--lrat|--pbp] "
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

  // Automatic selection drives the solver through the streaming-PBP stream.
  const ProofFormat proof_format =
      proof_family == CliProofFamily::Pbp ||
              proof_family == CliProofFamily::Automatic
          ? ProofFormat::PbpStreaming
          : proof_family == CliProofFamily::Lrat
          ? ProofFormat::Lrat
          : proof_family == CliProofFamily::Drat
                ? (binary_proof ? ProofFormat::DratBinary
                                : ProofFormat::DratAscii)
                : (binary_proof ? ProofFormat::DsrBinary
                                : ProofFormat::DsrAscii);
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
  if (structural_provenance_path) {
    for (const char *other : {cnf_path, proof_path, feedback_path,
                              feedback_replay_path}) {
      if (!other) continue;
      std::error_code a, b, c;
      if ((std::filesystem::weakly_canonical(structural_provenance_path, a) ==
           std::filesystem::weakly_canonical(other, b) && !a && !b) ||
          std::filesystem::equivalent(structural_provenance_path, other, c)) {
        fprintf(stderr, "tactix: structural provenance aliases another artifact\n");
        return 1;
      }
    }
  }
  if (proof_stage_path && (!proof_path ||
                           proof_family != CliProofFamily::Automatic))
    return fail_unknown("tactix: --proof-stage needs an automatic proof\n");
  if (proof_stage_path) {
    // The stage is truncated when opened: it must not name another artifact.
    for (const char *other : {cnf_path, proof_path, feedback_path,
                              feedback_replay_path,
                              structural_provenance_path}) {
      if (!other) continue;
      std::error_code a, b, c;
      if ((std::filesystem::weakly_canonical(proof_stage_path, a) ==
           std::filesystem::weakly_canonical(other, b) && !a && !b) ||
          std::filesystem::equivalent(proof_stage_path, other, c))
        return fail_unknown("tactix: proof stage aliases another artifact\n");
    }
  }
  if (feedback_path || feedback_replay_path) {
    // A feedback destination must never truncate an input or proof.
    // Replay input must likewise survive opening the requested proof output.
    const char *record = feedback_path ? feedback_path : feedback_replay_path;
    for (const char *other : {cnf_path, proof_path}) {
      if (!other) continue;
      std::error_code first_error, second_error, equivalent_error;
      const auto first = std::filesystem::weakly_canonical(record, first_error);
      const auto second = std::filesystem::weakly_canonical(other, second_error);
      if ((!first_error && !second_error && first == second) ||
          std::filesystem::equivalent(record, other, equivalent_error))
        return fail_unknown("tactix: feedback path aliases input or proof\n");
    }
  }
  Kernel kernel;
  kernel.affine_matrix_provenance_compact = compact_affine_provenance;
  StructuralActionSessions structural_action_sessions(kernel);
  kernel_defer_cold_state(kernel);
  kernel.bve_gate_mask = bve_gate_mask;
  // Each env knob below poses one `Kernel` config field; that field documents
  // its default and soundness class. They are read here rather than in
  // DefaultPolicy so configuration is posed before it owns the session.
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
  // Vivify's cross-candidate trail reuse ships ON; =0 also restores the older
  // probe/candidate order, which cannot be separated (Kernel::vivify_reuse).
  env_flag("TACTIX_VIVIFY_REUSE", kernel.vivify_reuse);
  env_flag("TACTIX_VIVIFY_FLUSH", kernel.vivify_flush);
  env_u32("TACTIX_FACTOR_SIZE", kernel.factor_size);
  // Kissat's `reduction > bound` acceptance threshold (factor.c:939), default 0
  // accepting any positive gain. Formula-shape-blind, which is why it and not a
  // scope gate is the candidate for making broad admission safe: Factor's
  // winners and losers sit at the same scope fraction.
  env_u32("TACTIX_FACTOR_BOUND", kernel.factor_bound);
  // Fresh-variable demotion ships ON (reference parity); `=0` is the off arm.
  env_flag("TACTIX_FACTOR_DEMOTE", kernel.factor_demote_fresh);
  env_u32("TACTIX_FACTOR_CAND_ROUNDS", kernel.factor_cand_rounds);
  env_u32("TACTIX_STOP_FUZZ", kernel.stop_fuzz); // see kernel_terminating
  if (getenv("TACTIX_SUBST_DEMOTE"))
    kernel.subst_demote = true; // rank substituted vars last, see Kernel
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
  // Conflict-clause reuse ships ON, matching Kissat's
  // one_literal_on_conflict_level shortcut; `=0` is the diagnostic revert.
  kernel.conflict_reuse = true;
  env_flag("TACTIX_CONFLICT_REUSE", kernel.conflict_reuse);
  // Kissat's `jumpreasons` ships ON; =0 restores direct antecedent reasons.
  env_flag("TACTIX_JUMP_REASONS", kernel.jump_reasons);
  // Both ship ON at their references' defaults (kissat `promote`, `tumble`);
  // `=0` reverts to the pre-parity behaviour and is the priced arm.
  env_flag("TACTIX_SHRINK_GLUE", kernel.shrink_glue_promote);
  env_flag("TACTIX_TUMBLE", kernel.tumble);
  // The stream is session-owned and outlives policy execution. Structural
  // actions require a complete provenance sink even when the caller does not
  // retain it; tmpfile keeps the normal CLI capable without creating a named
  // artifact. Exclusive creation protects explicitly retained provenance.
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
    structural_records.reset(std::tmpfile());
    if (!structural_records) {
      fprintf(stderr, "tactix: cannot create anonymous structural provenance stream\n");
      return 1;
    }
    kernel.structural_provenance = structural_records.get();
  }
  Proof proof;
  // Whole-run streaming PBP is textual, so --no-binary has no effect on it;
  // under automatic selection it spells the native-DSR copy in ASCII.
  kernel.startup_certificate_open =
      proof_format == ProofFormat::PbpStreaming ||
      proof_format == ProofFormat::DsrBinary ||
      proof_format == ProofFormat::DsrAscii;
  if (proof_path) {
    const bool opened =
        proof_family == CliProofFamily::Automatic
            ? proof.open_automatic(proof_path, binary_proof, proof_stage_path)
            : proof.open(proof_path, proof_format);
    if (!opened)
      return fail_unknown("tactix: cannot write proof to '%s'\n", proof_path);
    kernel.proof = &proof;
    proof.bind_variable_map(&kernel.internal_to_external);
    proof.bind_external_variable_map(&kernel.external_to_internal);
  }

  bool piped = false;
  FILE *in = open_input(cnf_path, &piped);
  if (!in)
    return fail_unknown("tactix: cannot read '%s'\n", cnf_path);
  const uint64_t parse_t0 = frontend_time ? tactix::now_nanos() : 0;
  const char *err = parse_dimacs(in, kernel);
  const uint64_t parse_nanos =
      frontend_time ? tactix::now_nanos() - parse_t0 : 0;
  // A decompressor that is missing or fails leaves a truncated formula, which
  // must not be solved as if it were the input.
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
    if (!feedback_metadata(cnf_path, bve_gate_mask, proof_format,
                           proof_path != nullptr, default_config, metadata)) {
      feedback_setup_error = "cannot read immutable feedback build/input provenance";
    } else {
      metadata += std::string("structural-provenance=1:") +
          (structural_provenance_path ? "1\n" : "0\n");
      if (compact_affine_provenance)
        metadata += "compact-affine-provenance=1:1\n";
      feedback.emplace();
      // Opening/provenance errors stay in the receiver. Even a rejected header
      // must not steer the selected policy or suppress its certificate/verdict.
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
    // Every registered rule gets a stats line, and most pair a yield with a
    // reachability witness: a zero on the witness means the mechanism never
    // ran, so an A/B arm reading zero was skipped, not tested. Only counters
    // whose reading is non-obvious carry a comment below.
    printf("c tactix 0.2.1\n");
    {
      const auto &affine = default_policy.affine_search_diagnostics();
      // The persistent affine search block prints only when the service is
      // enabled or one of its counters moved; each line keeps its format.
      const std::pair<const char *, uint64_t> affine_head[] = {
        {"enabled", default_config.affine_search.enabled},
        {"initializations", affine.initializations},
        {"normalization_rup_checks", kernel.stats.affine_matrix_init_normalization_rup_checks},
        {"shortening_additions", kernel.stats.affine_matrix_init_shortening_additions},
        {"maintenance_service_work", kernel.stats.affine_matrix_init_maintenance_service_work},
        {"discovery_recognized_groups", kernel.stats.affine_matrix_init_discovery_recognized_groups},
        {"discovery_skipped_groups", kernel.stats.affine_matrix_init_discovery_skipped_groups},
        {"discovery_recognizer_work", kernel.stats.affine_matrix_init_discovery_recognizer_work},
        {"refreshes", affine.refreshes},
        {"unchanged_refreshes", affine.unchanged_refreshes},
        {"empty_maintenance_skips", affine.empty_maintenance_skips},
        {"unchanged_census_skips", affine.unchanged_census_skips},
        {"usefulness_resumptions", affine.usefulness_resumptions},
        {"paused_search_skips", affine.paused_search_skips},
        {"root_services", affine.root_services},
        {"search_services", affine.search_services},
        {"reasons_published", affine.reasons_published},
        {"implications_consumed", affine.implications_consumed},
        {"reasons_deferred", affine.reasons_deferred},
        {"reasons_materialized", affine.reasons_materialized},
        {"analyze_handles", kernel.stats.affine_matrix_analyze_handles},
        {"analyze_materializations", kernel.stats.affine_matrix_analyze_materializations},
        {"analyze_vectors_constructed", kernel.stats.affine_matrix_analyze_vectors_constructed},
        {"analyze_literals_constructed", kernel.stats.affine_matrix_analyze_literals_constructed},
        {"reasons_detached", affine.reasons_detached},
        {"materialization_attempts", affine.materialization_attempts},
        {"materialization_inapplicable", affine.materialization_inapplicable},
        {"materialization_stale", affine.materialization_stale},
        {"materialization_invalid", affine.materialization_invalid},
        {"materialization_exhausted", affine.materialization_exhausted},
        {"materialization_interrupted", affine.materialization_interrupted},
        {"materialization_unsupported_proof", affine.materialization_unsupported_proof},
        {"materialization_retained_authentications", affine.materialization_retained_authentications},
        {"materialization_last_audit_stage", affine.materialization_last_audit_stage},
        {"admitted_sources", affine.admitted_sources},
        {"active_sources", affine.active_sources},
      };
      const std::pair<const char *, uint64_t> affine_tail[] = {
        {"retained_basis_rank", affine.retained_basis_rank},
        {"retained_support_variables", affine.retained_support_variables},
        {"rank_before_last_action", affine.rank_before_last_action},
        {"rank_after_last_action", affine.rank_after_last_action},
        {"support_before_last_action", affine.support_before_last_action},
        {"support_after_last_action", affine.support_after_last_action},
        {"recognition_rank_gains", affine.recognition_rank_gains},
        {"recognition_rank_losses", affine.recognition_rank_losses},
        {"maintenance_rank_gains", affine.maintenance_rank_gains},
        {"maintenance_rank_losses", affine.maintenance_rank_losses},
        {"service_rank_losses", affine.service_rank_losses},
        {"isolated_projection_rank_losses", affine.isolated_projection_rank_losses},
        {"native_session_disables", affine.native_session_disables},
        {"optional_failure_disables", affine.optional_failure_disables},
        {"service_inapplicable", affine.service_inapplicable},
        {"service_interrupted", affine.service_interrupted},
        {"service_unsupported_proof", affine.service_unsupported_proof},
        {"service_stale", affine.service_stale},
        {"service_invalid", affine.service_invalid},
        {"last_service_status", affine.last_service_status},
        {"last_terminal_status", affine.last_terminal_status},
        {"last_terminal_audit_stage", affine.last_terminal_audit_stage},
        {"last_terminal_transaction_stage", affine.last_terminal_transaction_stage},
        {"last_terminal_synchronization_stage", affine.last_terminal_synchronization_stage},
        {"paused_basis_rank", affine.paused_basis_rank},
        {"paused_support_variables", affine.paused_support_variables},
        {"last_survival_action", affine.last_survival_action},
        {"last_disable_cause", affine.last_disable_cause},
        {"retained_bytes",
         structural_action_sessions.affine_matrix_search.retained_bytes()},
        {"retired_sources", affine.retired_sources},
        {"projected_variables", affine.projected_variables},
        {"initialization_work", affine.initialization_work},
        {"service_work", affine.service_work},
        {"incremental_capture_calls", affine.incremental_capture_calls},
        {"unchanged_support_calls", affine.unchanged_support_calls},
        {"cached_event_calls", affine.cached_event_calls},
        {"restart_runs", affine.restart_runs},
        {"unchanged_skips", affine.unchanged_skips},
        {"trigger_work", affine.trigger_work},
        {"delayed_runs", affine.delayed_runs},
        {"fruitful_runs", affine.fruitful_runs},
        {"fruitless_runs", affine.fruitless_runs},
        {"run_conflict_limit", affine.run_conflict_limit},
        {"delay_current", affine.delay_current},
        {"run_effort_last", affine.run_effort_last},
        {"run_effort_total", affine.run_effort_total},
        {"run_work", affine.run_work},
        {"run_steps", affine.run_steps},
        {"pending_slices", affine.pending_slices},
        {"resumed_slices", affine.resumed_slices},
        {"completed_slice_steps", affine.completed_slice_steps},
        {"slice_work", affine.slice_work},
        {"slice_cumulative_work", affine.slice_cumulative_work},
        {"minimum_slice_work", affine.minimum_slice_work},
        {"capture_delta_literals", affine.capture_delta_literals},
        {"premise_maintenance_work", affine.premise_maintenance_work},
        {"alias_work", affine.alias_work},
        {"alias_proof_additions", affine.alias_proof_additions},
        {"proof_additions", affine.proof_additions},
        {"proof_deletions", affine.proof_deletions},
        {"overhead_nanos", affine.overhead_nanos},
        {"synchronization_nanos", affine.synchronization_nanos},
        {"publication_nanos", affine.publication_nanos},
        {"resource_declines", affine.resource_declines},
        {"usefulness_disabled", affine.usefulness_disabled},
      };
      bool affine_active = false;
      for (const auto &metric : affine_head) affine_active |= metric.second != 0;
      for (const auto &metric : affine_tail) affine_active |= metric.second != 0;
      if (affine_active)
        for (const auto &[name, value] : affine_head)
          printf("c affine-search %s=%llu\n", name, (unsigned long long)value);
      const auto &graph = default_policy.graph_labeling_diagnostics();
      const auto graph_metric = [](const char *name, uint64_t value) {
        printf("c graph-schedule %s=%llu\n", name,
               (unsigned long long)value);
      };
      graph_metric("enabled", default_config.graph_labeling_model.enabled);
      graph_metric("authentication_attempts", graph.authentication_attempts);
      graph_metric("authentication_successes", graph.authentication_successes);
      graph_metric("authentication_exhausted", graph.authentication_exhausted);
      graph_metric("authentication_no_structure", graph.authentication_no_structure);
      graph_metric("authentication_interrupted", graph.authentication_interrupted);
      graph_metric("authentication_work", graph.authentication_work);
      graph_metric("slices", graph.slices);
      graph_metric("pending_slices", graph.pending_slices);
      graph_metric("completed_trajectories", graph.completed_trajectories);
      graph_metric("useful_hints", graph.useful_hints);
      graph_metric("runs", graph.runs);
      graph_metric("delayed_runs", graph.delayed_runs);
      graph_metric("fruitful_runs", graph.fruitful_runs);
      graph_metric("fruitless_runs", graph.fruitless_runs);
      graph_metric("run_conflict_limit", graph.run_conflict_limit);
      graph_metric("delay_current", graph.delay_current);
      graph_metric("recognized", kernel.stats.graph_labeling_recognized);
      graph_metric("vertices", kernel.stats.graph_labeling_vertices);
      graph_metric("separation", kernel.stats.graph_labeling_separation);
      graph_metric("work", kernel.stats.graph_labeling_work);
      graph_metric("nodes", kernel.stats.graph_labeling_nodes);
      graph_metric("repairs", kernel.stats.graph_labeling_repairs);
      graph_metric("anneal_attempts", kernel.stats.graph_labeling_anneal_attempts);
      graph_metric("sat", kernel.stats.graph_labeling_sat);
      graph_metric("search_ticks", kernel.stats.search_ticks);
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
          printf("c affine-search %s=%llu\n", name, (unsigned long long)value);
      }
    }
    stat_line("c count-pb    ", {{kernel.stats.counting_calls, "calls"},
                                 {kernel.stats.counting_stars, "stars"},
                                 {kernel.stats.counting_unsat, "UNSAT"},
                                 {kernel.stats.counting_work, "work"},
                                 {kernel.stats.counting_interrupted, "interrupted"},
                                 {kernel.stats.counting_additions, "additions"}});
    stat_line("c input       ",
              {{kernel.stats.input_restated, "clauses restated (normalized literals)"}});
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
    stat_line("c minimized   ", {{kernel.stats.minimized, "lits"}});
    stat_line("c min-depth   ", {{kernel.stats.minimize_depth_cutoffs, "cutoffs"}});
    stat_line("c shrunken    ", {{kernel.stats.shrunken, "lits"}});
    printf("c reasonbump  %llu vars in %llu rounds (%llu depth2, %llu aborted)\n",
           (unsigned long long)kernel.stats.reason_bumped,
           (unsigned long long)kernel.stats.reason_bump_rounds,
           (unsigned long long)kernel.stats.reason_bump_depth2,
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
    // Both are the reachability readouts for their arms: a zero on either means
    // TACTIX_SHRINK_GLUE / TACTIX_TUMBLE cannot have changed anything.
    printf("c shrinkglue  %llu promoted (clamp %s)\n",
           (unsigned long long)kernel.stats.shrink_promoted,
           kernel.shrink_glue_promote ? "size-1" : "size");
    printf("c tumble      %llu vars seeded (%s)\n",
           (unsigned long long)kernel.stats.tumble_seeded,
           kernel.tumble ? "activation order" : "off");
    // Kissat reports these three; they are the only readout of the sweep
    // that seeds Walk, and Walk's own step count is downstream of them.
    stat_line("c warmup      ", {{kernel.stats.warmups, "warmups"},
                                 {kernel.stats.warming_decisions, "decisions"},
                                 {kernel.stats.warming_propagations, "propagations"}});
    stat_line("c denseocc    ", {{kernel.stats.dense_slice_moves, "slice moves"},
                                 {kernel.stats.dense_defrags, "defrags"}});
    stat_line("c sparsecol   ", {{kernel.stats.sparse_collections, "runs"},
                                 {kernel.stats.sparse_collected, "clauses"}});
    // `vivified` here counts strengthened clauses only, so it is not comparable
    // to kissat's counter of the same name, an aggregate (statistics.h:247)
    // over subsumed, promoted, shrunken, implied, asym and instantiated.
    stat_line("c vivify      ", {{kernel.stats.vivify_rounds, "rounds"},
                                 {kernel.stats.vivified, "vivified"},
                                 {kernel.stats.vivify_deleted, "deleted"},
                                 {kernel.stats.vivify_flushed, "flushed"},
                                 {kernel.stats.vivify_checked, "checked"}});
    // Vivify trail reuse is inert three independent ways, one number each
    // (docs/design/2026-08-05-vivify-trail-reuse.md §4).
    stat_line("c vivify2     ", {{kernel.stats.vivify_reused, "reused"},
                                 {kernel.stats.vivify_probes, "probes"},
                                 {kernel.stats.vivify_reuse_full, "full"},
                                 {kernel.stats.vivify_tier3, "tier3"},
                                 {kernel.stats.vivify_prioritized, "prio"},
                                 {kernel.stats.vivify_reprioritized, "rearm"},
                                 {kernel.stats.vivify_polls, "polls"},
                                 {kernel.stats.vivify_terminated, "term"}});
    stat_line("c vivify3     ", {{kernel.stats.vivify_subsumed, "subsumed"},
                                 {kernel.stats.vivify_promoted, "promoted"},
                                 {kernel.stats.vivify_shrunken, "shrunken"},
                                 {kernel.stats.vivify_implied, "implied"},
                                 {kernel.stats.vivify_asymmetric, "asym"},
                                 {kernel.stats.vivify_instantiated, "instantiated"},
                                 {kernel.stats.vivify_noop, "noop"}});
    stat_line("c probe       ", {{kernel.stats.probe_rounds, "rounds"},
                                 {kernel.stats.probed, "probed"},
                                 {kernel.stats.probe_failed, "failed"}});
    stat_line("c backbone    ", {{kernel.stats.backbone_rounds, "rounds"},
                                 {kernel.stats.backbone_probed, "probed"},
                                 {kernel.stats.backbone_failed, "failed"}});
    stat_line("c transitive  ", {{kernel.stats.transitive_rounds, "rounds"},
                                 {kernel.stats.transitive_checked, "checked"},
                                 {kernel.stats.transitive_deleted, "deleted"},
                                 {kernel.stats.transitive_units, "units"},
                                 {kernel.stats.transitive_ticks, "ticks"},
                                 {kernel.stats.transitive_polls, "polls"},
                                 {kernel.stats.transitive_terminated, "terminated"}});
    printf("c substitute  %llu rounds, %llu ticks, %llu non-trivial SCCs (%llu lits), "
           "%llu vars substituted, %llu rounds skipped / %llu partial "
           "(%llu lits poisoned) on the hazard check\n",
           (unsigned long long)kernel.stats.substitute_rounds,
           (unsigned long long)kernel.stats.substitute_ticks,
           (unsigned long long)kernel.stats.substitute_sccs,
           (unsigned long long)kernel.stats.substitute_scc_lits,
           (unsigned long long)kernel.stats.substitute_vars,
           (unsigned long long)kernel.stats.substitute_hazard_skips,
           (unsigned long long)kernel.stats.substitute_hazard_partial,
           (unsigned long long)kernel.stats.substitute_hazard_lits);
    stat_line("c bce         ", {{kernel.stats.bce_rounds, "rounds"},
                                 {kernel.stats.bce_deleted, "deleted"},
                                 {kernel.stats.bce_ticks, "ticks"}});
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
    stat_line("c cce2        ", {{kernel.stats.cce_mark_skipped, "skipped"},
                                 {kernel.stats.cce_mark_advances, "advances"},
                                 {kernel.stats.cce_rearmed, "rearmed"}}, " / ");
    stat_line("c instantiate ", {{kernel.stats.instantiate_rounds, "rounds"},
                                 {kernel.stats.instantiated, "literals removed"},
                                 {kernel.stats.instantiate_checked, "probed"}});
    printf("c sweep       %llu rounds, %llu swept, %llu environments (%llu clauses), "
           "%llu solves, %llu candidates, %llu equivalences, %llu units "
           "(%llu already binary), %llu env-unsat, %llu budget, %llu ticks\n",
           (unsigned long long)kernel.stats.sweep_rounds,
           (unsigned long long)kernel.stats.sweep_swept,
           (unsigned long long)kernel.stats.sweep_environments,
           (unsigned long long)kernel.stats.sweep_env_clauses,
           (unsigned long long)kernel.stats.sweep_solves,
           (unsigned long long)kernel.stats.sweep_candidates,
           (unsigned long long)kernel.stats.sweep_equivalences,
           (unsigned long long)kernel.stats.sweep_units,
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
    stat_line("c sweepflip   ", {{kernel.stats.sweep_flip_attempts, "attempts"},
                                 {kernel.stats.sweep_flipped, "successful"}});
    stat_line("c sweepback   ", {{kernel.stats.sweep_backbone_candidates, "candidates"},
                                 {kernel.stats.sweep_backbone_flipped, "flipped"}});
    printf("c sweepcost   %llu dedup ticks, %llu sub-solver ticks, "
           "%llu representation words; closure "
           "%llu/%llu occurrences, %llu/%llu vars whole\n",
           (unsigned long long)kernel.stats.sweep_ticks_dedup,
           (unsigned long long)kernel.stats.sweep_ticks_sub,
           (unsigned long long)kernel.stats.sweep_ticks_repr,
           (unsigned long long)kernel.stats.sweep_occ_env,
           (unsigned long long)kernel.stats.sweep_occ_f,
           (unsigned long long)kernel.stats.sweep_vars_closed,
           (unsigned long long)kernel.stats.sweep_vars_total);
    stat_line("c sweepemit   ", {{kernel.stats.sweep_binaries, "binaries"},
                                 {kernel.stats.sweep_not_rup, "not-rup"},
                                 {kernel.stats.sweep_general, "general pairs"},
                                 {kernel.stats.sweep_strip_cited, "strip units cited"}});
    printf("c sweepsched  %llu scheduled, %llu rescheduled, %llu resumed, "
           "%llu completed, %zu tail; limits %u vars, %u clauses, depth %u\n",
           (unsigned long long)kernel.stats.sweep_scheduled,
           (unsigned long long)kernel.stats.sweep_rescheduled,
           (unsigned long long)kernel.stats.sweep_resumed,
           (unsigned long long)kernel.stats.sweep_completed,
           static_cast<size_t>(kernel.sweep_live),
           kernel.stats.sweep_limit_vars,
           kernel.stats.sweep_limit_clauses,
           kernel.stats.sweep_limit_depth);
    stat_line("c ternary     ", {{kernel.stats.ternary_rounds, "rounds"},
                                 {kernel.stats.ternary_resolvents, "resolvents"},
                                 {kernel.stats.ternary_binaries, "binaries"},
                                 {kernel.stats.ternary_ticks, "ticks"}});
    // A resolvent is emitted only if the RUP replay re-derives it, so binaries
    // 0 with declines > 0 means the rule ran and refused.
    stat_line("c hbr         ", {{kernel.stats.hbr_rounds, "rounds"},
                                 {kernel.stats.hbr_probes, "probes"},
                                 {kernel.stats.hbr_binaries, "binaries"},
                                 {kernel.stats.hbr_subsumed, "subsumed"},
                                 {kernel.stats.hbr_declined, "declined"},
                                 {kernel.stats.hbr_ticks, "ticks"}});
    // `stale backjumps` should be 0: nonzero means conflicts are dropped and
    // re-derived, which is otherwise invisible in a tier run.
    // `re-arms` counts clause additions that landed behind the propagation
    // cursor; each one is a unit or falsified clause the trail-driven scan
    // would otherwise never revisit.
    stat_line("c chrono      ",
              {{kernel.stats.chronological, "chronological"},
               {kernel.stats.backjump_stale, "stale backjumps"},
               {kernel.stats.propagation_rearms, "propagation re-arms"}});
    // The second number separates the full mechanism from the subset a version
    // that declines instead of repairing would reach.
    stat_line("c reuse       ",
              {{kernel.stats.conflict_reused, "conflicts reused"},
               {kernel.stats.conflict_watch_repaired, "needed watch repair"}});
    printf("c jumpreason  %llu binary reasons jumped, %llu classifications, "
           "%llu big-binary; gate %s\n",
           (unsigned long long)kernel.stats.jumped_reasons,
           (unsigned long long)kernel.stats.jump_reason_classifications,
           (unsigned long long)kernel.stats.jump_reason_bigbig,
           kernel.jump_reasons_bigbig ? "on" : "off");
    // What a binary-first watch order would be worth: `ordered` over `scans` is
    // the reachability, `large` over `words` the ceiling on an early break.
    stat_line("c binprop     ", {{kernel.stats.binprop_scans, "scans"},
                                 {kernel.stats.binprop_flagged, "flagged"},
                                 {kernel.stats.binprop_words, "words"},
                                 {kernel.stats.binprop_skipped, "skipped"},
                                 {kernel.stats.binprop_ordered, "residual-ordered"},
                                 {kernel.stats.binprop_large, "residual-large"}});
    stat_line("c inprocess   ", {{kernel.stats.inprocess_phases, "phases"}});
    stat_line("c conggates   ", {{kernel.stats.congruence_gates_and, "and"},
                                 {kernel.stats.congruence_gates_xor, "xor"},
                                 {kernel.stats.congruence_gates_ite, "ite"}});
    stat_line("c congruence  ",
              {{kernel.stats.congruence_rounds, "rounds"},
               {kernel.stats.congruence_binary_rebuilds, "binary rebuilds"},
               {kernel.stats.congruence_gates, "gates"},
               {kernel.stats.congruence_merges, "merges"},
               {kernel.stats.congruence_units, "units"},
               {kernel.stats.congruence_unjustified, "unjustified"},
               {kernel.stats.congruence_ticks, "ticks"},
               {kernel.stats.congruence_budget_stops, "budget stops"}});
    stat_line("c congy1      ",
              {{kernel.stats.congruence_y1_found, "found"},
               {kernel.stats.congruence_y1_units, "units"},
               {kernel.stats.congruence_binary_equivalences, "binary equivalences"},
               {kernel.stats.congruence_y1_ticks, "ticks"}});
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
    printf("c compact     %llu passes removed %llu internal variables\n",
           (unsigned long long)kernel.stats.variable_compactions,
           (unsigned long long)kernel.stats.compacted_variables);
    stat_line("c deactivate  ",
              {{kernel.stats.deactivations, "deactivations"},
               {kernel.stats.deactivation_repeats, "repeated reports"},
               {kernel.stats.reactivations, "re-bumps of retired vars"}});
    printf("c substdec    %llu decisions on substituted vars (%.2f%% of %llu)\n",
           (unsigned long long)kernel.stats.decisions_substituted,
           percent_of(kernel.stats.decisions_substituted, kernel.stats.decisions),
           (unsigned long long)kernel.stats.decisions);
    printf("c extension   %u vars added, %llu decisions on them (%.2f%% of "
           "%llu)\n",
           kernel.first_extension_var == UINT32_MAX
               ? 0u
               : kernel.nvars - kernel.first_extension_var,
           (unsigned long long)kernel.stats.decisions_extension,
           percent_of(kernel.stats.decisions_extension, kernel.stats.decisions),
           (unsigned long long)kernel.stats.decisions);
    printf("c interrupt   %llu rule applications aborted on the stop flag "
           "(%llu stress fires)\n",
           (unsigned long long)kernel.stats.rule_interrupted,
           (unsigned long long)kernel.stats.stop_fuzz_fires);
    printf("c extbumps    %llu of %llu bumps on extension vars (%.2f%%), "
           "%llu reason-side only (%.2f%% of those)\n",
           (unsigned long long)kernel.stats.bumps_extension,
           (unsigned long long)kernel.stats.bumps,
           percent_of(kernel.stats.bumps_extension, kernel.stats.bumps),
           (unsigned long long)kernel.stats.bumps_ext_reason_only,
           percent_of(kernel.stats.bumps_ext_reason_only,
                      kernel.stats.bumps_extension));
    // factor_ticks is Factor's dosing currency and what factor_initial_ticks caps,
    // so it separates a bound opening burst from an inert arm.
    stat_line("c factor      ",
              {{kernel.stats.factorings, "factorings"},
               {kernel.stats.factor_large_remainders, "large remainders offered"},
               {kernel.stats.factor_ticks, "ticks"}});
    stat_line("c factor-rem  ",
              {{kernel.stats.factor_remainders_materialized, "materialized"},
               {kernel.stats.factor_remainders_reused, "reused"}});
    stat_line("c factor-bnd  ",
              {{kernel.stats.factor_rejected_by_bound, "grids rejected by the coupled bound"}});
    stat_line("c factor-dem  ",
              {{kernel.stats.factor_demotions, "demotions"},
               {kernel.stats.factor_demoted_vars, "fresh vars demoted"}});
    stat_line("c sr          ",
              {{kernel.stats.sr_applications, "applications"},
               {kernel.stats.sr_applied, "applied"},
               {kernel.stats.sr_validation_ticks, "validation ticks"},
               {kernel.stats.sr_witness_literals, "witness literals"},
               {kernel.stats.sr_unique_literal_amo_binaries, "unique-literal AMO binaries"}});
    stat_line("c sr-life     ",
              {{kernel.stats.sr_post_amo_nonhelper_applied, "post-AMO nonhelper consequences"},
               {kernel.stats.sr_post_amo_graph_root_units, "graph root units"}});
  printf("c sr-admit    %llu nonforcing cut batches declined, %llu cuts; "
         "%llu singleton forcing batches declined, %llu rows\n",
           (unsigned long long)kernel.stats.sr_nonforcing_cut_batches_declined,
           (unsigned long long)kernel.stats.sr_nonforcing_cuts_declined,
           (unsigned long long)kernel.stats.sr_singleton_forcing_batches_declined,
           (unsigned long long)kernel.stats.sr_singleton_forcing_rows_declined);
    // Residual search handoff is off by default; print it only when enabled
    // or when it ran.
    if (default_config.inprocess.structured_root_residual_search_handoff ||
        kernel.stats.residual_handoff_calls ||
        kernel.stats.residual_handoff_variables ||
        kernel.stats.residual_handoff_nanos)
      printf("c handoff     %llu residual seeds, %llu variables, %.3f seconds\n",
             (unsigned long long)kernel.stats.residual_handoff_calls,
             (unsigned long long)kernel.stats.residual_handoff_variables,
             kernel.stats.residual_handoff_nanos / 1e9);
    stat_line("c bve         ", {{kernel.stats.bve_rounds, "rounds"},
                                 {kernel.stats.bve_eliminated, "eliminated"},
                                 {kernel.stats.bve_resolutions, "resolutions"}});
    stat_line("c bve-otf     ", {{kernel.stats.bve_otf_strengthened, "strengthened"},
                                 {kernel.stats.bve_otf_subsumed, "subsumed"}});
    printf("c fastel      %llu checks, %llu candidates / %llu active, "
           "%llu discovery ticks, %llu budget stops, %llu minority declines, "
           "%llu root-suppressed, %llu extensions protected, %llu eliminated, "
           "%llu resolutions, "
           "%.3f seconds\n",
           (unsigned long long)kernel.stats.fastel_rounds,
           (unsigned long long)kernel.stats.fastel_candidates,
           (unsigned long long)kernel.stats.fastel_active,
           (unsigned long long)kernel.stats.fastel_discovery_ticks,
           (unsigned long long)kernel.stats.fastel_budget_stops,
           (unsigned long long)kernel.stats.fastel_declined,
           (unsigned long long)kernel.stats.fastel_root_suppressed,
           (unsigned long long)kernel.stats.fastel_extensions_protected,
           (unsigned long long)kernel.stats.fastel_eliminated,
           (unsigned long long)kernel.stats.fastel_resolutions,
           kernel.stats.fastel_nanos / 1e9);
    stat_line("c bve-bwd     ", {{kernel.stats.bve_bwd_queued, "subsumers"},
                                 {kernel.stats.bve_bwd_subsumed, "subsumed"},
                                 {kernel.stats.bve_bwd_strengthened, "strengthened"},
                                 {kernel.stats.bve_bwd_units, "units"}});
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
    stat_line("c bve-def     ",
              {{kernel.stats.bve_definitions_checked, "checked"},
               {kernel.stats.bve_definition_ticks, "ticks"},
               {kernel.stats.bve_definition_budget, "hit the tick ceiling"},
               {kernel.stats.bve_definition_units, "units"}});
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
           percent_of(kernel.stats.reduce_skippable_words,
                      kernel.stats.reduce_scan_words));
    printf("c mode        %s after %llu switches (%llu ticks, "
           "%llu target phases saved)\n",
           !kernel.h ? "unguided" : kernel.h->stable ? "stable" : "focused",
           (unsigned long long)kernel.stats.switched,
           (unsigned long long)kernel.stats.ticks,
           (unsigned long long)kernel.stats.target_saved);
    printf("c rephased    %llu (%llu best phases saved)\n",
           (unsigned long long)kernel.stats.rephased,
           (unsigned long long)kernel.stats.best_saved);
    stat_line("c reordered   ", {{kernel.stats.reorder_eligible, "eligible"},
                                 {kernel.stats.reordered, "decision re-rankings"}});
    printf("c lucky       %llu early, %llu late (%llu SAT), %llu probes\n",
           (unsigned long long)kernel.stats.lucky_early_calls,
           (unsigned long long)kernel.stats.lucky_late_calls,
           (unsigned long long)kernel.stats.lucky_late_sat,
           (unsigned long long)kernel.stats.lucky_probes);
    printf("c parity     %llu calls, %llu equations, rank %llu, "
           "%llu candidates, %llu SAT, %llu interrupted\n",
           (unsigned long long)kernel.stats.parity_model_calls,
           (unsigned long long)kernel.stats.parity_model_equations,
           (unsigned long long)kernel.stats.parity_model_rank,
           (unsigned long long)kernel.stats.parity_model_candidates,
           (unsigned long long)kernel.stats.parity_model_sat,
           (unsigned long long)kernel.stats.parity_model_interrupted);
    stat_line("c parity-ref ",
              {{kernel.stats.parity_refutation_probe_declines, "probe declines"},
               {kernel.stats.parity_refutation_recognized, "recognized"},
               {kernel.stats.parity_refutation_unsat, "UNSAT"},
               {kernel.stats.parity_refutation_proof_additions, "proof additions"},
               {kernel.stats.parity_refutation_proof_deletions, "proof deletions"}});
    stat_line("c theory-act ",
              {{kernel.stats.parity_consequence_attempts, "parity"},
               {kernel.stats.affine_consequence_attempts, "affine"},
               {kernel.stats.exact_cover_extension_attempts, "exact-cover"},
               {kernel.stats.independent_set_cover_attempts, "independent-set"},
               {kernel.stats.semantic_variable_advice_attempts, "advice attempts"}});
    stat_line("c directphp  ",
              {{kernel.stats.direct_php_attempts, "attempts"},
               {kernel.stats.direct_php_applied, "applied"},
               {kernel.stats.direct_php_not_recognized, "not-recognized"},
               {kernel.stats.direct_php_resource_exhausted, "resource-exhausted"},
               {kernel.stats.direct_php_interrupted, "interrupted"},
               {kernel.stats.direct_php_stale, "stale"},
               {kernel.stats.direct_php_replay_mismatch, "replay-mismatch"}});
    stat_line("c gf3-gauss  ",
              {{kernel.stats.gf3_gaussian_attempts, "attempts"},
               {kernel.stats.gf3_gaussian_applied, "applied"},
               {kernel.stats.gf3_gaussian_no_structure, "no-structure"},
               {kernel.stats.gf3_gaussian_no_supported_outcome, "no-outcome"},
               {kernel.stats.gf3_gaussian_exhausted, "exhausted"},
               {kernel.stats.gf3_gaussian_interrupted, "interrupted"},
               {kernel.stats.gf3_gaussian_stale, "stale"},
               {kernel.stats.gf3_gaussian_unsupported_proof, "unsupported-proof"}});
    stat_line("c gf3-row    ",
              {{kernel.stats.gf3_general_row_attempts, "attempts"},
               {kernel.stats.gf3_general_row_applied, "applied"},
               {kernel.stats.gf3_general_row_no_structure, "no-structure"},
               {kernel.stats.gf3_general_row_no_consequence, "no-consequence"},
               {kernel.stats.gf3_general_row_already_published, "already-published"},
               {kernel.stats.gf3_general_row_exhausted, "exhausted"},
               {kernel.stats.gf3_general_row_interrupted, "interrupted"},
               {kernel.stats.gf3_general_row_stale, "stale"},
               {kernel.stats.gf3_general_row_unsupported_proof, "unsupported-proof"},
               {kernel.stats.gf3_general_row_installed_clauses, "clauses"}});
    stat_line("c card-search ", {{kernel.stats.cardinality_search_attempts, "attempts"},
                                 {kernel.stats.cardinality_search_applied, "applied"}});
    stat_line("c tseitin-cycle ",
              {{kernel.stats.tseitin_cycle_attempts, "attempts"},
               {kernel.stats.tseitin_cycle_applied, "applied"},
               {kernel.stats.tseitin_cycle_no_structure, "no-structure"},
               {kernel.stats.tseitin_cycle_exhausted, "exhausted"},
               {kernel.stats.tseitin_cycle_interrupted, "interrupted"},
               {kernel.stats.tseitin_cycle_stale, "stale"},
               {kernel.stats.tseitin_cycle_unsupported_proof, "unsupported-proof"}});
    stat_line("c nary-eq    ",
              {{kernel.stats.nary_csp_equality_attempts, "attempts"},
               {kernel.stats.nary_csp_equality_applied, "applied"},
               {kernel.stats.nary_csp_equality_no_structure, "no-structure"},
               {kernel.stats.nary_csp_equality_no_consequence, "no-consequence"},
               {kernel.stats.nary_csp_equality_already_published, "already-published"},
               {kernel.stats.nary_csp_equality_invalid, "invalid"},
               {kernel.stats.nary_csp_equality_inapplicable, "inapplicable"},
               {kernel.stats.nary_csp_equality_resource_exhausted, "exhausted"},
               {kernel.stats.nary_csp_equality_interrupted, "interrupted"},
               {kernel.stats.nary_csp_equality_stale, "stale"},
               {kernel.stats.nary_csp_equality_unsupported_proof, "unsupported-proof"},
               {kernel.stats.nary_csp_equality_introduced_variables, "introduced"}});
    stat_line("c nary-eq-amo ",
              {{kernel.stats.nary_csp_equality_amo_attempts, "attempts"},
               {kernel.stats.nary_csp_equality_amo_applied, "applied"},
               {kernel.stats.nary_csp_equality_amo_missing_equality, "missing-equality"},
               {kernel.stats.nary_csp_equality_amo_stale, "stale"},
               {kernel.stats.nary_csp_equality_amo_duplicate, "duplicate"},
               {kernel.stats.nary_csp_equality_amo_declined, "declined"},
               {kernel.stats.nary_csp_equality_amo_binary_clauses, "clauses"}});
    stat_line("c affine-matrix-init ",
              {{kernel.stats.affine_matrix_init_attempts, "attempts"},
               {kernel.stats.affine_matrix_init_applied, "applied"},
               {kernel.stats.affine_matrix_init_applied_partial, "partial"},
               {kernel.stats.affine_matrix_init_incomplete_image, "incomplete-image"},
               {kernel.stats.affine_matrix_init_incomplete_matrix, "incomplete-matrix"},
               {kernel.stats.affine_matrix_init_exhausted, "exhausted"},
               {kernel.stats.affine_matrix_init_interrupted, "interrupted"},
               {kernel.stats.affine_matrix_init_normalization_rup_checks, "normalization-RUP-checks"},
               {kernel.stats.affine_matrix_init_shortening_additions, "shortening-additions"},
               {kernel.stats.affine_matrix_init_maintenance_service_work, "maintenance-service-work"}});
    stat_line("c affine_discovery ",
              {{kernel.stats.affine_matrix_init_discovery_recognized_groups, "recognized-groups"},
               {kernel.stats.affine_matrix_init_discovery_skipped_groups, "unchanged-groups"},
               {kernel.stats.affine_matrix_init_discovery_recognizer_work, "recognizer-work"}});
    stat_line("c affine     ", {{kernel.stats.affine_model_calls, "calls"},
                                {kernel.stats.affine_model_equations, "equations"},
                                {kernel.stats.affine_model_projection, "coordinates"},
                                {kernel.stats.affine_model_entries, "entries"},
                                {kernel.stats.affine_model_candidates, "candidates"},
                                {kernel.stats.affine_model_sat, "SAT"},
                                {kernel.stats.affine_model_work, "work"},
                                {kernel.stats.affine_model_budget, "budgets"},
                                {kernel.stats.affine_model_unavailable, "unavailable"},
                                {kernel.stats.affine_model_interrupted, "interrupted"}});
    printf("c onehot     %llu calls, %llu domains, %llu equations, rank %llu, "
           "%llu candidates, %llu SAT, %llu interrupted\n",
           (unsigned long long)kernel.stats.onehot_model_calls,
           (unsigned long long)kernel.stats.onehot_model_domains,
           (unsigned long long)kernel.stats.onehot_model_equations,
           (unsigned long long)kernel.stats.onehot_model_rank,
           (unsigned long long)kernel.stats.onehot_model_candidates,
           (unsigned long long)kernel.stats.onehot_model_sat,
           (unsigned long long)kernel.stats.onehot_model_interrupted);
    printf("c directcsp  %llu calls, %llu domains, %llu edges, %llu restarts, "
           "%llu infeasible, %llu work, best %llu, %llu SAT\n",
           (unsigned long long)kernel.stats.direct_csp_model_calls,
           (unsigned long long)kernel.stats.direct_csp_model_domains,
           (unsigned long long)kernel.stats.direct_csp_model_edges,
           (unsigned long long)kernel.stats.direct_csp_model_restarts,
           (unsigned long long)kernel.stats.direct_csp_model_infeasible,
           (unsigned long long)kernel.stats.direct_csp_model_work,
           (unsigned long long)(kernel.stats.direct_csp_model_best == UINT64_MAX
                                    ? 0
                                    : kernel.stats.direct_csp_model_best),
           (unsigned long long)kernel.stats.direct_csp_model_sat);
    stat_line("c exact-one  ",
              {{kernel.stats.exact_one_refutation_calls, "calls"},
               {kernel.stats.exact_one_refutation_recognized, "recognized"},
               {kernel.stats.exact_one_refutation_equations, "equations"},
               {kernel.stats.exact_one_refutation_unsat, "UNSAT"},
               {kernel.stats.exact_one_refutation_interrupted, "interrupted"},
               {kernel.stats.exact_one_refutation_proof_additions, "proof additions"},
               {kernel.stats.exact_one_refutation_proof_deletions, "proof deletions"}});
    stat_line("c balance    ",
              {{kernel.stats.balanced_signature_calls, "calls"},
               {kernel.stats.balanced_signature_recognized, "recognized"},
               {kernel.stats.balanced_signature_forced_ttf, "forced TTF"},
               {kernel.stats.balanced_signature_candidates, "candidates"},
               {kernel.stats.balanced_signature_restarts, "restarts"},
               {kernel.stats.balanced_signature_steps, "steps"},
               {kernel.stats.balanced_signature_completion_rounds, "completion rounds"},
               {kernel.stats.balanced_signature_sat, "SAT"},
               {kernel.stats.balanced_signature_interrupted, "interrupted"}});
    stat_line("c fermat     ",
              {{kernel.stats.fermat_model_calls, "calls"},
               {kernel.stats.fermat_model_recognized, "recognized"},
               {kernel.stats.fermat_model_candidates, "candidates"},
               {kernel.stats.fermat_model_factor_iterations, "factor iterations"},
               {kernel.stats.fermat_model_completion_rounds, "completion rounds"},
               {kernel.stats.fermat_model_sat, "SAT"},
               {kernel.stats.fermat_model_interrupted, "interrupted"}});
    stat_line("c cerny      ", {{kernel.stats.cerny_model_calls, "calls"},
                                {kernel.stats.cerny_model_recognized, "recognized"},
                                {kernel.stats.cerny_model_states, "states"},
                                {kernel.stats.cerny_model_length, "length"},
                                {kernel.stats.cerny_model_sat, "SAT"},
                                {kernel.stats.cerny_model_interrupted, "interrupted"}});
    stat_line("c lookup     ",
              {{kernel.stats.lookup_circuit_model_calls, "calls"},
               {kernel.stats.lookup_circuit_model_recognized, "recognized"},
               {kernel.stats.lookup_circuit_model_gates, "gates"},
               {kernel.stats.lookup_circuit_model_support, "support"},
               {kernel.stats.lookup_circuit_model_vector_evaluations, "vector gate evaluations"},
               {kernel.stats.lookup_circuit_model_csp_work, "CSP work"},
               {kernel.stats.lookup_circuit_model_sat, "SAT"},
               {kernel.stats.lookup_circuit_model_interrupted, "interrupted"}});
    stat_line("c simon      ", {{kernel.stats.simon_model_calls, "calls"},
                                {kernel.stats.simon_model_recognized, "recognized"},
                                {kernel.stats.simon_model_rounds, "rounds"},
                                {kernel.stats.simon_model_candidates, "candidates"},
                                {kernel.stats.simon_model_sat, "SAT"},
                                {kernel.stats.simon_model_interrupted, "interrupted"}});
    printf("c stconn     %llu calls, %llu recognized, width %llu, "
           "%llu columns, %llu RUP clauses, %llu budget stops, %llu UNSAT, "
           "%llu interrupted\n",
           (unsigned long long)kernel.stats.stconn_proof_calls,
           (unsigned long long)kernel.stats.stconn_proof_recognized,
           (unsigned long long)kernel.stats.stconn_proof_width,
           (unsigned long long)kernel.stats.stconn_proof_columns,
           (unsigned long long)kernel.stats.stconn_proof_rup_clauses,
           (unsigned long long)kernel.stats.stconn_proof_budget_exhausted,
           (unsigned long long)kernel.stats.stconn_proof_unsat,
           (unsigned long long)kernel.stats.stconn_proof_interrupted);
    stat_line("c signedcard ",
              {{kernel.stats.signed_cardinality_proof_calls, "calls"},
               {kernel.stats.signed_cardinality_proof_recognized, "recognized"},
               {kernel.stats.signed_cardinality_proof_groups, "groups"},
               {kernel.stats.signed_cardinality_proof_bdd_nodes, "BDD nodes"},
               {kernel.stats.signed_cardinality_proof_additions, "proof additions"},
               {kernel.stats.signed_cardinality_proof_budget_exhausted, "budget stops"},
               {kernel.stats.signed_cardinality_proof_unsat, "UNSAT"},
               {kernel.stats.signed_cardinality_proof_interrupted, "interrupted"}});
    stat_line("c exactcover ",
              {{kernel.stats.exact_cover_model_calls, "calls"},
               {kernel.stats.exact_cover_model_recognized, "recognized"},
               {kernel.stats.exact_cover_model_blocks, "blocks"},
               {kernel.stats.exact_cover_model_nodes, "nodes"},
               {kernel.stats.exact_cover_model_restarts, "restarts"},
               {kernel.stats.exact_cover_model_budget_exhausted, "budget stops"},
               {kernel.stats.exact_cover_model_sat, "SAT"},
               {kernel.stats.exact_cover_model_interrupted, "interrupted"}});
    printf("c indepset   %llu calls, %llu recognized, %llu vertices, %llu target, "
           "%llu greedy, %llu found, %llu steps, %llu completion rounds, "
           "%llu SAT, %llu interrupted; %llu stable cores, %llu covers, "
           "%llu authenticated resolvents\n",
           (unsigned long long)kernel.stats.independent_set_model_calls,
           (unsigned long long)kernel.stats.independent_set_model_recognized,
           (unsigned long long)kernel.stats.independent_set_model_vertices,
           (unsigned long long)kernel.stats.independent_set_model_target,
           (unsigned long long)kernel.stats.independent_set_model_greedy_size,
           (unsigned long long)kernel.stats.independent_set_model_size,
           (unsigned long long)kernel.stats.independent_set_model_steps,
           (unsigned long long)
               kernel.stats.independent_set_model_completion_rounds,
           (unsigned long long)kernel.stats.independent_set_model_sat,
           (unsigned long long)kernel.stats.independent_set_model_interrupted,
           (unsigned long long)kernel.stats.independent_set_stable_cores,
           (unsigned long long)
               kernel.stats.independent_set_stable_cover_clauses,
           (unsigned long long)
               kernel.stats.independent_set_stable_resolvents);
    stat_line("c graphlabel ",
              {{kernel.stats.graph_labeling_calls, "calls"},
               {kernel.stats.graph_labeling_recognized, "recognized"},
               {kernel.stats.graph_labeling_vertices, "vertices"},
               {kernel.stats.graph_labeling_separation, "separation"},
               {kernel.stats.graph_labeling_seeds, "seeds"},
               {kernel.stats.graph_labeling_nodes, "nodes"},
               {kernel.stats.graph_labeling_repairs, "repairs"},
               {kernel.stats.graph_labeling_anneal_attempts, "anneal attempts"},
               {kernel.stats.graph_labeling_work, "work"},
               {kernel.stats.graph_labeling_budget, "budget stops"},
               {kernel.stats.graph_labeling_interrupted, "interrupted"},
               {kernel.stats.graph_labeling_models_rejected, "models rejected"},
               {kernel.stats.graph_labeling_sat, "SAT"}});
    printf("c graphinterv %llu calls, %llu recognized, %llu vertices, "
           "%llu gates, %llu intervals, %llu groups, %llu selected, "
           "%llu work, %llu source clauses/%llu literals/%llu bytes, "
           "%llu resource stops/%llu interrupted, (%llu RUP ticks), %llu RUP rejected, "
           "%llu duplicates, %llu additions, %llu proof literals\n",
           (unsigned long long)kernel.stats.graph_interval_equivalence_calls,
           (unsigned long long)kernel.stats.graph_interval_equivalence_recognized,
           (unsigned long long)kernel.stats.graph_interval_equivalence_vertices,
           (unsigned long long)kernel.stats.graph_interval_equivalence_gates,
           (unsigned long long)kernel.stats.graph_interval_equivalence_intervals,
           (unsigned long long)kernel.stats.graph_interval_equivalence_groups,
           (unsigned long long)kernel.stats.graph_interval_equivalence_selected,
           (unsigned long long)kernel.stats.graph_interval_equivalence_work,
           (unsigned long long)kernel.stats.graph_interval_equivalence_source_clauses,
           (unsigned long long)kernel.stats.graph_interval_equivalence_source_literals,
           (unsigned long long)kernel.stats.graph_interval_equivalence_source_workspace,
           (unsigned long long)kernel.stats.graph_interval_equivalence_resource_exhausted,
           (unsigned long long)kernel.stats.graph_interval_equivalence_interrupted,
           (unsigned long long)kernel.stats.graph_interval_equivalence_rup_ticks,
           (unsigned long long)kernel.stats.graph_interval_equivalence_rup_rejected,
           (unsigned long long)kernel.stats.graph_interval_equivalence_duplicates,
           (unsigned long long)kernel.stats.graph_interval_equivalence_published,
           (unsigned long long)kernel.stats.graph_interval_equivalence_proof_literal_occurrences);
    stat_line("c ramsey     ",
              {{kernel.stats.ramsey_startup_attempts, "attempts"},
               {kernel.stats.ramsey_startup_recognized, "recognized"},
               {kernel.stats.ramsey_startup_recognition_work, "recognition work"},
               {kernel.stats.ramsey_startup_certificate_work, "certificate work"},
               {kernel.stats.ramsey_startup_nodes, "nodes"},
               {kernel.stats.ramsey_startup_additions, "additions"},
               {kernel.stats.ramsey_startup_deletions, "deletions"},
               {kernel.stats.ramsey_startup_helpers, "helpers"},
               {kernel.stats.ramsey_startup_exhausted, "budget stops"},
               {kernel.stats.ramsey_startup_unsat, "UNSAT"},
               {kernel.stats.ramsey_startup_interrupted, "interrupted"}});
    printf("c localsearch %llu hints offered (%llu phases, %llu replacements), "
           "%llu applied / %llu target installs from %llu attempts "
           "(%llu eligible, %llu changed, "
           "%llu matching, %llu replaced)\n",
           (unsigned long long)kernel.stats.local_search_hints_offered,
           (unsigned long long)kernel.stats.local_search_hint_phases_offered,
           (unsigned long long)kernel.stats.local_search_hints_replaced,
           (unsigned long long)kernel.stats.local_search_hints_applied,
           (unsigned long long)kernel.stats.local_search_target_installs,
           (unsigned long long)kernel.stats.local_search_target_install_attempts,
           (unsigned long long)kernel.stats.local_search_target_eligible_phases,
           (unsigned long long)kernel.stats.local_search_target_changed_phases,
           (unsigned long long)kernel.stats.local_search_target_matching_phases,
           (unsigned long long)kernel.stats.local_search_target_replaced_phases);
    printf("c localphase %llu decisions (%llu matched, %llu overridden), "
           "%llu native / %llu rephase / %llu walk overwrites\n",
           (unsigned long long)kernel.stats.local_search_target_decisions,
           (unsigned long long)kernel.stats.local_search_target_decision_matches,
           (unsigned long long)kernel.stats.local_search_target_decision_overrides,
           (unsigned long long)kernel.stats.local_search_target_overwritten_native,
           (unsigned long long)kernel.stats.local_search_target_overwritten_rephase,
           (unsigned long long)kernel.stats.local_search_target_overwritten_walk);
    printf("c walk        %llu walks (%llu improved, %llu flips, %llu steps)\n",
           (unsigned long long)kernel.stats.walks,
           (unsigned long long)kernel.stats.walk_improved,
           (unsigned long long)kernel.stats.walk_flips,
           (unsigned long long)kernel.stats.walk_steps);
    printf("c walkseed    %llu imports / %llu phases (%llu improved, %llu exported), %llu steps, "
           "%llu budget stops, %llu interrupted, %llu stale hints\n",
           (unsigned long long)kernel.stats.walk_seed_imports,
           (unsigned long long)kernel.stats.walk_seed_imported_phases,
           (unsigned long long)kernel.stats.walk_seed_improved,
           (unsigned long long)kernel.stats.walk_seed_exported,
           (unsigned long long)kernel.stats.walk_seed_steps,
           (unsigned long long)kernel.stats.walk_seed_budget_exhausted,
           (unsigned long long)kernel.stats.walk_seed_interrupted,
           (unsigned long long)kernel.stats.local_search_hints_stale);
    stat_line("c randec      ", {{kernel.stats.random_sequences, "sequences"},
                                 {kernel.stats.random_decisions, "random decisions"}});
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
    stat_line("c budgetstop  ",
              {{kernel.stats.budget_stops_c1, "C1 rounds"},
               {kernel.stats.budget_stops_c2, "C2 rounds stopped mid-round"}});
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
    stat_line("c subhold     ",
              {{kernel.stats.subsume_holds, "elimination rounds held open by C1/C2"}});
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
    stat_line("c touch4      Factor ",
              {{kernel.stats.factor_mark_skipped, "seeds skipped"},
               {kernel.stats.factor_rearmed, "rearmed"}}, " / ");
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
      printf("c phasecost   structured %.0f ms, entry %.0f ms, dense-connect %.0f ms, "
             "slot-loop %.0f ms, exit %.0f ms\n",
             kernel.stats.structured_root_nanos / 1e6,
             kernel.stats.inprocess_entry_nanos / 1e6,
             kernel.stats.inprocess_dense_connect_nanos / 1e6,
             kernel.stats.inprocess_slot_loop_nanos / 1e6,
             kernel.stats.inprocess_exit_nanos / 1e6);
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
    {
      if (std::getenv("TACTIX_STRATEGY_CONFIG"))
        printf("c strategy-config %s\n",
               default_policy.strategy_config_snapshot().c_str());
      if (std::getenv("TACTIX_SLOT_STATS"))
        default_policy.inprocess().report(kernel);
    }
    // Why the automatic proof is (or is still) its format: `dsr-selected`
    // or `pbp-predicted-faster` after the empty clause, `pending` without
    // one, else the PBP trigger. The cost line gives the compared estimates
    // and the DSR copy's checker-work census (check_cost.hpp).
    if (proof_path && proof.automatic()) {
      printf("c auto-proof    %s\n",
             automatic_proof_reason_name(proof.automatic_reason()));
      if (const tactix::AutomaticCheckEstimate *e = proof.automatic_estimate()) {
        const tactix::DsrCheckCensus &c = e->census;
        printf("c auto-proof-cost dsr-scan %.0f s pbp %.0f s; %s %" PRIu64
               " bytes, pbp %" PRIu64 " bytes; rat %" PRIu64 " sr %" PRIu64
               " maxvar %" PRIu32 " watch %" PRIu64 " body %" PRIu64
               " dead %" PRIu64 " live %" PRIu64 " live-literal %" PRIu64 "\n",
               e->dsr_scan_seconds, e->pbp_seconds,
               e->witnessed ? "dsr-trestle" : "dsr-cake_lpr", e->dsr_bytes,
               e->pbp_bytes, c.rat_lines, c.sr_lines, e->max_variable,
               c.watch_scan, c.watched_literal_scan, c.deleted_slot_scan,
               c.live_scan, c.live_literal_scan);
      }
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
  } else if (res == 20) {
    if (proof_path)
      printf("c proof-route %s\n", proof.verification_route());
    printf("s UNSATISFIABLE\n");
  } else {
    printf("s UNKNOWN\n");
  }
  fflush(stdout);
  if (feedback_failed) return 1; // Solver verdict and certificate remain intact.
  return res == 10 ? 10 : res == 20 ? 20 : 0;
}
