#include "rules/congruence/congruence.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp"   // inprocess_reach_root
#include "rules/substitute/substitute.hpp" // big_path_chain (bridged LRAT chains)

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {

namespace {

// Arity cap: an AND gate over more inputs is skipped (kissat caps too), which
// bounds extraction and the equivalence proof chain.
constexpr uint32_t CONG_ARITY_LIMIT = 64;
// safety cap on gates extracted per round.
constexpr size_t CONG_GATE_LIMIT = 20000000;
// Max inputs of an extracted XOR gate; the match proof chain is O(2^arity), so
// it stays small (kissat --congruencexorarity). An arity-a XOR is a size-(a+1)
// clause group, hence the 3..CONG_XOR_ARITY+1 scan range.
constexpr uint32_t CONG_XOR_ARITY = 4;

// --- the meter and the mid-round budget --------------------------------------
// `stats.congruence_ticks` is charged at every scan site, so the policy can
// price a round in the rule's own currency. The stop predicate is consulted
// only where `kernel_terminating` already is, inheriting that soundness
// argument; `work_budget == 0` is unbounded. Round-scoped state, Congruence
// being one canonical round with no reentrancy. Design
// `docs/design/2026-08-05-g1-congruence-governor-design.md` §4.
struct Budget {
  uint64_t start = 0;
  uint64_t limit = 0; // 0 = unbounded
  bool cut = false; // set at whichever stop actually fired; counted once/round
  bool over(const Kernel &k) {
    if (!limit || k.stats.congruence_ticks - start <= limit)
      return false;
    cut = true;
    return true;
  }
};
Budget cong_budget;
// Scoped so every exit path leaves the budget cleared for the next caller.
struct BudgetScope {
  const Kernel &k;
  explicit BudgetScope(const Kernel &kk) : k(kk) {
    cong_budget = {kk.stats.congruence_ticks, kk.work_budget};
  }
  ~BudgetScope() {
    if (cong_budget.cut)
      k.stats.congruence_budget_stops++; // once per cut round, not per site
    cong_budget = {};
  }
};

// Mark the binary-watch partners of `lit`.
template <int V> void mark_bin_partners(const Kernel &k, Lit lit,
                                        std::vector<uint8_t> &mark) {
  for (const WatchEntry w : k.watches.entries(lit))
    if (w.binary())
      mark[w.other()] = V;
}

} // namespace

bool congruence_guard(const Kernel &k) {
  return !k.inconsistent && k.level == 0;
}

void congruence_extract(const Kernel &k, std::vector<CongGate> &out) {
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint8_t> mark(2 * k.nvars, 0); // binary partners of ¬output
  // AND gate `out = AND(inputs)`: a long base clause holding `out` where every
  // other literal l has the binary (¬out ∨ ¬l), so input = ¬l; OR is the dual,
  // found by trying every literal as the output. `pairs` is reused and the
  // `reserve`s exact — one malloc per gate is a measurable share of the solve.
  std::vector<std::pair<Lit, uint64_t>> pairs;
  const auto emit = [&](const Clause *c, Lit outp) {
    const Lit nout = negate(outp);
    CongGate g;
    g.output = outp;
    g.base_id = c->id();
    g.inputs.reserve(c->size - 1);
    if (lrat)
      g.input_bin_ids.reserve(c->size - 1);
    for (const Lit l : *c)
      if (l != outp) {
        const Lit inp = negate(l);
        g.inputs.push_back(inp);
        if (lrat)
          g.input_bin_ids.push_back(kernel_binary_id(k, nout, inp));
      }
    // canonicalize inputs (the structural key) — keep bin_ids parallel
    if (lrat) {
      pairs.clear();
      pairs.resize(g.inputs.size());
      for (size_t i = 0; i < g.inputs.size(); i++)
        pairs[i] = {g.inputs[i], g.input_bin_ids[i]};
      std::sort(pairs.begin(), pairs.end());
      for (size_t i = 0; i < pairs.size(); i++) {
        g.inputs[i] = pairs[i].first;
        g.input_bin_ids[i] = pairs[i].second;
      }
    } else {
      std::sort(g.inputs.begin(), g.inputs.end());
    }
    out.push_back(std::move(g));
  };
  // "candidate (c, i) is tried" ≡ lits[0..i] all unassigned: the clause-major
  // path breaks at the first assigned literal, and the literal-major path must
  // reproduce that exactly.
  if (k.dense) {
    // Literal-major over the phase occ index: mark ¬out's binary partners once
    // per output literal, not once per clause-occurrence. The tag sort below
    // restores the exact clause-major emission sequence — a hard constraint,
    // since merge order feeds the search trajectory.
    struct Tag {
      Ref ref;
      uint32_t idx;  // outp's index within the clause
      uint32_t gate; // index into `out` where the gate was appended
    };
    std::vector<Tag> tags;
    const size_t base = out.size();
    const Lit nlits = 2 * k.nvars;
    for (Lit outp = 0; outp < nlits; outp++) {
      // Read-only work: the gates already emitted stay valid, and the closure
      // simply sees fewer of them.
      if (!(outp & 255) && cong_budget.over(k))
        break;
      const std::vector<Ref> &occ = k.dense->large[outp];
      if (occ.empty())
        continue;
      const Lit nout = negate(outp);
      bool marked = false;
      k.stats.congruence_ticks += occ.size();
      for (const Ref ref : occ) {
        const Clause *c = clause_at(k.arena, ref);
        if (c->garbage() || c->redundant() || c->size < 3 ||
            c->size > CONG_ARITY_LIMIT + 1)
          continue;
        k.stats.congruence_ticks += c->size;
        // locate outp, verifying the unassigned prefix up to it
        uint32_t idx = UINT32_MAX;
        for (uint32_t j = 0; j < c->size; j++) {
          if (k.value(c->lits[j]) != 0)
            break; // clause-major would have stopped here
          if (c->lits[j] == outp) {
            idx = j;
            break;
          }
        }
        if (idx == UINT32_MAX)
          continue; // assigned prefix, or a stale occ entry
        if (!marked) {
          mark_bin_partners<1>(k, nout, mark);
          marked = true;
        }
        bool all = true;
        for (const Lit l : *c) {
          if (l == outp)
            continue;
          if (!mark[negate(l)]) { // need binary (¬out ∨ ¬l)
            all = false;
            break;
          }
        }
        if (!all)
          continue;
        tags.push_back({ref, idx, static_cast<uint32_t>(out.size())});
        emit(c, outp);
      }
      if (marked)
        mark_bin_partners<0>(k, nout, mark); // unmark once per literal
    }
    // restore clause-major order: (arena ref, candidate index within clause)
    std::sort(tags.begin(), tags.end(), [](const Tag &a, const Tag &b) {
      return a.ref != b.ref ? a.ref < b.ref : a.idx < b.idx;
    });
    std::vector<CongGate> ordered;
    ordered.reserve(tags.size());
    // clause-major CONG_GATE_LIMIT semantics: checked before each new clause
    for (size_t i = 0; i < tags.size();) {
      if (base + ordered.size() >= CONG_GATE_LIMIT)
        break;
      const Ref r = tags[i].ref;
      for (; i < tags.size() && tags[i].ref == r; i++)
        ordered.push_back(std::move(out[tags[i].gate]));
    }
    out.resize(base);
    for (CongGate &g : ordered)
      out.push_back(std::move(g));
    return;
  }
  // Extraction is read-only, so aborting is free: the caller returns NoOp.
  uint32_t xtick = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only work, fewer gates, sound
    }
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    ref += words;
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_ARITY_LIMIT + 1)
      continue;
    if (out.size() >= CONG_GATE_LIMIT)
      return;
    k.stats.congruence_ticks += c->size;
    for (const Lit outp : *c) {
      if (k.value(outp) != 0)
        break; // clause has an assigned literal → not a clean gate base
      const Lit nout = negate(outp);
      mark_bin_partners<1>(k, nout, mark);
      bool all = true;
      for (const Lit l : *c) {
        if (l == outp)
          continue;
        if (!mark[negate(l)]) { // need binary (¬out ∨ ¬l)
          all = false;
          break;
        }
      }
      mark_bin_partners<0>(k, nout, mark); // unmark
      if (!all)
        continue;
      emit(c, outp);
    }
  }
}

