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
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {

namespace {

// arity cap — an AND gate over more than this many inputs is skipped (kissat caps
// too; keeps extraction and the equivalence proof chain bounded).
constexpr uint32_t CONG_ARITY_LIMIT = 64;
// safety cap on gates extracted per round.
constexpr size_t CONG_GATE_LIMIT = 20000000;
// max #inputs of an extracted XOR gate (kissat --congruencexorarity default cap
// is small; the match proof chain is O(2^arity), so keep it bounded). An arity-a
// XOR is a size-(a+1) clause group, so scan clauses of size 3..CONG_XOR_ARITY+1.
constexpr uint32_t CONG_XOR_ARITY = 4;

// mark the binary-watch partners of `lit` (each `p` with a binary (lit ∨ p)).
template <int V> void mark_bin_partners(const Kernel &k, Lit lit,
                                        std::vector<uint8_t> &mark) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[lit].offset;
  const uint32_t sz = k.watches.heads[lit].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      mark[w.other()] = V;
      i += 1;
    } else {
      i += 2;
    }
  }
}

// true iff a live binary (a ∨ b) exists.
bool binary_exists(const Kernel &k, Lit a, Lit b) {
  const uint32_t *ws = k.watches.arena.data() + k.watches.heads[a].offset;
  const uint32_t sz = k.watches.heads[a].size;
  for (uint32_t i = 0; i < sz;) {
    const Watch w{ws[i]};
    if (w.binary()) {
      if (w.other() == b)
        return true;
      i += 1;
    } else {
      i += 2;
    }
  }
  return false;
}

} // namespace

bool congruence_guard(const Kernel &k) {
  return !k.inconsistent && k.level == 0;
}

void congruence_extract(const Kernel &k, std::vector<CongGate> &out) {
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<uint8_t> mark(2 * k.nvars, 0); // binary partners of ¬output
  // AND gate `out = AND(inputs)`: a long base clause where `out` appears and every
  // OTHER literal l has the binary (¬out ∨ ¬l) — then input = ¬l. (OR gates are the
  // dual: for OR, `¬out` is the base literal and the same scan on the negated
  // clause finds them, so trying every literal of every clause as the output
  // covers both.)
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    const uint32_t words = clause_words(c->size);
    ref += words;
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_ARITY_LIMIT + 1)
      continue;
    if (out.size() >= CONG_GATE_LIMIT)
      return;
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
      CongGate g;
      g.output = outp;
      g.base_id = c->id();
      for (const Lit l : *c)
        if (l != outp) {
          const Lit inp = negate(l);
          g.inputs.push_back(inp);
          if (lrat)
            g.input_bin_ids.push_back(kernel_binary_id(k, nout, inp));
        }
      // canonicalize inputs (the structural key) — keep bin_ids parallel
      if (lrat) {
        std::vector<std::pair<Lit, uint64_t>> pairs(g.inputs.size());
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

// XOR extraction: a size-s clause group (s ∈ 3..CONG_XOR_ARITY+1) over variables
// {v0..v_{s-1}} is an XOR relation v0⊕…⊕v_{s-1}=c iff ALL 2^{s-1} clauses of one
// fixed sign-parity over those variables are present (kissat requires the full
// set — anything less is not the relation, so emitting a gate would be UNSOUND).
// From the relation we emit s XOR gates, one per choice of output vᵢ: inputs =
// the other variables (all positive), output literal = pos(vᵢ)^c so its value is
// c⊕vᵢ = ⊕_{j≠i} vⱼ. Two gates with the same input set have equivalent outputs.
using ClauseIndex = std::unordered_map<std::vector<Lit>, uint64_t, InputsHash>;

// Index every irredundant, fully-unassigned clause of size 3..CONG_XOR_ARITY+1 by
// its sorted literal vector → proof id. Shared by XOR and ITE extraction (built
// once per apply); the gate CLAUSES are never rewritten within an apply, so one
// snapshot is valid for the whole closure.
void build_clause_index(const Kernel &k, ClauseIndex &index) {
  std::vector<Lit> ls;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *c = clause_at(k.arena, ref);
    ref += clause_words(c->size);
    if (c->garbage() || c->redundant() || c->size < 3 ||
        c->size > CONG_XOR_ARITY + 1)
      continue;
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
    std::sort(ls.begin(), ls.end());
    index.emplace(ls, c->id());
  }
}

