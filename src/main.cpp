// Competition frontend (plan §6, compete skill): DIMACS in (plain or
// .xz/.gz via decompressor pipe), `s`/`v` lines on stdout, exit codes
// 10/20/other, proof to argv[2], SIGTERM/SIGINT/SIGXCPU -> flush proof,
// print `s UNKNOWN`, exit cleanly.
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/random_policy.hpp"
#include "proof/proof.hpp"

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
  for (Var v = 0; v < k.nvars; v++) {
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
  bool binary_proof = true;
  bool quiet = false;
  bool random_policy = false;
  unsigned long long random_seed = 20260710;

  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--version")) {
      puts("0.1.0");
      return 0;
    } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
      quiet = true;
    } else if (!strcmp(a, "--no-binary")) {
      binary_proof = false;
    } else if (!strncmp(a, "--random-policy", 15)) {
      // soundness testing mode (plan §8.2 rule-order fuzzing): random
      // legal rule schedule, random branching instances, proofs intact
      random_policy = true;
      if (a[15] == '=')
        random_seed = strtoull(a + 16, nullptr, 10);
    } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      printf("usage: tactix [-q] [--no-binary] [--random-policy[=SEED]] "
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

  Kernel kernel;
  Proof proof;
  if (proof_path) {
    if (!proof.open(proof_path, binary_proof)) {
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

  const int res = random_policy
                      ? random_solve(kernel, random_seed, &caught_signal)
                      : default_solve(kernel, &caught_signal);

  proof.flush();
  if (!quiet) {
    printf("c tactix 0.1.0\n");
    printf("c decisions   %llu (%llu via API)\n",
           (unsigned long long)kernel.stats.decisions,
           (unsigned long long)kernel.stats.api_decisions);
    printf("c conflicts   %llu\n", (unsigned long long)kernel.stats.conflicts);
    printf("c propagations %llu\n",
           (unsigned long long)kernel.stats.propagations);
    printf("c learned     %llu (%llu units)\n",
           (unsigned long long)kernel.stats.learned,
           (unsigned long long)kernel.stats.learned_units);
    printf("c minimized   %llu lits\n",
           (unsigned long long)kernel.stats.minimized);
    printf("c restarts    %llu\n", (unsigned long long)kernel.stats.restarts);
    printf("c reductions  %llu (%llu clauses deleted)\n",
           (unsigned long long)kernel.stats.reductions,
           (unsigned long long)kernel.stats.deleted);
  }
  if (res == 10) {
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
