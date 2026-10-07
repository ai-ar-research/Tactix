#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
#include <vector>
using namespace tactix::affine_semantic_extension;
namespace {
MatrixLimits limits(){return {100000,32,32,32,32,32};}
bool model(const std::vector<Equation>& equations,unsigned mask){
 for(const auto &eq:equations){bool value=false;for(int v:eq.variables)value^=(mask>>(v-1))&1;if(value!=eq.rhs)return false;}return true;
}
void projected_signed_relation(){
 std::vector<Equation> sources={{{2,3,4},false,{},{}},{{3,4,5},true,{},{}},{{1,2},true,{},{}}};
 AffineMatrix matrix;assert(build_affine_matrix(sources,17,limits(),[](auto){return true;},matrix)==MatrixStatus::Complete);
 uint64_t work=0;assert(matrix.project({2,4},[&](auto n){work+=n;return true;})==MatrixStatus::Complete&&work);
 for(const auto &row:matrix.admitted_rows())for(int v:row.variables)assert(v!=2&&v!=4);
 MatrixEvent event;assert(matrix.synchronize({{1,true}},[](auto){return true;},event)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Implication&&event.implied_variable==5&&event.implied_value);
 assert(event.reason==std::vector<int>({-1,5}));
 assert(matrix.authenticates(17,sources,event,[](auto){return true;}));
 for(unsigned m=0;m<32;++m)if(model(sources,m)){bool clause=false;for(int l:event.reason)clause|=bool((m>>(abs(l)-1))&1)==(l>0);assert(clause);}
 assert(matrix.synchronize({},[](auto){return true;},event)==MatrixStatus::Complete&&event.kind==MatrixEventKind::None);
}
void failed_projection_is_atomic(){
 std::vector<Equation> sources={{{1,2,3},false,{},{}},{{2,3,4},true,{},{}}};
 AffineMatrix matrix;assert(build_affine_matrix(sources,19,limits(),[](auto){return true;},matrix)==MatrixStatus::Complete);
 const auto before=matrix.admitted_rows();
 assert(matrix.project({2},[](auto){return false;})==MatrixStatus::Interrupted);assert(matrix.admitted_rows()==before);
 assert(matrix.project({3,2},[](auto){return true;})==MatrixStatus::Invalid);assert(matrix.admitted_rows()==before);
 assert(matrix.project({2,2},[](auto){return true;})==MatrixStatus::Invalid);assert(matrix.admitted_rows()==before);
 assert(matrix.project({2},[](auto){return true;})==MatrixStatus::Complete);
 MatrixEvent event;assert(matrix.synchronize({{1,false}},[](auto){return true;},event)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::Implication&&event.implied_variable==4&&event.implied_value);
}
}
int main(){projected_signed_relation();failed_projection_is_atomic();}
