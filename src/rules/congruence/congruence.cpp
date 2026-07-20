#include "rules/congruence/congruence.hpp"

#include "kernel/kernel.hpp"
#include "proof/proof.hpp"
#include "rules/propagate/propagate.hpp" // inprocess_reach_root

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <unordered_map>
#include <utility>

namespace tactix {

namespace {

// arity cap — an AND gate over more than this many inputs is skipped (kissat caps
// too; keeps extraction and the equivalence proof chain bounded).
constexpr uint32_t CONG_ARITY_LIMIT = 64;
// safety cap on gates extracted per round.
constexpr size_t CONG_GATE_LIMIT = 20000000;

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

// Add one equivalence a==b (from two congruent gates) as the binaries (¬a∨b),
// (a∨¬b). Both are single-step RUP from the two AND gates' clauses:
//   (¬a∨b): assume a,¬b; a's binaries (¬a∨inp) force each inp; b's base clause
//           (b∨¬inp₁∨…) then conflicts. chain = a.input_bin_ids + [b.base_id].
//   (a∨¬b): mirror — chain = b.input_bin_ids + [a.base_id].
// Returns false normally; true if the addition derives ∅ (Unsat).
bool add_equivalence(Kernel &k, const CongGate &ga, const CongGate &gb) {
  const bool lrat = k.proof && k.proof->lrat();
  const Lit a = ga.output, b = gb.output;
  // emit (x ∨ y) with the given chain, unless it already exists.
  auto add_bin = [&](Lit x, Lit y, const std::vector<uint64_t> &inp_ids,
                     uint64_t base_id) {
    if (binary_exists(k, x, y))
      return;
    const uint64_t id = ++k.next_clause_id;
    const Lit lits[2] = {x, y};
    if (k.proof) {
      if (lrat) {
        std::vector<uint64_t> chain(inp_ids);
        chain.push_back(base_id);
        k.proof->add_rup(id, lits, 2, chain.data(),
                         static_cast<uint32_t>(chain.size()));
      } else {
        k.proof->add_rup(0, lits, 2, nullptr, 0);
      }
    }
    kernel_add_binary_clause(k, x, y, id);
  };
  add_bin(negate(a), b, ga.input_bin_ids, gb.base_id); // (¬a ∨ b)
  add_bin(a, negate(b), gb.input_bin_ids, ga.base_id); // (a ∨ ¬b)
  return false;
}

ApplyResult congruence_apply(Kernel &k) {
  assert(congruence_guard(k));
  if (inprocess_reach_root(k))
    return ApplyResult::Unsat;
  k.stats.congruence_rounds++;

  std::vector<CongGate> gates;
  congruence_extract(k, gates);
  if (gates.empty())
    return ApplyResult::NoOp;

  // Internal congruence closure (kissat), incremental worklist: a union-find over
  // literals; two gates whose inputs are equal UNDER the current representatives
  // have equivalent outputs → merge those outputs (add the equivalence binaries)
  // and re-process only the gates whose inputs contain the merged literal, so the
  // collapse propagates through the circuit layers in ONE apply without re-hashing
  // every gate each round. The gate CLAUSES are never rewritten — only the
  // union-find + the added binaries; V3 Substitute does the whole-formula collapse.
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

  std::unordered_map<std::vector<Lit>, uint32_t, InputsHash> table;
  table.reserve(gates.size() * 2);
  std::vector<std::vector<uint32_t>> occ(nlits); // canonical-input lit → gate ids
  std::deque<uint32_t> work;
  std::vector<uint8_t> inq(gates.size(), 1);
  for (uint32_t i = 0; i < gates.size(); i++)
    work.push_back(i);

  uint64_t neq = 0;
  bool unsat = false;
  std::vector<Lit> key;
  while (!work.empty() && !unsat) {
    const uint32_t i = work.front();
    work.pop_front();
    inq[i] = 0;
    const CongGate &g = gates[i];
    // canonical key = sorted representatives of the inputs
    key.assign(g.inputs.begin(), g.inputs.end());
    for (Lit &l : key)
      l = find(l);
    std::sort(key.begin(), key.end());
    bool degenerate = false; // AND(…,x,¬x,…)=0 — deferred; skip so we don't mis-hash
    for (size_t j = 1; j < key.size(); j++)
      if (key[j] == key[j - 1] || key[j] == negate(key[j - 1])) {
        degenerate = true;
        break;
      }
    if (degenerate)
      continue;
    for (const Lit l : key)
      occ[l].push_back(i); // register for rescheduling when an input merges
    auto it = table.find(key);
    if (it == table.end()) {
      table.emplace(key, i);
      continue;
    }
    const Lit oa = find(gates[it->second].output);
    const Lit ob = find(g.output);
    if (oa == ob || oa == negate(ob))
      continue; // already equal, or a contradiction (deferred)
    // congruent outputs → add oa==ob (DRAT-RUP; native-LRAT chains are increment 2)
    if (add_equivalence(k, gates[it->second], g))
      return ApplyResult::Unsat;
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

} // namespace

TACTIX_REGISTER_RULE(Congruence, RuleTier::Inprocess, congruence_guard,
                     congruence_apply, congruence_instances,
                     congruence_apply_instance);

} // namespace tactix
