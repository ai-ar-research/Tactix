#undef NDEBUG
#include "affine_formula_support.hpp"
#include "dimacs_text.hpp"
#include "test_io.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/semantic_extension/affine_image.hpp"
#include "frontend/parse.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <fstream>
#include <cstdio>
using namespace tactix;

namespace tactix {
struct DefaultAffineSearchTestAccess {
  static AffineMatrixInitResult refresh(DefaultPolicy &policy, Kernel &kernel,
                                        bool initial, bool recognize) {
    return policy.initialize_affine_search(kernel, !initial, recognize);
  }
  static AffineMatrixServiceResult service(DefaultPolicy &policy, Kernel &kernel) {
    return policy.service_affine_search(kernel, !kernel.level);
  }
  static bool discharge(DefaultPolicy &policy, Kernel &kernel) {
    return policy.discharge_affine_reason(kernel);
  }
  static bool paused(const DefaultPolicy &policy) { return policy.affine_usefulness_paused_; }
  static uint64_t calls(const DefaultPolicy &policy) { return policy.affine_usefulness_calls_; }
  static bool additional_recognition_due(const DefaultPolicy &policy,
                                         const Kernel &kernel) {
    return policy.affine_additional_recognition_due(kernel);
  }
};
}

// A parsed kernel with an attached matrix session and journal. open binds a
// proof at path over the parsed input: shadow arms an empty population,
// otherwise the variable maps are bound. The path is removed afterwards
// unless keep.
struct PolicyFixture {
  Kernel kernel;
  AffineMatrixSearchSession session;
  FILE *records = temporary_records();
  Proof proof;
  std::string path;
  bool keep = false;
  explicit PolicyFixture(const std::string &text) {
    parse_dimacs_text(text, kernel);
    kernel.affine_matrix_search_session = &session;
    kernel.structural_provenance = records;
  }
  ~PolicyFixture() {
    kernel.proof = nullptr;
    kernel.structural_provenance = nullptr;
    std::fclose(records);
    if (!path.empty() && !keep) std::remove(path.c_str());
  }
  Proof &open(std::string proof_path, bool shadow,
              ProofFormat format = ProofFormat::DsrAscii, bool keep_path = false) {
    path = std::move(proof_path);
    keep = keep_path;
    assert(proof.open(path.c_str(), format));
    if (shadow) {
      proof.arm_population_shadow();
    } else {
      proof.bind_variable_map(&kernel.internal_to_external);
      proof.bind_external_variable_map(&kernel.external_to_internal);
    }
    proof.finish_input_transaction();
    kernel.proof = &proof;
    return proof;
  }
};

// Add one irredundant input clause and seed it into proof under its id.
void add_seeded(Kernel &kernel, Proof &proof, const std::vector<int32_t> &clause) {
  std::vector<Lit> body;
  for (int32_t literal : clause) body.push_back(kernel_internalize(kernel, literal));
  const auto id = ++kernel.next_clause_id;
  kernel_add_large_clause(kernel, body.data(), body.size(), false, 0, id);
  proof.seed_population(id, body.data(), body.size());
}

