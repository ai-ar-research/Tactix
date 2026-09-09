// Actual C2 and slot consumers, not a mirror of their implementation.
#undef NDEBUG
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/heuristics.hpp"
#include "rules/strengthen/strengthen.hpp"
#include "rules/subsume_forward/subsume_forward.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
using namespace tactix;

static Ref large(Kernel &k, std::initializer_list<int> values, bool red=false) {
  std::vector<Lit> lits;
  for (int x : values) lits.push_back(internalize(x));
  return kernel_add_large_clause(k, lits.data(), lits.size(), red, red ? 2 : 0);
}
static void flags(const Kernel &k, std::initializer_list<unsigned> armed) {
  std::vector<uint8_t> expected(k.nvars, 0);
  for (unsigned v : armed) expected[v-1] = 1;
  assert(k.subsume_material == expected);
}

static void binary_filter() {
  for (unsigned armed=0; armed<=2; ++armed) {
    Kernel k; kernel_reserve_vars(k, 3);
    const Lit a=internalize(1), b=internalize(2);
    kernel_add_binary_clause(k,a,b); kernel_add_binary_clause(k,a,b);
    kernel_clear_subsume_material(k);
    // Shared advice alone must not admit either endpoint.
    kernel_touch_var(k,0); kernel_touch_var(k,1);
    for (unsigned v=0; v<armed; ++v) kernel_mark_subsume_material(k,v);
    const uint64_t count=k.subsume_material_count;
    assert(strengthen_apply(k)==(armed==2 ? ApplyResult::Ok : ApplyResult::NoOp));
    assert(kernel_binary_copies(k,a,b)==(armed==2 ? 1 : 2));
    flags(k,{}); assert(k.subsume_material_count==count);
    kernel_check_invariants(k);
  }
}

static void candidate_and_connection() {
  for (unsigned armed=0; armed<=3; ++armed) {
    Kernel k; kernel_reserve_vars(k,4);
    large(k,{1,2,3}); large(k,{1,2,3,4});
    kernel_clear_subsume_material(k);
    for (Var v=0; v<4; ++v) kernel_touch_var(k,v);
    for (Var v=0; v<armed; ++v) kernel_mark_subsume_material(k,v);
    const uint64_t count=k.subsume_material_count;
    assert(strengthen_apply(k)==(armed==3 ? ApplyResult::Ok : ApplyResult::NoOp));
    assert(k.stats.strengthen_checked==(armed<2 ? 0 : 2));
    assert(k.stats.strengthen_subsumed==(armed==3));
    flags(k,{}); assert(k.subsume_material_count==count);
  }
}

static void rewritten_rearm() {
  for (bool binary : {false,true}) {
    Kernel k; kernel_reserve_vars(k,5);
    if (binary) large(k,{1,2,3}); else large(k,{1,2,3,4});
    kernel_add_binary_clause(k,internalize(-1),internalize(2));
    const uint64_t count=k.subsume_material_count;
    assert(strengthen_apply(k)==ApplyResult::Ok);
    assert(k.stats.strengthened==1 && !k.strengthen_complete);
    if (binary) flags(k,{2,3}); else flags(k,{2,3,4});
    // Existing armed variables and no-increment tail rearming cannot
    // synthesize a new invitation from the round's own retained work.
    assert(k.subsume_material_count==count);
    assert(strengthen_apply(k)==ApplyResult::NoOp);
    assert(k.strengthen_complete); flags(k,{});
    assert(k.subsume_material_count==count);
    kernel_check_invariants(k);
  }
}

static void budget_tail_and_band() {
  for (bool band : {false,true}) for (bool filter : {false,true}) {
    Kernel k; kernel_reserve_vars(k,6);
    large(k,{1,2,3},band); large(k,{4,5,6},band);
    k.touch_redundant=band; k.touch_filter=filter;
    if (band) kernel_clear_subsume_material(k);
    const uint64_t count=k.subsume_material_count;
    k.midround_budget=true; k.work_budget=1;
    assert(strengthen_apply(k)==ApplyResult::NoOp);
    assert(k.stats.strengthen_checked==1 && k.stats.budget_stops_c2==1);
    assert(!k.strengthen_complete && k.subsume_material_count==count);
    if (band) flags(k,{}); else flags(k,{4,5,6});
    k.midround_budget=false; k.work_budget=0;
    assert(strengthen_apply(k)==ApplyResult::NoOp);
    assert(k.strengthen_complete && k.subsume_material_count==count);
    flags(k,{});
  }
}

