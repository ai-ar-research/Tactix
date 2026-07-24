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

// Iterating an unordered_map is implementation-ordered (libstdc++ and libc++
// disagree), and anything feeding gate/emission ORDER feeds the closure's
// merge order and hence the search trajectory — which must not fork across
// toolchains (the battleship lesson, Log 2026-07-23). Extraction loops
// iterate via these sorted keys instead.
static std::vector<const std::vector<Lit> *> sorted_keys(const ClauseIndex &index) {
  std::vector<const std::vector<Lit> *> keys;
  keys.reserve(index.size());
  for (const auto &e : index)
    keys.push_back(&e.first);
  std::sort(keys.begin(), keys.end(),
            [](const std::vector<Lit> *a, const std::vector<Lit> *b) {
              return *a < *b;
            });
  return keys;
}

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
  for (const std::vector<Lit> *base_p : sorted_keys(index)) {
    const std::vector<Lit> &base = *base_p;
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
    // The relation's parity R. A clause whose negated-literal set is N is
    // falsified by exactly the assignment "vⱼ true iff j ∈ N", whose true-count
    // parity is |N| ≡ base_parity; requiring ALL clauses of that negation parity
    // therefore forbids exactly the assignments of true-parity base_parity, so
    //     ⊕ⱼ vⱼ = base_parity ⊕ 1 =: R.
    // For output vᵢ, ⊕_{j≠i} vⱼ = R ⊕ vᵢ, so the output literal is pos[i]
    // flipped by R. (Until 2026-07-21 this also XORed in `s`, which negated the
    // output of every even-arity gate. That was invisible: the only consumer is
    // XOR↔XOR key matching, and cancellation removes variables in even multiples,
    // so two gates sharing a key share their arity parity and the offset cancels
    // in the equivalence. It is NOT invisible to a unit/merge channel that reads
    // the output's polarity — see PROGRESS item C0.)
    const Lit r = static_cast<Lit>(base_parity ^ 1);
    for (uint32_t i = 0; i < s; i++) {
      CongGate g;
      g.kind = GateKind::Xor;
      g.output = pos[i] ^ r; // value = R ⊕ vᵢ = ⊕_{j≠i} vⱼ
      for (uint32_t j = 0; j < s; j++)
        if (j != i)
          g.inputs.push_back(pos[j]); // all positive, already sorted
      out.push_back(std::move(g));
    }
  }
}

bool normalize_ite(Lit &cond, Lit &then_l, Lit &else_l); // defined below

// A ladder lemma: RUP-checkable, cited by the level below it, and dead once the
// conclusion is in. Kept so the round can `del` them (D-CONG-DELETE) — kissat
// deletes its two extended shrinking lemmas the same way, immediately after
// emitting the conclusion. LRAT deletes by id; DRAT needs the literals.
struct TransientLemma {
  uint64_t id;
  std::vector<Lit> lits;
};

// Drop the transient ladder lemmas, newest first (a checker's DB is a set, but
// reverse order keeps the stream readable as an unwinding of the ladder).
void delete_transients(Kernel &k, std::vector<TransientLemma> &t) {
  if (k.proof)
    for (size_t i = t.size(); i-- > 0;)
      k.proof->del(k.next_clause_id, t[i].id, t[i].lits.data(),
                   static_cast<uint32_t>(t[i].lits.size()));
  t.clear();
}

// TACTIX_CONG_PROBE: count the degenerate-gate discards by cause. These are the
// inferences V6 does NOT draw (deviation D-CONG-UNIT) — the diagnostic that sizes
// closing it. Env-gated, default-identical, like TACTIX_CONG_TRACE.
static const bool cong_probe = std::getenv("TACTIX_CONG_PROBE") != nullptr;
static uint64_t cong_n_and = 0, cong_n_ite = 0, cong_n_xor = 0, cong_n_clash = 0;

// What canonicalizing a gate under the current representatives yielded.
// A degenerate gate is NOT a dead end — each degeneracy is itself an inference
// over F (calculus §6), which is what deviation D-CONG-UNIT was about.
enum class Canon {
  Ok,    // `key` + `out_lit` valid — file the gate in the table
  Unit,  // the gate forces `out_lit` to a constant: the root unit is ¬out_lit
  Merge, // the gate collapsed to out_lit ≡ key[0] — no second gate needed
  Skip,  // degenerate in a way this rule does not exploit (ITE)
};