// A completed empty census survives root-only retries; a newly admitted source
// must invalidate it. This exercises the same Default scheduling entrypoint,
// without making recognition work depend on a machine-speed threshold.
void empty_census_retry() {
  PolicyFixture fixture("p cnf 6 2\n1 2 3 0\n4 5 6 0\n");
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  // These fixtures seed input after this point, which production never does.
  // They never convert, so the PBP input count that would go wrong is never read.
  Proof &proof = fixture.open("/tmp/tactix-default-empty-census.dsr", false);
  DefaultStrategyConfig settings;
  settings.affine_search.enabled = true;
  DefaultPolicy policy(settings);
  auto first = DefaultAffineSearchTestAccess::refresh(policy, kernel, true, true);
  assert(first.status == AffineMatrixInitStatus::NoStructure);
  assert(!session.enabled());
  const auto work = policy.affine_search_diagnostics().initialization_work;
  assert(work > 0);
  for (unsigned retry = 0; retry < 8; ++retry) {
    DefaultAffineSearchTestAccess::refresh(policy, kernel, false, false);
    const auto unchanged = DefaultAffineSearchTestAccess::refresh(policy, kernel, false, true);
    assert(unchanged.unchanged && unchanged.status == AffineMatrixInitStatus::NoStructure);
  }
  assert(policy.affine_search_diagnostics().initialization_work == work);
  assert(policy.affine_search_diagnostics().refreshes == 0);
  assert(policy.affine_search_diagnostics().empty_maintenance_skips == 8);
  assert(policy.affine_search_diagnostics().unchanged_census_skips == 8);
  for (const auto &clause : affine_semantic_extension::clausal_form({4,5,6}, false))
    add_seeded(kernel, proof, clause);
  // Changed sources alone do not earn another recognition dose.
  DefaultAffineSearchTestAccess::refresh(policy, kernel, false, false);
  assert(!session.enabled());
  assert(policy.affine_search_diagnostics().initialization_work == work);
  const auto admitted = DefaultAffineSearchTestAccess::refresh(policy, kernel, false, true);
  assert(admitted.status == AffineMatrixInitStatus::Applied);
  assert(session.enabled() && session.active_sources() == 1);
  assert(session.persistence_limits().maximum_deferred_deletions > 0);
  assert(session.persistence_limits().maximum_deferred_literal_work > 0);
  assert(policy.affine_search_diagnostics().initialization_work > work);
  session.disable(kernel);
}

struct Fixture {
  uint32_t variables;
  std::vector<std::vector<int32_t>> clauses;
};
Fixture mixed(uint32_t pigeons,uint32_t holes) {
  Fixture f{pigeons*holes,{}};
  auto cell=[&](uint32_t p,uint32_t h){return int32_t(1+p*holes+h);};
  for(uint32_t p=0;p<pigeons;++p) {
    std::vector<int32_t> cover;
    for(uint32_t h=0;h<holes;++h)cover.push_back(cell(p,h));
    f.clauses.push_back(cover);
  }
  for(uint32_t h=0;h<holes;++h)for(uint32_t p=0;p<pigeons;++p)
    for(uint32_t q=p+1;q<pigeons;++q)f.clauses.push_back({-cell(p,h),-cell(q,h)});
  // Shared original variables, no fresh-variable constructor: elimination of
  // 7/13 derives x1 xor x19 xor x20 = 1. None of its support is root-fixed,
  // so restart-root theory must not manufacture an assumption-dependent reason.
  for(const auto &variables:{std::vector<int32_t>{1,7,13},std::vector<int32_t>{7,13,19,20}})
    for(auto &clause:affine_semantic_extension::clausal_form(variables,variables.front()==7))f.clauses.push_back(clause);
  f.clauses.push_back({-1,19}); // One-sided CNF link, not another XOR equation.
  // Delayed forced gates defeat the uniform forward/backward Lucky probes.
  // The gadget is satisfiable, has no root unit, and requires ordinary search
  // to justify its gates; both ends precede the auxiliaries in one probe order.
  const int32_t n=f.variables;
  f.variables+=6;
  for (int32_t gate : {n+1,-(n+2),n+5,-(n+6)})
    for (int32_t a : {n+3,-(n+3)}) for (int32_t b : {n+4,-(n+4)})
      f.clauses.push_back({gate,a,b});
  return f;
}
std::string cnf(const Fixture &f) { return dimacs_text(f.variables, f.clauses); }

// x1+x3+x4 = 0 and x2+x3+x4+x5 = 0 over six variables, as clauses.
Fixture two_equations() {
  return {6, xor_cnf({{{1, 3, 4}, false}, {{2, 3, 4, 5}, false}})};
}
DefaultStrategyConfig config(bool enabled) {
  DefaultStrategyConfig c;c.affine_search.enabled=enabled;
  c.affine_root_closure.enabled=false;
  c.affine_model.enabled=false;c.parity_model.enabled=false;c.onehot_model.enabled=false;
  c.ramsey_startup.enabled=false;c.signed_cardinality_proof.enabled=false;
  c.balanced_signature_model.enabled=false;
  c.exact_cover_model.enabled=false;c.independent_set_model.enabled=false;
  c.fermat_model.enabled=false;c.cerny_model.enabled=false;
  c.lookup_circuit_model.enabled=false;c.simon_model.enabled=false;c.stconn_proof.enabled=false;
  c.inprocess.program.clear();c.inprocess.append_unlisted_rules=false;
  c.inprocess.structured_root_stages.clear();
  c.inprocess.structured_root_bounded_backtracking_stages.clear();
  c.inprocess.structured_root_large_domain_stages.clear();
  c.inprocess.initial_fast_elimination=false;
  c.rephase.walk_warmup=false;c.rephase.late_lucky=false;
  c.mode.force=1;c.branching.focused_phase_switch_span=1;
  c.branching.focused_phase_pattern={-1}; // General fixed phase arm, no decision script.
  return c;
}

