// No C API verdict may precede successful certificate publication.
#undef NDEBUG
#include "tactix.h"
#include <cassert>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static int rule(const char *name) {
  for (int i = 0; i < tactix_rule_count(); ++i)
    if (!std::strcmp(name, tactix_rule_name(i)))
      return i;
  _exit(101);
}

int main() {
  for (int format : {TACTIX_PROOF_DRAT_BINARY, TACTIX_PROOF_DRAT_ASCII,
                     TACTIX_PROOF_LRAT, TACTIX_PROOF_DSR_BINARY,
                     TACTIX_PROOF_DSR_ASCII, TACTIX_PROOF_AUTOMATIC})
    for (int entry = 0; entry < 5; ++entry) {
      const pid_t child = fork();
      assert(child >= 0);
      if (!child) {
        const struct rlimit no_core = {0, 0};
        if (setrlimit(RLIMIT_CORE, &no_core))
          _exit(102);
        tactix *solver = tactix_init();
        if (tactix_proof(solver, "/dev/full", format))
          _exit(103);
        if (entry < 2) {
          for (int literal : {1, 0, -1, 0})
            tactix_add(solver, literal);
        } else {
          for (int literal : {1, 2, 0, -1, 2, 0, 1, -2, 0, -1, -2, 0})
            tactix_add(solver, literal);
        }
        int result;
        if (entry == 0)
          result = tactix_solve(solver);
        else if (entry == 1)
          result = tactix_status(solver);
        else if (entry == 2) {
          tactix_online_step step{};
          step.step_id = 1;
          step.rule_id = rule("Probe");
          step.granularity = TACTIX_ONLINE_GRANULARITY_CANONICAL;
          step.repeat_mode = TACTIX_ONLINE_REPEAT_ONCE;
          step.repeat_limit = 1;
          step.work_unit = TACTIX_ONLINE_WORK_APPLICATIONS;
          step.post_application = TACTIX_ONLINE_POST_PROPAGATE;
          step.budget_unlimited = 1;
          tactix_online_program program{&step, 1, TACTIX_ONLINE_REPEAT_ONCE, 1};
          if (tactix_online_configure(solver, &program, 1,
                                      TACTIX_ONLINE_RECONFIGURE_RESET))
            _exit(104);
          tactix_online_result advanced{};
          if (tactix_online_advance(solver, &advanced))
            _exit(105);
          result = advanced.status == TACTIX_ONLINE_UNSAT ? 20 : 0;
        } else if (entry == 3) {
          result = tactix_apply(solver, rule("Probe"));
          if (result == 0)
            result = tactix_apply(solver, rule("Probe"));
        } else {
          uint64_t instance;
          if (tactix_rule_instances(solver, rule("Probe"), &instance, 1) != 1)
            _exit(106);
          result = tactix_apply_instance(solver, rule("Probe"), instance);
          if (result == 0)
            result = tactix_apply_instance(solver, rule("Probe"), instance);
        }
        _exit(result == 20 ? 20 : 1); // release must not be needed to detect failure
      }
      int status;
      assert(waitpid(child, &status, 0) == child);
      if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT)
        std::fprintf(stderr, "output failure: format=%d entry=%d status=%d\n", format, entry, status);
      assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    }
}