// Canonicalize gate g under the current union-find into (key, out_lit): key is the
// structural hash (positive, sorted per kind); out_lit is the literal whose class
// is merged on a key collision, and whose VALUE is f(key). `cancelled` receives
// the distinct positive representative variables that dropped out of an XOR key
// (each occurred an even number of times) — the ladder in
// `add_xor_degeneracy_chain` quantifies over exactly those.
Canon canonicalize(const CongGate &g, const std::function<Lit(Lit)> &find,
                   std::vector<Lit> &key, Lit &out_lit,
                   std::vector<Lit> &cancelled) {
  cancelled.clear();
  if (g.kind == GateKind::And) {
    key.assign(g.inputs.begin(), g.inputs.end());
    for (Lit &l : key)
      l = find(l);
    std::sort(key.begin(), key.end());
    out_lit = g.output;
    // AND(…,x,¬x,…) = 0 ⇒ root unit ¬out, and AND(…,x,x,…) = AND(…,x,…) ⇒ a
    // possible unary gate: both are real inferences (calculus §6) and both are
    // still deferred. Deliberately NOT part of this increment — TACTIX_CONG_PROBE
    // measured them firing 0 times on hwmcc17miters, and their LRAT chains need
    // care the XOR channel does not (a gate with a duplicated input carries a
    // duplicated `input_bin_ids` hint, which is neither unit nor falsified at its
    // position in the chain). PROGRESS item C3b.
    for (size_t j = 1; j < key.size(); j++)
      if (key[j] == key[j - 1] || key[j] == negate(key[j - 1])) {
        if (cong_probe) cong_n_and++;
        return Canon::Skip;
      }
    return Canon::Ok;
  }
  if (g.kind == GateKind::Ite) {
    Lit cond = find(g.inputs[0]), th = find(g.inputs[1]), el = find(g.inputs[2]);
    // defer any ITE whose (representative) operands share a variable — then==else,
    // then==¬else (an XOR), or cond touching a branch collapse it to a simpler
    // gate that a later increment/propagation handles; skipping stays sound.
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

// --- native-LRAT chains by RUP replay (D-CONG-LRAT) -------------------------
// V6's three lemma families (the degeneracy ladder, the XOR matching ladder, the
// ITE case-split) are each RUP over the current DB, but their antecedent chains
// are painful to derive by hand: canonicalization folds a SIGN layer on top of
// the union-find bridging, so a canonical lemma's literals do not correspond
// by-position-and-sign to either gate's raw clauses. That intricacy is why
// D-CONG-LRAT stayed open.
//
// So do not derive them — REPLAY them. Assume the negation of the lemma at a
// probing scope, propagate, and read the antecedents off the conflict's reason
// walk. This is the same primitive V1 Probe and C3 Vivify already use, and it is
// strictly more trustworthy than hand-derivation: the chain comes out in the
// checker's own propagation order by construction, which is the one property a
// hand-built chain silently gets wrong (`substitute.cpp` big_path_chain's
// "chain order is load-bearing" note).
//
// Returns false when the lemma is NOT RUP over the DB (no conflict) — the caller
// must then emit nothing and skip the inference (D-CONG-SKIP). A lemma that is
// genuinely RUP but whose replay fails would be a bug in the ladder, and this
// fails safe rather than emitting an unverifiable line.
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
      // `lits[i]` is already TRUE under the assumptions made so far, so
      // assuming its negation conflicts immediately — the reason that forced it
      // IS the refutation. (Missing this case is not a missed optimisation: on a
      // miter the ladder's own prefix propagates the conclusion, so treating it
      // as "not RUP" loses exactly the inferences the ladder exists for.)
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
    std::vector<uint8_t> seen(k.nvars, 0);
    if (implied != UINT32_MAX)
      // the walk includes the implied literal's OWN reason, and sorting by trail
      // position puts it last — so it closes the chain, no separate conflict id
      kernel_reason_walk(k, implied, seen, entries);
    else
      last = kernel_conflict_reason_walk(k, seen, entries);
  }
  k.conflict_valid = false;
  k.conflict_ref = INVALID_REF;
  if (k.level > 0)
    kernel_backtrack(k, 0, /*update_phases=*/false);
  kernel_probing_end(k);
  // A lemma that fails to replay is a dropped inference (D-CONG-SKIP), which the
  // `congruence_unjustified` stat counts. Under TACTIX_CONG_PROBE, say WHICH —
  // a nonzero count with no obvious cause is how a ladder bug hides.
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
  return true;
}