namespace {

struct InputsHash {
  size_t operator()(const std::vector<Lit> &v) const {
    size_t h = 1469598103934665603ull;
    for (const Lit l : v) {
      h ^= static_cast<size_t>(l);
      h *= 1099511628211ull;
    }
    return h;
  }
};

// Fixed-capacity key for the small-clause index and the XOR/ITE dedup sets: no
// per-key heap node, no vector hashing. Semantics match a std::vector key's — a
// sorted key strictly increases, so the zero padding never aliases a literal.
struct SmallKey {
  Lit lits[CONG_XOR_ARITY + 1] = {0, 0, 0, 0, 0};
  uint32_t size = 0;
  bool operator==(const SmallKey &o) const {
    if (size != o.size)
      return false;
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++)
      if (lits[i] != o.lits[i])
        return false;
    return true;
  }
  bool operator<(const SmallKey &o) const {
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++)
      if (lits[i] != o.lits[i])
        return lits[i] < o.lits[i];
    return size < o.size; // unreachable under the strict-increase invariant
  }
};
struct SmallKeyHash {
  size_t operator()(const SmallKey &sk) const {
    size_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < CONG_XOR_ARITY + 1; i++) {
      h ^= static_cast<size_t>(sk.lits[i]);
      h *= 1099511628211ull;
    }
    h ^= sk.size;
    h *= 1099511628211ull;
    return h;
  }
};
// sorted key from ≤ CONG_XOR_ARITY+1 literals
inline SmallKey small_key(const Lit *ls, uint32_t n) {
  SmallKey sk;
  sk.size = n;
  for (uint32_t i = 0; i < n; i++) {
    const Lit x = ls[i];
    uint32_t j = i;
    for (; j > 0 && sk.lits[j - 1] > x; j--)
      sk.lits[j] = sk.lits[j - 1];
    sk.lits[j] = x;
  }
  return sk;
}

// Flat open-addressed SmallKey set — the clause index and both extraction dedup
// sets. A set, not a map: XOR and ITE only ask "does this clause exist", and
// keys live in the bucket array so a lookup touches one cache line. `size == 0`
// is the empty marker; iteration order is hash-dependent, see `sorted_keys`.
class SmallKeySet {
public:
  SmallKeySet() : buckets_(64), mask_(63) {}
  size_t size() const { return size_; }
  const std::vector<SmallKey> &buckets() const { return buckets_; }
  bool contains(const SmallKey &sk) const {
    return buckets_[slot(sk)].size != 0;
  }
  // true iff `sk` was not already present (std::unordered_set::insert().second)
  bool insert(const SmallKey &sk) {
    if ((size_ + 1) * 10 > (static_cast<size_t>(mask_) + 1) * 7)
      grow();
    const uint32_t b = slot(sk);
    if (buckets_[b].size != 0)
      return false;
    buckets_[b] = sk;
    size_++;
    return true;
  }

private:
  // FNV's low bits mix poorly across key positions, which linear probing
  // punishes; finalize with the murmur3 avalanche. Bucket placement only.
  static uint32_t bucket_hash(const SmallKey &sk) {
    uint64_t h = SmallKeyHash{}(sk);
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 29;
    return static_cast<uint32_t>(h);
  }
  // the bucket holding `sk`, or the empty one it would take (load < 70 %, so
  // the probe always terminates)
  uint32_t slot(const SmallKey &sk) const {
    uint32_t b = bucket_hash(sk) & mask_;
    while (buckets_[b].size != 0 && !(buckets_[b] == sk))
      b = (b + 1) & mask_;
    return b;
  }
  void grow() {
    std::vector<SmallKey> old;
    old.swap(buckets_);
    buckets_.assign(2 * old.size(), SmallKey{});
    mask_ = static_cast<uint32_t>(buckets_.size() - 1);
    for (const SmallKey &sk : old)
      if (sk.size != 0)
        buckets_[slot(sk)] = sk;
  }
  std::vector<SmallKey> buckets_;
  uint32_t mask_;
  size_t size_ = 0;
};

using ClauseIndex = SmallKeySet;

// Iterating a hash table is implementation-ordered, and gate emission order
// feeds the closure's merge order and hence the search trajectory, which must
// not fork across toolchains. `operator<` totally orders stored keys, so this
// is independent of the table's. Built once per apply, by value.
static std::vector<SmallKey> sorted_keys(const Kernel &k,
                                         const ClauseIndex &index) {
  std::vector<SmallKey> keys;
  keys.reserve(index.size());
  for (const SmallKey &sk : index.buckets())
    if (sk.size != 0)
      keys.push_back(sk);
  k.stats.congruence_ticks += index.buckets().size() + keys.size();
  std::sort(keys.begin(), keys.end());
  return keys;
}

// Index every irredundant, fully-unassigned clause of size 3..CONG_XOR_ARITY+1
// by its sorted literals. Gate clauses are never rewritten within an apply, so
// one snapshot is valid for the whole closure.
void build_clause_index(const Kernel &k, ClauseIndex &index) {
  std::vector<Lit> ls;
  // Extraction is read-only, so aborting is free: the caller returns NoOp.
  uint32_t xtick = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // a truncated index only ever loses gates: XOR and ITE both
                // require every defining clause to be present, so a missing
                // entry can suppress a gate but never fabricate one.
    }
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_XOR_ARITY + 1)
      continue;
    k.stats.congruence_ticks += c->size;
    bool ok = true;
    ls.clear();
    for (const Lit l : *c) {
      if (k.value(l) != 0) {
        ok = false;
        break;
      }
      ls.push_back(l);
    }
    if (!ok)
      continue;
    index.insert(small_key(ls.data(), static_cast<uint32_t>(ls.size())));
  }
}

// A size-s clause group over {v0..v_{s-1}} is the XOR relation v0⊕…⊕v_{s-1} = c
// only when all 2^{s-1} clauses of one fixed sign parity are present; less is
// not the relation, and the gate would be unsound (`docs/calculus.md` V6). It
// yields s gates, one per output vᵢ, with the other variables as inputs.
void congruence_extract_xor(const Kernel &k, const ClauseIndex &index,
                            const std::vector<SmallKey> &sorted,
                            std::vector<CongGate> &out) {
  (void)k;
  SmallKeySet seen;     // by sorted positive vars
  std::vector<Lit> pos; // positive lits, sorted
  Lit probe[CONG_XOR_ARITY + 1];
  uint32_t xtick = 0;
  for (const SmallKey &base : sorted) {
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only, fewer gates
    }
    const uint32_t s = base.size;
    k.stats.congruence_ticks += s;
    // positive-lit form of the variables, sorted; dedup a relation to once.
    pos.clear();
    int base_parity = 0;
    for (uint32_t bi = 0; bi < s; bi++) {
      const Lit l = base.lits[bi];
      pos.push_back(make_lit(var_of(l), false));
      base_parity ^= static_cast<int>(negated(l));
    }
    std::sort(pos.begin(), pos.end());
    bool dup = false; // a repeated variable ⇒ not a clean relation
    for (uint32_t i = 1; i < s; i++)
      if (pos[i] == pos[i - 1]) {
        dup = true;
        break;
      }
    if (dup)
      continue;
    const SmallKey pos_key = small_key(pos.data(), s);
    if (seen.contains(pos_key))
      continue;
    // require all 2^{s-1} clauses of parity == base_parity to exist.
    bool all = true;
    const uint32_t combos = 1u << (s - 1);
    k.stats.congruence_ticks += combos;
    for (uint32_t m = 0; m < combos && all; m++) {
      int par = 0;
      for (uint32_t i = 0; i + 1 < s; i++) {
        const int sign = (m >> i) & 1;
        par ^= sign;
        probe[i] = pos[i] ^ static_cast<Lit>(sign);
      }
      probe[s - 1] = pos[s - 1] ^ static_cast<Lit>(base_parity ^ par); // fix parity
      if (!index.contains(small_key(probe, s)))
        all = false;
    }
    if (!all)
      continue;
    seen.insert(pos_key);
    // The relation's parity R = base_parity ⊕ 1, so for output vᵢ the output
    // literal is pos[i] flipped by R. Never also XOR in `s`: that negates every
    // even-arity gate's output, invisibly to XOR↔XOR key matching but not to any
    // unit/merge channel reading the output's polarity.
    const Lit r = static_cast<Lit>(base_parity ^ 1);
    for (uint32_t i = 0; i < s; i++) {
      CongGate g;
      g.kind = GateKind::Xor;
      g.output = pos[i] ^ r; // value = R ⊕ vᵢ = ⊕_{j≠i} vⱼ
      g.inputs.reserve(s - 1); // exact: push_back's growth costs 2-3 mallocs
      for (uint32_t j = 0; j < s; j++)
        if (j != i)
          g.inputs.push_back(pos[j]); // all positive, already sorted
      out.push_back(std::move(g));
    }
  }
}

bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l); // defined below

// A ladder lemma: RUP-checkable, cited by the level below it, dead once the
// conclusion is in, and kept so the round can `del` it. LRAT deletes by id;
// DRAT needs the literals.
struct TransientLemma {
  uint64_t id;
  std::vector<Lit> lits;
};

