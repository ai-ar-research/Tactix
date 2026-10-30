#include "rules/lucky/affine_model.hpp"
#include "rules/lucky/model.hpp"
#include "engine/affine/projection.hpp"
#include "rules/lucky/parity_model.hpp"
#include <map>
#include <set>
#include <random>
#include <unordered_map>
namespace tactix {
namespace {
struct AffineScopeHash {
  size_t operator()(const std::vector<size_t> &scope) const noexcept {
    // Representation-only hash mixing; exact vector equality resolves collisions.
    size_t hash = scope.size();
    for (size_t variable : scope) hash = hash * 0x9e3779b1u + variable;
    return hash;
  }
};
}
RuleEligibility affine_model_guard(const Kernel &k) {
  return parity_model_guard(k);
}
ApplyResult affine_model_apply(Kernel &k) {
  const strategy::AffineModelParameters &s = k.affine_model_parameters;
  ++k.stats.affine_model_calls;
  using namespace strategy::affine;
  if (!s.valid() || !affine_model_guard(k)) return ApplyResult::NoOp;
  if (k.nvars > s.maximum_variables) {
    ++k.stats.affine_model_budget; return ApplyResult::NoOp;
  }
  uint64_t validated=0, completions=0, equations_count=0, projected=0, right_entries=0;
  std::vector<int8_t> guidance;
  const auto stop = [&] { return kernel_rule_interrupt(k, k.stats.affine_model_interrupted); };
  const uint64_t work_limit = kernel_has_work_budget(k)
      ? std::min(s.maximum_work, k.work_budget) : s.maximum_work;
  Work meter(work_limit, stop);
  const auto charge = [&](uint64_t amount) { return meter.charge(amount); };
  uint64_t import_used = 0;
  bool admission_failed = false;
  const auto reserve_import = [&](uint64_t amount) {
    if (amount > s.import_bytes - import_used) {
      admission_failed = true; return false;
    }
    import_used += amount; return true;
  };
  const auto finish = [&](const char *, ApplyResult result=ApplyResult::NoOp) {
    k.stats.affine_model_equations += equations_count;
    k.stats.affine_model_projection += projected;
    k.stats.affine_model_entries += right_entries;
    k.stats.affine_model_candidates += completions;
    k.stats.affine_model_sat += validated;
    k.stats.affine_model_work += meter.used();
    k.stats.affine_model_budget += meter.exhausted() || admission_failed;
    if (result != ApplyResult::Sat && !guidance.empty())
      kernel_offer_local_search_partial_hint(k, strategy::LocalSearchKind::Affine,
                                     guidance, UINT64_MAX, false,
                                     {meter.used(), strategy::LocalSearchEffortUnit::AffineWork});
    return result;
  };
  const auto budget_exit = [&] {
    if (!meter.interrupted()) meter.exhaust();
    return finish("search-budget");
  };
  if (!charge(k.nvars) || !reserve_import(uint64_t(k.nvars) *
          (3 * sizeof(size_t) + 5 * sizeof(uint8_t))))
    return finish("import-budget");
  std::vector<std::vector<Lit>> clauses;
  const auto append = [&](std::vector<Lit> clause) {
    if (!charge(clause.size()+1)) return false;
    if (clauses.size()>=s.maximum_constraints) { admission_failed=true; return false; }
    // Conservative logical payload reservation before recognition owns copies;
    // spare container capacity and allocator bookkeeping are excluded.
    const uint64_t payload = sizeof(std::vector<Lit>) +
        3 * sizeof(std::vector<size_t>) + 2 * sizeof(Bits) + sizeof(Equation) +
        2 * sizeof(size_t) + clause.size() * (sizeof(Lit) + 2 * sizeof(size_t)) +
        2 * ((clause.size()+63)/64) * sizeof(uint64_t);
    if (!reserve_import(payload)) return false;
    std::sort(clause.begin(),clause.end());
    clause.erase(std::unique(clause.begin(),clause.end()),clause.end());
    for(size_t i=1;i<clause.size();++i)if(clause[i]==negate(clause[i-1]))return true;
    clauses.push_back(std::move(clause));return true;
  };
  for(Ref ref=0;ref<k.arena.size();) {
    const Clause*c=clause_at(k.arena,ref);
    if(!c->garbage()&&!append(std::vector<Lit>(c->begin(),c->end())))return finish("import-budget");
    ref+=clause_actual_words(c);
  }
  for(Lit lit=0;lit<2*k.nvars;++lit) {
    if(!charge(1))return finish("import-budget");
    for(const WatchEntry w:k.watches.entries(lit))
      if(w.binary()&&lit<=w.other()&&!append({lit,w.other()}))return finish("import-budget");
  }
  for(Var v=0;v<k.nvars;++v) {
    const int value=k.value(make_lit(v,false));
    if(value&&!append({make_lit(v,value<0)}))return finish("import-budget");
  }
  using Groups = std::unordered_map<std::vector<size_t>, std::vector<size_t>,
                                    AffineScopeHash>;
  Groups groups;
  for(size_t id=0;id<clauses.size();++id) {
    const auto&c=clauses[id];
    if(!charge(c.size()+1))return finish("recognition-budget");
    if(c.empty())return finish("empty-clause");
    if(c.size()>s.maximum_arity && c.size()!=1)continue;
    std::vector<size_t> scope;
    scope.reserve(c.size());
    for (Lit literal : c) scope.push_back(var_of(literal));
    groups[std::move(scope)].push_back(id);
  }
  // Fewer than 2^(arity-1) clauses cannot contain a complete parity group.
  // This necessary count test avoids constructing sign maps for impossible
  // scopes. Duplicate clauses count here, but exact patterns below still
  // determine completeness. Sort only viable scopes to preserve the previous
  // deterministic equation order independently of hash-table iteration.
  std::vector<const Groups::value_type *> viable;
  for (const auto &group : groups) {
    if (!charge(1)) return finish("recognition-budget");
    const size_t arity = group.first.size();
    if (arity - 1 >= std::numeric_limits<size_t>::digits) continue;
    if (group.second.size() >= (size_t{1} << (arity - 1)))
      viable.push_back(&group);
  }
  std::sort(viable.begin(), viable.end(), [](const auto *a, const auto *b) {
    return a->first < b->first;
  });
  std::vector<uint8_t> covered(clauses.size(),0),present(k.nvars,0),boundary(k.nvars,0);
  std::vector<Equation> equations;
  uint64_t nonunit_equations=0;
  for (const auto *group : viable) {
    const auto &scope = group->first;
    std::map<Bits, std::vector<size_t>> patterns;
    for (size_t id : group->second) {
      const auto &clause = clauses[id];
      if (!charge(clause.size() + 1)) return finish("recognition-budget");
      Bits signs((clause.size() + 63) / 64, 0);
      for (size_t i = 0; i < clause.size(); ++i)
        if (negated(clause[i])) flip(signs, i);
      patterns[std::move(signs)].push_back(id);
    }
    const size_t expected=size_t{1}<<(scope.size()-1);
    size_t counts[2]={0,0};
    for(const auto &[signs,ids]:patterns) {
      if(!charge(signs.size()+1))return finish("recognition-budget");
      size_t parity=0;for(uint64_t w:signs)parity^=std::popcount(w)&1;
      ++counts[parity];
    }
    for(size_t parity=0;parity<2;++parity)if(counts[parity]==expected) {
      equations.push_back({scope,!bool(parity)});
      nonunit_equations += scope.size()>1;
      for(size_t v:scope)present[v]=1;
      for(const auto &[signs,ids]:patterns) {
        if(!charge(signs.size()+ids.size()))return finish("recognition-budget");
        size_t p=0;for(uint64_t w:signs)p^=std::popcount(w)&1;
        if(p==parity)for(size_t id:ids)covered[id]=1;
      }
    }
  }
  equations_count=equations.size();
  if(nonunit_equations<s.minimum_equations)return finish("no-equations");
  for(size_t id=0;id<clauses.size();++id)if(!covered[id]) {
    if(!charge(clauses[id].size()+1))return finish("boundary-budget");
    for(Lit l:clauses[id])boundary[var_of(l)]=1;
  }
  std::vector<Lit> explicit_coordinates;
  if (s.projection_mode == strategy::AffineProjectionMode::Explicit) {
    for (int external : s.projection) {
      if (!charge(1)) return finish("coordinate-budget");
      const Lit lit = kernel_internalize(k, external);
      if (lit == INVALID_LIT ||
          (kernel_var_retired(k, var_of(lit)) && !k.value(lit))) {
        ++k.stats.affine_model_unavailable;
        return finish("coordinate-unavailable");
      }
      explicit_coordinates.push_back(lit);
      present[var_of(lit)] = 1;
    }
  }
  std::vector<size_t> vars,index(k.nvars,SIZE_MAX);
  std::vector<Coordinate> coords;
  for(Var v=0;v<k.nvars;++v) {
    if (!charge(1)) return finish("coordinate-budget");
    if(!present[v]) continue;
    index[v]=vars.size();vars.push_back(v);
    if(s.projection_mode != strategy::AffineProjectionMode::Explicit &&
       (boundary[v] || s.projection_mode == strategy::AffineProjectionMode::AllParity) &&
       !k.value(make_lit(v,false)))
      coords.push_back({index[v],s.automatic_negated});
  }
  for (Lit lit : explicit_coordinates)
    coords.push_back({index[var_of(lit)],negated(lit)});
  projected=coords.size();
  for(auto&e:equations)for(size_t&v:e.variables)v=index[v];
  if(projected>s.maximum_projection) {
    // The syndrome search is exponential in the projected boundary width, but
    // Gaussian completion of one point in the recovered affine space is not.
    // Preserve that distinction: decline the configured model search while
    // still publishing a deterministic GF-consistent partial phase hint.  A
    // later public LocalSearchGuidance action decides whether to install it.
    // No SAT/UNSAT conclusion is drawn from this advisory completion.
    admission_failed=true;
    Projection hint;
    const std::vector<Coordinate> no_coordinates;
    const auto status = project(vars.size(), equations, no_coordinates,
                                s.linear_bytes, meter, hint);
    if (status == ProjectionStatus::Ready) {
      std::vector<uint8_t> values(vars.size(), uint8_t(s.free_phase));
      const Bits selected;
      uint64_t used = 0;
      if (reconstruct(hint, selected, values, meter, used)) {
        guidance.assign(k.nvars, int8_t{0});
        for (size_t i = 0; i < vars.size(); ++i)
          guidance[vars[i]] = values[i] ? int8_t{1} : int8_t{-1};
      }
    }
    return finish("projection-limit");
  }
  Projection p;
  const auto ps=project(vars.size(),equations,coords,s.linear_bytes,meter,p);
  if(ps!=ProjectionStatus::Ready) {
    admission_failed |= ps==ProjectionStatus::Budget;
    return finish("projection-unavailable");
  }
  std::vector<size_t> order(coords.size());std::iota(order.begin(),order.end(),0);
  std::mt19937_64 random(s.seed);
  std::vector<uint8_t> values(vars.size());
  std::vector<int8_t> assignment(k.nvars);
  std::vector<uint8_t> model(k.nvars);
  std::set<Bits> attempted;
  const auto accept = [&](const Bits &mask) {
    if (completions>=s.candidate_budget) { meter.exhaust(); return false; }
    if(completions>=s.candidate_budget||!charge(vars.size()+k.nvars))return false;
    Bits selected((coords.size()+63)/64,0);
    for(size_t i=0;i<coords.size();++i)if(bit(mask,i))flip(selected,order[i]);
    // Repeated scopes/permutations may produce the same full coordinate
    // assignment. Completion is deterministic in this read-only invocation;
    // remembering its failure preserves every distinct candidate.
    if (!charge(selected.size()+1)) return false;
    if (attempted.contains(selected)) return false;
    if (!reserve_import(sizeof(Bits)+selected.size()*sizeof(uint64_t))) {
      meter.exhaust(); return false;
    }
    attempted.insert(selected);
    std::fill(values.begin(),values.end(),uint8_t(s.free_phase));
    uint64_t used=0;
    const bool extended=reconstruct(p,selected,values,meter,used);
    if(!extended)return false;
    ++completions;
    std::fill(assignment.begin(),assignment.end(),int8_t{0});
    for(size_t i=0;i<vars.size();++i)assignment[vars[i]]=values[i]?1:-1;
    for(Var v=0;v<k.nvars;++v) {
      const int value=k.value(make_lit(v,false));
      if(!value)continue;
      if(assignment[v]&&assignment[v]!=value){return false;}
      assignment[v]=value;
    }
    // Private model completion: private, bounded UP only. The configured
    // phase fills unassigned coordinates once; it is accepted only if a final
    // independent whole-live-CNF scan succeeds. No decisions or learning.
    bool changed=true;
    while(changed) {
      changed=false;
      for(const auto&c:clauses) {
        if(!charge(c.size()+1))return false;
        size_t open=0;Lit unit=0;bool sat=false;
        for(Lit l:c){const int val=assignment[var_of(l)]*(negated(l)?-1:1);if(val>0){sat=true;break;}if(!val){++open;unit=l;}}
        if(sat)continue;
        if(!open)return false;
        if(open==1){assignment[var_of(unit)]=negated(unit)?-1:1;changed=true;}
      }
    }
    if (guidance.empty()) guidance=assignment;
    for(auto&value:assignment)if(!value)value=s.residual_phase?1:-1;
    for (Var v=0;v<k.nvars;++v) model[v]=assignment[v]>0;
    if (!lucky_model_valid(k, model, charge) || !charge(k.nvars)) return false;
    ++validated;
    return true;
  };
  for(uint64_t trial=0;trial<s.permutations;++trial) {
    if (!charge(1) || completions>=s.candidate_budget)
      return budget_exit();
    if(trial || s.shuffle_first) {
      // Explicit Fisher--Yates mapping, independent of std::shuffle's library
      // implementation. The standard mt19937_64 engine defines the seed stream.
      for(size_t i=order.size();i>1;--i) {
        if (!charge(1)) return finish("permutation-budget");
        std::swap(order[i-1],order[random()%i]);
      }
    }
    std::vector<Bits> columns;
    for(size_t i:order) {
      if (!charge(p.columns[i].size())) return finish("column-budget");
      columns.push_back(p.columns[i]);
    }
    for(const auto &scope : s.scopes) {
      if(!charge(1)||completions>=s.candidate_budget)return budget_exit();
      const uint64_t split = scope.left_size == strategy::AffineSearchScope::AUTO_SPLIT
                                 ? coords.size()/2 : scope.left_size;
      if (split>coords.size()) { ++k.stats.affine_model_unavailable; continue; }
      const SearchLimits limits{size_t(split),
          size_t(std::min<uint64_t>(scope.left_weight,coords.size())),
          size_t(std::min<uint64_t>(scope.right_weight,coords.size())),
          size_t(std::min<uint64_t>(scope.total_weight,coords.size())),
          s.maximum_entries,s.table_bytes};
      SearchStats stats;
      const auto status=search(columns,p.target,limits,meter,accept,stats);
      right_entries+=stats.right_entries;
      if(status==SearchStatus::Found) {
        lucky_publish_model(k,model);
        return finish("sat",ApplyResult::Sat);
      }
      if(status==SearchStatus::Budget) admission_failed=true;
      if(meter.exhausted()||meter.interrupted()) return budget_exit();
    }
  }
  return finish("exhausted");
}
}