void congruence_extract_xor(const Kernel &k, const ClauseIndex &index,
                            std::vector<CongGate> &out) {
  (void)k;
  std::unordered_set<std::vector<Lit>, InputsHash> seen; // by sorted positive vars
  std::vector<Lit> pos;                                  // positive lits, sorted
  std::vector<Lit> probe;
  for (const auto &entry : index) {
    const std::vector<Lit> &base = entry.first;
    const uint32_t s = static_cast<uint32_t>(base.size());
    // positive-lit form of the variables, sorted; dedup a relation to once.
    pos.clear();
    int base_parity = 0;
    for (const Lit l : base) {
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
    if (dup || seen.count(pos))
      continue;
    // require all 2^{s-1} clauses of parity == base_parity to exist.
    bool all = true;
    const uint32_t combos = 1u << (s - 1);
    for (uint32_t m = 0; m < combos && all; m++) {
      int par = 0;
      probe.assign(s, 0);
      for (uint32_t i = 0; i + 1 < s; i++) {
        const int sign = (m >> i) & 1;
        par ^= sign;
        probe[i] = pos[i] ^ static_cast<Lit>(sign);
      }
      probe[s - 1] = pos[s - 1] ^ static_cast<Lit>(base_parity ^ par); // fix parity
      std::sort(probe.begin(), probe.end());
      if (!index.count(probe))
        all = false;
    }
    if (!all)
      continue;
    seen.insert(pos);
    const int c = static_cast<int>((s ^ static_cast<uint32_t>(base_parity) ^ 1u) & 1u);
    for (uint32_t i = 0; i < s; i++) {
      CongGate g;
      g.kind = GateKind::Xor;
      g.output = pos[i] ^ static_cast<Lit>(c); // value = c ⊕ vᵢ = ⊕_{j≠i} vⱼ
      for (uint32_t j = 0; j < s; j++)
        if (j != i)
          g.inputs.push_back(pos[j]); // all positive, already sorted
      out.push_back(std::move(g));
    }
  }
}

bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l); // defined below

// TACTIX_CONG_PROBE: count the degenerate-gate discards by cause. These are the
// inferences V6 does NOT draw (deviation D-CONG-UNIT) — the diagnostic that sizes
// closing it. Env-gated, default-identical, like TACTIX_CONG_TRACE.
static const bool cong_probe = std::getenv("TACTIX_CONG_PROBE") != nullptr;
static uint64_t cong_n_and = 0, cong_n_ite = 0, cong_n_xor = 0, cong_n_clash = 0;

// Canonicalize gate g under the current union-find into (key, out_lit): key is the
// structural hash (positive, sorted per kind); out_lit is the literal whose class
// is merged on a key collision. Returns false if the gate is degenerate under the
// current representatives (skip — a later increment handles the derived unit).
bool canonicalize(const CongGate &g, const std::function<Lit(Lit)> &find,
                  std::vector<Lit> &key, Lit &out_lit) {
  if (g.kind == GateKind::And) {
    key.assign(g.inputs.begin(), g.inputs.end());
    for (Lit &l : key)
      l = find(l);
    std::sort(key.begin(), key.end());
    for (size_t j = 1; j < key.size(); j++)
      if (key[j] == key[j - 1] || key[j] == negate(key[j - 1])) {
        if (cong_probe) cong_n_and++; 
        return false; // AND(…,x,x/¬x,…): idempotent/false — defer
      }
    out_lit = g.output;
    return true;
  }
  if (g.kind == GateKind::Ite) {
    Lit cond = find(g.inputs[0]), th = find(g.inputs[1]), el = find(g.inputs[2]);
    // defer any ITE whose (representative) operands share a variable — then==else,
    // then==¬else (an XOR), or cond touching a branch collapse it to a simpler
    // gate that a later increment/propagation handles; skipping stays sound.
    if (var_of(cond) == var_of(th) || var_of(cond) == var_of(el) ||
        var_of(th) == var_of(el)) {
      if (cong_probe) cong_n_ite++; 
      return false;
    }
    const bool neg_lhs = normalize_ite(cond, th, el);
    key = {cond, th, el}; // fixed order — the structural key
    out_lit = neg_lhs ? negate(g.output) : g.output;
    return true;
  }
  // XOR: map inputs to reps, absorb each rep's sign into a global parity, and
  // cancel repeated variables (x⊕x=0). The parity flips the output literal.
  int parity = 0;
  std::vector<Lit> reps;
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
    j += cnt;
  }
  if (key.empty()) {
    if (cong_probe) cong_n_xor++; 
    return false; // all cancelled → output is a constant; defer
  }
  out_lit = parity ? negate(g.output) : g.output;
  return true;
}