// Drop the transient ladder lemmas, newest first.
void delete_transients(Kernel &k, std::vector<TransientLemma> &t) {
  if (k.proof)
    for (size_t i = t.size(); i-- > 0;)
      k.proof->del(k.next_clause_id, t[i].id, t[i].lits.data(),
                   static_cast<uint32_t>(t[i].lits.size()));
  t.clear();
}

// TACTIX_CONG_PROBE: count the degenerate-gate discards by cause — the
// inferences this rule does not draw. Env-gated, default-identical.
static const bool cong_probe = std::getenv("TACTIX_CONG_PROBE") != nullptr;
static uint64_t cong_n_and = 0, cong_n_ite = 0, cong_n_xor = 0, cong_n_clash = 0;
// Accepted merges whose representative is already root-assigned — see val_merge.
static uint64_t cong_val_merge = 0;
// Root-assigned literals seen by the closure at apply entry / exit.
static uint64_t cong_root_in = 0, cong_root_out = 0;

// TACTIX_CONG_PROBE, part 2 — the closure/matching discriminator, per gate
// kind: a funnel from canon_ok through insert / collide to merged, with stale,
// already_eq, clash and chain_fail the ways a collision fails to merge.
// `collide` ≈ 0 against a large `insert` puts the fault in canonicalization; a
// large `collide` with a small `merged` puts it downstream.
struct CongMatchStats {
  uint64_t canon_ok = 0, canon_skip = 0, canon_unit = 0, canon_merge = 0;
  uint64_t insert = 0, collide = 0, stale = 0, already_eq = 0;
  uint64_t clash = 0, chain_fail = 0, merged = 0;
  // The missing value channel: kissat's closure keeps `values[]` live, shrinking
  // a valued gate and turning a merge with a valued participant into a unit.
  // This closure reads no value, so these columns size the gap.
  uint64_t val_gate = 0, val_merge = 0;
};
static CongMatchStats cong_match[3]; // indexed by GateKind

// TACTIX_CONG_TIME: where one application's wall clock goes, by stage — the
// split that says which currency a governor should charge.
static const bool cong_time = std::getenv("TACTIX_CONG_TIME") != nullptr;
struct CongTime {
  uint64_t extract_and = 0, index = 0, sort = 0, xor_x = 0, ite_x = 0;
  uint64_t canon = 0, table = 0, proof = 0, closure_all = 0, apply_all = 0;
  uint64_t items = 0;
};
static CongTime cong_t;
// RAII stage accumulator; when the probe is off it never reads the clock.
struct Stage {
  uint64_t *sink;
  uint64_t t0;
  explicit Stage(uint64_t *s) : sink(s), t0(cong_time ? now_nanos() : 0) {}
  ~Stage() {
    if (cong_time)
      *sink += now_nanos() - t0;
  }
};

// What canonicalizing a gate under the current representatives yielded. A
// degenerate gate is not a dead end: each degeneracy is itself an inference
// over F (`docs/calculus.md` V6).
enum class Canon {
  Ok,    // `key` + `out_lit` valid — file the gate in the table
  Unit,  // the gate forces `out_lit` to a constant: the root unit is ¬out_lit
  Merge, // the gate collapsed to out_lit ≡ key[0] — no second gate needed
  Skip,  // degenerate in a way this rule does not exploit (ITE)
};

// Canonicalize gate g under the current union-find into (key, out_lit): key the
// structural hash, out_lit the literal whose class is merged on a key collision
// and whose value is f(key). `cancelled` receives the positive representatives
// that dropped out of an XOR key, which `add_xor_degeneracy_chain`'s ladder
// quantifies over. `reps` is caller-owned scratch and `Find` a template
// parameter for the same reason: this is the rule's hottest loop.
template <class Find>
Canon canonicalize(const CongGate &g, Find &find, std::vector<Lit> &key,
                   Lit &out_lit, std::vector<Lit> &cancelled,
                   std::vector<Lit> &reps) {
  cancelled.clear();
  if (g.kind == GateKind::And) {
    key.assign(g.inputs.begin(), g.inputs.end());
    for (Lit &l : key)
      l = find(l);
    std::sort(key.begin(), key.end());
    out_lit = g.output;
    // AND degeneracies: AND(…,x,¬x,…) = 0 gives the root unit ¬out, and
    // AND(…,x,x,…) deduplicates, possibly down to the unary gate out ≡ x. These
    // are directly RUP over the live DB, unlike the XOR parity cases, so
    // `cancelled` stays empty and the ladder collapses to its top==0 form.
    {
      size_t n = 0;
      for (size_t j = 0; j < key.size(); j++) {
        if (n && key[j] == key[n - 1]) {
          if (cong_probe) cong_n_and++;
          continue; // idempotent duplicate: same representative twice
        }
        if (n && key[j] == negate(key[n - 1])) {
          if (cong_probe) cong_n_and++;
          return Canon::Unit; // clashing inputs: value(out_lit) is constant 0
        }
        key[n++] = key[j];
      }
      key.resize(n);
    }
    if (key.size() == 1)
      return Canon::Merge; // unary AND(x) ⇒ out_lit ≡ key[0]
    return Canon::Ok;
  }
  if (g.kind == GateKind::Ite) {
    Lit cond = find(g.inputs[0]), th = find(g.inputs[1]), el = find(g.inputs[2]);
    // Defer any ITE whose representative operands share a variable: then==else,
    // then==¬else (an XOR), or cond touching a branch all collapse it to a
    // simpler gate a later round or propagation handles. Skipping stays sound.
    if (var_of(cond) == var_of(th) || var_of(cond) == var_of(el) ||
        var_of(th) == var_of(el)) {
      if (cong_probe) cong_n_ite++;
      return Canon::Skip;
    }
    const bool neg_lhs = normalize_ite(cond, th, el);
    key = {cond, th, el}; // fixed order — the structural key
    out_lit = neg_lhs ? negate(g.output) : g.output;
    return Canon::Ok;
  }
  // XOR: map inputs to reps, absorb each rep's sign into a global parity, and
  // cancel repeated variables (x⊕x=0). The parity flips the output literal.
  int parity = 0;
  reps.clear();
  reps.reserve(g.inputs.size());
  for (const Lit in : g.inputs) {
    Lit r = find(in);
    if (negated(r)) {
      parity ^= 1;
      r = negate(r);
    }
    reps.push_back(r); // positive rep lit
  }
  std::sort(reps.begin(), reps.end());
  key.clear();
  for (size_t j = 0; j < reps.size();) {
    size_t cnt = 1;
    while (j + cnt < reps.size() && reps[j + cnt] == reps[j])
      cnt++;
    if (cnt & 1)
      key.push_back(reps[j]); // odd multiplicity survives
    else
      cancelled.push_back(reps[j]); // fully cancelled — the ladder quantifies here
    j += cnt;
  }
  // value(out_lit) = ⊕key, by construction.
  out_lit = parity ? negate(g.output) : g.output;
  if (key.empty()) {
    if (cong_probe) cong_n_xor++;
    return Canon::Unit; // every variable cancelled ⇒ out_lit is the constant 0
  }
  if (key.size() == 1)
    return Canon::Merge; // XOR(x) ⇒ out_lit ≡ x
  return Canon::Ok;
}

