#undef NDEBUG
#include "engine/clausal_replay.hpp"
#include "proof/direct_php_certificate.hpp"
#include <algorithm>
#include <cassert>
#include <climits>
#include <map>

using tactix::ClausalReplay;
using Clause=ClausalReplay::Clause;
namespace php=tactix::direct_php_certificate;

static void local_semantics() {
  ClausalReplay c(4,UINT64_MAX);
  assert(c.import({-1,2}) && c.import({1,2}));
  assert(!c.rup({1}) && c.rat({1})); // strict RAT, not RUP
  auto added=c.import({1});assert(added);
  assert(c.rup({2}));
  assert(c.erase(added));assert(!c.rup({1}) && c.rup({2}));
  ClausalReplay d(2,UINT64_MAX);
  auto unit=d.import({1});auto reason=d.import({-1,2});
  assert(d.rup({2}));assert(d.erase(reason));assert(!d.rup({2}));
  assert(d.erase(unit));assert(!d.rup({1}));
  assert(!d.erase(unit));
  ClausalReplay opposite(2,UINT64_MAX);
  auto h=opposite.import({-1});assert(h);
  assert(!opposite.rat({1})); // omitting the sole opposing clause is unsound
  assert(opposite.erase(h) && opposite.rat({1}));
}
static void watches_and_limits() {
  ClausalReplay c(4,UINT64_MAX);
  assert(c.import({1,2,3,4}) && c.import({-1}));
  for (int i=0;i<10;++i) {assert(c.rup({2,3,4}));assert(!c.rup({2,3}));}
  assert(c.import({-2}) && c.import({-3}));assert(c.rup({4}));
  ClausalReplay behind_root(2,UINT64_MAX);
  assert(behind_root.import({1}) && behind_root.rup({1}));
  assert(behind_root.import({-1,2}) && behind_root.rup({2}));
  ClausalReplay empty(0,UINT64_MAX);
  assert(!empty.rup({}));auto e=empty.import({});assert(e && empty.rup({}));
  assert(empty.erase(e) && !empty.rup({}));
  ClausalReplay budget(2,0);assert(budget.status()==ClausalReplay::Status::Exhausted);
  assert(!budget.import({1}));
  volatile sig_atomic_t stop=1;
  ClausalReplay interrupted(2,100,&stop);
  assert(interrupted.status()==ClausalReplay::Status::Interrupted);
  ClausalReplay invalid(2,100);assert(!invalid.rup({INT_MIN}));
  assert(invalid.status()==ClausalReplay::Status::Invalid);
  ClausalReplay tautology(2,100);assert(tautology.rup({1,-1}));
}
static Clause key(Clause c) {std::sort(c.begin(),c.end());return c;}
static void cubic_replay(unsigned n,bool keep_input) {
  php::Matrix matrix(n+1,std::vector<int32_t>(n));int32_t fresh=1;
  for (auto &row:matrix) for (auto &v:row) v=fresh++;
  php::Plan plan;assert(php::plan(matrix,fresh,plan)==php::Status::Ok);
  ClausalReplay c(uint32_t(plan.accounting.next_fresh-1),UINT64_MAX);
  std::map<Clause,std::vector<ClausalReplay::Handle>> handles;
  auto add=[&](const Clause &row) {
    auto handle=c.import(row);assert(handle);handles[key(row)].push_back(handle);
  };
  for (const auto &row:matrix) add(row);
  for (unsigned j=0;j<n;++j)
    for (unsigned i=0;i<=n;++i)
      for (unsigned k=i+1;k<=n;++k) add({-matrix[i][j],-matrix[k][j]});
  bool corrupt_cover_checked=false;
  for (const auto &event:plan.events) {
    if (event.kind==php::EventKind::Delete) {
      if (keep_input && event.role==php::EventRole::RetireOriginalAmo) continue;
      auto &list=handles[key(event.clause)];assert(!list.empty());
      assert(c.erase(list.back()));list.pop_back();continue;
    }
    if (!corrupt_cover_checked && event.role==php::EventRole::PigeonCover) {
      assert(!c.rup({}));corrupt_cover_checked=true;
    }
    assert(event.kind==php::EventKind::AddRat ? c.rat(event.clause) : c.rup(event.clause));
    add(event.clause);
  }
  assert(corrupt_cover_checked && c.rup({}));
  assert(c.status()==ClausalReplay::Status::Ready);
}
// Exhaustive finite-state comparison with an independent full-scan UP oracle.
// Every two-clause formula on three variables, every target and every pivot,
// before and after deleting one occurrence (including duplicate clauses).
static bool naive_rup(const std::vector<Clause> &formula,const Clause &target) {
  int values[3]={0,0,0};
  auto assign=[&](int l) {
    const int v=std::abs(l)-1, sign=l>0 ? 1:-1;
    if (values[v]) return values[v]==sign;
    values[v]=sign;return true;
  };
  for (int l:target) if (!assign(-l)) return true;
  bool changed;
  do {
    changed=false;
    for (const auto &c:formula) {
      bool satisfied=false;int open=0,count=0;
      for (int l:c) {
        int value=values[std::abs(l)-1];
        if (value==(l>0 ? 1:-1)) {satisfied=true;break;}
        if (!value) {open=l;++count;}
      }
      if (satisfied) continue;
      if (!count) return true;
      if (count==1) {if (!assign(open)) return true;changed=true;}
    }
  } while (changed);
  return false;
}
static bool naive_rat(const std::vector<Clause> &formula,const Clause &c) {
  assert(!c.empty());int pivot=c.front();
  for (const auto &d:formula) {
    if (std::find(d.begin(),d.end(),-pivot)==d.end()) continue;
    Clause r;
    for (int l:c) if (l!=pivot) r.push_back(l);
    for (int l:d) if (l!=-pivot) r.push_back(l);
    if (!naive_rup(formula,r)) return false;
  }
  return true;
}
static void exhaustive_three_variables() {
  std::vector<Clause> clauses;
  for (int code=0;code<27;++code) {
    int digits=code;Clause c;
    for (int v=1;v<=3;++v,digits/=3) {
      if (digits%3==1) c.push_back(v);
      if (digits%3==2) c.push_back(-v);
    }
    clauses.push_back(c);
  }
  for (const auto &a:clauses) for (const auto &b:clauses) {
    ClausalReplay check(3,UINT64_MAX);
    auto first=check.import(a);assert(first && check.import(b));
    std::vector<Clause> formula{a,b};
    for (int phase=0;phase<2;++phase) {
      for (auto target:clauses) {
        assert(check.rup(target)==naive_rup(formula,target));
        for (size_t pivot=0;pivot<target.size();++pivot) {
          std::swap(target[0],target[pivot]);
          assert(check.rat(target)==naive_rat(formula,target));
          std::swap(target[0],target[pivot]);
        }
      }
      if (!phase) {assert(check.erase(first));formula.erase(formula.begin());}
    }
  }
}
int main() {
  local_semantics();watches_and_limits();exhaustive_three_variables();
  for (unsigned n=2;n<=10;++n) {cubic_replay(n,false);cubic_replay(n,true);}
}