// Pin DefaultPolicy's public deferred-reason boundary independently of the
// lower-level affine service test.  The original CNF used by the external
// checker includes the two decisions and the contradictory consequence.
void deferred_policy_boundary(const std::string &stem = "") {
  const Fixture f = two_equations();
  PolicyFixture fixture(cnf(f));
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  // Reserve the original-CNF identities for -1, -2, and 5 before emitting a
  // derived certificate.  They are checker inputs, not solver root facts.
  kernel.next_clause_id += 3;
  Proof &proof = fixture.open(stem.empty() ? "/tmp/tactix-default-deferred-policy.dsr"
                                           : stem + ".dsr",
                              true, ProofFormat::DsrAscii, !stem.empty());
  auto settings = config(true);
  settings.affine_search.reason_mode =
      DefaultStrategyConfig::AffineSearch::ReasonMode::Deferred;
  DefaultPolicy policy(settings);
  const auto initialized =
      DefaultAffineSearchTestAccess::refresh(policy, kernel, true, true);
  assert(initialized.status == AffineMatrixInitStatus::Applied);
  decide_all(kernel, {-1, -2});
  const auto deferred = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(deferred.status == AffineMatrixServiceStatus::Applied &&
         deferred.reason_deferred && session.has_deferred_reason());
  const auto services = policy.affine_search_diagnostics().search_services;
  const auto forbidden = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(forbidden.status == AffineMatrixServiceStatus::Disabled &&
         session.has_deferred_reason() &&
         policy.affine_search_diagnostics().search_services == services);
  assert(propagate_apply(kernel) == ApplyResult::Ok &&
         kernel_external_value(kernel, internalize(-5)) > 0);
  assert(DefaultAffineSearchTestAccess::discharge(policy, kernel) &&
         !session.has_deferred_reason());
  const auto &stats = policy.affine_search_diagnostics();
  assert(stats.reasons_deferred == 1 && stats.reasons_materialized == 1 &&
         stats.reasons_published == 1 && stats.implications_consumed == 1 &&
         stats.materialization_attempts == 1 &&
         stats.materialization_retained_authentications == 1 &&
         !stats.materialization_inapplicable && !stats.materialization_stale &&
         !stats.materialization_invalid && !stats.materialization_exhausted &&
         !stats.materialization_interrupted &&
         !stats.materialization_unsupported_proof);
  assert(proof.add_external_rup(++kernel.next_clause_id, nullptr, 0));
  proof.flush();
  kernel.proof = nullptr;
  if (!stem.empty()) {
    auto original = f.clauses;
    for (int32_t unit : {-1, -2, 5}) original.push_back({unit});
    write_file((stem + ".cnf").c_str(), dimacs_text(6, original));
  }
  session.disable(kernel);
}

// The detached arms travel through the same DefaultPolicy public action port
// as the shipped eager arm: the implication reason is proof visible and
// analyzable, but absent from ordinary watched propagation.
void detached_policy_boundary(bool minimal) {
  PolicyFixture fixture(cnf(two_equations()));
  Kernel &kernel = fixture.kernel;
  Proof &proof = fixture.open(minimal ? "/tmp/tactix-default-detached-minimal-policy.dsr"
                                      : "/tmp/tactix-default-detached-source-policy.dsr",
                              true);
  auto settings = config(true);
  settings.affine_search.reason_mode = minimal
      ? DefaultStrategyConfig::AffineSearch::ReasonMode::DetachedMinimal
      : DefaultStrategyConfig::AffineSearch::ReasonMode::DetachedSource;
  DefaultPolicy policy(settings);
  assert(DefaultAffineSearchTestAccess::refresh(policy, kernel, true, true)
             .status == AffineMatrixInitStatus::Applied);
  decide_all(kernel, {-1, -2});
  const auto service = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.live.detached_reason && service.reason_width == 3);
  assert(policy.affine_search_diagnostics().reasons_detached == 1);
  const Lit implied = kernel_internalize(kernel, -5);
  assert(kernel.values[implied] > 0);
  const Ref ref = kernel.assigned[var_of(implied)].reason;
  const Clause *reason = clause_at(kernel.arena, ref);
  assert(reason->garbage() && reason->reason() && reason->detached_reason());
  for (Lit lit = 0; lit < 2 * kernel.nvars; ++lit)
    for (const WatchEntry watch : kernel.watches.entries(lit))
      assert(watch.binary() || watch.ref() != ref);

  kernel_backtrack(kernel, 0);
  assert(!clause_at(kernel.arena, ref)->reason());
  fixture.session.disable(kernel);
  proof.flush();
}