// --- native-LRAT chains by RUP replay ---------------------------------------
// V6's lemma families are each RUP over the current DB, but their antecedent
// chains resist hand-derivation: canonicalization folds a sign layer on top of
// the union-find bridging. Replay instead (the primitive V1 Probe and C3 Vivify
// use) and the chain comes out in the checker's own propagation order by
// construction — the one property a hand-built chain silently gets wrong.
// Returns false when the lemma is not RUP; the caller then drops the inference
// rather than write an unverifiable line.
bool congruence_rup_chain(Kernel &k, const Lit *lits, uint32_t n,
                          std::vector<uint64_t> &chain) {
  assert(k.level == 0 && !k.conflict_valid);
  chain.clear();
  kernel_probing_begin(k);
  bool conflict = false;
  Var implied = UINT32_MAX; // a lemma literal the assumptions already forced TRUE
  for (uint32_t i = 0; i < n && !conflict; i++) {
    const Lit neg = negate(lits[i]);
    const int8_t v = k.value(neg);
    if (v < 0) {
      // `lits[i]` is already true, so assuming its negation conflicts at once
      // and the reason that forced it is the refutation. Not an optimisation:
      // on a miter the ladder's own prefix propagates the conclusion, so
      // treating this as "not RUP" loses what the ladder exists for.
      implied = var_of(lits[i]);
      conflict = true;
      break;
    }
    if (v > 0)
      continue; // already implied by an earlier assumption — nothing to assume
    k.level++;
    k.frames.push_back(static_cast<uint32_t>(k.trail.size()));
    kernel_assign(k, neg, k.level, DECISION_REASON, false);
    if (kernel_propagate_probing(k))
      conflict = true;
  }
  uint64_t last = 0;
  std::vector<std::pair<uint32_t, uint64_t>> entries;
  if (conflict) {
    // Persistent walk scratch: a fresh `seen(k.nvars, 0)` per replayed lemma
    // dominates the native-LRAT path — never reintroduce it. Restored all-zero
    // in O(marked), so it is clean on entry for any kernel this thread uses.
    static thread_local std::vector<uint8_t> seen;
    static thread_local std::vector<Var> touched;
    if (seen.size() < k.nvars)
      seen.resize(k.nvars, 0);
    touched.clear();
    if (implied != UINT32_MAX)
      // the walk includes the implied literal's own reason, which sorting by
      // trail position puts last, so it closes the chain
      kernel_reason_walk(k, implied, seen, entries, &touched);
    else
      last = kernel_conflict_reason_walk(k, seen, entries, &touched);
    for (const Var tv : touched)
      seen[tv] = 0;
  }
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  kernel_probing_end(k);
  // A failed replay is a dropped inference, counted in
  // `congruence_unjustified`; a nonzero count is how a ladder bug hides.
  if (cong_probe && !conflict) {
    std::fprintf(stderr, "[rup-replay] not RUP:");
    for (uint32_t i = 0; i < n; i++)
      std::fprintf(stderr, " %d(v=%d)", externalize(lits[i]),
                   (int)k.value(lits[i]));
    std::fprintf(stderr, "\n");
  }
  if (!conflict)
    return false;
  // (trail position ascending, conflict last) — kernel.hpp's chain contract
  std::sort(entries.begin(), entries.end());
  chain.reserve(entries.size() + 1);
  for (const auto &e : entries)
    chain.push_back(e.second);
  if (last)
    chain.push_back(last);
  // An empty chain is not a proof: a trivially true lemma conflicts against its
  // own assumption, so the walk yields no antecedents and every LRAT checker
  // rejects the line. Refusing here covers all three ladders at one choke
  // point, at the cost of completeness where the line would cost N2.
  if (chain.empty())
    return false;
  return true;
}

// Degeneracy proof chain for one XOR gate (kissat's
// add_xor_shrinking_proof_chain; `docs/calculus.md` V6). A collapsed canonical
// key forces the gate's output, and that conclusion is not RUP on its own — the
// parity clauses propagate nothing until all but one literal is assigned — so
// the ladder is not optional. For size = |cancelled| … 1 it emits every sign
// pattern over cancelled[0..size-1] appended to each conclusion; the top level
// falsifies one parity clause, each lower level closes on the two above it.
bool add_xor_degeneracy_chain(Kernel &k, const std::vector<Lit> &cancelled,
                              const std::vector<std::vector<Lit>> &concl,
                              std::vector<std::vector<uint64_t>> &concl_chains,
                              std::vector<TransientLemma> &transient) {
  const bool lrat = k.proof && k.proof->lrat();
  const size_t nc = concl.size();
  concl_chains.assign(nc, {});
  if (!k.proof)
    return true;
  // cancellation removes variables in pairs, so this is ≤ 2 in practice and the
  // ladder is 2^(n+1)−2 lemmas
  assert(cancelled.size() <= CONG_XOR_ARITY / 2 + 1 && "ladder would blow up");
  const int top = static_cast<int>(cancelled.size());
  std::vector<Lit> lemma;
  std::vector<uint64_t> prev, cur, chain;

  if (top == 0) { // no cancelled variables ⇒ the conclusion stands on its own
    for (size_t c = 0; c < nc; c++)
      if (lrat && !congruence_rup_chain(k, concl[c].data(),
                                        static_cast<uint32_t>(concl[c].size()),
                                        concl_chains[c]))
        return false;
    return true;
  }

  for (int size = top; size >= 1; size--) {
    const uint32_t combos = 1u << size;
    cur.assign(static_cast<size_t>(combos) * nc, 0);
    for (uint32_t m = 0; m < combos; m++)
      for (size_t c = 0; c < nc; c++) {
        lemma.clear();
        for (int i = 0; i < size; i++)
          lemma.push_back(cancelled[static_cast<size_t>(i)] ^
                          static_cast<Lit>((m >> i) & 1));
        lemma.insert(lemma.end(), concl[c].begin(), concl[c].end());
        // A cancelled literal reappearing in the conclusion makes this rung
        // tautological, hence uncheckable; skip the merge in both formats.
        if (Proof::tautological(lemma.data(),
                                static_cast<uint32_t>(lemma.size())))
          return false;
        if (!lrat) {
          k.proof->add_rup(0, lemma.data(),
                           static_cast<uint32_t>(lemma.size()), nullptr, 0);
          continue;
        }
        if (size == top) {
          if (!congruence_rup_chain(k, lemma.data(),
                                    static_cast<uint32_t>(lemma.size()), chain))
            return false; // not RUP — emit nothing, the caller skips the merge
        } else {
          const size_t hi = static_cast<size_t>(m | (1u << size));
          chain.assign({prev[static_cast<size_t>(m) * nc + c],
                        prev[hi * nc + c]});
        }
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         chain.data(), static_cast<uint32_t>(chain.size()));
        cur[static_cast<size_t>(m) * nc + c] = id;
        transient.push_back({id, lemma}); // dropped after the merge
      }
    prev.swap(cur);
  }
  if (lrat)
    for (size_t c = 0; c < nc; c++)
      concl_chains[c] = {prev[c], prev[nc + c]}; // the two size-1 parents
  return true;
}

// XOR-match proof chain (kissat add_xor_matching_proof_chain): make the two
// equivalence binaries checkable by a parity-elimination ladder, one rhs
// variable per level, closing as the degeneracy ladder does. Returns false if
// the top level does not replay. `rhs.back()` is absorbed by the gates'
// clauses, so the ladder starts one short.
bool add_xor_matching_proof_chain(Kernel &k, const std::vector<Lit> &rhs, Lit a,
                                  Lit b, std::vector<uint64_t> &chain_fwd,
                                  std::vector<uint64_t> &chain_rev,
                                  std::vector<TransientLemma> &transient) {
  chain_fwd.clear();
  chain_rev.clear();
  if (!k.proof)
    return true;
  const bool lrat = k.proof->lrat();
  const int top = static_cast<int>(rhs.size()) - 1;
  if (top < 1) {
    // Nothing to eliminate; the binaries stand on the gates alone, which under
    // LRAT would mean empty hint chains. Replay each binary for its own chain
    // instead; a failed replay skips the merge, which is sound.
    if (!lrat)
      return true;
    const Lit fwd[2] = {negate(a), b};
    const Lit rev[2] = {a, negate(b)};
    return congruence_rup_chain(k, fwd, 2, chain_fwd) &&
           congruence_rup_chain(k, rev, 2, chain_rev);
  }
  std::vector<Lit> lemma;
  std::vector<uint64_t> prev, cur, ch;
  for (int size = top; size >= 1; size--) {
    const uint32_t combos = 1u << size;
    cur.assign(static_cast<size_t>(combos) * 2, 0);
    for (uint32_t m = 0; m < combos; m++)
      for (int dir = 0; dir < 2; dir++) {
        lemma.clear();
        for (int i = 0; i < size; i++)
          lemma.push_back(rhs[static_cast<size_t>(i)] ^
                          static_cast<Lit>((m >> i) & 1));
        if (dir == 0) {
          lemma.push_back(negate(a));
          lemma.push_back(b);
        } else {
          lemma.push_back(a);
          lemma.push_back(negate(b));
        }
        // A merged output coinciding with an rhs literal or the other output
        // makes this rung tautological or duplicate-carrying: a tautology
        // replays to an empty chain, and cake_lpr reads a repeated literal as a
        // PR witness and rejects the line. Skip the merge.
        if (Proof::tautological(lemma.data(),
                                static_cast<uint32_t>(lemma.size())))
          return false;
        if (!lrat) {
          k.proof->add_rup(0, lemma.data(),
                           static_cast<uint32_t>(lemma.size()), nullptr, 0);
          continue;
        }
        if (size == top) {
          if (!congruence_rup_chain(k, lemma.data(),
                                    static_cast<uint32_t>(lemma.size()), ch))
            return false;
        } else {
          const size_t hi = static_cast<size_t>(m | (1u << size));
          ch.assign({prev[static_cast<size_t>(m) * 2 + static_cast<size_t>(dir)],
                     prev[hi * 2 + static_cast<size_t>(dir)]});
        }
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         ch.data(), static_cast<uint32_t>(ch.size()));
        cur[static_cast<size_t>(m) * 2 + static_cast<size_t>(dir)] = id;
        transient.push_back({id, lemma});
      }
    prev.swap(cur);
  }
  if (lrat) {
    chain_fwd = {prev[0], prev[2]}; // the two size-1 parents, dir 0
    chain_rev = {prev[1], prev[3]}; // dir 1
  }
  return true;
}

