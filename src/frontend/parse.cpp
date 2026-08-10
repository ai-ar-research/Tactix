#include "frontend/parse.hpp"

#include "policies/heuristics.hpp"
#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// N2 and the input path. A checker numbers the original file's clauses 1..N by
// position — lrat-check and cake_lpr both ignore the header count — so a chain
// citing id `i` asks it to propagate the clause the file holds at position `i`,
// and every normalization ingest_clause performs is a potential divergence.
// Every case is enumerated with its verdict in
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2; two are
// breaches, both fixed here. Repeats and already-root-false literals leave the
// stored clause narrower than the checker clause: restate it under a fresh id,
// using the root units as hints where needed. This is CaDiCaL's lemma shape
// (clause.cpp add_new_original_clause), deferred to end of input so no header
// need be trusted. A mid-input refutation is likewise deferred to finish_input.
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          const std::vector<Lit> &units, Ref ref) {
  // Record that the checker's clause `id` is not the clause just stored. The
  // count is unconditional, being the reachability probe for this path; the
  // restatement matters only with a proof, since without one no id names
  // anything.
  k.stats.input_restated++;
  if (!k.proof)
    return;
  Kernel::InputRestatement r;
  r.id = id;
  r.begin = static_cast<uint32_t>(k.input_restate_lits.size());
  r.size = static_cast<uint32_t>(lits.size());
  r.unit_begin = static_cast<uint32_t>(k.input_restate_units.size());
  r.unit_size = static_cast<uint32_t>(units.size());
  r.ref = ref;
  k.input_restate_lits.insert(k.input_restate_lits.end(), lits.begin(),
                              lits.end());
  k.input_restate_units.insert(k.input_restate_units.end(), units.begin(),
                               units.end());
  k.input_restatements.push_back(r);
}

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // Every input clause consumes one proof id in input order, including
  // tautologies and duplicates that are never stored (LRAT input numbering).
  const uint64_t id = ++k.next_clause_id;

  // Grow the kernel to cover every mentioned variable.
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty())
    kernel_reserve_vars(k, maxv + 1);

  // Kissat's default `tumble=1` imports variables by first occurrence and
  // activates the literals of each surviving clause in that same file order
  // (internal.c `kissat_add` -> flags.c `kissat_activate_literals`).  Retain
  // that order across the set-semantics check below.  Do not publish it yet: a
  // tautology activates nothing and returns below.
  const std::vector<Lit> file_order = scratch;
  for (const Lit l : file_order)
    kernel_record_import(k, var_of(l));
  std::vector<Var> activation_order;
  if (k.tumble) {
    activation_order.reserve(scratch.size());
    for (const Lit l : scratch)
      activation_order.push_back(var_of(l));
  }

  // Set semantics: dedupe; tautologies contribute nothing.  The sorted copy
  // is only the membership check.  Original clauses are stored in stable
  // first-occurrence file order, as Kissat does while all literals are
  // unassigned; sorting storage here changes the initial watched pair and the
  // order of every affected watch list.
  const size_t file_size = scratch.size();
  std::sort(scratch.begin(), scratch.end());
  scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
  for (size_t i = 1; i < scratch.size(); i++)
    if (scratch[i] == negate(scratch[i - 1]))
      return; // tautology
  const std::vector<Lit> canonical = scratch;
  std::vector<uint8_t> emitted(canonical.size(), 0);
  scratch.clear();
  scratch.reserve(canonical.size());
  for (const Lit lit : file_order) {
    const auto it = std::lower_bound(canonical.begin(), canonical.end(), lit);
    assert(it != canonical.end() && *it == lit);
    const size_t pos = static_cast<size_t>(it - canonical.begin());
    if (!emitted[pos]) {
      emitted[pos] = 1;
      scratch.push_back(lit);
    }
  }
  // Kissat internal.c:kissat_add normalizes every input clause against units
  // already fixed while parsing.  A root-true literal satisfies the clause;
  // a root-false literal is removed, with its true complement retained as the
  // LRAT antecedent for the deferred restatement below.  Tactix deliberately
  // does not propagate during parsing (derived ids cannot precede later input
  // ids), but explicit and earlier root-shrunk units still have the same
  // effect on every subsequent clause.
  std::vector<Lit> root_units;
  auto out = scratch.begin();
  for (const Lit lit : scratch) {
    const int8_t value = k.values[lit];
    if (value > 0)
      return; // satisfied input clause: consume its id, activate/store nothing
    if (value < 0)
      root_units.push_back(negate(lit));
    else
      *out++ = lit;
  }
  scratch.erase(out, scratch.end());

  // N2: the checker holds the file clause under `id`, repeats and all, so a
  // shrunk clause may not keep it. Restate at finish_input, but only when this
  // clause is actually stored — a dropped clause's id is never cited.
  const bool shrunk = scratch.size() < file_size;

  if (k.inconsistent)
    return; // already refuted; further clauses don't matter

  // Activation, kissat's `tumble` order (heuristics_seed_activation_order).
  // Placed after the tautology return and before the store so it matches
  // kissat's per-clause activation in `kissat_add`: a tautology activates
  // nothing, and first activation follows file-literal order rather than the
  // canonical sorted storage order.  input_activated removes repeated
  // literals in one clause and variables activated by an earlier clause.
  if (k.tumble) {
    if (k.input_activated.size() < k.nvars)
      k.input_activated.resize(k.nvars, 0);
    for (const Var v : activation_order) {
      if (k.values[make_lit(v, false)] == 0 && !k.input_activated[v]) {
        k.input_activated[v] = 1;
        k.input_activation_order.push_back(v);
      }
    }
  }

  if (scratch.empty()) { // empty input clause: immediate refutation
    k.inconsistent = true;
    k.input_refuted = true; // the input empty clause is the whole chain
    k.input_refutation_id = id;
    k.input_refutation_units = root_units;
  } else if (scratch.size() == 1) {
    const Lit u = scratch[0];
    if (k.values[u] > 0)
      return; // duplicate unit (its id is consumed, never referenced)
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      k.inconsistent = true;
      k.input_refuted = true;
      k.input_refutation_id = id;
      k.input_refutation_units = {negate(u)};
      return;
    }
    k.unit_ids[var_of(u)] = id;
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0);
    kernel_assign_unit(k, u);
  } else if (scratch.size() == 2) {
    // Duplicate binary: id consumed, never stored, following the duplicate-unit
    // precedent — the checker keeps its never-referenced input copy and no
    // proof line is emitted. Nothing is deleted during parse, so presence in
    // binary_ids is equivalent to a live stored copy. Kissat's dense mode
    // likewise dedups before inprocessing runs; duplicated pairs otherwise
    // inflate Factor's watch-score seeds and block BVE through the
    // singular-delete hazard, and on an input carrying six figures of them this
    // deduplication alone is worth an order of magnitude.
    if (k.binary_ids.count(kernel_binary_key(scratch[0], scratch[1])))
      return;
    kernel_add_binary_clause(k, scratch[0], scratch[1], id);
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0);
  } else {
    const Ref ref = kernel_add_large_clause(
        k, scratch.data(), static_cast<uint32_t>(scratch.size()), false, 0, id);
    if (shrunk)
      restate_input(k, id, scratch, root_units, ref);
  }
}