void lazy_policy_boundary(bool minimal) {
  PolicyFixture fixture(cnf(two_equations()));
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  Proof &proof = fixture.open(minimal ? "/tmp/tactix-default-lazy-minimal-policy.dsr"
                                      : "/tmp/tactix-default-lazy-source-policy.dsr",
                              true);
  auto settings = config(true);
  settings.affine_search.reason_mode =
      minimal ? DefaultStrategyConfig::AffineSearch::ReasonMode::LazyMinimal
              : DefaultStrategyConfig::AffineSearch::ReasonMode::LazySource;
  DefaultPolicy policy(settings);
  assert(DefaultAffineSearchTestAccess::refresh(policy, kernel, true, true)
             .status == AffineMatrixInitStatus::Applied);
  decide_all(kernel, {-1, -2});
  const uint64_t before = kernel.next_clause_id;
  const size_t arena_before = kernel.arena.size();
  const auto service = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(service.status == AffineMatrixServiceStatus::Applied &&
         service.reason_deferred && !service.live.detached_reason &&
         session.lazy_reason_count() == 1 && kernel.next_clause_id == before &&
         service.reason_width == 3 && kernel.arena.size() == arena_before);
  const Lit implied = kernel_internalize(kernel, -5);
  assert(kernel.assigned[var_of(implied)].affine &&
         kernel.assigned[var_of(implied)].reason == AFFINE_REASON);
  const auto materialized = affine_matrix_materialize_reason(kernel, session);
  assert(materialized.reason_materialized &&
         session.lazy_reason_count() == 0 &&
         !kernel.assigned[var_of(implied)].affine &&
         kernel.assigned[var_of(implied)].reason != AFFINE_REASON);
  proof.flush();
}

