// C API implementation — v1, frozen at the phase-2 review (plan §5.1,
// rev 20/21; O3 + O5 decided; post-v1 changes additive only).
// Deliberately does NOT include tactix.h: the alias `typedef struct
// tactix_solver tactix` there collides with our internal namespace in C++
// TUs. C linkage matches by symbol name; the C compile of
// tests/test_api_c.c against tactix.h keeps the two in sync.
#include "frontend/parse.hpp"
#include "policies/heuristics.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "proof/proof.hpp"
#include "rules/decide/decide.hpp"
#include "rules/registry.hpp"

#include <vector>

using namespace tactix;

struct tactix_solver {
  Kernel kernel;
  Heuristics heuristics; // the SESSION owns H (Track H · H7); Kernel holds only a
                         // non-owning pointer. Attached below; outlives every
                         // solve(), so tactix_decide before any solve has an H.
  Proof proof;
  std::vector<Lit> scratch;
  tactix_solver() { kernel.h = &heuristics; }
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

int tactix_proof(tactix_solver *s, const char *path, int format) {
  // formats per tactix.h: 0 binary DRAT, 1 ascii DRAT, 2 native LRAT
  ProofFormat f;
  switch (format) {
  case 0:
    f = ProofFormat::DratBinary;
    break;
  case 1:
    f = ProofFormat::DratAscii;
    break;
  case 2:
    f = ProofFormat::Lrat;
    break;
  default:
    return -1;
  }
  if (!s->proof.open(path, f))
    return -1;
  s->kernel.proof = &s->proof;
  return 0;
}

void tactix_set_bve_gates(tactix_solver *s, unsigned mask) {
  s->kernel.bve_gate_mask = mask;
}

// Kernel configuration setters (rev 69) — the §5.6 pose surface. Plain
// writes to CONFIG fields (kernel.hpp documents each field's soundness
// class); no scheduling state, no guard interaction.
void tactix_set_bve_bound(tactix_solver *s, unsigned bound) {
  s->kernel.bve_bound = bound;
}
void tactix_set_factor_bound(tactix_solver *s, unsigned bound) {
  s->kernel.factor_bound = bound;
}
void tactix_set_factor_composite(tactix_solver *s, int on) {
  s->kernel.factor_composite = on != 0;
}
void tactix_set_work_budget(tactix_solver *s, uint64_t budget) {
  s->kernel.work_budget = budget;
}
void tactix_set_vivify_scope(tactix_solver *s, unsigned scope) {
  s->kernel.vivify_scope = static_cast<uint8_t>(scope <= 2 ? scope : 0);
}

int tactix_solve(tactix_solver *s) {
  const int r = default_solve(s->kernel);
  if (r == 10)
    kernel_extend_model(s->kernel); // rebuild model through rho before reads
  return r;
}

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

int tactix_action_kind(int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  return static_cast<int>(rules[rule_id].kind);
}

int tactix_rules_applicable(tactix_solver *s, int *out, int cap) {
  const auto &rules = tactix_rules();
  int n = 0;
  for (size_t i = 0; i < rules.size() && n < cap; i++)
    if (rules[i].guard(s->kernel))
      out[n++] = static_cast<int>(i);
  return n;
}

static int result_code(ApplyResult res) {
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

int tactix_apply(tactix_solver *s, int rule_id) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  const ApplyResult res = r.apply(s->kernel);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(res);
}

int tactix_rule_instances(tactix_solver *s, int rule_id, uint64_t *out,
                          int cap) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()) || cap < 0)
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  if (!r.instances) { // no per-application choice: canonical only
    if (cap >= 1)
      out[0] = INSTANCE_CANONICAL;
    return cap >= 1 ? 1 : 0;
  }
  return static_cast<int>(
      r.instances(s->kernel, out, static_cast<uint32_t>(cap)));
}

int tactix_apply_instance(tactix_solver *s, int rule_id, uint64_t instance) {
  const auto &rules = tactix_rules();
  if (rule_id < 0 || rule_id >= static_cast<int>(rules.size()))
    return -1;
  const RuleVTable &r = rules[rule_id];
  if (!r.guard(s->kernel))
    return -1;
  ApplyResult res;
  if (instance == INSTANCE_CANONICAL || !r.apply_instance)
    res = r.apply(s->kernel); // canonical (the only instance if no slot)
  else
    res = r.apply_instance(s->kernel, instance);
  kernel_check_invariants(s->kernel); // no-op in NDEBUG
  return result_code(res);
}

uint64_t tactix_stat(tactix_solver *s, int stat_id) {
  const Kernel &k = s->kernel;
  switch (stat_id) {
  case 0: // TACTIX_STAT_CONFLICTS
    return k.stats.conflicts;
  case 1: // TACTIX_STAT_DECISIONS
    return k.stats.decisions;
  case 2: // TACTIX_STAT_PROPAGATIONS
    return k.stats.propagations;
  case 3: // TACTIX_STAT_TICKS
    return k.stats.ticks;
  case 4: // TACTIX_STAT_LEARNED
    return k.stats.learned;
  case 5: // TACTIX_STAT_RESTARTS
    return k.stats.restarts;
  case 6: // TACTIX_STAT_REDUCTIONS
    return k.stats.reductions;
  case 7: // TACTIX_STAT_SWITCHES
    return k.stats.switched;
  case 8: // TACTIX_STAT_LEVEL
    return k.level;
  case 9: // TACTIX_STAT_UNASSIGNED
    return k.unassigned;
  case 10: // TACTIX_STAT_LAST_LEARNED_GLUE (pending.glue persists
           // after Backjump consumes the pending clause)
    return k.pending.glue;
  case 11: // TACTIX_STAT_STABLE (0 for an unguided solver with no H, H5)
    return k.h && k.h->stable ? 1 : 0;
  case 12: // TACTIX_STAT_REPHASED
    return k.stats.rephased;
  case 13: // TACTIX_STAT_SUBSUMPTIONS
    return k.stats.forward_subsumptions;
  case 14: // TACTIX_STAT_IRREDUNDANT_CLAUSES
    return kernel_irredundant_clauses(k);
  case 15: // TACTIX_STAT_ROOT_FIXED
    return kernel_root_fixed(k);
  case 16: // TACTIX_STAT_BINARY_CLAUSES
    return k.binary_ids.size();
  case 17: // TACTIX_STAT_STRENGTHENED
    return k.stats.strengthened;
  case 18: // TACTIX_STAT_VIVIFIED
    return k.stats.vivified;
  case 19: // TACTIX_STAT_PROBE_FAILED
    return k.stats.probe_failed;
  case 20: // TACTIX_STAT_BACKBONE_FAILED
    return k.stats.backbone_failed;
  case 21: // TACTIX_STAT_TRANSITIVE_DELETED
    return k.stats.transitive_deleted;
  case 22: // TACTIX_STAT_FACTORINGS
    return k.stats.factorings;
  case 23: // TACTIX_STAT_FACTOR_TICKS
    return k.stats.factor_ticks;
  case 24: // TACTIX_STAT_BVE_ELIMINATED
    return k.stats.bve_eliminated;
  case 25: // TACTIX_STAT_BVE_RESOLUTIONS
    return k.stats.bve_resolutions;
  default:
    return UINT64_MAX;
  }
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
