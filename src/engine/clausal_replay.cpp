#include "engine/clausal_replay.hpp"
#include <algorithm>
#include <climits>
#include <limits>

namespace tactix {
uint32_t ClausalReplay::literal(int32_t x) {
  return 2u*(uint32_t(x<0 ? -x : x)-1u)+(x<0);
}
ClausalReplay::ClausalReplay(uint32_t extent,uint64_t maximum_work,
                           const volatile sig_atomic_t *stop)
    : extent_(extent), maximum_work_(maximum_work), stop_(stop) {
  if (extent>INT32_MAX) {status_=Status::Invalid;return;}
  if (!take(2*uint64_t(extent))) return;
  watches_.resize(2*size_t(extent));occurrences_.resize(2*size_t(extent));
  values_.resize(2*size_t(extent));marks_.resize(extent);
}
bool ClausalReplay::take(uint64_t n) {
  if (status_!=Status::Ready) return false;
  if (stop_ && *stop_) {status_=Status::Interrupted;return false;}
  if (effort_.work>maximum_work_ || n>maximum_work_-effort_.work) {
    status_=Status::Exhausted;return false;
  }
  effort_.work+=n;return true;
}
bool ClausalReplay::normalize(const Clause &input,Clause &out,bool &tautology) {
  tautology=false;out.clear();touched_.clear();
  if (!take(input.size())) return false;
  out.reserve(input.size());
  for (int32_t x:input) {
    if (!x || x==INT32_MIN || uint32_t(x<0 ? -x : x)>extent_) {
      status_=Status::Invalid;break;
    }
    const auto v=literal(x)/2;const int8_t sign=x<0 ? -1 : 1;
    if (marks_[v]==-sign) {tautology=true;break;}
    if (marks_[v]==sign) continue;
    marks_[v]=sign;touched_.push_back(v);out.push_back(x);
  }
  for (auto v:touched_) marks_[v]=0;
  return status_==Status::Ready;
}
bool ClausalReplay::assign(uint32_t l) {
  if (values_[l]) return values_[l]>0;
  values_[l]=1;values_[l^1]=-1;trail_.push_back(l);return true;
}
void ClausalReplay::undo(size_t saved) {
  while (trail_.size()>saved) {
    const auto l=trail_.back();trail_.pop_back();values_[l]=values_[l^1]=0;
  }
  head_=std::min(head_,saved);
}
bool ClausalReplay::propagate() {
  while (head_<trail_.size()) {
    if (!take()) return false;
    const uint32_t false_lit=trail_[head_++]^1;++effort_.propagated;
    auto &list=watches_[false_lit];size_t i=0,kept=0;
    while (i<list.size()) {
      const Handle h=list[i++];
      if (!take()) {
        list[kept++]=h;
        while (i<list.size()) list[kept++]=list[i++];
        list.resize(kept);return false;
      }
      ++effort_.watch_visits;
      auto &record=clauses_[h-1];
      if (!record.live) continue;
      auto &c=record.literals;
      if (literal(c[0])==false_lit) std::swap(c[0],c[1]);
      const auto other=literal(c[0]);
      if (values_[other]>0) {list[kept++]=h;continue;}
      size_t next=2;
      while (next<c.size()) {
        if (!take()) break;
        if (values_[literal(c[next])]>=0) break;
        ++next;
      }
      if (status_!=Status::Ready) {
        list[kept++]=h;
        while (i<list.size()) list[kept++]=list[i++];
        list.resize(kept);return false;
      }
      if (next<c.size()) {
        std::swap(c[1],c[next]);watches_[literal(c[1])].push_back(h);
      } else {
        list[kept++]=h;
        if (!assign(other)) {
          while (i<list.size()) list[kept++]=list[i++];
          list.resize(kept);return false;
        }
      }
    }
    list.resize(kept);
  }
  return true;
}
bool ClausalReplay::settle() {
  if (!take()) return false;
  if (rebuild_root_) {
    undo(0);head_=root_=0;root_conflict_=false;rebuild_root_=false;
    for (Handle h:units_) if (live(h)) {
      if (!take()) return false;
      if (!assign(literal(clauses_[h-1].literals[0]))) root_conflict_=true;
    }
  }
  if (empty_) root_conflict_=true;
  if (!root_conflict_ && !propagate() && status_==Status::Ready) root_conflict_=true;
  root_=trail_.size();return status_==Status::Ready;
}
ClausalReplay::Handle ClausalReplay::import(const Clause &input) {
  if (!settle() || clauses_.size()>=UINT32_MAX) return 0;
  Clause c;bool tautology=false;
  if (!normalize(input,c,tautology) || tautology) {status_=Status::Invalid;return 0;}
  const Handle h=uint32_t(clauses_.size()+1);
  for (int32_t x:c) occurrences_[literal(x)].push_back(h);
  clauses_.push_back({std::move(c),true});auto &stored=clauses_.back().literals;
  if (stored.empty()) {++empty_;root_conflict_=true;}
  else if (stored.size()==1) {
    units_.push_back(h);
    if (!assign(literal(stored[0]))) root_conflict_=true;
  } else {
    // Prefer unassigned/satisfied watches, so adding behind the root cursor
    // cannot hide a new root unit or conflict.
    size_t front=0;
    for (size_t j=0;j<stored.size() && front<2;++j)
      if (values_[literal(stored[j])]>=0) std::swap(stored[front++],stored[j]);
    watches_[literal(stored[0])].push_back(h);
    watches_[literal(stored[1])].push_back(h);
    if (!front) root_conflict_=true;
    else if (front==1 && !assign(literal(stored[0]))) root_conflict_=true;
  }
  root_=trail_.size();return h;
}
bool ClausalReplay::refute(const Clause &target) {
  ++effort_.rup_queries;
  if (!settle()) return false;
  if (root_conflict_) return true;
  const size_t saved=root_;
  bool conflict=false;
  for (int32_t x:target) {
    if (!take()) break;
    if (!assign(literal(x)^1)) {conflict=true;break;}
  }
  if (status_==Status::Ready && !conflict) conflict=!propagate();
  undo(saved);head_=saved;
  return status_==Status::Ready && conflict;
}
bool ClausalReplay::rup(const Clause &input) {
  Clause c;bool tautology=false;
  if (!normalize(input,c,tautology)) return false;
  return tautology || refute(c);
}
bool ClausalReplay::rat(const Clause &input) {
  Clause c;bool tautology=false;
  if (!normalize(input,c,tautology)) return false;
  if (tautology) return true;
  if (c.empty()) return false;
  if (!settle()) return false;
  const int32_t pivot=c.front();
  Clause combined,resolvent;
  // Occurrences are complete and fixed when a clause is imported. Watched
  // positions may change during RUP but neither membership nor handles do.
  for (Handle h:occurrences_[literal(-pivot)]) {
    if (!take()) return false;
    if (!live(h)) continue;
    ++effort_.rat_resolvents;
    combined.clear();
    for (int32_t x:c) if (x!=pivot) combined.push_back(x);
    for (int32_t x:clauses_[h-1].literals) if (x!=-pivot) combined.push_back(x);
    if (!normalize(combined,resolvent,tautology)) return false;
    if (!tautology && !refute(resolvent)) return false;
  }
  return true;
}
bool ClausalReplay::erase(Handle h) {
  if (!take() || !live(h)) return false;
  auto &c=clauses_[h-1];c.live=false;
  if (c.literals.empty()) --empty_;
  // Root assignments may cite the deleted clause transitively. Rebuild from
  // live units, never keep a deleted unit or reason as an implicit premise.
  if (!trail_.empty() || root_conflict_) rebuild_root_=true;
  return true;
}
} // namespace tactix