void run(const Fixture &f,bool with_proof,bool enabled,const std::string &stem="",ProofFormat format=ProofFormat::DsrAscii) {
  const auto text=cnf(f);
  PolicyFixture fixture(text);
  Kernel &kernel=fixture.kernel;
  AffineMatrixSearchSession &session=fixture.session;
  if(with_proof)
    fixture.open(stem.empty()?"/tmp/tactix-default-affine-fixed.dsr":stem+".dsr",false,format,!stem.empty());
  DefaultPolicy policy(config(enabled));int status=policy.solve(kernel);
  const auto &stats=policy.affine_search_diagnostics();
  if(f.variables==26) {
    assert(status==10);kernel_extend_model(kernel);
    std::ofstream model;if(!stem.empty())model.open(stem+".model");
    for(auto &c:f.clauses) {
      bool satisfied=false;
      for(int32_t x:c)if(kernel.model_values[internalize(x)]>0)satisfied=true;
      assert(satisfied);
    }
    if(model){model<<"s SATISFIABLE\nv ";for(uint32_t v=1;v<=f.variables;++v)model<<(kernel.model_values[internalize(v)]>0?int(v):-int(v))<<' ';model<<"0\n";}
  } else assert(status==20);
  if(enabled && with_proof) {
    assert(stats.initializations==1 && stats.admitted_sources==2);
    // This small search never earns another recognition quantum. Root
    // maintenance must not count the same two source equations as new census;
    // it runs only in natural full-restart windows after a support root fact.
    // The retained matrix is also serviced at propagated search fixpoints.
    assert(stats.recognized_equations==2 && stats.refreshes==stats.restart_runs);
    assert(kernel.stats.decisions>0 && stats.root_services>0 && stats.search_services>0);
    // These fixtures authenticate retained equations, but have no initial
    // root-relevant consequence. CDCL may finish before any later root fact
    // makes one useful. If a restart does publish, it remains proof-backed;
    // paused_owner_retry below supplies the deterministic nonvacuous case.
    if(stats.reasons_published) {
      assert(stats.widest_reason>=2);
      assert(stats.proof_additions>0);
    }
    assert(stats.initialization_work>0);
  }
  if(!with_proof)assert(!session.persistent() && stats.reasons_published==0);
  if(!enabled)assert(stats.initializations==0 && stats.search_services==0);
  if(!stem.empty())write_file((stem+".cnf").c_str(),text);
  session.disable(kernel);
}
// An explicit short sampling profile isolates the scheduling protocol; the
// ordinary reference-derived thresholds remain unchanged in production.
void paused_owner_retry(const std::string &stem = "",
                        ProofFormat format = ProofFormat::DsrAscii) {
  Fixture f{9, {{5,6,7}, {8,9,1}}};
  for (auto &clause : xor_cnf({{{1, 2, 3}, false}, {{2, 3, 4}, false}}))
    f.clauses.push_back(std::move(clause));
  PolicyFixture fixture(cnf(f));
  Kernel &kernel = fixture.kernel;
  AffineMatrixSearchSession &session = fixture.session;
  Proof &proof = fixture.open(stem.empty() ? "/tmp/tactix-default-paused-owner.dsr"
                                           : stem + ".dsr",
                              false, format, !stem.empty());
  auto settings = config(true);
  settings.affine_search.autodisable_minimum_calls = 1;
  settings.affine_search.autodisable_check_every = 1;
  settings.affine_search.minimum_usefulness = 1;
  DefaultPolicy policy(settings);
  auto first = DefaultAffineSearchTestAccess::refresh(policy, kernel, true, true);
  assert(first.status == AffineMatrixInitStatus::Applied);
  assert(policy.affine_search_diagnostics().retained_basis_rank == 2);
  assert(policy.affine_search_diagnostics().retained_support_variables == 4);
  assert(policy.affine_search_diagnostics().recognition_rank_gains == 2);
  assert(policy.affine_search_diagnostics().rank_before_last_action == 0);
  assert(policy.affine_search_diagnostics().rank_after_last_action == 2);
  kernel.stats.search_ticks = UINT64_MAX;
  assert(!DefaultAffineSearchTestAccess::additional_recognition_due(policy,
                                                                    kernel));
  auto pause = [&] {
    for (int32_t literal : {2,-2}) {
      kernel_assign_decision(kernel, kernel_internalize(kernel, literal));
      assert(propagate_apply(kernel) == ApplyResult::Ok);
      const auto owner_before = session.owner_version();
      const auto trail_before = std::vector<Lit>(kernel.trail.begin(),kernel.trail.end());
      const auto searched = DefaultAffineSearchTestAccess::service(policy, kernel);
      // The retained matrix now has a legal above-root search step. This
      // fixture leaves two row variables unassigned, so it has no implication
      // yet and must preserve both the owner and the Boolean trail.
      assert(searched.status == AffineMatrixServiceStatus::NoEvent &&
             session.owner_version() == owner_before && kernel.trail == trail_before);
      kernel_backtrack(kernel, 0);
      assert(propagate_apply(kernel) == ApplyResult::Ok);
      const auto result = DefaultAffineSearchTestAccess::service(policy, kernel);
      assert(result.status == AffineMatrixServiceStatus::NoEvent);
    }
    assert(DefaultAffineSearchTestAccess::paused(policy));
    assert(session.enabled() && session.persistent());
    assert(policy.affine_search_diagnostics().paused_basis_rank == session.basis_rank());
    assert(policy.affine_search_diagnostics().paused_support_variables == session.support_variables());
  };
  pause();
  const auto sampled = DefaultAffineSearchTestAccess::calls(policy);
  const auto owner = session.owner_version();
  const auto limits = session.persistence_limits();
  // Change the physical CNF owner and force a paid recognition pass, without
  // adding a complete XOR family or a root unit. Snapshot change alone must
  // not be treated as fresh evidence of search usefulness.
  add_seeded(kernel, proof, {5, 8, 9});
  f.clauses.push_back({5, 8, 9});
  for (unsigned retry = 0; retry < 4; ++retry) {
    DefaultAffineSearchTestAccess::refresh(policy, kernel, false, false);
    const auto duplicate = DefaultAffineSearchTestAccess::refresh(policy, kernel, false, true);
    assert(duplicate.status == AffineMatrixInitStatus::NoStructure && duplicate.unchanged);
    if (!retry) assert(duplicate.recognition_work > 0);
    assert(DefaultAffineSearchTestAccess::paused(policy));
    assert(DefaultAffineSearchTestAccess::calls(policy) == sampled);
    assert(session.owner_version() == owner);
    assert(policy.affine_search_diagnostics().retained_basis_rank == 2);
    assert(policy.affine_search_diagnostics().recognition_rank_gains == 2);
    assert(policy.affine_search_diagnostics().recognition_rank_losses == 0);
    assert(policy.affine_search_diagnostics().maintenance_rank_losses == 0);
    assert(policy.affine_search_diagnostics().native_session_disables == 0);
    assert(session.persistence_limits().maximum_deferred_deletions == limits.maximum_deferred_deletions);
    assert(session.persistence_limits().maximum_deferred_literal_work == limits.maximum_deferred_literal_work);
    const auto root = DefaultAffineSearchTestAccess::service(policy, kernel);
    assert(root.status == AffineMatrixServiceStatus::NoEvent);
    assert(DefaultAffineSearchTestAccess::paused(policy));
  }
  kernel_assign_decision(kernel, kernel_internalize(kernel, 2));
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  const auto services = policy.affine_search_diagnostics().search_services;
  const auto paused_skips = policy.affine_search_diagnostics().paused_search_skips;
  const auto searched = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(searched.status == AffineMatrixServiceStatus::NoEvent);
  // The direct test accessor bypasses the policy loop's pause gate; it still
  // records a charged no-event service, whereas the loop skips this call.
  assert(policy.affine_search_diagnostics().search_services == services + 1);
  assert(policy.affine_search_diagnostics().paused_search_skips == paused_skips);
  kernel_backtrack(kernel, 0);
  for (const auto &clause : affine_semantic_extension::clausal_form({5,6,7}, false)) {
    add_seeded(kernel, proof, clause);
    f.clauses.push_back(clause);
  }
  // Search ticks and a changed generic clause population do not prove that a
  // new eligible XOR source exists. Default therefore keeps maintaining the
  // retained owner; an explicit paid recognition refresh admits this extension.
  assert(!DefaultAffineSearchTestAccess::additional_recognition_due(policy,
                                                                    kernel));
  const auto changed = DefaultAffineSearchTestAccess::refresh(policy, kernel, false, true);
  assert(changed.status == AffineMatrixInitStatus::Applied);
  assert(session.active_sources() == 3 && session.owner_version() != owner);
  assert(policy.affine_search_diagnostics().retained_basis_rank == 3);
  assert(policy.affine_search_diagnostics().retained_support_variables == 7);
  assert(policy.affine_search_diagnostics().recognition_rank_gains == 3);
  assert(!DefaultAffineSearchTestAccess::paused(policy));
  assert(DefaultAffineSearchTestAccess::calls(policy) == 0);
  assert(policy.affine_search_diagnostics().usefulness_resumptions == 1);
  pause();
  // Extend this evolving fixed input with a root fact. The two authenticated
  // XORs imply x1=x4, although ordinary Boolean propagation alone cannot discover it here.
  const Lit unit = kernel_internalize(kernel, 1);
  proof.seed_population(++kernel.next_clause_id, &unit, 1);
  f.clauses.push_back({1});
  kernel_assign_unit(kernel, unit);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.values[kernel_internalize(kernel, 4)] == 0);
  const auto root = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(root.status == AffineMatrixServiceStatus::Applied && root.bcp_pending);
  assert(!DefaultAffineSearchTestAccess::paused(policy));
  assert(policy.affine_search_diagnostics().usefulness_resumptions == 2);
  assert(propagate_apply(kernel) == ApplyResult::Ok);
  assert(kernel.values[kernel_internalize(kernel, 4)] > 0);
  // A transient native guard decline at a restart root retains the bounded,
  // authenticated owner and its proof leases for a later root window.
  const auto rank_before_decline = session.basis_rank();
  const auto support_before_decline = session.support_variables();
  const auto sources_before_decline = session.active_sources();
  const auto losses_before_decline =
      policy.affine_search_diagnostics().service_rank_losses;
  const auto disables_before_decline =
      policy.affine_search_diagnostics().native_session_disables;
  const auto reasons_before_decline =
      policy.affine_search_diagnostics().reasons_published;
  const auto inapplicable_before_decline =
      policy.affine_search_diagnostics().service_inapplicable;
  const auto owner_before_decline = session.owner_version();
  const auto premise_bytes_before_decline = proof.premise_bytes();
  kernel.large_watches_connected = false;
  const auto guard_declined = DefaultAffineSearchTestAccess::service(policy, kernel);
  kernel.large_watches_connected = true;
  assert(guard_declined.status == AffineMatrixServiceStatus::Inapplicable);
  const auto &survival = policy.affine_search_diagnostics();
  assert(session.enabled() && survival.active_sources == sources_before_decline);
  assert(survival.retained_basis_rank == rank_before_decline &&
         survival.retained_support_variables == support_before_decline);
  assert(survival.rank_before_last_action == rank_before_decline &&
         survival.rank_after_last_action == rank_before_decline);
  assert(survival.service_rank_losses == losses_before_decline);
  assert(survival.native_session_disables == disables_before_decline &&
         survival.optional_failure_disables == 0);
  assert(survival.service_inapplicable == inapplicable_before_decline + 1);
  assert(survival.last_service_status ==
         static_cast<uint64_t>(AffineMatrixServiceStatus::Inapplicable));
  assert(!survival.last_terminal_status &&
         !survival.last_terminal_audit_stage);
  assert(survival.last_survival_action == 3 && survival.last_disable_cause == 0);
  assert(survival.isolated_projection_rank_losses == 0);
  // The next valid root window resumes the same owner.  With no new root fact,
  // it neither reacquires premises nor republishes the already consumed reason.
  const auto resumed = DefaultAffineSearchTestAccess::service(policy, kernel);
  assert(resumed.status == AffineMatrixServiceStatus::NoEvent);
  assert(session.enabled() && session.owner_version() == owner_before_decline);
  assert(session.active_sources() == sources_before_decline);
  assert(proof.premise_bytes() == premise_bytes_before_decline);
  assert(policy.affine_search_diagnostics().reasons_published ==
         reasons_before_decline);
  session.disable(kernel);
  if (!stem.empty()) {
    // The exact union of the staged input premises: every proof prefix is
    // checked against this satisfiable input.
    write_file((stem + ".cnf").c_str(), cnf(f));
    write_file((stem + ".model").c_str(), "s SATISFIABLE\nv 1 -2 3 4 5 6 -7 -8 -9 0\n");
  }
}

