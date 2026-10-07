#undef NDEBUG
#include "proof/proof.hpp"
#include "kernel/kernel.hpp"
#include "rules/semantic_extension/affine_live.hpp"
#include <cassert>
#include <cstdio>
#include <new>
#include <string>
#include <unistd.h>
using namespace tactix;
namespace {
std::string path(const char *suffix) {
  return "/tmp/tactix-proof-stream-"+std::to_string(getpid())+suffix;
}
AffineLiveSnapshot capture(Kernel &k) {
  AffineLiveRequest request;
  request.image={100,100,100,100000,4,16,32,4};
  request.maximum_snapshot_literals=1000;
  request.maximum_snapshot_work=100000;
  request.maximum_snapshot_bytes=100000;
  request.full_root_view=true;
  AffineLiveSnapshot snapshot;
  assert(capture_affine_live_snapshot(k,request,snapshot));
  return snapshot;
}
void cached_unit_binding() {
  Kernel k;kernel_reserve_vars(k,1);
  Proof first,second;
  const auto a=path("-first"),b=path("-second");
  assert(first.open(a.c_str(),ProofFormat::DsrAscii));
  assert(second.open(b.c_str(),ProofFormat::DsrAscii));
  assert(first.stream_identity()&&second.stream_identity()&&first.stream_identity()!=second.stream_identity());
  first.bind_variable_map(&k.internal_to_external);
  first.bind_external_variable_map(&k.external_to_internal);
  const Lit unit=kernel_internalize(k,1);
  first.seed_population(1,&unit,1);first.arm_population_shadow();
  k.next_clause_id=1;k.proof=&first;kernel_assign_unit(k,unit);
  assert(k.level==0&&!k.model_extended&&!k.unit_ids[0]);
  kernel_materialize_dsr_root_unit(k,unit);
  const auto id=k.next_clause_id;
  auto image=capture(k);
  assert(image.sources.size()==1&&image.sources[0].kind==AffineSourceKind::RootUnit);
  assert(image.sources[0].clause==std::vector<int32_t>{1});
  assert(!image.sources[0].clause_id&&!image.sources[0].proof_id_exact);
  k.proof=nullptr;assert(capture(k).sources.empty());
  k.proof=&first;assert(capture(k).sources.size()==1);
  kernel_materialize_dsr_root_unit(k,unit);assert(k.next_clause_id==id);
  k.proof=&second;assert(capture(k).sources.empty());
  // A failed second open invalidates authority even though the old file is
  // still owned by the same Proof object.
  k.proof=&first;assert(!first.open(a.c_str(),ProofFormat::DsrAscii));
  assert(!first.stream_identity()&&capture(k).sources.empty());
  k.proof=nullptr;std::remove(a.c_str());std::remove(b.c_str());
}
void address_reuse() {
  alignas(Proof) unsigned char storage[sizeof(Proof)];
  const auto a=path("-reuse-a"),b=path("-reuse-b");
  auto *first=new(storage) Proof;
  assert(first->open(a.c_str(),ProofFormat::DsrAscii));
  const auto token=first->stream_identity();assert(token);first->~Proof();
  auto *second=new(storage) Proof;
  assert(!second->stream_identity());
  assert(second->open(b.c_str(),ProofFormat::DsrAscii));
  assert(second->stream_identity()&&second->stream_identity()!=token);
  second->~Proof();std::remove(a.c_str());std::remove(b.c_str());
  Proof failed;
  assert(!failed.open("/nonexistent-tactix-stream-directory/proof",ProofFormat::DsrAscii));
  assert(!failed.stream_identity());
}
}
int main(){cached_unit_binding();address_reuse();}
