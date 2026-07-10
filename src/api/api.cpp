// C API implementation. Deliberately does NOT include tactix.h: the alias
// `typedef struct tactix_solver tactix` there collides with our internal
// namespace in C++ TUs. C linkage matches by symbol name; the C compile of
// tests/test_api_c.c against tactix.h keeps the two in sync (O3 note for
// the phase-2 API v1 freeze).
#include "frontend/parse.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"

#include <vector>

using namespace tactix;

struct tactix_solver {
  Kernel kernel;
  Proof proof;
  std::vector<Lit> scratch;
};

extern "C" {

tactix_solver *tactix_init(void) { return new tactix_solver(); }

void tactix_release(tactix_solver *s) { delete s; }

void tactix_add(tactix_solver *s, int lit) {
  if (lit == 0) {
    ingest_clause(s->kernel, s->scratch);
    s->scratch.clear();
  } else {
    s->scratch.push_back(internalize(lit));
  }
}

int tactix_proof(tactix_solver *s, const char *path, int binary) {
  if (!s->proof.open(path, binary != 0))
    return -1;
  s->kernel.proof = &s->proof;
  return 0;
}

int tactix_solve(tactix_solver *s) { return default_solve(s->kernel); }

int tactix_value(tactix_solver *s, int elit) {
  const Lit l = internalize(elit);
  if (var_of(l) >= s->kernel.nvars)
    return -elit; // unmentioned variable: any value works
  const int8_t v = s->kernel.values[l];
  return v > 0 ? elit : -elit;
}

int tactix_rule_count(void) {
  return static_cast<int>(tactix_rules().size());
}

const char *tactix_rule_name(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return nullptr;
  return rules[rule_id].name;
}

int tactix_rules_applicable(tactix_solver *s, int *out, int cap) {
  const auto &rules = tactix_rules();
  int n = 0;
  for (size_t i = 0; i < rules.size() && n < cap; i++)
    if (rules[i].guard(s->kernel))
      out[n++] = static_cast<int>(i);
  return n;
}

int tactix_rule_advised(tactix_solver *s, int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return 0;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return 0;
  return (!r.advised || r.advised(s->kernel)) ? 1 : 0;
}

int tactix_apply(tactix_solver *s, int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  const ApplyResult res = r.apply(s->kernel);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  switch (res) {
  case ApplyResult::Ok:
    return 0;
  case ApplyResult::NoOp:
    return 1;
  case ApplyResult::Conflict:
    return 2;
  case ApplyResult::Sat:
    return 10;
  case ApplyResult::Unsat:
    return 20;
  }
  return 1;
}

int tactix_decide_candidates(tactix_solver *s, int *lits, int cap) {
  if (cap <= 0)
    return 0;
  std::vector<Lit> buf(static_cast<size_t>(cap));
  const uint32_t n =
      decide_candidates(s->kernel, buf.data(), static_cast<uint32_t>(cap));
  for (uint32_t i = 0; i < n; i++)
    lits[i] = externalize(buf[i]);
  return static_cast<int>(n);
}

int tactix_decide(tactix_solver *s, int elit) {
  if (!decide_guard(s->kernel))
    return -1;
  const Lit l = internalize(elit);
  if (var_of(l) >= s->kernel.nvars)
    return -1;
  const int ok = decide_apply(s->kernel, l, true) == ApplyResult::Ok ? 0 : -1;
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return ok;
}

int tactix_status(tactix_solver *s) {
  if (s->kernel.inconsistent)
    return 20;
  // SAT requires a total, fully propagated assignment with NO conflict
  // in flight: a recorded conflict (conflict_valid) or an analyzed-but-
  // not-yet-backjumped one (pending.valid) means the current assignment
  // still falsifies a clause even though the propagation head moved on
  if (s->kernel.unassigned == 0 && !s->kernel.conflict_valid &&
      !s->kernel.pending.valid &&
      s->kernel.propagate_head == s->kernel.trail.size())
    return 10;
  return 0;
}

} // extern "C"