int main(int argc,char **argv) {
  assert(argc==1 || argc==2);
  empty_census_retry();
  detached_policy_boundary(true);
  detached_policy_boundary(false);
  lazy_policy_boundary(false);
  lazy_policy_boundary(true);
  deferred_policy_boundary(argc == 2
      ? std::string(argv[1]) + "/deferred-policy" : "");
  for (const auto &[format, suffix] : {
      std::pair{ProofFormat::DsrAscii, "/paused"},
      std::pair{ProofFormat::DsrBinary, "/paused-dsr-binary"}})
    paused_owner_retry(argc == 2 ? std::string(argv[1]) + suffix : "", format);
  Kernel k;kernel_reserve_vars(k,30);auto c=config(true);
  auto p=default_affine_search_parameters(k,c);assert(p && p->valid());
  assert(p->maximum_arity>3 && p->basis_maximum_intermediate_width==30);
  assert(p->maximum_session_work == c.affine_search.maximum_retained_bytes);
  c.affine_search.maximum_retained_bytes=1;assert(!default_affine_search_parameters(k,c));
  c=config(true);c.affine_root_closure.maximum_proof_deletions=0;
  assert(!default_affine_search_parameters(k,c));
  for(bool sat:{true,false}) {
    auto f=mixed(sat?4:6,5);std::string stem=argc==2?std::string(argv[1])+(sat?"/sat":"/unsat"):"";
    run(f,true,true,stem);
    run(f,false,true);
    run(f,false,false);
  }
}