// XOR-match proof chain (kissat add_xor_matching_proof_chain): make the two
// equivalence binaries (¬a∨b),(a∨¬b) checkable by RUP. `rhs` = the shared
// canonical (representative) inputs, all positive; a,b = the two sign-adjusted
// outputs. Emits a parity-elimination ladder: eliminate one rhs var at a time,
// at each level enumerating all sign patterns of the still-present rhs prefix and
// emitting {prefix,¬a,b} and {prefix,a,¬b}. Every lemma is RUP over the current
// DB (the two gates' side clauses + any bridging equivalence binaries), so under
// DRAT each is a bare add_rup; drat-trim replays them in order. The two size-0
// lemmas ARE the equivalence binaries and are added persistently by the caller.
// (LRAT chains for each lemma are a follow-up; DRAT verifies via full-DB RUP.)
void add_xor_matching_proof_chain(Kernel &k, const std::vector<Lit> &rhs, Lit a,
                                  Lit b) {
  if (!k.proof)
    return;
  // ladder over rhs[0 .. size-2]; rhs.back() is absorbed by the gate clauses' RUP.
  int size = static_cast<int>(rhs.size()) - 1;
  std::vector<Lit> lemma;
  for (; size >= 1; size--) {
    const uint32_t combos = 1u << size;
    for (uint32_t m = 0; m < combos; m++) {
      for (int dir = 0; dir < 2; dir++) {
        lemma.clear();
        for (int i = 0; i < size; i++)
          lemma.push_back(rhs[i] ^ static_cast<Lit>((m >> i) & 1));
        if (dir == 0) {
          lemma.push_back(negate(a));
          lemma.push_back(b);
        } else {
          lemma.push_back(a);
          lemma.push_back(negate(b));
        }
        k.proof->add_rup(0, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         nullptr, 0);
      }
    }
  }
}

// Normalize an ITE triple [cond,then,else] in place to the canonical key form
// (cond positive; then positive) and return whether the OUTPUT must be negated.
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

