#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
using namespace tactix::affine_semantic_extension;
int main() {
  const std::vector<Equation> equations={Equation{{1,2,3},false,{},{}},Equation{{2,3,4},true,{},{}}};
  MatrixLimits limits{100,32,32,32,32,32};
  AffineMatrix matrix;
  assert(build_affine_matrix(equations,1,limits,[](uint64_t){return true;},matrix,true)==MatrixStatus::Complete);
  const auto rows=matrix.rows();const auto trail=matrix.trail();const auto work=matrix.work();
  MatrixEvent event;event.reason={99};
  MatrixIncrementalLimits ample{1000000};
  assert(matrix.synchronize({},[](uint64_t){return true;},event,true,&ample)==MatrixStatus::Exhausted);
  assert(matrix.last_synchronization_work_exhausted());
  assert(matrix.rows()==rows&&matrix.trail()==trail&&matrix.work()==work&&event.reason==std::vector<int32_t>{99});
  MatrixIncrementalLimits tiny{1};
  assert(matrix.synchronize({},[](uint64_t){return true;},event,true,&tiny)==MatrixStatus::Exhausted);
  assert(!matrix.last_synchronization_work_exhausted());
  assert(matrix.synchronize({},[](uint64_t){return false;},event,true,&ample)==MatrixStatus::Interrupted);
  assert(!matrix.last_synchronization_work_exhausted());
  assert(matrix.rows()==rows&&matrix.trail()==trail&&matrix.work()==work&&event.reason==std::vector<int32_t>{99});
  unsigned doses=0;bool resumed=false;
  MatrixStatus status;
  do {
    status=matrix.synchronize_sliced({},[](uint64_t){return true;},event,{100,1000000});
    resumed|=matrix.last_slice_effort().resumed;
    assert(status==MatrixStatus::Pending||status==MatrixStatus::Complete);
    assert(++doses<100);
  }while(status==MatrixStatus::Pending);
  assert(doses>1&&resumed);
}
