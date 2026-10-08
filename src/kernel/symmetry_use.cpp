// SymmetryUse bookkeeping (observation only; see kernel.hpp). Kept out of
// kernel.cpp so that translation unit's inlining of the propagation engine is
// unchanged by it.
#include "kernel/kernel.hpp"

#include <algorithm>

namespace tactix {

static Lit symuse_internal(const Kernel &k, Lit external) {
  const Var ev = var_of(external);
  if (ev >= k.external_to_internal.size())
    return INVALID_LIT;
  const Var iv = k.external_to_internal[ev];
  return iv == INVALID_VAR ? INVALID_LIT : make_lit(iv, negated(external));
}

static void symuse_mark(Kernel &k, Lit lit, uint8_t bit) {
  std::vector<uint8_t> &marks = k.symuse.lit_marks;
  if (lit >= marks.size())
    marks.resize(std::max<size_t>(size_t{lit} + 1, 2 * size_t{k.nvars}), 0);
  marks[lit] |= bit;
}

static void symuse_index_binary(Kernel &k, const SymmetryUse::StableBinary &b) {
  const Lit a = symuse_internal(k, b.a), c = symuse_internal(k, b.b);
  if (a == INVALID_LIT || c == INVALID_LIT)
    return;
  symuse_mark(k, a, SymmetryUse::BINARY_MARK);
  symuse_mark(k, c, SymmetryUse::BINARY_MARK);
  k.symuse.binary_tags.insert_or_assign(kernel_binary_key(a, c), b.tag);
}

static void symuse_index_unit(Kernel &k, const SymmetryUse::StableUnit &u) {
  const Lit lit = symuse_internal(k, u.lit);
  if (lit == INVALID_LIT)
    return;
  symuse_mark(k, negate(lit), SymmetryUse::UNIT_MARK);
  k.symuse.unit_tags.insert_or_assign(lit, u.tag);
}

static uint32_t symuse_binary_tag(const Kernel &k, Lit a, Lit b) {
  const std::vector<uint8_t> &marks = k.symuse.lit_marks;
  if (a >= marks.size() || b >= marks.size() ||
      !(marks[a] & marks[b] & SymmetryUse::BINARY_MARK))
    return 0;
  return static_cast<uint32_t>(
      k.symuse.binary_tags.get(kernel_binary_key(a, b)));
}

static uint32_t symuse_large_tag(const Kernel &k, uint64_t id) {
  if (id < k.symuse.large_min || id > k.symuse.large_max)
    return 0;
  return static_cast<uint32_t>(k.symuse.large_tags.get(id));
}

static SymmetryUseRun &symuse_run(Kernel &k, uint32_t tag) {
  if (tag & SymmetryUse::AFFINE) {
    assert((tag >> SymmetryUse::AFFINE_SHIFT) < k.symuse.affine_runs.size());
    return k.symuse.affine_runs[tag >> SymmetryUse::AFFINE_SHIFT];
  }
  assert((tag & 0xffu) && (tag & 0xffu) <= k.symuse.runs.size());
  return k.symuse.runs[(tag & 0xffu) - 1];
}

static void symuse_tag_with(Kernel &k, const Lit *lits, uint32_t size,
                            uint64_t id, uint32_t tag, bool helper) {
  assert(size > 0);
  SymmetryUse &u = k.symuse;
  SymmetryUseRun &run = symuse_run(k, tag);
  run.clauses++;
  run.helpers += helper;
  if (size == 1) {
    run.units++;
    u.has_units = true;
    u.units.push_back({kernel_external_lit(k, lits[0]), tag});
    symuse_index_unit(k, u.units.back());
  } else if (size == 2) {
    run.binaries++;
    u.binaries.push_back({kernel_external_lit(k, lits[0]),
                          kernel_external_lit(k, lits[1]), tag});
    symuse_index_binary(k, u.binaries.back());
  } else {
    run.large++;
    u.live_large++;
    u.large_tags.insert_or_assign(id, tag);
    u.large_min = std::min(u.large_min, id);
    u.large_max = std::max(u.large_max, id);
  }
  k.symuse_active = true;
}

void kernel_symuse_tag(Kernel &k, const Lit *lits, uint32_t size, uint64_t id,
                       bool helper) {
  SymmetryUse &u = k.symuse;
  if (u.runs.size() <= u.run)
    u.runs.resize(size_t{u.run} + 1);
  const uint32_t tag = (uint32_t{u.run} + 1) |
                       (helper ? static_cast<uint32_t>(SymmetryUse::HELPER) : 0);
  symuse_tag_with(k, lits, size, id, tag, helper);
}

void kernel_symuse_tag_affine(Kernel &k, const Lit *lits, uint32_t size,
                              uint64_t id, uint64_t epoch) {
  const uint32_t e = static_cast<uint32_t>(
      std::min<uint64_t>(epoch, SymmetryUse::AFFINE_EPOCHS - 1));
  if (k.symuse.affine_runs.size() <= e)
    k.symuse.affine_runs.resize(size_t{e} + 1);
  symuse_tag_with(k, lits, size, id,
                  static_cast<uint32_t>(SymmetryUse::AFFINE) |
                      (e << SymmetryUse::AFFINE_SHIFT),
                  false);
}

uint64_t kernel_symuse_affine_uses(const Kernel &k, uint64_t epoch) {
  const uint64_t e = std::min<uint64_t>(epoch, SymmetryUse::AFFINE_EPOCHS - 1);
  if (e >= k.symuse.affine_runs.size())
    return 0;
  const SymmetryUseRun &r = k.symuse.affine_runs[e];
  return r.root_forced + r.analysis_uses + r.search_propagations;
}

static void symuse_note_assign(Kernel &k, Lit lit, uint32_t level,
                               uint32_t reason, bool binary, bool jumped) {
  const std::vector<uint8_t> &marks = k.symuse.lit_marks;
  uint32_t tag = 0, unit_tag = 0;
  if (binary) {
    // A jumped assignment's own clause is (lit, jumped_over); its reason field
    // names the jumped-over literal's antecedent instead. Both a binary tag
    // and a root-forced attribution need `other` marked.
    const Lit other = jumped ? k.jumped_over[var_of(lit)] : reason;
    if (other >= marks.size() || !marks[other])
      return;
    tag = symuse_binary_tag(k, lit, other);
    if (!level && other < marks.size() &&
        (marks[other] & SymmetryUse::UNIT_MARK))
      unit_tag = static_cast<uint32_t>(k.symuse.unit_tags.get(negate(other)));
  } else {
    if (reason == DECISION_REASON || reason == UNIT_REASON)
      return;
    if (level && !k.symuse.large_max)
      return;
    const Clause *c = clause_at(k.arena, reason);
    tag = symuse_large_tag(k, c->id());
    if (!level && k.symuse.has_units)
      for (const Lit m : *c)
        if (m != lit && m < marks.size() &&
            (marks[m] & SymmetryUse::UNIT_MARK)) {
          unit_tag = static_cast<uint32_t>(k.symuse.unit_tags.get(negate(m)));
          break;
        }
  }
  if (tag) {
    SymmetryUseRun &run = symuse_run(k, tag);
    run.propagations++;
    run.helper_propagations += (tag & SymmetryUse::HELPER) != 0;
    run.search_propagations +=
        !k.inprocessing && !k.warming && !k.probing.recording &&
        (level || !(tag & SymmetryUse::AFFINE));
    if (tag & SymmetryUse::AFFINE)
      return; // affine reasons never credit the units they mention
  }
  if (unit_tag)
    symuse_run(k, unit_tag).root_forced++;
}

void kernel_symuse_note_trail(Kernel &k, size_t from) {
  // With no tagged binary and no live tagged large clause, nothing can be a
  // tagged reason; only root-forced attribution remains, observed at the root.
  const SymmetryUse &u = k.symuse;
  if (u.binaries.empty() && !u.live_large && (k.level || !u.has_units))
    return;
  for (size_t i = from; i < k.trail.size(); i++) {
    const Lit lit = k.trail[i];
    const Assigned &a = k.assigned[var_of(lit)];
    symuse_note_assign(k, lit, a.level, a.reason, a.binary, a.jumped);
  }
}

void kernel_symuse_note_binary_use(Kernel &k, Lit a, Lit b) {
  if (const uint32_t tag = symuse_binary_tag(k, a, b)) {
    SymmetryUseRun &run = symuse_run(k, tag);
    run.analysis_uses++;
    run.helper_analysis_uses += (tag & SymmetryUse::HELPER) != 0;
  }
}

void kernel_symuse_note_large_use(Kernel &k, uint64_t id) {
  if (const uint32_t tag = symuse_large_tag(k, id)) {
    SymmetryUseRun &run = symuse_run(k, tag);
    run.analysis_uses++;
    run.helper_analysis_uses += (tag & SymmetryUse::HELPER) != 0;
  }
}

void kernel_symuse_note_delete(Kernel &k, uint64_t id) {
  if (const uint32_t tag = symuse_large_tag(k, id)) {
    k.symuse.large_tags.insert_or_assign(id, 0);
    symuse_run(k, tag).large_deleted++;
    k.symuse.live_large--;
  }
}

void kernel_symuse_rebuild_filters(Kernel &k) {
  SymmetryUse &u = k.symuse;
  u.lit_marks.assign(2 * size_t{k.nvars}, 0);
  u.binary_tags = BinaryIdMap{};
  u.unit_tags = BinaryIdMap{};
  for (const SymmetryUse::StableBinary &b : u.binaries)
    symuse_index_binary(k, b);
  for (const SymmetryUse::StableUnit &unit : u.units)
    symuse_index_unit(k, unit);
}

uint64_t kernel_symuse_alive(const Kernel &k, uint32_t run) {
  if (run >= k.symuse.runs.size())
    return 0;
  const SymmetryUseRun &r = k.symuse.runs[run];
  uint64_t alive = r.units + r.large - r.large_deleted;
  for (const SymmetryUse::StableBinary &b : k.symuse.binaries) {
    if ((b.tag & 0xffu) != run + 1)
      continue;
    const Lit a = symuse_internal(k, b.a), c = symuse_internal(k, b.b);
    alive += a != INVALID_LIT && c != INVALID_LIT && kernel_binary_live(k, a, c);
  }
  return alive;
}

} // namespace tactix