// ITE extraction. lhs = cond ? then : else is defined by four ternary clauses:
//   (cond∨¬else∨lhs),(cond∨else∨¬lhs)   — under ¬cond, lhs≡else
//   (¬cond∨¬then∨lhs),(¬cond∨then∨¬lhs) — under  cond, lhs≡then
// We find per-lhs "conditional equivalences" lhs≡w under a condition literal from
// clause pairs (A∨B∨lhs)+(A∨¬B∨¬lhs) ⇒ lhs≡¬B when ¬A, then pair one under c with
// one under ¬c into an ITE candidate. EVERY candidate is verified against all four
// defining clauses before emission, so a mis-detected condeq can never yield an
// unsound gate. Output = lhs, inputs = [cond,then,else] (canonicalized later).
void congruence_extract_ite(const Kernel &k, const ClauseIndex &index,
                            std::vector<CongGate> &out) {
  (void)k;
  std::vector<Lit> probe(3);
  auto has = [&](Lit a, Lit b, Lit c) -> bool {
    probe[0] = a;
    probe[1] = b;
    probe[2] = c;
    std::sort(probe.begin(), probe.end());
    return index.count(probe) > 0;
  };
  // collect conditional equivalences per lhs literal
  std::unordered_map<Lit, std::vector<std::pair<Lit, Lit>>> ce;
  for (const auto &e : index) {
    const std::vector<Lit> &cl = e.first; // sorted 3 lits
    for (int li = 0; li < 3; li++) {
      const Lit L = cl[li], A = cl[(li + 1) % 3], B = cl[(li + 2) % 3];
      // (A∨B∨L) + (A∨¬B∨¬L) ⇒ L≡¬B when ¬A
      if (has(A, negate(B), negate(L)))
        ce[L].push_back({negate(A), negate(B)});
      // (A∨B∨L) + (¬A∨B∨¬L) ⇒ L≡¬A when ¬B
      if (has(negate(A), B, negate(L)))
        ce[L].push_back({negate(B), negate(A)});
    }
  }
  std::unordered_set<std::vector<Lit>, InputsHash> seen;
  for (auto &kv : ce) {
    const Lit L = kv.first;
    auto &lst = kv.second;
    if (lst.size() > 256)
      continue; // bound the O(n²) pairing on pathological hubs
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
        // soundness: require all four ITE-defining clauses to exist
        if (!(has(cond, negate(else_l), L) && has(cond, else_l, negate(L)) &&
              has(negate(cond), negate(then_l), L) &&
              has(negate(cond), then_l, negate(L))))
          continue;
        std::vector<Lit> keyd = {L, cond, then_l, else_l};
        if (!seen.insert(keyd).second)
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
// shared condition. Emits the four ternary lemmas that make the two equivalence
// binaries (a∨¬b),(¬a∨b) RUP; each is a resolvent of one cond-side clause of each
// gate and is RUP over the current DB, so under DRAT they are bare adds replayed
// in order. The two binaries themselves are added by add_equivalence_bare.
void add_ite_matching_proof_chain(Kernel &k, Lit cond, Lit a, Lit b) {
  if (!k.proof)
    return;
  auto emit = [&](Lit x, Lit y, Lit z) {
    const Lit l[3] = {x, y, z};
    k.proof->add_rup(0, l, 3, nullptr, 0);
  };
  emit(a, negate(b), cond);
  emit(a, negate(b), negate(cond));
  emit(negate(a), b, cond);
  emit(negate(a), b, negate(cond));
}

// Add the equivalence a==b from two congruent AND gates as (¬a∨b),(a∨¬b).
// Under the fixpoint the two gates' inputs are equal only UNDER THE CURRENT
// REPRESENTATIVES (bridged by earlier added equivalences), so the LRAT chain must
// thread those bridging binaries — the naive `input_bin_ids + base_id` is only
// valid for a direct (unbridged) match. For (¬a∨b): assume a,¬b; a's input
// binaries force ga's inputs, `big_path_chain` bridges each ga input to the gb
// input in its representative class, and gb's base then conflicts. (a∨¬b) mirrors.
// DRAT emits bare adds (the checker searches). Returns true iff ∅ derived.
bool add_equivalence(Kernel &k, const CongGate &ga, const CongGate &gb,
                     const std::function<Lit(Lit)> &find) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit a = ga.output, b = gb.output;
  std::vector<uint64_t> chain, br;
  // chain forcing conflict for (¬g_from.out ∨ g_to.out): g_from.out=1 forces
  // g_from's inputs, each bridges to the same-class g_to input, g_to.base falsifies.
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
    return true;
  };
  auto add_bin = [&](Lit x, Lit y, const CongGate &g_from, const CongGate &g_to) {
    if (binary_exists(k, x, y))
      return;
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof) {
      if (lrat) {
        const bool ok = build(g_from, g_to);
        k.proof->add_rup(id, lits, 2, ok ? chain.data() : nullptr,
                         ok ? static_cast<uint32_t>(chain.size()) : 0);
      } else {
        k.proof->add_rup(0, lits, 2, nullptr, 0);
      }
    }
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b, ga, gb); // (¬a∨b): ga.out=1 → ga inputs → bridge → gb.base
  add_bin(a, negate(b), gb, ga); // (a∨¬b): gb.out=1 → gb inputs → bridge → ga.base
  return false;
}

// Add the equivalence a==b as (¬a∨b),(a∨¬b) with a BARE proof add (DRAT-RUP over
// the current DB). Used for XOR matches, whose RUP is already made checkable by
// the preceding add_xor_matching_proof_chain lemmas. XOR extraction is gated on
// !lrat, so no LRAT id/chain is needed here yet.
void add_equivalence_bare(Kernel &k, Lit a, Lit b) {
  auto add_bin = [&](Lit x, Lit y) {
    if (binary_exists(k, x, y))
      return;
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(0, lits, 2, nullptr, 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b);
  add_bin(a, negate(b));
}

} // namespace — public rule entry points (header-declared) below

