#include "rules/semantic_extension/nary_csp_relation_owner.hpp"

#include "rules/semantic_extension/csp_exact_theory.hpp"
#include "rules/semantic_extension/csp_physical_scan.hpp"

#include <algorithm>

namespace tactix {
namespace {

using csp_exact_theory::add_count;

bool owner_less(const NaryCspRelationOwnerSnapshot &first,
                const NaryCspRelationOwnerSnapshot &second) {
  const auto &left = first.relation.domains;
  const auto &right = second.relation.domains;
  const size_t common = std::min(left.size(), right.size());
  for (size_t index = 0; index < common; ++index) {
    if (left[index].values != right[index].values)
      return left[index].values < right[index].values;
  }
  return left.size() < right.size();
}

bool owner_work(const NaryCspRelationOwnerSnapshot &owner, uint64_t &work) {
  uint64_t result = 1;
  for (const auto &premise : owner.source)
    if (!add_count(uint64_t{1} + premise.literals.size(), result))
      return false;
  for (const auto &domain : owner.relation.domains) {
    if (!add_count(2, result) ||
        !add_count(domain.values.size(), result) ||
        !add_count(domain.at_most_one.size(), result))
      return false;
  }
  for (const auto &tuple : owner.relation.forbidden)
    if (!add_count(uint64_t{1} + tuple.value_indices.size(), result))
      return false;
  work = result;
  return true;
}

bool session_work(
    const std::vector<NaryCspRelationOwnerSnapshot> &snapshots,
    uint64_t &work) {
  uint64_t result = 1;
  for (const auto &snapshot : snapshots) {
    uint64_t owner = 0;
    if (!owner_work(snapshot, owner) || !add_count(owner, result))
      return false;
  }
  work = result;
  return true;
}

} // namespace

const NaryCspRelationOwnerSnapshot *NaryCspRelationOwnerSession::snapshot(
    const std::vector<std::vector<int32_t>> &domains) const {
  for (const auto &owner : snapshots_) {
    if (owner.relation.domains.size() != domains.size()) continue;
    bool match = true;
    for (size_t index = 0; index < domains.size(); ++index)
      match &= owner.relation.domains[index].values == domains[index];
    if (match) return &owner;
  }
  return nullptr;
}

NaryCspRelationCaptureResult nary_csp_relation_prepare_owner(
    Kernel &kernel, const NaryCspRelationOwnerSession &session,
    const NaryCspRelationCaptureRequest &request,
    const NaryCspRelationSessionLimits &limits,
    NaryCspRelationOwnerSession &output) {
  NaryCspRelationCaptureResult result;
  if (!request.valid() || !limits.valid()) return result;
  if (session.retained() && !session.belongs_to(kernel)) {
    result.status = NaryCspRelationCaptureStatus::Stale;
    return result;
  }

  const auto *previous = session.snapshot(request.domains);
  const csp_physical_scan::FormulaMark before =
      csp_physical_scan::formula_mark(kernel);
  NaryCspRelationOwnerSnapshot captured;
  result = nary_csp_relation_capture_owner(kernel, request, captured);
  // Declines report the unchanged session's owner count and work.
  const auto decline = [&](NaryCspRelationCaptureStatus status) {
    result.status = status;
    result.retained_owners = session.retained_owners();
    result.session_work = session.session_work();
    return result;
  };
  if (result.status != NaryCspRelationCaptureStatus::Applied)
    return decline(
        previous &&
                (result.status == NaryCspRelationCaptureStatus::NoStructure ||
                 result.status == NaryCspRelationCaptureStatus::Stale)
            ? NaryCspRelationCaptureStatus::Stale
            : result.status);
  if (previous && !same_authenticated_image(*previous, captured))
    return decline(NaryCspRelationCaptureStatus::Stale);
  if (previous) return decline(NaryCspRelationCaptureStatus::Duplicate);
  if (session.version_ == UINT64_MAX ||
      session.snapshots_.size() >= limits.maximum_owners)
    return decline(NaryCspRelationCaptureStatus::ResourceExhausted);

  uint64_t retained_work = 0;
  uint64_t added_work = 0;
  if (!session_work(session.snapshots_, retained_work) ||
      !owner_work(captured, added_work) ||
      !add_count(added_work, retained_work)) {
    result.status = NaryCspRelationCaptureStatus::ResourceExhausted;
    return result;
  }
  result.session_work = retained_work;
  if (retained_work > limits.maximum_work) {
    result.status = NaryCspRelationCaptureStatus::ResourceExhausted;
    result.retained_owners = session.retained_owners();
    return result;
  }

  NaryCspRelationOwnerSession staged;
  staged.owner_ = &kernel;
  staged.version_ = session.version_ + 1;
  staged.session_work_ = retained_work;
  staged.snapshots_ = session.snapshots_;
  staged.snapshots_.push_back(std::move(captured));
  std::sort(staged.snapshots_.begin(), staged.snapshots_.end(), owner_less);

  if (kernel_terminating(kernel))
    return decline(NaryCspRelationCaptureStatus::Interrupted);
  if (csp_physical_scan::formula_mark(kernel) != before)
    return decline(NaryCspRelationCaptureStatus::Stale);

  result.status = NaryCspRelationCaptureStatus::Applied;
  result.retained_owners = staged.retained_owners();
  result.owner_retained = true;
  output = std::move(staged);
  return result;
}

} // namespace tactix