// Normalize an ITE triple [cond,then,else] in place to the canonical key form
// (cond and then positive); returns whether the output must be negated.
bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l) {
  if (negated(cond)) { // cond ? t : e  ==  ¬cond ? e : t
    cond = negate(cond);
    std::swap(then_l, else_l);
  }
  if (negated(then_l)) { // ¬(cond ? ¬t : ¬e) = cond ? t : e, flipping the output
    then_l = negate(then_l);
    else_l = negate(else_l);
    return true;
  }
  return false;
}

// ITE extraction. Per-lhs conditional equivalences lhs≡w come from clause pairs
// (A∨B∨lhs)+(A∨¬B∨¬lhs) ⇒ lhs≡¬B when ¬A; one under c pairs with one under ¬c
// into a candidate. Every candidate is verified against all four defining
// ternaries before emission, so a mis-detected pair cannot yield an unsound
// gate.
void congruence_extract_ite(const Kernel &k, const ClauseIndex &index,
                            const std::vector<SmallKey> &sorted,
                            std::vector<CongGate> &out) {
  (void)k;
  auto has = [&](Lit a, Lit b, Lit c) -> bool {
    const Lit probe[3] = {a, b, c};
    return index.contains(small_key(probe, 3));
  };
  // collect conditional equivalences per lhs literal
  std::unordered_map<Lit, std::vector<std::pair<Lit, Lit>>> ce;
  uint32_t xtick = 0;
  for (const SmallKey &cl : sorted) {
    // sorted; only the first 3 literals are scanned
    if (k.stop_fuzz || !(++xtick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        return;
      }
      if (cong_budget.over(k))
        return; // budget stop — read-only, fewer gates
    }
    k.stats.congruence_ticks += 6; // two index probes per rotation, three
    for (int li = 0; li < 3; li++) {
      const Lit L = cl.lits[li], A = cl.lits[(li + 1) % 3],
                B = cl.lits[(li + 2) % 3];
      // (A∨B∨L) + (A∨¬B∨¬L) ⇒ L≡¬B when ¬A
      if (has(A, negate(B), negate(L)))
        ce[L].push_back({negate(A), negate(B)});
      // (A∨B∨L) + (¬A∨B∨¬L) ⇒ L≡¬A when ¬B
      if (has(negate(A), B, negate(L)))
        ce[L].push_back({negate(B), negate(A)});
    }
  }
  SmallKeySet seen;
  std::vector<Lit> ce_keys; // canonical order (see sorted_keys)
  ce_keys.reserve(ce.size());
  for (const auto &kv : ce)
    ce_keys.push_back(kv.first);
  std::sort(ce_keys.begin(), ce_keys.end());
  for (const Lit L : ce_keys) {
    if (cong_budget.over(k))
      return; // budget stop — read-only, fewer gates
    auto &lst = ce[L];
    if (lst.size() > 256)
      continue; // bound the O(n²) pairing on pathological hubs
    k.stats.congruence_ticks += lst.size() * lst.size() / 2 + 1;
    for (size_t i = 0; i < lst.size(); i++)
      for (size_t j = i + 1; j < lst.size(); j++) {
        const Lit c1 = lst[i].first, w1 = lst[i].second;
        const Lit c2 = lst[j].first, w2 = lst[j].second;
        if (c1 != negate(c2))
          continue; // need complementary conditions
        Lit cond, then_l, else_l;
        if (!negated(c1)) { // c1 is the positive condition
          cond = c1;
          then_l = w1;
          else_l = w2;
        } else {
          cond = c2;
          then_l = w2;
          else_l = w1;
        }
        // A candidate with else ≡ ¬then is a ternary XOR in an ITE costume, and
        // XOR extraction already finds that relation from the same clauses. The
        // ITE copies could only ever be discarded, since `unite` keeps
        // repr[¬big]=¬repr[big]. kissat drops them at construction too
        // (congruence.c new_ite_gate).
        if (else_l == negate(then_l))
          continue;
        // soundness: require all four ITE-defining clauses to exist
        if (!(has(cond, negate(else_l), L) && has(cond, else_l, negate(L)) &&
              has(negate(cond), negate(then_l), L) &&
              has(negate(cond), then_l, negate(L))))
          continue;
        // identity key in the fixed sequence [L,cond,then,else], not sorted
        SmallKey keyd;
        keyd.size = 4;
        keyd.lits[0] = L;
        keyd.lits[1] = cond;
        keyd.lits[2] = then_l;
        keyd.lits[3] = else_l;
        if (!seen.insert(keyd))
          continue;
        CongGate g;
        g.kind = GateKind::Ite;
        g.output = L;
        g.inputs = {cond, then_l, else_l}; // [cond,then,else] fixed order
        out.push_back(std::move(g));
      }
  }
}

// ITE-match proof chain (kissat add_ite_matching_proof_chain): case-split on the
// shared condition, emitting the four ternary lemmas that make the two
// equivalence binaries RUP, each a resolvent of one cond-side clause of each
// gate. Under LRAT each is replayed for its chain and the two binaries close on
// their cond-sides. Returns false if any lemma fails to replay.
bool add_ite_matching_proof_chain(Kernel &k, Lit cond, Lit a, Lit b,
                                  std::vector<uint64_t> &chain_fwd,
                                  std::vector<uint64_t> &chain_rev,
                                  std::vector<TransientLemma> &transient) {
  chain_fwd.clear();
  chain_rev.clear();
  if (!k.proof)
    return true;
  const bool lrat = k.proof->lrat();
  std::vector<uint64_t> ch;
  // (¬a∨b) closes on its two cond-sides; (a∨¬b) on the other two.
  auto emit = [&](Lit x, Lit y, Lit z, std::vector<uint64_t> &into) -> bool {
    const Lit l[3] = {x, y, z};
    // The case-split literal can coincide with a merged output, making the
    // lemma tautological. Both formats bail, so the two proof modes keep the
    // same search trajectory.
    if (Proof::tautological(l, 3))
      return false;
    if (!lrat) {
      k.proof->add_rup(0, l, 3, nullptr, 0);
      return true;
    }
    if (!congruence_rup_chain(k, l, 3, ch))
      return false;
    const uint64_t id = ++k.next_clause_id;
    k.proof->add_rup(id, l, 3, ch.data(), static_cast<uint32_t>(ch.size()));
    transient.push_back({id, {l[0], l[1], l[2]}});
    into.push_back(id);
    return true;
  };
  return emit(negate(a), b, cond, chain_fwd) &&
         emit(negate(a), b, negate(cond), chain_fwd) &&
         emit(a, negate(b), cond, chain_rev) &&
         emit(a, negate(b), negate(cond), chain_rev);
}

// Add the equivalence a==b from two congruent AND gates as (¬a∨b),(a∨¬b). The
// gates' inputs are equal only under the current representatives, so the LRAT
// chain must thread the bridging binaries: a's input binaries force ga's
// inputs, `big_path_chain` bridges each to the gb input in its class, and gb's
// base conflicts. Returns false iff that chain fails — its only route can run
// through a root-assigned literal — and the caller must then skip the merge
// (N2). All-or-nothing: both binaries are justified before either is added.
template <class Find>
bool add_equivalence(Kernel &k, const CongGate &ga, const CongGate &gb,
                     Find &find) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit a = ga.output, b = gb.output;
  std::vector<uint64_t> chain, br;
  // chain forcing conflict for (¬g_from.out ∨ g_to.out)
  auto build = [&](const CongGate &g_from, const CongGate &g_to) -> bool {
    chain.assign(g_from.input_bin_ids.begin(), g_from.input_bin_ids.end());
    std::unordered_map<Lit, Lit> rep2from;
    for (const Lit in : g_from.inputs)
      rep2from[find(in)] = in;
    for (const Lit to_in : g_to.inputs) {
      const auto it = rep2from.find(find(to_in));
      if (it == rep2from.end())
        return false; // no matching input class (should not happen for a match)
      const Lit from_in = it->second;
      if (from_in == to_in)
        continue;
      if (!big_path_chain(k, from_in, to_in, br))
        return false;
      chain.insert(chain.end(), br.begin(), br.end());
    }
    chain.push_back(g_to.base_id);
    // Two same-class inputs can thread bridge paths sharing binary ids, and a
    // repeated hint is illegal LRAT. It propagates nothing at its second
    // position, so dropping it leaves the propagation sequence intact.
    {
      std::unordered_set<uint64_t> seen_ids;
      size_t w = 0;
      for (const uint64_t idh : chain)
        if (seen_ids.insert(idh).second)
          chain[w++] = idh;
      chain.resize(w);
    }
    return true;
  };
  // Justify first, mutate second: under LRAT each direction's chain is built
  // and kept before anything is added, so a failure aborts the equivalence.
  std::vector<uint64_t> chain_fwd, chain_rev;
  if (lrat) {
    const bool need_fwd = !kernel_binary_live(k, negate(a), b);
    const bool need_rev = !kernel_binary_live(k, a, negate(b));
    if (need_fwd) {
      if (!build(ga, gb))
        return false;
      chain_fwd = chain;
    }
    if (need_rev) {
      if (!build(gb, ga))
        return false;
      chain_rev = chain;
    }
  }
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &ch) {
    if (kernel_binary_live(k, x, y))
      return;
    // a degenerate pair (outputs equal or complementary) would make this
    // binary duplicate-carrying or tautological, neither emittable
    assert(x != y && x != negate(y));
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b, chain_fwd); // ga.out=1 → ga inputs → bridge → gb.base
  add_bin(a, negate(b), chain_rev); // gb.out=1 → gb inputs → bridge → ga.base
  return true;
}

