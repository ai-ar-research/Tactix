#undef NDEBUG
#include "policies/default_policy.hpp"
#include "kernel/kernel.hpp"
#include "rules/propagate/propagate.hpp"
#include <cassert>
#include <limits>
using namespace tactix;
namespace tactix {
struct DefaultAffineSearchTestAccess {
 static bool due(DefaultPolicy &p,const Kernel &k,bool root=false){return p.affine_service_due(k,root);}
 static void completed(DefaultPolicy &p,const Kernel &k,const AffineMatrixServiceResult &r){p.schedule_affine_service(k,r);}
 static void track_unit(DefaultPolicy &p,uint64_t id){p.affine_reason_ids_={id};}
 static void consume(DefaultPolicy &p,const Kernel &k,size_t begin){p.observe_affine_consumption(k,begin);}
 static void old_owner_stamp(DefaultPolicy &p){p.affine_service_wake_stamp_[0]=UINT64_MAX;}
};
}
static DefaultStrategyConfig config(uint64_t quantum){DefaultStrategyConfig c;c.affine_search.enabled=true;c.affine_search.maximum_service_work=quantum;c.inprocess.minimum_effort_ticks=13;return c;}
int main(){
 // Shared ladder is exactly the existing saturating-grow/halve behavior.
 for(uint32_t start:{0u,1u,2u,UINT32_MAX-1,UINT32_MAX}){
  uint32_t current=start,count=0;update_inprocess_delay(current,count,true);
  assert(current==start+(start<UINT32_MAX)&&count==current);
  update_inprocess_delay(current,count,false);assert(current==(start+(start<UINT32_MAX))/2&&count==current);
 }
 Kernel k;kernel_reserve_vars(k,3);DefaultPolicy p(config(10));
 assert(DefaultAffineSearchTestAccess::due(p,k));
 AffineMatrixServiceResult none;none.status=AffineMatrixServiceStatus::NoEvent;
 DefaultAffineSearchTestAccess::completed(p,k,none);
 assert(p.affine_search_diagnostics().service_due_ticks==20&&p.affine_search_diagnostics().service_delay_current==1);
 k.stats.search_ticks=19;assert(!DefaultAffineSearchTestAccess::due(p,k));
 assert(!DefaultAffineSearchTestAccess::due(p,k,true)); // A restart alone does not bypass the tick schedule.
 k.stats.search_ticks=20;assert(DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);
 assert(p.affine_search_diagnostics().service_due_ticks==50&&p.affine_search_diagnostics().service_delay_current==2);
 // Pending work is not completed no-yield and gets one charged/base interval.
 AffineMatrixServiceResult pending;pending.status=AffineMatrixServiceStatus::Pending;pending.slice_pending=true;
 pending.synchronization_work=25;pending.slice_work=25;pending.slice_resumed=true;pending.slice_completed_steps=3;
 DefaultAffineSearchTestAccess::completed(p,k,pending);
 assert(p.affine_search_diagnostics().service_due_ticks==45&&p.affine_search_diagnostics().service_delay_current==2);
 assert(p.affine_search_diagnostics().pending_slices==1&&p.affine_search_diagnostics().resumed_slices==1&&p.affine_search_diagnostics().no_yield_delays==2);
 // Actual native root-unit consumption uses the existing reason-ID observer.
 DefaultAffineSearchTestAccess::track_unit(p,7);k.unit_ids[0]=7;
 kernel_assign_unit(k,make_lit(0,false));assert(propagate_apply(k)==ApplyResult::Ok);
 DefaultAffineSearchTestAccess::consume(p,k,0);
 assert(p.affine_search_diagnostics().implications_consumed==1&&p.affine_search_diagnostics().service_delay_current==1);
 assert(p.affine_search_diagnostics().service_due_ticks==k.stats.search_ticks);
 assert(DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);
 // A new root fact wakes the retained service, while restart count and
 // decision-trail churn do not describe a changed root theory state.
 k.stats.search_ticks=21;assert(!DefaultAffineSearchTestAccess::due(p,k));
 kernel_assign_unit(k,make_lit(1,false));assert(propagate_apply(k)==ApplyResult::Ok);
 assert(DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);++k.stats.restarts;assert(!DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);
 kernel_assign_decision(k,make_lit(2,false));assert(propagate_apply(k)==ApplyResult::Ok);kernel_backtrack(k,0);
 assert(!DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);
 kernel_assign_decision(k,make_lit(2,true));assert(propagate_apply(k)==ApplyResult::Ok);kernel_backtrack(k,0);
 assert(!DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);kernel_affine_note_namespace_change(k);
 assert(DefaultAffineSearchTestAccess::due(p,k));
 DefaultAffineSearchTestAccess::completed(p,k,none);DefaultAffineSearchTestAccess::old_owner_stamp(p);
 assert(DefaultAffineSearchTestAccess::due(p,k));
 // Both clock addition and a growing ladder saturate, never wrap early.
 k.stats.search_ticks=UINT64_MAX-5;DefaultAffineSearchTestAccess::completed(p,k,pending);
 assert(p.affine_search_diagnostics().service_due_ticks==UINT64_MAX);
 k.stats.search_ticks=UINT64_MAX-1;assert(!DefaultAffineSearchTestAccess::due(p,k));
 k.stats.search_ticks=UINT64_MAX;assert(DefaultAffineSearchTestAccess::due(p,k));
 Kernel fallback;DefaultPolicy inherited(config(0));DefaultAffineSearchTestAccess::completed(inherited,fallback,pending);
 assert(inherited.affine_search_diagnostics().service_due_ticks==25); // actual work exceeds inherited13.
 pending.synchronization_work=0;DefaultAffineSearchTestAccess::completed(inherited,fallback,pending);
 assert(inherited.affine_search_diagnostics().service_due_ticks==13);
}