void finish_input(Kernel &k) {
  // Input ids are now settled: next_clause_id equals the number of input
  // clauses, exactly the checker's numbering, because ingest_clause consumes
  // one id per clause and nothing else consumed any. Derived ids start here.
  // Restatements are non-empty only when a proof was attached before the input;
  // attaching one later cannot be repaired, the input ids are already spent.
  assert(k.input_restatements.empty() || k.proof);
  // A declared but unmentioned variable is absent from Kissat's import map.
  // Tactix has already reserved it from the DIMACS header; rank it after every
  // actually imported variable so it cannot perturb reference-ordered ties.
  for (Var v = 0; v < k.nvars; v++)
    kernel_record_import(k, v);
  std::vector<uint64_t> chain;
  for (const Kernel::InputRestatement &r : k.input_restatements) {
    const Lit *lits = k.input_restate_lits.data() + r.begin;
    const uint64_t nid = ++k.next_clause_id;
    chain.clear();
    for (uint32_t i = 0; i < r.unit_size; i++) {
      const Lit unit = k.input_restate_units[r.unit_begin + i];
      assert(k.values[unit] > 0);
      const uint64_t uid = k.unit_ids[var_of(unit)];
      assert(uid);
      chain.push_back(uid);
    }
    // Root units first remove every root-false literal; the original file
    // clause then conflicts under the negated normalized clause. Duplicate-
    // only normalization has no units and retains the old one-hint proof.
    chain.push_back(r.id);
    k.proof->add_rup(nid, lits, r.size, chain.data(),
                     static_cast<uint32_t>(chain.size()));
    if (r.size == 1) {
      assert(k.unit_ids[var_of(lits[0])] == r.id);
      k.unit_ids[var_of(lits[0])] = nid;
    } else if (r.size == 2) {
      const uint64_t key = kernel_binary_key(lits[0], lits[1]);
      assert(k.binary_ids.at(key) == r.id);
      k.binary_ids.insert_or_assign(key, nid);
    } else {
      Clause *c = clause_at(k.arena, r.ref);
      assert(c->id() == r.id);
      c->set_id(nid);
    }
  }
  k.input_restatements.clear();
  k.input_restate_lits.clear();
  k.input_restate_units.clear();
  // The decision order is `H` state, so this is a no-op for an unguided kernel
  // and for a solver whose `H` is attached later — ScopedHeuristics re-seeds
  // from ascending order in that case, which is the documented `tumble=0`
  // behaviour rather than a silent half-application.
  heuristics_seed_activation_order(k, k.input_activation_order);
  k.input_activation_order.clear();
  k.input_activation_order.shrink_to_fit();
  k.input_activated.clear();
  k.input_activated.shrink_to_fit();
  if (k.input_refuted) {
    k.input_refuted = false;
    if (k.proof) {
      chain.clear();
      for (const Lit unit : k.input_refutation_units) {
        assert(k.values[unit] > 0);
        const uint64_t uid = k.unit_ids[var_of(unit)];
        assert(uid);
        chain.push_back(uid);
      }
      chain.push_back(k.input_refutation_id);
      k.proof->add_empty(++k.next_clause_id, chain.data(),
                         static_cast<uint32_t>(chain.size()));
    }
    k.input_refutation_units.clear();
  }
}

