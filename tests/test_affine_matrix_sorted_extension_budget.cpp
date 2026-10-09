#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
using namespace tactix::affine_semantic_extension;
int main(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{4,5,6},true,{},{}}};
 AffineMatrix matrix;const MatrixLimits cap{16,32,32,32,32,32,true};
 assert(build_affine_matrix(sources,83,cap,[](auto){return true;},matrix)==MatrixStatus::Complete);
 MatrixEvent event;assert(matrix.synchronize({{3,false}},[](auto){return true;},event,true)==MatrixStatus::Complete);
 const auto rows=matrix.rows();const auto trail=matrix.trail();const auto old_kind=event.kind;
 // Difference staging and conditioning fit; Gauss-Jordan back-substitution
 // exceeds the allowance. A partial private basis must not replace the
 // complete owner.
 assert(matrix.synchronize({{1,true},{3,false}},[](auto){return true;},event,true)==MatrixStatus::Exhausted);
 assert(matrix.rows()==rows&&matrix.trail()==trail&&event.kind==old_kind);
 assert(matrix.synchronize({},[](auto){return true;},event,true)==MatrixStatus::Complete);
 assert(event.kind==MatrixEventKind::None);
}
