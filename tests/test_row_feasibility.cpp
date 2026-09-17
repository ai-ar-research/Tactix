#undef NDEBUG
#include "rules/substitution_redundancy/row_feasibility.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
using namespace tactix;
using namespace tactix::symmetry_backend;

struct Meter { uint64_t spent=0, limit=UINT64_MAX; };
static bool charge(void *opaque) {
  auto &meter=*static_cast<Meter *>(opaque);
  if(meter.spent==meter.limit)return false;
  ++meter.spent;return true;
}

static void compare_independent_columns() {
  const uint32_t values[]={0,1,2,3,4,5,6,7,UINT32_MAX};
  unsigned compared=0;
  for(bool wide:{false,true}) {
    const std::vector<std::vector<Lit>> seed=wide
      ?std::vector<std::vector<Lit>>{{0,2},{2,0},{4,6}}
      :std::vector<std::vector<Lit>>{{0},{2},{4}};
    for(uint32_t a:values)for(uint32_t b:values)for(uint32_t c:values)for(uint32_t d:values) {
      std::vector<uint32_t> images(24,UINT32_MAX);
      images[4]=4;images[6]=6;images[8+4]=a;images[8+6]=b;images[16+4]=c;images[16+6]=d;
      RowColumnFeasibility feedback(seed);Meter meter;
      for(uint32_t prefix=1;prefix<=3;++prefix) {
        std::vector<uint32_t> expected;
        for(uint32_t source=0;source<8;++source) {
          bool possible=true;
          for(Lit used:seed.front())possible&=var_of(source)!=var_of(used);
          std::set<Var> variables;
          for(uint32_t row=0;row<prefix;++row) {
            const auto target=images[row*8+source];
            possible&=target<8;
            for(Lit used:seed[row])possible&=var_of(target)!=var_of(used);
            variables.insert(var_of(target));
          }
          possible&=variables.size()==prefix;
          if(possible)expected.push_back(source);
        }
        const RefinementRowPrefix view{prefix,3,8,images.data()};
        assert(RowColumnFeasibility::observe(view,charge,&meter,&feedback)==!expected.empty());
        assert(feedback.remaining_sources()==expected);++compared;
      }
    }
  }
  assert(compared==39366);
  // Every charged prefix can abort without modifying the caller's seed or
  // images. A full allowance includes the final comparison exactly.
  const std::vector<std::vector<Lit>> seed{{0},{2},{4}};
  const std::vector<uint32_t> images={UINT32_MAX,UINT32_MAX,2,UINT32_MAX,4,UINT32_MAX,6,UINT32_MAX,
                                    UINT32_MAX,UINT32_MAX,4,UINT32_MAX,6,UINT32_MAX,0,UINT32_MAX,
                                    UINT32_MAX,UINT32_MAX,6,UINT32_MAX,0,UINT32_MAX,2,UINT32_MAX};
  RowColumnFeasibility full(seed);Meter unlimited;
  for(uint32_t row=1;row<=3;++row)assert(RowColumnFeasibility::observe({row,3,8,images.data()},charge,&unlimited,&full));
  for(uint64_t limit=0;limit<=unlimited.spent;++limit) {
    RowColumnFeasibility bounded(seed);Meter meter{0,limit};bool complete=true;
    for(uint32_t row=1;row<=3&&complete;++row)complete=RowColumnFeasibility::observe({row,3,8,images.data()},charge,&meter,&bounded);
    assert(meter.spent<=limit);assert(complete==(limit==unlimited.spent));
  }
  std::printf("row feasibility: %u independent prefix oracles and every work prefix PASS\n",compared);
}

struct Feedback {
  unsigned calls=0, decline_at=0;
  bool stop=false, stop_in_feedback=false, refused=false;
};
static bool stopped(void *p) noexcept {return static_cast<Feedback *>(p)->stop;}
static bool observe(const RefinementRowPrefix &view,RowWorkCallback work,void *meter,void *opaque) {
  auto &state=*static_cast<Feedback *>(opaque);++state.calls;
  assert(view.completed_rows==state.calls && view.anchor_count==2 && view.domain_size==4);
  assert(view.images && view.images[0]==0);
  for(unsigned i=0;i<3;++i){if(state.stop_in_feedback&&i==1)state.stop=true;if(!work(meter)){state.refused=true;return false;}}
  return state.decline_at!=view.completed_rows;
}
static void generic_feedback_is_bounded() {
  const uint32_t colors[]={0,0,0,0},degrees[]={1,1,1,1},edges[]={0,3,1,2},anchors[]={0,1};
  Graph graph{4,2,colors,degrees,edges};RefinementSession *session=nullptr;uint64_t ticks=0;
  assert(prepare_refinement(graph,UINT64_MAX,nullptr,nullptr,session,ticks)==Status::Complete);
  RefinementRows original;assert(refine_rows(*session,anchors,2,4,UINT64_MAX,nullptr,nullptr,original)==Status::Complete);
  for(unsigned decline:{0u,1u,2u})for(uint64_t limit=0;limit<=original.ticks+6;++limit) {
    Feedback state;state.decline_at=decline;RefinementRows rows;
    const auto status=refine_rows(*session,anchors,2,4,limit,stopped,&state,rows,observe,&state);
    assert(rows.ticks<=limit);assert(status==Status::Incomplete||status==Status::Complete);
    if(status==Status::Complete&&decline)assert(!rows.images&&!rows.sources&&state.calls==decline);
    if(status==Status::Complete&&!decline) {
      assert(rows.ticks==original.ticks+6 && state.calls==2);
      assert(rows.source_count==original.source_count);
      assert(std::equal(rows.sources,rows.sources+rows.source_count,original.sources));
      assert(std::equal(rows.images,rows.images+8,original.images));
    }
    if(status==Status::Incomplete&&state.refused)assert(!rows.images);
    if(status==Status::Incomplete&&!state.refused)assert(!rows.source_count);
    release(rows);
  }
  Feedback state;state.stop_in_feedback=true;RefinementRows interrupted;
  assert(refine_rows(*session,anchors,2,4,UINT64_MAX,stopped,&state,interrupted,observe,&state)==Status::Interrupted);
  assert(!interrupted.images&&!interrupted.sources&&state.calls==1);release(interrupted);
  release(original);release(session);
}
int main(){compare_independent_columns();generic_feedback_is_bounded();}
