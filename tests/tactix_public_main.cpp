// tactix-public: the shipped DefaultPolicy as a command-line solver whose
// actions leave through PublicPort (tests/default_public_port.hpp), i.e. as
// tactix.h requests on a C session. Input ingestion and solver setup mirror
// `tactix --auto-proof <cnf> [proof]` (src/main.cpp) so certificates and
// trajectories are comparable with the production binary: the CLI parser
// with its advisory header, Kissat's default BVE gates, deferred cold state,
// and H attached by DefaultPolicy after the startup opportunity rather than
// owned by the session. Setup is not a strategy action; only the policy's
// requests are routed through the public interface.
#include "api/api_internal.hpp"
#include "default_public_port.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/bve/bve.hpp"

#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstring>

using namespace tactix;

namespace {

volatile sig_atomic_t caught_signal = 0;
void handle_signal(int) { caught_signal = 1; }

// The CLI's model format (src/main.cpp print_model).
void print_model(const Kernel &k) {
  char line[4096];
  char *cursor = line;
  *cursor++ = 'v';
  auto flush_line = [&] {
    *cursor++ = '\n';
    fwrite(line, 1, static_cast<size_t>(cursor - line), stdout);
    cursor = line;
    *cursor++ = 'v';
  };
  for (Var v = 0; v < kernel_num_output_vars(k); v++) {
    const Lit pos = make_lit(v, false);
    const int e = static_cast<int>(v) + 1;
    if (cursor - line + 16 > 4094) flush_line();
    *cursor++ = ' ';
    const int value = k.model_values[pos] > 0 ? e : -e;
    cursor = std::to_chars(cursor, line + 4094, value).ptr;
  }
  *cursor++ = ' ';
  *cursor++ = '0';
  flush_line();
}

void print_census() {
  const auto &census = port::PublicPort::census;
  uint64_t calls = 0, fallbacks = 0, declined = 0;
  calls = census.total_calls;
  for (const auto &[kind, count] : census.fallbacks) fallbacks += count;
  for (const auto &[kind, count] : census.declined) declined += count;
  printf("c public-port requests %llu fallbacks %llu declined %llu\n",
         static_cast<unsigned long long>(calls),
         static_cast<unsigned long long>(fallbacks),
         static_cast<unsigned long long>(declined));
  for (const auto &[kind, count] : census.fallbacks)
    printf("c public-port fallback %s %llu\n", kind.c_str(),
           static_cast<unsigned long long>(count));
  for (const auto &[kind, count] : census.declined)
    printf("c public-port declined %s %llu\n", kind.c_str(),
           static_cast<unsigned long long>(count));
}

} // namespace

int main(int argc, char **argv) {
  const char *cnf_path = nullptr, *proof_path = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--auto-proof")) continue;
    if (argv[i][0] == '-' && argv[i][1]) {
      fprintf(stderr, "tactix-public: unsupported option '%s'\n", argv[i]);
      printf("s UNKNOWN\n");
      return 1;
    }
    if (!cnf_path) cnf_path = argv[i];
    else if (!proof_path) proof_path = argv[i];
    else {
      fprintf(stderr, "usage: tactix-public [--auto-proof] <cnf> [proof]\n");
      return 1;
    }
  }
  if (!cnf_path) {
    fprintf(stderr, "usage: tactix-public [--auto-proof] <cnf> [proof]\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);
#ifdef SIGXCPU
  signal(SIGXCPU, handle_signal);
#endif

  tactix_c_handle *s = tactix_init();
  Kernel &kernel = api_detail::kernel_for_fixed_testing(s);
  // As the default CLI frontend: H belongs to DefaultPolicy, not the session.
  kernel.h = nullptr;
  kernel.saved_phases = nullptr;
  kernel_defer_cold_state(kernel);
  if (tactix_set_guidance_publication(s, TACTIX_GUIDANCE_NATIVE) != 0) return 1;
  tactix_set_bve_gates(s, BVE_GATE_KISSAT_DEFAULT);
  // The CLI's anonymous structural provenance stream.
  FILE *provenance = std::tmpfile();
  if (!provenance || tactix_bind_structural_provenance(s, provenance) != 0) {
    fprintf(stderr, "tactix-public: cannot create structural provenance stream\n");
    printf("s UNKNOWN\n");
    return 1;
  }
  if (proof_path && tactix_proof(s, proof_path, TACTIX_PROOF_AUTOMATIC) != 0) {
    fprintf(stderr, "tactix-public: cannot write proof to '%s'\n", proof_path);
    printf("s UNKNOWN\n");
    return 1;
  }
  FILE *in = std::fopen(cnf_path, "r");
  if (!in) {
    fprintf(stderr, "tactix-public: cannot read '%s'\n", cnf_path);
    printf("s UNKNOWN\n");
    return 1;
  }
  const int read = tactix_read_dimacs(s, in);
  std::fclose(in);
  if (read != 0) {
    fprintf(stderr, "tactix-public: parse error in '%s'\n", cnf_path);
    printf("s UNKNOWN\n");
    return 1;
  }

  port::PublicPort::session = s;
  port::PublicPort::retirements_held = false;
  port::PublicPort::census.clear();
  port::PublicPort::census.detailed = false; // keep the per-call tally cheap
  int res;
  {
    DefaultStrategyConfig config;
    DefaultPolicy policy(config);
    res = policy.solve(kernel, &caught_signal);
  }
  port::PublicPort::session = nullptr;
  if (kernel.proof) kernel.proof->flush();

  printf("c tactix-public 0.1.0\n");
  printf("c conflicts %llu\n", static_cast<unsigned long long>(kernel.stats.conflicts));
  printf("c decisions %llu\n", static_cast<unsigned long long>(kernel.stats.decisions));
  printf("c propagations %llu\n", static_cast<unsigned long long>(kernel.stats.propagations));
  print_census();
  if (res == 10) {
    kernel_extend_model(kernel);
    printf("s SATISFIABLE\n");
    print_model(kernel);
  } else if (res == 20) {
    if (kernel.proof) printf("c proof-route %s\n", kernel.proof->verification_route());
    printf("s UNSATISFIABLE\n");
  } else {
    printf("s UNKNOWN\n");
  }
  fflush(stdout);
  tactix_release(s);
  std::fclose(provenance);
  return res == 10 ? 10 : res == 20 ? 20 : 0;
}
