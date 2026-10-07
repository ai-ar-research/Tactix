#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
#include <vector>
using namespace tactix::affine_semantic_extension;
namespace {
Equation row(std::initializer_list<External> variables,bool rhs){Equation e;e.variables=variables;e.rhs=rhs;return e;}
void reason_valid(const std::vector<Equation>&sources,const MatrixEvent&event){
 for(unsigned values=0;values<32;++values){
  bool premises=true;
  for(size_t index:event.row.source_equations){bool parity=false;for(auto v:sources[index].variables)parity^=bool(values&(1u<<(v-1)));premises&=parity==sources[index].rhs;}
  if(!premises)continue;
  bool clause=false;for(auto lit:event.reason){auto v=lit<0?-lit:lit;clause|=bool(values&(1u<<(v-1)))==(lit>0);}assert(clause);
 }
}
}
int main(){
 const std::vector<Equation> sources={row({1,3,4},false),row({2,3,4,5},false)};
 MatrixLimits limits{1000000,64,64,64,64,64};AffineMatrix matrix;
 auto charge=[](uint64_t){return true;};
 assert(build_affine_matrix(sources,987,limits,charge,matrix)==MatrixStatus::Complete);
 MatrixEvent event;assert(matrix.synchronize({{2,false}},charge,event,true)==MatrixStatus::Complete);
 const auto prior_rows=matrix.rows();const auto prior_trail=matrix.trail();const auto prior_work=matrix.work();
 const auto prior_event=event;
 // A newly assigned lower coordinate is a set extension, not a reassignment.
 const std::vector<MatrixAssignment> extended={{1,false},{2,false}};
 assert(matrix.synchronize(extended,[](uint64_t){return false;},event,true)==MatrixStatus::Interrupted);
 assert(matrix.rows()==prior_rows && matrix.trail()==prior_trail && matrix.work()==prior_work);
 assert(event.kind==prior_event.kind && event.reason==prior_event.reason && event.row==prior_event.row);
 assert(matrix.synchronize(extended,charge,event,true)==MatrixStatus::Complete);
 assert(matrix.last_synchronization_incremental() && matrix.last_synchronization_conditioned()==1);
 assert(matrix.last_synchronization_kind()==MatrixSynchronizationKind::Extension);
 assert(event.kind==MatrixEventKind::Implication && event.implied_variable==5 && !event.implied_value);
 assert((event.row.source_equations==std::vector<size_t>{0,1}));
 assert((event.reason==std::vector<External>{1,2,-5}));reason_valid(sources,event);
 assert(matrix.authenticates(987,sources,event,charge));
 const auto saved_rows=matrix.rows();const auto saved_trail=matrix.trail();const auto saved_work=matrix.work();const auto saved_event=event;
 uint64_t allowance=1;
 assert(matrix.synchronize({{1,false},{2,false},{5,true}},[&](uint64_t n){if(n>allowance)return false;allowance-=n;return true;},event,true)==MatrixStatus::Interrupted);
 assert(matrix.rows()==saved_rows && matrix.trail()==saved_trail && matrix.work()==saved_work);
 assert(event.row==saved_event.row && event.reason==saved_event.reason && event.kind==saved_event.kind);
 assert(matrix.synchronize({{1,false},{2,false},{5,true}},charge,event,true)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Conflict);reason_valid(sources,event);
 assert(matrix.synchronize(extended,charge,event,true)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Implication && !event.implied_value);
 assert(matrix.last_synchronization_kind()==MatrixSynchronizationKind::Backtrack);
 assert(matrix.synchronize({{1,true},{2,false}},charge,event,true)==MatrixStatus::Complete);
 assert(!matrix.last_synchronization_incremental() && matrix.last_synchronization_kind()==MatrixSynchronizationKind::Reassignment);
 assert(event.kind==MatrixEventKind::Implication && event.implied_value);reason_valid(sources,event);
 // Assignment capacity exhaustion is atomic independently of callback decline.
 MatrixLimits bounded=limits;bounded.maximum_assignments=1;AffineMatrix capped;
 assert(build_affine_matrix(sources,988,bounded,charge,capped)==MatrixStatus::Complete);
 MatrixEvent capped_event;assert(capped.synchronize({{2,false}},charge,capped_event,true)==MatrixStatus::Complete);
 const auto capped_rows=capped.rows();const auto capped_trail=capped.trail();const auto capped_work=capped.work();
 assert(capped.synchronize(extended,charge,capped_event,true)==MatrixStatus::Exhausted);
 assert(capped.rows()==capped_rows && capped.trail()==capped_trail && capped.work()==capped_work);
}