// Degeneracy proof chain for a SINGLE XOR gate (kissat's
// add_xor_shrinking_proof_chain is the same construction). A gate whose canonical
// key collapsed forces its output, and that conclusion is NOT RUP on its own —
// measured: drat-trim rejects a bare add of the unit, because the gate's parity
// clauses are (k+1)-ary and propagate nothing until all but one literal is
// assigned. The ladder supplies the missing resolution steps.
//
// `concl` are the conclusion clauses: {¬out_lit} for the constant case, or the
// two equivalence binaries for the unary case. `cancelled` are the distinct
// POSITIVE representative variables that dropped out of the key.
//
// For size = |cancelled| … 1, emit every sign pattern over cancelled[0..size-1]
// appended to each conclusion. The TOP level's negation assigns every cancelled
// representative, which the bridging equivalence binaries propagate to all of the
// gate's raw inputs, falsifying one parity clause; each lower level closes on the
// two lemmas directly above it (they differ only in the sign of the dropped
// variable, so one goes unit and the other falsifies); the size-0 lemmas ARE the
// conclusions, added persistently by the caller with the chains returned here.
//
// Validated against drat-trim BEFORE implementation, in all four shapes —
// |cancelled| = 1 and 2, both relation parities, and the unary-survivor form —
// with the bare conclusion rejected in every one. See the 2026-07-21 log entry.
bool add_xor_degeneracy_chain(Kernel &k, const std::vector<Lit> &cancelled,
                              const std::vector<std::vector<Lit>> &concl,
                              std::vector<std::vector<uint64_t>> &concl_chains,
                              std::vector<TransientLemma> &transient) {
  const bool lrat = k.proof && k.proof->lrat();
  const size_t nc = concl.size();
  concl_chains.assign(nc, {});
  if (!k.proof)
    return true;
  // XOR arity is capped at CONG_XOR_ARITY and cancellation removes variables in
  // pairs, so this is ≤ 2 in practice; the ladder is 2^(n+1)−2 lemmas.
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
        if (!lrat) {
          k.proof->add_rup(0, lemma.data(),
                           static_cast<uint32_t>(lemma.size()), nullptr, 0);
          continue;
        }
        if (size == top) {
          if (!congruence_rup_chain(k, lemma.data(),
                                    static_cast<uint32_t>(lemma.size()), chain))
            return false; // not RUP — emit nothing, caller skips (D-CONG-SKIP)
        } else {
          const size_t hi = static_cast<size_t>(m | (1u << size));
          chain.assign({prev[static_cast<size_t>(m) * nc + c],
                        prev[hi * nc + c]});
        }
        const uint64_t id = ++k.next_clause_id;
        k.proof->add_rup(id, lemma.data(), static_cast<uint32_t>(lemma.size()),
                         chain.data(), static_cast<uint32_t>(chain.size()));
        cur[static_cast<size_t>(m) * nc + c] = id;
        transient.push_back({id, lemma}); // D-CONG-DELETE: dropped after the merge
      }
    prev.swap(cur);
  }
  if (lrat)
    for (size_t c = 0; c < nc; c++)
      concl_chains[c] = {prev[c], prev[nc + c]}; // the two size-1 parents
  return true;
}