static void interruption_and_other_consumer() {
  Kernel k; kernel_reserve_vars(k,3); large(k,{1,2,3});
  const auto initial=k.subsume_material;
  const uint64_t count=k.subsume_material_count;
  volatile sig_atomic_t stop=1;
  { KernelStopPublisher publish(k,&stop);
    assert(strengthen_apply(k)==ApplyResult::NoOp);
  }
  assert(!k.strengthen_complete && k.subsume_material==initial);
  assert(k.subsume_material_count==count);
  assert(subsume_forward_apply(k)==ApplyResult::NoOp);
  assert(k.subsume_material==initial && k.subsume_material_count==count);
  assert(strengthen_apply(k)==ApplyResult::NoOp);
  flags(k,{});
  // The explicit band round does not consume canonical flags, even when it
  // examines an irredundant partner using its separate touch protocol.
  kernel_mark_subsume_material(k,0);
  k.touch_redundant=true;
  assert(strengthen_apply(k)==ApplyResult::NoOp);
  flags(k,{1});
}

static void material_only_invitation() {
  Kernel k; kernel_reserve_vars(k,3);
  DefaultPolicy p; auto &s=p.inprocess(); s.pose_initialized();
  s.elim_clock.rounds=s.probe_clock.rounds=1;
  s.elim_clock.next_conflicts=s.probe_clock.next_conflicts=100;
  s.elim_clock.marks_mark=k.stats.bve_marks;
  s.elim_clock.fixed_mark=kernel_root_fixed(k);
  s.elim_clock.material_mark=k.subsume_material_count;
  s.probe_clock.reductions_mark=k.stats.reductions;
  k.stats.conflicts=100;
  assert(!s.want_phase(k,SlotGroup::Eliminate));
  kernel_clear_subsume_material(k);
  kernel_touch_var(k,0); kernel_touch_kept_var(k,1);
  kernel_rearm_subsume_material(k,0);
  assert(!s.want_phase(k,SlotGroup::Eliminate));
  kernel_add_binary_clause(k,internalize(2),internalize(3));
  assert(k.stats.bve_marks==s.elim_clock.marks_mark);
  assert(kernel_root_fixed(k)==s.elim_clock.fixed_mark);
  assert(k.subsume_material_count==s.elim_clock.material_mark+2);
  k.stats.conflicts=99; assert(!s.want_phase(k,SlotGroup::Eliminate));
  k.stats.conflicts=100; assert(s.want_phase(k,SlotGroup::Eliminate));
  assert(!s.want_phase(k,SlotGroup::Probe));
  ++k.stats.reductions;
  assert(!s.want_phase(k,SlotGroup::Probe)); // strict Probe deadline
  ++k.stats.conflicts; assert(s.want_phase(k,SlotGroup::Probe));
  // Repeated admission queries cannot consume the counter.
  assert(s.elim_clock.material_mark+2==k.subsume_material_count);
  s.elim_clock.material_mark=k.subsume_material_count;
  assert(!s.want_phase(k,SlotGroup::Eliminate));
  kernel_removed_var(k,0); assert(s.want_phase(k,SlotGroup::Eliminate));
  s.elim_clock.marks_mark=k.stats.bve_marks;
  assert(!s.want_phase(k,SlotGroup::Eliminate));
  kernel_assign_unit(k,internalize(1));
  assert(s.want_phase(k,SlotGroup::Eliminate));
}

static void no_entry_consumption() {
  for (SlotGroup group : {SlotGroup::Probe,SlotGroup::Eliminate}) {
    Kernel k; ScopedHeuristics h(k); kernel_reserve_vars(k,3);
    k.stats.conflicts=1;
    DefaultPolicy p; auto &s=p.inprocess();
    s.elim_clock.material_mark=1; s.probe_clock.material_mark=2;
    // Explicitly disabled BVE ensures no owning visit. Even the Eliminate
    // entry/exit machinery must leave both saved material limits unchanged.
    DefaultInprocessScheduler::SlotEntry entry;
    entry.rule=tactix_rule_id("BVE"); entry.group=SlotGroup::Eliminate;
    entry.enabled=false; s.entries.push_back(entry);
    assert(s.run_phase(k,group,nullptr)==0);
    assert(s.elim_clock.material_mark==1 && s.probe_clock.material_mark==2);
    assert(k.subsume_material_count==3);
  }
}

int main() {
  binary_filter(); candidate_and_connection(); rewritten_rearm();
  budget_tail_and_band(); interruption_and_other_consumer();
  material_only_invitation(); no_entry_consumption();
  std::puts("PASS actual material scheduling predicates, lifecycle and admission");
}