// Add the equivalence a==b as (¬a∨b),(a∨¬b): bare adds under DRAT, where the
// preceding ladder lemmas already made them RUP, and under LRAT each binary
// carries the chain its ladder returned, so no hint-less line is emitted.
bool add_equivalence_chained(Kernel &k, Lit a, Lit b,
                             const std::vector<uint64_t> &chain_fwd,
                             const std::vector<uint64_t> &chain_rev) {
  const bool lrat = k.proof && k.proof->lrat();
  // All-or-nothing, checked before either binary is added: under LRAT a binary
  // that is still missing needs a non-empty chain or the line is uncheckable.
  // Skipping is sound.
  if (lrat) {
    if (!kernel_binary_live(k, negate(a), b) && chain_fwd.empty())
      return false;
    if (!kernel_binary_live(k, a, negate(b)) && chain_rev.empty())
      return false;
  }
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &ch) {
    if (kernel_binary_live(k, x, y))
      return;
    // a degenerate pair (outputs equal or complementary) would make this
    // binary duplicate-carrying or tautological, neither emittable
    assert(x != y && x != negate(y));
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b, chain_fwd);
  add_bin(a, negate(b), chain_rev);
  return true;
}

// The binary-resolution unit channel (`docs/calculus.md` V6a). Two binaries
// (l ∨ o) and (l ∨ ¬o) resolve on `o` to {l}, and `l`'s watch list holds both
// partners, so one linear sweep finds every such unit. A strict special case of
// V2 Backbone's precondition, kept here for coverage at linear cost; placement
// argument and removal path in V6a and `docs/implementation-notes.md` §13.3.
// `record` batches, so no unit is asserted mid-scan.
const bool cong_y1 = [] {
  const char *s = std::getenv("TACTIX_CONG_Y1");
  return !s || *s != '0';
}();

template <class Record> bool find_binary_units(Kernel &k, Record &&record) {
  if (!cong_y1)
    return false;
  // Persistent scratch, restored in O(marked): a per-call 2·nvars vector would
  // make this sweep memset-bound on a large formula.
  static thread_local std::vector<uint8_t> mark;
  static thread_local std::vector<Lit> touched;
  const size_t nlits = 2 * static_cast<size_t>(k.nvars);
  if (mark.size() < nlits)
    mark.resize(nlits, 0);
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint64_t> chain;
  for (Var v = 0; v < k.nvars; v++) {
    if (kernel_var_retired(k, v))
      continue;
    if (cong_budget.over(k))
      break; // dropping inferences is sound
    for (int s = 0; s < 2; s++) {
      const Lit l = make_lit(v, s != 0);
      // A root-assigned `l` has nothing to learn: already true, and false is
      // impossible at a quiescent root.
      if (k.value(l) != 0)
        continue;
      k.stats.congruence_ticks += k.watches.size(l);
      touched.clear();
      bool found = false;
      for (const WatchEntry w : k.watches.entries(l)) {
        if (!w.binary())
          continue;
        const Lit o = w.other();
        if (k.value(o) != 0)
          continue; // satisfied or already-propagated binary
        if (mark[negate(o)]) {
          found = true;
          break;
        }
        if (!mark[o]) {
          mark[o] = 1;
          touched.push_back(o);
        }
      }
      for (const Lit t : touched)
        mark[t] = 0;
      if (!found)
        continue;
      k.stats.congruence_y1_found++;
      // Replayed, never hand-built: a hand-built pair silently gets propagation
      // order wrong. The replay charges real propagation, so meter it here.
      const uint64_t t0 = k.stats.ticks;
      const bool ok = !lrat || congruence_rup_chain(k, &l, 1, chain);
      k.stats.congruence_ticks += k.stats.ticks - t0;
      if (!ok) {
        k.stats.congruence_unjustified++;
        continue; // sound: the inference is dropped, nothing is emitted
      }
      if (record(l, chain))
        return true; // the batch refuted
    }
  }
  return false;
}


} // namespace — public rule entry points (header-declared) below

// External-linkage forwarder for the primitive Sweep shares.
bool congruence_replay_rup(Kernel &k, const Lit *lits, uint32_t n,
                           std::vector<uint64_t> &chain) {
  return congruence_rup_chain(k, lits, n, chain);
}

void congruence_extract_gates(const Kernel &k, std::vector<CongGate> &out,
                              bool with_xor_ite) {
  const size_t base = out.size();
  {
    Stage s(&cong_t.extract_and);
    congruence_extract(k, out);
  }
  k.stats.congruence_gates_and += out.size() - base; // see Stats
  if (with_xor_ite) {
    ClauseIndex index; // built once, shared by XOR + ITE extraction
    {
      Stage s(&cong_t.index);
      build_clause_index(k, index);
    }
    // …and so is its canonical iteration order: the index is immutable for the
    // whole apply, so sorting it per consumer would recompute the same vector.
    std::vector<SmallKey> sorted;
    {
      Stage s(&cong_t.sort);
      sorted = sorted_keys(k, index);
    }
    const size_t before_xor = out.size();
    {
      Stage s(&cong_t.xor_x);
      congruence_extract_xor(k, index, sorted, out);
    }
    k.stats.congruence_gates_xor += out.size() - before_xor;
    const size_t before_ite = out.size();
    {
      Stage s(&cong_t.ite_x);
      congruence_extract_ite(k, index, sorted, out);
    }
    k.stats.congruence_gates_ite += out.size() - before_ite;
  }
}