// XOR-match proof chain (kissat add_xor_matching_proof_chain): make the two
// equivalence binaries (¬a∨b),(a∨¬b) checkable. `rhs` = the shared canonical
// (representative) inputs, all positive; a,b = the two sign-adjusted outputs.
// A parity-elimination ladder: eliminate one rhs variable per level, at each
// level enumerating all sign patterns of the still-present prefix and emitting
// {prefix,¬a,b} and {prefix,a,¬b}. Under DRAT each is a bare add. Under LRAT the
// TOP level is replayed (congruence_rup_chain) and every lower level closes on
// its two parents, exactly as the degeneracy ladder does — so the sign layer that
// canonicalization folds on top of the union-find bridging never has to be
// unpicked by hand. Returns false if the top level does not replay.
// `rhs.back()` is absorbed by the gates' clauses, so the ladder starts one short.
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
  if (top < 1)
    return true; // nothing to eliminate; the binaries stand on the gates alone
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
  for (const std::vector<Lit> *cl_p : sorted_keys(index)) {
    const std::vector<Lit> &cl = *cl_p; // sorted 3 lits
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
  std::vector<Lit> ce_keys; // canonical order (see sorted_keys)
  ce_keys.reserve(ce.size());
  for (const auto &kv : ce)
    ce_keys.push_back(kv.first);
  std::sort(ce_keys.begin(), ce_keys.end());
  for (const Lit L : ce_keys) {
    auto &lst = ce[L];
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
        // A candidate with else ≡ ¬then is a ternary XOR wearing an ITE costume:
        // its four defining clauses become (cond∨then∨L),(cond∨¬then∨¬L),
        // (¬cond∨¬then∨L),(¬cond∨then∨¬L) — negation counts 0,2,2,2, i.e. exactly
        // the complete even-parity clause set over {cond,then,L}. So
        // congruence_extract_xor finds the SAME relation from the SAME clauses and
        // emits 3 XOR gates for it, and the ITE copies can only ever be discarded:
        // `unite` maintains repr[¬big]=¬repr[big], so find(else)==¬find(then)
        // holds forever and the gate is degenerate on every canonicalization.
        // Dropping them here is free — measured on hwmcc17miters: 463 596 of the
        // 493 584 ITE degenerate discards, and the XOR twin already draws every
        // one of their inferences in the same round (0 new merges, 0 new units).
        // kissat drops the same candidates at construction ("skipping ternary XOR
        // gate", congruence.c new_ite_gate).
        if (else_l == negate(then_l))
          continue;
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
// binaries (¬a∨b),(a∨¬b) RUP; each is a resolvent of one cond-side clause of each
// gate. Under DRAT they are bare adds the checker replays in order; under LRAT
// each is REPLAYED for its chain (congruence_rup_chain) and the two binaries then
// close on their two cond-sides. Returns false if any lemma fails to replay — the
// caller emits nothing further and skips the merge (D-CONG-SKIP).
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

// Add the equivalence a==b from two congruent AND gates as (¬a∨b),(a∨¬b).
// Under the fixpoint the two gates' inputs are equal only UNDER THE CURRENT
// REPRESENTATIVES (bridged by earlier added equivalences), so the LRAT chain must
// thread those bridging binaries — the naive `input_bin_ids + base_id` is only
// valid for a direct (unbridged) match. For (¬a∨b): assume a,¬b; a's input
// binaries force ga's inputs, `big_path_chain` bridges each ga input to the gb
// input in its representative class, and gb's base then conflicts. (a∨¬b) mirrors.
// DRAT emits bare adds (the checker searches).
//
// **Returns false iff the equivalence could NOT be justified** — under LRAT the
// bridging chain can fail (a `big_path_chain` whose only route runs through a
// root-assigned literal is not traversable), and there is then no checkable line
// to emit. The caller must SKIP the merge: losing an inference is sound, emitting
// an unverifiable proof line is not (N2). All-or-nothing — both binaries are
// justified before either is added, so the formula never carries one half of an
// equivalence the union-find believes in whole. (Before 2026-07-21 a failed chain
// silently emitted `add_rup(id, lits, 2, nullptr, 0)`, i.e. an LRAT line with an
// EMPTY hint list, which every checker rejects — measured on both lrat-check and
// cake_lpr. Unreachable then only because nothing assigned mid-round; C3's unit
// channel makes it reachable.)
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
  // Justify FIRST, mutate second. Under LRAT each direction's chain is built (and
  // kept) before anything is added, so a failure aborts the whole equivalence.
  std::vector<uint64_t> chain_fwd, chain_rev;
  if (lrat) {
    const bool need_fwd = !binary_exists(k, negate(a), b);
    const bool need_rev = !binary_exists(k, a, negate(b));
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
    if (binary_exists(k, x, y))
      return;
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

// Add the equivalence a==b as (¬a∨b),(a∨¬b). Under DRAT these are bare adds: the
// preceding proof-chain lemmas have already made them RUP and the checker
// searches for the antecedents itself. Under LRAT each binary carries the chain
// its ladder returned (its two size-1 parents), so no hint-less line is ever
// emitted (the invariant C1 established).
void add_equivalence_chained(Kernel &k, Lit a, Lit b,
                             const std::vector<uint64_t> &chain_fwd,
                             const std::vector<uint64_t> &chain_rev) {
  const bool lrat = k.proof && k.proof->lrat();
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &ch) {
    if (binary_exists(k, x, y))
      return;
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, lits, 2, lrat ? ch.data() : nullptr,
                       lrat ? static_cast<uint32_t>(ch.size()) : 0);
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b, chain_fwd);
  add_bin(a, negate(b), chain_rev);
}

} // namespace — public rule entry points (header-declared) below