const char *parse_dimacs(FILE *f, Kernel &k) {
  std::vector<Lit> scratch;
  int c = getc(f);
  for (;;) {
    // Skip whitespace.
    while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      c = getc(f);
    if (c == EOF)
      break;
    if (c == 'c') { // comment line
      while (c != '\n' && c != EOF)
        c = getc(f);
      continue;
    }
    if (c == 'p') { // header: "p cnf <vars> <clauses>" (counts advisory)
      while (c != '\n' && c != EOF) {
        if (c >= '0' && c <= '9') { // first number = declared variables
          uint64_t n = 0;
          while (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            if (n > (1u << 28))
              return "variable count out of range";
            c = getc(f);
          }
          kernel_reserve_vars(k, static_cast<uint32_t>(n));
          while (c != '\n' && c != EOF) // ignore the clause count
            c = getc(f);
          break;
        }
        c = getc(f);
      }
      continue;
    }
    // a literal
    bool neg = false;
    if (c == '-') {
      neg = true;
      c = getc(f);
    }
    if (c < '0' || c > '9')
      return "expected literal";
    uint64_t n = 0;
    while (c >= '0' && c <= '9') {
      n = n * 10 + (c - '0');
      if (n > (1u << 28))
        return "literal out of range";
      c = getc(f);
    }
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != EOF)
      return "unexpected character after literal";
    if (n == 0) {
      ingest_clause(k, scratch);
      scratch.clear();
    } else {
      if (neg)
        scratch.push_back(internalize(-static_cast<int>(n)));
      else
        scratch.push_back(internalize(static_cast<int>(n)));
    }
  }
  if (!scratch.empty()) // tolerate a missing final 0
    ingest_clause(k, scratch);
  finish_input(k);
  return nullptr;
}

} // namespace tactix
