#undef NDEBUG
#include "rules/semantic_extension/affine_matrix.hpp"
#include <cassert>
#include <cstdlib>
#include <new>
using namespace tactix::affine_semantic_extension;
namespace {bool tracking=false;size_t watched_bytes=0,allocations=0;}
void *operator new(size_t bytes){if(tracking&&bytes==watched_bytes)++allocations;void *p=std::malloc(bytes?bytes:1);if(!p)std::abort();return p;}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
int main(){
 const std::vector<Equation> sources={{{1,2,3},false,{},{}},{{4,5,6},true,{},{}}};
 AffineMatrix matrix;const MatrixLimits limits{10000,32,32,32,32,11};
 assert(build_affine_matrix(sources,91,limits,[](auto){return true;},matrix)==MatrixStatus::Complete);
 std::vector<MatrixAssignment> before={{2,false}},after={{1,true}};
 for(int variable=7;variable<=16;++variable){before.push_back({variable,false});after.push_back({variable,false});}
 MatrixEvent event;assert(matrix.synchronize(before,[](auto){return true;},event,true)==MatrixStatus::Complete);
 // Reassignment requires a full snapshot workspace. A speculative full delta
 // would allocate a second equally sized buffer, exceeding the admitted peak.
 watched_bytes=after.size()*sizeof(MatrixAssignment);allocations=0;tracking=true;
 const auto status=matrix.synchronize(after,[](auto){return true;},event,true);tracking=false;
 assert(status==MatrixStatus::Complete&&matrix.last_synchronization_kind()==MatrixSynchronizationKind::Reassignment);
 assert(allocations==1&&matrix.trail()==after);
 const auto saved=matrix.trail();after.push_back({17,true});tracking=true;allocations=0;
 assert(matrix.synchronize(after,[](auto){return true;},event,true)==MatrixStatus::Exhausted);tracking=false;
 assert(!allocations&&matrix.trail()==saved);
}