void congruence_extract_gates(const Kernel &k, std::vector<CongGate> &out,
                              bool with_xor_ite) {
  congruence_extract(k, out);
  if (with_xor_ite) {
    ClauseIndex index; // built once, shared by XOR + ITE extraction
    build_clause_index(k, index);
    congruence_extract_xor(k, index, out);
    congruence_extract_ite(k, index, out);
  }
}

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
  // D-CONG-LRAT CLOSED: XOR and ITE now run on every proof path. Their lemma
  // chains are replayed rather than derived (congruence_rup_chain), so the sign
  // layer canonicalization folds on top of the union-find bridging — the reason
  // this was DRAT-only — never has to be unpicked by hand.
  congruence_extract_gates(k, gates, /*with_xor_ite=*/true);
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
  std::vector<Lit> key, key_j, cancelled, cancelled_j;
  std::vector<TransientLemma> transient; // ladder lemmas, del'd after each use

  // Root units derived from degenerate gates (D-CONG-UNIT). Asserted in a batch
  // once the closure is done, exactly as V1 Probe and V3 Substitute do — the
  // closure itself reasons purely over the union-find, so a unit changes nothing
  // it has already computed, and M1 owns the propagation cascade. Deduped per
  // variable; an opposite-sign pair within one round IS a refutation ({u} and
  // {¬u} resolve to ∅ — V3's record_unit, corpus 007).
  std::vector<Lit> units;
  std::vector<Lit> pending_unit(k.nvars, INVALID_LIT);
  auto record_unit = [&](Lit u, const std::vector<uint64_t> &uchain) -> bool {
    const Var v = var_of(u);
    if (k.value(u) > 0)
      return false; // already true at the root — the inference adds nothing
    // Read the incumbent id BEFORE overwriting it: if u contradicts a unit
    // already on the trail, that unit's id is the second antecedent of ∅.
    const uint64_t trail_id = k.unit_ids[v];
    const bool contradicts_trail = k.value(u) < 0;
    const uint64_t id = ++k.next_clause_id;
    if (k.proof)
      k.proof->add_rup(lrat ? id : 0, &u, 1, lrat ? uchain.data() : nullptr,
                       lrat ? static_cast<uint32_t>(uchain.size()) : 0);
    if (contradicts_trail) {
      if (k.proof) {
        if (lrat) {
          const uint64_t ch[2] = {id, trail_id};
          k.proof->add_empty(++k.next_clause_id, ch, 2);
        } else {
          k.proof->add_empty(0, nullptr, 0);
        }
      }
      k.inconsistent = true;
      return true;
    }
    if (pending_unit[v] == INVALID_LIT) {
      pending_unit[v] = u;
      k.unit_ids[v] = id;
      units.push_back(u);
      k.stats.congruence_units++;
      return false;
    }
    if (pending_unit[v] == u)
      return false; // same-sign duplicate (the s gates of one relation agree)
    if (k.proof) { // opposite sign: {u} + {¬u} ⊢ ∅
      if (lrat) {
        const uint64_t ch[2] = {id, k.unit_ids[v]};
        k.proof->add_empty(++k.next_clause_id, ch, 2);
      } else {
        k.proof->add_empty(0, nullptr, 0);
      }
    }
    k.inconsistent = true;
    return true;
  };

  // Merge two provably-equivalent literals: unite their classes and reschedule
  // the gates whose canonical inputs just changed. The proof for the equivalence
  // is the CALLER's job (it differs per kind and per degeneracy).
  auto merge_lits = [&](Lit x, Lit y) {
    const Lit oa = find(x), ob = find(y);
    if (oa == ob)
      return;
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

  while (!work.empty() && !unsat) {
    const uint32_t i = work.front();
    work.pop_front();
    inq[i] = 0;
    Lit out_i;
    const Canon ci = canonicalize(gates[i], find, key, out_i, cancelled);
    if (ci == Canon::Skip)
      continue; // degenerate in a way this rule does not exploit — sound
    // --- D-CONG-UNIT: a degenerate gate is an inference, not a dead end ------
    if (ci == Canon::Unit) {
      // value(out_lit) = ⊕key = ⊕∅ = 0, so ¬out_lit is a root unit. Only XOR
      // reaches here (the AND/ITE degeneracies still return Skip).
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
      // value(out_lit) = ⊕key = key[0], so out_lit ≡ key[0]: a merge with no
      // second gate needed (calculus §6's unary gate).
      const Lit other = key[0];
      if (find(out_i) == find(other))
        continue; // already known
      if (find(out_i) == negate(find(other))) {
        if (cong_probe) cong_n_clash++;
        continue; // x ≡ ¬x — the refutation is C3b (measured: 0 occurrences)
      }
      const std::vector<std::vector<Lit>> concl = {{negate(out_i), other},
                                                  {out_i, negate(other)}};
      std::vector<std::vector<uint64_t>> cch;
      transient.clear();
      if (!add_xor_degeneracy_chain(k, cancelled, concl, cch, transient)) {
        k.stats.congruence_unjustified++;
        continue; // ladder did not replay — emit nothing, drop the inference
      }
      add_equivalence_chained(k, out_i, other, cch[0], cch[1]);
      delete_transients(k, transient);
      merge_lits(out_i, other);
      continue;
    }
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
    // the SAME current key — then their outputs are provably equal. A stored gate
    // that has since become degenerate is evicted, not exploited here: it is still
    // in the worklist (its inputs merged, which is what rescheduled it), so its
    // own pop draws the inference.
    const uint32_t j = it->second;
    Lit out_j;
    if (canonicalize(gates[j], find, key_j, out_j, cancelled_j) != Canon::Ok ||
        key_j != key) {
      it->second = i; // evict stale/degenerate entry; current gate represents key
      continue;
    }
    const Lit oa = find(out_j);
    const Lit ob = find(out_i);
    if (oa == ob || oa == negate(ob)) {
      if (cong_probe && oa == negate(ob)) cong_n_clash++; 
      continue; // already equal, or a contradiction (deferred)
    }
    // congruent outputs → add oa==ob (DRAT-RUP over the full DB; XOR/ITE LRAT
    // chains are C3). An AND equivalence whose bridging chain cannot be built is
    // SKIPPED rather than emitted unjustified — sound, and N2 holds (see
    // add_equivalence).
    if (gates[i].kind == GateKind::Xor || gates[i].kind == GateKind::Ite) {
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
        if (cong_probe) std::fprintf(stderr, "[cong-probe] MATCH chain failed (kind=%d)\n", (int)gates[i].kind);
        continue; // no checkable chain — drop the inference, keep N2
      }
      add_equivalence_chained(k, out_j, out_i, cf, cr);
      delete_transients(k, transient);
    } else if (!add_equivalence(k, gates[j], gates[i], find)) {
      k.stats.congruence_unjustified++;
      continue; // no checkable chain — drop the inference, keep the proof valid
    }
    merge_lits(out_j, out_i);
    if (k.inconsistent) {
      unsat = true;
      break;
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

  // Assert the batch of derived root units, then close the root. This is the
  // step that makes the unit channel worth having: on an equivalence-checking
  // miter, congruence proves aᵢ ≡ bᵢ for the two circuits' matching signals,
  // every output XOR then cancels to the unit ¬outᵢ, and propagating those units
  // falsifies the miter's output constraint — a refutation inside congruence at
  // zero conflicts, which is how kissat solves that family.
  for (const Lit u : units) {
    assert(k.value(u) >= 0 && "record_unit refutes rather than batching a false unit");
    if (k.value(u) == 0)
      kernel_assign_unit(k, u);
  }
  if (!units.empty() && inprocess_reach_root(k))
    return ApplyResult::Unsat;
  if (k.inconsistent)
    return ApplyResult::Unsat;
  return (neq || !units.empty()) ? ApplyResult::Ok : ApplyResult::NoOp;
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