ApplyResult congruence_apply(Kernel &k) {
  assert(congruence_guard(k));
  Stage apply_stage(&cong_t.apply_all);
  // The round's work budget, in `congruence_ticks`, handed by the policy
  // through `Kernel::work_budget` (0 = unbounded). Cleared on every exit.
  BudgetScope budget_scope(k);
  const uint64_t apply_t0 = cong_time ? now_nanos() : 0; // in-flight, for the print
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  kernel_canonicalize_binary_watches(k);
  k.stats.congruence_rounds++;
  if (cong_probe)
    cong_root_in = kernel_root_fixed(k);

  // AND, XOR and ITE gates all participate in one union-find closure: an AND
  // input can be an XOR/ITE output.
  const bool lrat = k.proof && k.proof->lrat();

  // Root units derived by this round, batched and asserted once every channel
  // has run, as V1 Probe and V3 Substitute do: the closure reasons purely over
  // the union-find, so a unit changes nothing already computed and M1 owns the
  // cascade. Deduped per variable, an opposite-sign pair being a refutation; a
  // unit contradicting one on the trail refutes against the trail id.
  PendingUnits pending;
  pending.init(k.nvars);
  auto record_unit = [&](Lit u, const std::vector<uint64_t> &uchain,
                         uint64_t *counter = nullptr) -> bool {
    const Var v = var_of(u);
    if (k.value(u) > 0)
      return false; // already true at the root — the inference adds nothing
    const uint64_t trail_id = k.unit_ids[v];
    const bool contradicts_trail = k.value(u) < 0;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, &u, 1, lrat ? uchain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(uchain.size()) : 0);
    if (contradicts_trail) {
      kernel_emit_empty_from_pair(k, id, trail_id);
      return true;
    }
    const PendingUnits::Outcome o = pending.record(k, u, id);
    if (o == PendingUnits::Outcome::Recorded)
      ++(counter ? *counter : k.stats.congruence_units);
    return o == PendingUnits::Outcome::Refuted;
  };
  // Assert whatever the round batched and close the root. Every exit path that
  // can carry pending units must go through this.
  auto finish = [&](bool merged) -> ApplyResult {
    for (const Lit u : pending.units) {
      assert(k.value(u) >= 0 &&
             "record_unit refutes rather than batching a false unit");
      if (k.value(u) == 0)
        kernel_assign_unit(k, u);
    }
    if (!pending.units.empty() && inprocess_reach_root(k))
      return ApplyResult::Unsat;
    if (cong_probe)
      cong_root_out = kernel_root_fixed(k);
    if (k.inconsistent)
      return ApplyResult::Unsat;
    return (merged || !pending.units.empty()) ? ApplyResult::Ok
                                              : ApplyResult::NoOp;
  };

  // The binary-resolution unit sweep, at the head of the round because its
  // units seed the value cascade (see `find_binary_units`).
  if (find_binary_units(k, [&](Lit u, const std::vector<uint64_t> &ch) {
        return record_unit(u, ch, &k.stats.congruence_y1_units);
      }))
    return ApplyResult::Unsat;

  std::vector<CongGate> gates;
  // XOR and ITE run on every proof path: their lemma chains are replayed rather
  // than derived, so the sign layer canonicalization folds on top of the
  // union-find bridging never has to be unpicked by hand.
  congruence_extract_gates(k, gates, /*with_xor_ite=*/true);
  if (gates.empty())
    return finish(/*merged=*/false);
  // Extraction is read-only, so an interrupt raised during it costs nothing to
  // honour — nothing has been applied yet.
  if (kernel_terminating(k)) {
    k.stats.rule_interrupted++;
    return finish(/*merged=*/false);
  }

  // The congruence closure (kissat's), an incremental worklist over a union-find
  // on literals: merge the outputs of two gates that canonicalize to the same
  // (kind, key), then re-process only the gates whose inputs contain the merged
  // literal, so the collapse propagates through the circuit layers in one apply
  // (`docs/calculus.md` V6).
  const uint32_t nlits = 2 * k.nvars;
  std::vector<Lit> repr(nlits);
  for (Lit l = 0; l < static_cast<Lit>(nlits); l++)
    repr[l] = l;
  auto find = [&](Lit l) {
    while (repr[l] != l)
      l = repr[l] = repr[repr[l]];
    return l;
  };
  auto unite = [&](Lit small, Lit big) { // big's class → small's (small<big)
    repr[big] = small;
    repr[negate(big)] = negate(small);
  };

  // Per-kind tables keyed by the canonical input key (kinds must not collide).
  std::unordered_map<std::vector<Lit>, uint32_t, InputsHash> table[3];
  table[0].reserve(gates.size() * 2);
  std::vector<std::vector<uint32_t>> occ(nlits); // canonical-input lit → gate ids
  std::deque<uint32_t> work;
  std::vector<uint8_t> inq(gates.size(), 1);
  for (uint32_t i = 0; i < gates.size(); i++)
    work.push_back(i);

  uint64_t neq = 0;
  bool unsat = false;
  std::vector<Lit> key, key_j, cancelled, cancelled_j;
  std::vector<Lit> reps; // canonicalize's XOR scratch (not reentrant — one buffer)
  std::vector<TransientLemma> transient; // ladder lemmas, del'd after each use

  // The contradictory merge: the closure proved a ≡ b while the union-find holds
  // a ≡ ¬b, so the formula is UNSAT and each opposing unit is directly RUP
  // (a propagates b over the new binary and ¬a over the in-class bridge path).
  // `record_unit` pairs them into the empty clause; returns true iff refuted.
  auto refute_contradictory = [&](Lit a) -> bool {
    const Lit u1 = negate(a), u2 = a;
    std::vector<uint64_t> ch1, ch2;
    if (lrat && !congruence_rup_chain(k, &u1, 1, ch1)) {
      k.stats.congruence_unjustified++;
      return false;
    }
    if (record_unit(u1, ch1))
      return true;
    if (lrat && !congruence_rup_chain(k, &u2, 1, ch2)) {
      k.stats.congruence_unjustified++;
      return false; // {¬a} stays pending — half the inference, still sound
    }
    return record_unit(u2, ch2);
  };

  // Merge two provably-equivalent literals: unite their classes and reschedule
  // the gates whose canonical inputs just changed. Proving the equivalence is
  // the caller's job — it differs per kind and per degeneracy.
  auto merge_lits = [&](Lit x, Lit y) {
    const Lit oa = find(x), ob = find(y);
    if (oa == ob)
      return;
    if (cong_probe && (k.value(oa) != 0 || k.value(ob) != 0))
      cong_val_merge++;
    const Lit sm = oa < ob ? oa : ob, big = oa < ob ? ob : oa;
    unite(sm, big);
    neq++;
    for (const Lit side : {big, negate(big)})
      for (const uint32_t gg : occ[side])
        if (!inq[gg]) {
          inq[gg] = 1;
          work.push_back(gg);
        }
  };

  // The closure is the long block, so it must be interruptible: abort between
  // worklist items, each all-or-nothing (binaries and proof chain both emitted,
  // or nothing). The batched root units are still asserted from what it did.
  uint32_t stop_tick = 0;
  const uint64_t closure_t0 = cong_time ? now_nanos() : 0;
  while (!work.empty() && !unsat) {
    if (k.stop_fuzz || !(++stop_tick & 255)) {
      if (kernel_terminating(k)) {
        k.stats.rule_interrupted++;
        break;
      }
      // The budget rides the same between-items stop as the termination flag,
      // inheriting its soundness argument verbatim.
      if (cong_budget.over(k))
        break;
    }
    cong_t.items++;
    const uint32_t i = work.front();
    work.pop_front();
    inq[i] = 0;
    Lit out_i;
    Canon ci;
    {
      Stage s(&cong_t.canon);
      ci = canonicalize(gates[i], find, key, out_i, cancelled, reps);
    }
    k.stats.congruence_ticks += 1 + gates[i].inputs.size();
    CongMatchStats &cm = cong_match[static_cast<int>(gates[i].kind)];
    if (cong_probe)
      (ci == Canon::Ok     ? cm.canon_ok
       : ci == Canon::Skip ? cm.canon_skip
       : ci == Canon::Unit ? cm.canon_unit
                           : cm.canon_merge)++;
    if (ci == Canon::Skip)
      continue; // degenerate in a way this rule does not exploit — sound
    // --- a degenerate gate is an inference, not a dead end ------------------
    if (ci == Canon::Unit) {
      // value(out_lit) = 0, so ¬out_lit is a root unit. XOR arrives with its
      // cancelled set; an AND clash arrives with `cancelled` empty, its unit
      // directly RUP, so the ladder's top==0 form just replays the chain.
      const std::vector<std::vector<Lit>> concl = {{negate(out_i)}};
      std::vector<std::vector<uint64_t>> cch;
      transient.clear();
      if (!add_xor_degeneracy_chain(k, cancelled, concl, cch, transient)) {
        k.stats.congruence_unjustified++;
        if (cong_probe) std::fprintf(stderr, "[cong-probe] UNIT ladder replay failed (|cancelled|=%zu)\n", cancelled.size());
        continue; // ladder did not replay — emit nothing, drop the inference
      }
      const bool refuted = record_unit(negate(out_i), cch[0]);
      delete_transients(k, transient);
      if (refuted) {
        unsat = true;
        break;
      }
      continue; // the gate is consumed by its own inference
    }
    if (ci == Canon::Merge) {
      // value(out_lit) = ⊕key = key[0], so out_lit ≡ key[0]: the unary-gate
      // merge, with no second gate needed (`docs/calculus.md` V6).
      const Lit other = key[0];
      if (find(out_i) == find(other))
        continue; // already known
      // x ≡ ¬x: justify the equivalence exactly as the normal merge would,
      // then refute instead of uniting the classes
      const bool clash = find(out_i) == negate(find(other));
      if (clash && cong_probe)
        cong_n_clash++;
      if (clash && out_i == negate(other)) {
        // literal-form contradiction: the equivalence binaries degenerate to
        // two opposing units, directly RUP for AND. The XOR form would need a
        // ladder over degenerate conclusions and stays dropped, which is sound.
        if (gates[i].kind == GateKind::And) {
          if (refute_contradictory(out_i)) {
            unsat = true;
            break;
          }
        } else
          k.stats.congruence_unjustified++;
        continue;
      }
      const std::vector<std::vector<Lit>> concl = {{negate(out_i), other},
                                                  {out_i, negate(other)}};
      std::vector<std::vector<uint64_t>> cch;
      transient.clear();
      if (!add_xor_degeneracy_chain(k, cancelled, concl, cch, transient)) {
        k.stats.congruence_unjustified++;
        continue; // ladder did not replay — emit nothing, drop the inference
      }
      if (!add_equivalence_chained(k, out_i, other, cch[0], cch[1])) {
        k.stats.congruence_unjustified++;
        delete_transients(k, transient);
        continue; // an unjustifiable binary — drop the inference, keep N2
      }
      delete_transients(k, transient);
      if (clash) {
        if (refute_contradictory(out_i)) {
          unsat = true;
          break;
        }
        continue; // replay failed — the inference is dropped
      }
      merge_lits(out_i, other);
      continue;
    }
    if (cong_probe) {
      bool valued = k.value(out_i) != 0;
      for (const Lit l : key)
        valued = valued || k.value(l) != 0;
      cm.val_gate += valued;
    }
    Stage table_stage(&cong_t.table);
    for (const Lit l : key)
      occ[l].push_back(i); // register for rescheduling when an input merges
    auto &tab = table[static_cast<int>(gates[i].kind)];
    auto it = tab.find(key);
    if (it == tab.end()) {
      if (cong_probe)
        cm.insert++;
      tab.emplace(key, i);
      continue;
    }
    if (cong_probe)
      cm.collide++;
    // Recompute the stored gate's canonical form: the union-find may have moved
    // on, and only a shared current key proves the outputs equal. A degenerate
    // stored gate is evicted rather than exploited here — it is still in the
    // worklist, so its own pop draws the inference.
    const uint32_t j = it->second;
    Lit out_j;
    if (canonicalize(gates[j], find, key_j, out_j, cancelled_j, reps) !=
            Canon::Ok ||
        key_j != key) {
      if (cong_probe)
        cm.stale++;
      it->second = i; // evict stale/degenerate entry; current gate represents key
      continue;
    }
    const Lit oa = find(out_j);
    const Lit ob = find(out_i);
    if (oa == ob) {
      if (cong_probe)
        cm.already_eq++;
      continue; // already equal
    }
    // x ≡ ¬x: justify the congruence like any other match, then refute
    const bool clash = (oa == negate(ob));
    if (clash && cong_probe) {
      cong_n_clash++;
      cm.clash++;
    }
    if (clash && out_i == negate(out_j)) {
      // literal-form contradiction (same variable, opposite signs) — see the
      // Merge-branch twin of this guard.
      if (gates[i].kind == GateKind::And) {
        if (refute_contradictory(out_j)) {
          unsat = true;
          break;
        }
      } else
        k.stats.congruence_unjustified++;
      continue;
    }
    // Congruent outputs, so add oa == ob. An equivalence whose chain cannot be
    // built is skipped rather than emitted unjustified: sound, and N2 holds.
    if (gates[i].kind == GateKind::Xor || gates[i].kind == GateKind::Ite) {
      Stage proof_stage(&cong_t.proof);
      std::vector<uint64_t> cf, cr;
      transient.clear();
      const bool ok =
          gates[i].kind == GateKind::Xor
              ? add_xor_matching_proof_chain(k, key, out_j, out_i, cf, cr,
                                             transient)
              : add_ite_matching_proof_chain(k, key[0], out_j, out_i, cf, cr,
                                             transient); // key[0] = cond
      if (!ok) {
        k.stats.congruence_unjustified++;
        if (cong_probe) {
          cm.chain_fail++;
          std::fprintf(stderr, "[cong-probe] MATCH chain failed (kind=%d)\n", (int)gates[i].kind);
        }
        continue; // no checkable chain — drop the inference, keep N2
      }
      if (!add_equivalence_chained(k, out_j, out_i, cf, cr)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        delete_transients(k, transient);
        continue; // an unjustifiable binary — drop the inference, keep N2
      }
      delete_transients(k, transient);
    } else {
      Stage proof_stage(&cong_t.proof);
      if (!add_equivalence(k, gates[j], gates[i], find)) {
        k.stats.congruence_unjustified++;
        if (cong_probe)
          cm.chain_fail++;
        continue; // no checkable chain — drop the inference, keep proof valid
      }
    }
    if (clash) {
      if (refute_contradictory(out_j)) {
        unsat = true;
        break;
      }
      continue; // replay failed — the inference is dropped
    }
    if (cong_probe)
      cm.merged++;
    merge_lits(out_j, out_i);
    if (k.inconsistent) {
      unsat = true;
      break;
    }
  }
  if (cong_time) {
    cong_t.closure_all += now_nanos() - closure_t0;
    const double ms = 1e-6;
    std::fprintf(stderr,
                 "[cong-time] cumulative ms: apply %.0f | extract{and %.0f "
                 "index %.0f sort %.0f xor %.0f ite %.0f} | closure %.0f "
                 "{canon %.0f match %.0f (of which proof %.0f)} | items %llu\n",
                 (cong_t.apply_all + (now_nanos() - apply_t0)) * ms,
                 cong_t.extract_and * ms,
                 cong_t.index * ms, cong_t.sort * ms, cong_t.xor_x * ms,
                 cong_t.ite_x * ms, cong_t.closure_all * ms, cong_t.canon * ms,
                 cong_t.table * ms, cong_t.proof * ms,
                 (unsigned long long)cong_t.items);
  }
  if (cong_probe) {
    std::fprintf(stderr,
                 "[cong-probe] degeneracies (AND/XOR/clash DRAWN since C3b; "
                 "ITE-share still a discard): "
                 "AND=%llu ITE-share=%llu "
                 "XOR-cancel=%llu | contradictory merges (refuted): %llu\n",
                 (unsigned long long)cong_n_and, (unsigned long long)cong_n_ite,
                 (unsigned long long)cong_n_xor,
                 (unsigned long long)cong_n_clash);
    // The closure funnel, per kind; cumulative across every application in
    // the process, like the degeneracy counters above.
    static const char *kind_name[3] = {"and", "xor", "ite"};
    std::fprintf(stderr,
                 "[cong-probe] closure funnel (cumulative, per kind)\n"
                 "[cong-probe] %-4s %12s %12s %12s %12s %12s %10s %10s %10s "
                 "%12s\n",
                 "kind", "canon_ok", "canon_skip", "insert", "collide", "stale",
                 "already_eq", "chain_fail", "merged", "val_gate");
    for (int kk = 0; kk < 3; ++kk) {
      const CongMatchStats &c = cong_match[kk];
      std::fprintf(stderr,
                   "[cong-probe] %-4s %12llu %12llu %12llu %12llu %12llu "
                   "%10llu %10llu %10llu %12llu\n",
                   kind_name[kk], (unsigned long long)c.canon_ok,
                   (unsigned long long)c.canon_skip,
                   (unsigned long long)c.insert, (unsigned long long)c.collide,
                   (unsigned long long)c.stale,
                   (unsigned long long)c.already_eq,
                   (unsigned long long)c.chain_fail,
                   (unsigned long long)c.merged,
                   (unsigned long long)c.val_gate);
    }
    std::fprintf(stderr,
                 "[cong-probe] value channel: root-assigned at entry %llu / at "
                 "exit %llu | merges with a valued participant %llu | units "
                 "%llu\n",
                 (unsigned long long)cong_root_in,
                 (unsigned long long)cong_root_out,
                 (unsigned long long)cong_val_merge,
                 (unsigned long long)k.stats.congruence_units);
  }
  if (std::getenv("TACTIX_CONG_TRACE"))
    std::fprintf(stderr, "[cong] %zu gates, %llu equivalences added\n",
                 gates.size(), (unsigned long long)neq);
  k.stats.congruence_gates += gates.size();
  k.stats.congruence_merges += neq;
  if (unsat)
    return ApplyResult::Unsat;

  // Assert the batch of derived root units, then close the root. On a miter this
  // is what refutes at zero conflicts: congruence proves aᵢ ≡ bᵢ for the
  // matching signals, every output XOR cancels to the unit ¬outᵢ, and
  // propagating those falsifies the output constraint.
  return finish(/*merged=*/neq != 0);
}

uint32_t congruence_instances(Kernel &, uint64_t *out, uint32_t cap) {
  if (cap >= 1)
    out[0] = INSTANCE_CANONICAL;
  return 1;
}

ApplyResult congruence_apply_instance(Kernel &k, uint64_t inst) {
  if (inst == INSTANCE_CANONICAL)
    return congruence_apply(k);
  return ApplyResult::NoOp; // only the canonical (full-round) instance
}

TACTIX_REGISTER_RULE(Congruence, RuleTier::Inprocess, congruence_guard,
                     congruence_apply, congruence_instances,
                     congruence_apply_instance);

} // namespace tactix
