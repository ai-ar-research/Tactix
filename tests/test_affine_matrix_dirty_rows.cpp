#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
#include <set>
using namespace tactix::affine_semantic_extension;
namespace {
MatrixLimits limits(){return {1000000,256,1024,64,64,1024,true};}
AffineMatrix build(const std::vector<Equation>&s){AffineMatrix m;assert(build_affine_matrix(s,73,limits(),[](auto){return true;},m)==MatrixStatus::Complete);return m;}
const MatrixIncrementalLimits bytes{16*1024*1024};
void independent_rows(){
 std::vector<Equation> sources;
 for(int i=0;i<100;++i)sources.push_back({{1+i*3,2+i*3,3+i*3},bool(i&1),{},{}});
 auto matrix=build(sources);MatrixEvent event;
 assert(matrix.synchronize({},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 const auto unchanged=matrix.rows()[90];
 auto single=build({sources.front()});MatrixEvent single_event;
 assert(single.synchronize({},[](auto){return true;},single_event,true,&bytes)==MatrixStatus::Complete);
 uint64_t many_work=0,single_work=0;
 assert(matrix.synchronize({{1,true}},[&](auto n){many_work+=n;return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(single.synchronize({{1,true}},[&](auto n){single_work+=n;return true;},single_event,true,&bytes)==MatrixStatus::Complete);
 assert(many_work==single_work); // Unaffected 99 row bodies add no work.
 assert(matrix.last_dirty_update()&&matrix.last_dirty_rows()==1&&matrix.rows()[90]==unchanged&&event.kind==MatrixEventKind::None);
 assert(matrix.synchronize({{1,true},{2,false}},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(matrix.last_dirty_rows()==1&&event.kind==MatrixEventKind::Implication&&event.implied_variable==3&&event.implied_value);
 assert((event.reason==std::vector<int32_t>{-1,2,3}));assert(matrix.authenticates(73,sources,event,[](auto){return true;}));
 const auto rows=matrix.rows();const auto trail=matrix.trail();const auto reason=event.reason;
 auto tiny=bytes;tiny.maximum_index_bytes=matrix.incremental_index_bytes()-1;
 assert(matrix.synchronize({{1,true},{2,false},{4,true}},[](auto){return true;},event,true,&tiny)==MatrixStatus::Exhausted);
 assert(matrix.rows()==rows&&matrix.trail()==trail&&event.reason==reason);
 assert(matrix.synchronize({{1,true},{2,false},{4,true}},[](auto){return false;},event,true,&bytes)==MatrixStatus::Interrupted);
 assert(matrix.rows()==rows&&matrix.trail()==trail&&event.reason==reason);
 assert(matrix.synchronize({},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete&&event.kind==MatrixEventKind::None);
}
void source_oracle(const std::vector<Equation>&sources,const std::vector<MatrixAssignment>&trail,const MatrixEvent&event){
 if(event.kind==MatrixEventKind::None)return;
 // Independent clause-entailment oracle over all total original models.
 for(unsigned model=0;model<16;++model){
  bool satisfies=true;
  for(const auto &equation:sources){bool parity=false;for(int v:equation.variables)parity^=bool(model&(1u<<(v-1)));satisfies&=parity==equation.rhs;}
  if(!satisfies)continue;
  bool clause=false;for(int literal:event.reason){bool value=bool(model&(1u<<(std::abs(literal)-1)));clause|=literal>0?value:!value;}
  assert(clause);
 }
 std::set<External> variables;bool rhs=false;
 for(size_t source:event.row.source_equations){assert(source<sources.size());rhs^=sources[source].rhs;for(int v:sources[source].variables){if(variables.contains(v))variables.erase(v);else variables.insert(v);}}
 for(auto assignment:trail)if(variables.erase(assignment.variable))rhs^=assignment.value;
 assert((std::vector<External>(variables.begin(),variables.end())==event.row.variables)&&rhs==event.row.rhs);
}
void fixed_truth_table(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{2,3,4},true,{},{}}};
 // Every partial assignment of this fixed four-coordinate system; no random
 // formulas or seed campaign. Reassignments require exact bounded rebuilds.
 auto matrix=build(sources);MatrixEvent event;
 for(unsigned encoding=0;encoding<81;++encoding){
  unsigned digits=encoding;std::vector<MatrixAssignment> trail;
  for(int variable=1;variable<=4;++variable){unsigned digit=digits%3;digits/=3;if(digit)trail.push_back({variable,digit==2});}
  assert(matrix.synchronize(trail,[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
  source_oracle(sources,trail,event);assert(event.kind==MatrixEventKind::None||matrix.authenticates(73,sources,event,[](auto){return true;}));
 }
}
void late_atomic(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{2,3,4},true,{},{}}};
 auto reference=build(sources),stopped=build(sources);MatrixEvent a,b;
 assert(reference.synchronize({},[](auto){return true;},a,true,&bytes)==MatrixStatus::Complete);
 assert(stopped.synchronize({},[](auto){return true;},b,true,&bytes)==MatrixStatus::Complete);
 size_t successful_calls=0;
 assert(reference.synchronize({{1,true}},[&](auto){++successful_calls;return true;},a,true,&bytes)==MatrixStatus::Complete);
 assert(successful_calls>20);
 const auto old_rows=stopped.rows();const auto old_trail=stopped.trail();
 const auto old_work=stopped.work(),old_bytes=stopped.incremental_index_bytes();
 auto scratch_cap=bytes;scratch_cap.maximum_index_bytes=old_bytes;
 size_t scratch_calls=0;
 assert(stopped.synchronize({{1,true}},[&](auto){++scratch_calls;return true;},b,true,&scratch_cap)==MatrixStatus::Exhausted);
 assert(scratch_calls>1); // Passed admission; failed private transaction scratch.
 assert(stopped.rows()==old_rows&&stopped.trail()==old_trail&&stopped.work()==old_work&&stopped.incremental_index_bytes()==old_bytes&&b.kind==MatrixEventKind::None);
 size_t calls=0;
 assert(stopped.synchronize({{1,true}},[&](auto){return ++calls<successful_calls-1;},b,true,&bytes)==MatrixStatus::Interrupted);
 assert(stopped.rows()==old_rows&&stopped.trail()==old_trail&&stopped.work()==old_work&&stopped.incremental_index_bytes()==old_bytes);
 assert(b.kind==MatrixEventKind::None);
 assert(stopped.synchronize({{1,true}},[](auto){return true;},b,true,&bytes)==MatrixStatus::Complete);
 assert(b.reason==a.reason&&b.row==a.row);
}
void indexed_predecessor_restore(){
 const std::vector<Equation> sources={{{1,2},false,{},{}},{{3,4,5},false,{},{}}};
 auto prepare=[&](){auto m=build(sources);MatrixEvent e;assert(m.synchronize({{1,true}},[](auto){return true;},e)==MatrixStatus::Complete);assert(m.synchronize({{1,true},{3,false}},[](auto){return true;},e)==MatrixStatus::Complete);return m;};
 auto reference=prepare(),stopped=prepare();MatrixEvent a,b;
 size_t calls=0;
 assert(reference.synchronize({{1,true}},[&](auto){++calls;return true;},a,true,&bytes)==MatrixStatus::Complete);
 assert(reference.last_synchronization_restored_predecessor());
 assert((a.reason==std::vector<int32_t>{-1,2}));
 const auto rows=stopped.rows();const auto trail=stopped.trail();const auto work=stopped.work();
 size_t failed=0;
 assert(stopped.synchronize({{1,true}},[&](auto){return ++failed<calls;},b,true,&bytes)==MatrixStatus::Interrupted);
 assert(stopped.rows()==rows&&stopped.trail()==trail&&stopped.work()==work&&b.kind==MatrixEventKind::None);
 auto tiny=bytes;tiny.maximum_index_bytes=1;
 assert(stopped.synchronize({{1,true}},[](auto){return true;},b,true,&tiny)==MatrixStatus::Exhausted);
 assert(stopped.rows()==rows&&stopped.trail()==trail&&stopped.work()==work);
 assert(stopped.synchronize({{1,true}},[](auto){return true;},b,true,&bytes)==MatrixStatus::Complete);
 assert(b.row==a.row&&b.reason==a.reason&&stopped.authenticates(73,sources,b,[](auto){return true;}));
}
void pivot_collision_and_conflict(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{2,3,4},true,{},{}}};
 auto matrix=build(sources);MatrixEvent event;
 assert(matrix.synchronize({},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(matrix.synchronize({{1,true}},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 // Gauss-Jordan basis {1,4}=1, {2,3,4}=1: x1 moves the first row's pivot to
 // x4, which column elimination removes from the second row as well.
 assert(matrix.last_dirty_rows()==2&&event.kind==MatrixEventKind::Implication&&event.implied_variable==4&&!event.implied_value);
 assert((event.reason==std::vector<int32_t>{-1,-4}));assert(matrix.authenticates(73,sources,event,[](auto){return true;}));
 const auto historical=event;
 assert(matrix.synchronize({{1,true},{4,true}},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Conflict&&(event.reason==std::vector<int32_t>{-1,-4}));
 assert(matrix.authenticates(73,sources,event,[](auto){return true;}));
 // The earlier implication is no longer the tableau's selected event, but its
 // complete falsifying clause remains independently derivable from the exact
 // immutable source rows.  This is required before a reason may be deferred.
 assert(!matrix.authenticates(73,sources,historical,[](auto){return true;}));
 assert(matrix.authenticates_historical(73,sources,historical,[](auto){return true;}));
 auto corrupted=historical;corrupted.reason[0]=1;
 assert(!matrix.authenticates_historical(73,sources,corrupted,[](auto){return true;}));
 size_t historical_calls=0;
 assert(!matrix.authenticates_historical(73,sources,historical,
     [&](auto){return ++historical_calls<3;}));
 assert(matrix.synchronize({},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete&&event.kind==MatrixEventKind::None);
 assert(matrix.synchronize({{1,false}},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Implication&&event.implied_variable==4&&event.implied_value);
 assert((event.reason==std::vector<int32_t>{1,4}));
}
void cancelled_support_is_a_sound_reason_weakening(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{2,3,4},true,{},{}}};
 auto matrix=build(sources);MatrixEvent event;
 assert(matrix.synchronize({},[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 // The selected row is E1 xor E2: x1 xor x4 = 1.  Variables x2 and x3
 // cancel from its minimal GF(2) support.  Keeping their false trail literals
 // is nevertheless a sound clausal weakening, and the historical audit must
 // authenticate the source-derived {-1,-4} core rather than require equality.
 const std::vector<MatrixAssignment> trail={{1,true},{2,false},{3,true}};
 assert(matrix.synchronize(trail,[](auto){return true;},event,true,&bytes)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Implication&&event.implied_variable==4&&!event.implied_value);
 assert((event.row.source_equations==std::vector<size_t>{0,1}));
 assert((event.reason==std::vector<int32_t>{-1,2,-3,-4}));
 source_oracle(sources,trail,event);
 assert(matrix.authenticates(73,sources,event,[](auto){return true;}));
 assert(matrix.authenticates_historical(73,sources,event,[](auto){return true;}));
}
}
int main(){indexed_predecessor_restore();independent_rows();pivot_collision_and_conflict();cancelled_support_is_a_sound_reason_weakening();late_atomic();fixed_truth_table();}