ApplyResult congruence_apply(Kernel &k) {
  assert(congruence_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.congruence_rounds++;

  // AND, XOR, and ITE gates all participate in ONE union-find closure (an AND
  // input can be an XOR/ITE output). AND matches emit native-LRAT bridged chains
  // (add_equivalence). XOR/ITE match binaries use a bare DRAT-RUP add preceded by
  // their proof-chain lemmas, whose LRAT chains are not yet produced — so XOR/ITE
  // extraction is gated on DRAT (D-CONG-LRAT, docs/calculus.md); AND congruence
  // runs on every path.
  const bool lrat = k.proof && k.proof->lrat();
  std::vector<CongGate> gates;
  congruence_extract(k, gates);
  if (!lrat) {
    ClauseIndex index; // built once, shared by XOR + ITE extraction
    build_clause_index(k, index);
    congruence_extract_xor(k, index, gates);
    congruence_extract_ite(k, index, gates);
  }
  if (gates.empty())
    return ApplyResult::NoOp;

  // Internal congruence closure (kissat), incremental worklist: a union-find over
  // literals; two gates that canonicalize (under the current representatives) to
  // the same (kind, key) have equivalent outputs → merge those outputs (add the
  // equivalence binaries + proof chain) and re-process only the gates whose inputs
  // contain the merged literal, so the collapse propagates through the circuit
  // layers in ONE apply. Gate CLAUSES are never rewritten — only the union-find +
  // the added binaries; V3 Substitute does the whole-formula collapse afterward.
  const uint32_t nlits = 2 * k.nvars;
  std::vector<Lit> repr(nlits);
  for (Lit l = 0; l < static_cast<Lit>(nlits); l++)
    repr[l] = l;
  std::function<Lit(Lit)> find = [&](Lit l) {
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
  std::vector<Lit> key, key_j;
  while (!work.empty() && !unsat) {
    const uint32_t i = work.front();
    work.pop_front();
    inq[i] = 0;
    Lit out_i;
    if (!canonicalize(gates[i], find, key, out_i))
      continue; // degenerate under current reps — deferred
    for (const Lit l : key)
      occ[l].push_back(i); // register for rescheduling when an input merges
    auto &tab = table[static_cast<int>(gates[i].kind)];
    auto it = tab.find(key);
    if (it == tab.end()) {
      tab.emplace(key, i);
      continue;
    }
    // Recompute the stored gate's current canonical form: the union-find may have
    // moved on since it was inserted (stale key). Only merge when both gates share
    // the SAME current key — then their outputs are provably equal.
    const uint32_t j = it->second;
    Lit out_j;
    if (!canonicalize(gates[j], find, key_j, out_j) || key_j != key) {
      it->second = i; // evict stale/degenerate entry; current gate represents key
      continue;
    }
    const Lit oa = find(out_j);
    const Lit ob = find(out_i);
    if (oa == ob || oa == negate(ob)) {
      if (cong_probe && oa == negate(ob)) cong_n_clash++; 
      continue; // already equal, or a contradiction (deferred)
    }
    // congruent outputs → add oa==ob (DRAT-RUP over the full DB; LRAT chains TODO).
    if (gates[i].kind == GateKind::Xor) {
      add_xor_matching_proof_chain(k, key, out_j, out_i);
      add_equivalence_bare(k, out_j, out_i);
    } else if (gates[i].kind == GateKind::Ite) {
      add_ite_matching_proof_chain(k, key[0], out_j, out_i); // key[0] = cond
      add_equivalence_bare(k, out_j, out_i);
    } else if (add_equivalence(k, gates[j], gates[i], find)) {
      return ApplyResult::Unsat;
    }
    const Lit sm = oa < ob ? oa : ob, big = oa < ob ? ob : oa;
    unite(sm, big);
    neq++;
    if (k.inconsistent) {
      unsat = true;
      break;
    }
    // gates using the merged-away representative as a canonical input changed key
    for (const Lit side : {big, negate(big)})
      for (const uint32_t gg : occ[side])
        if (!inq[gg]) {
          inq[gg] = 1;
          work.push_back(gg);
        }
  }
  if (cong_probe)
    std::fprintf(stderr,
                 "[cong-probe] discards (= inferences NOT drawn, D-CONG-UNIT): "
                 "AND-clash=%llu ITE-share=%llu "
                 "XOR-cancel=%llu | merges skipped as x==~x: %llu\n",
                 (unsigned long long)cong_n_and, (unsigned long long)cong_n_ite,
                 (unsigned long long)cong_n_xor,
                 (unsigned long long)cong_n_clash);
  if (std::getenv("TACTIX_CONG_TRACE"))
    std::fprintf(stderr, "[cong] %zu gates, %llu equivalences added\n",
                 gates.size(), (unsigned long long)neq);
  if (unsat)
    return ApplyResult::Unsat;
  return neq ? ApplyResult::Ok : ApplyResult::NoOp;
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
