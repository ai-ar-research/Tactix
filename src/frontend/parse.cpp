#include "frontend/parse.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// N2 and the input path. A checker numbers the original file's clauses 1..N by
// position — lrat-check and cake_lpr both ignore the header count — so a chain
// citing id `i` asks it to propagate the clause the file holds at position `i`,
// and every normalization ingest_clause performs is a potential divergence.
// Every case is enumerated with its verdict in
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2; two are
// breaches, both fixed here. A repeated literal leaves the stored clause
// narrower than the one the checker holds under that id, since a clause is a
// list and not a set: fixed by restating it under a fresh id, CaDiCaL's lemma
// shape (clause.cpp add_new_original_clause) but deferred to end of input, so
// no header need be trusted. A refutation emitted mid-input consumed an id
// already given to a later input clause: likewise deferred, to finish_input.
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          Ref ref) {
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
  r.ref = ref;
  k.input_restate_lits.insert(k.input_restate_lits.end(), lits.begin(),
                              lits.end());
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

  // Set semantics: dedupe; tautologies contribute nothing.
  const size_t file_size = scratch.size();
  std::sort(scratch.begin(), scratch.end());
  scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
  for (size_t i = 1; i < scratch.size(); i++)
    if (scratch[i] == negate(scratch[i - 1]))
      return; // tautology
  // N2: the checker holds the file clause under `id`, repeats and all, so a
  // shrunk clause may not keep it. Restate at finish_input, but only when this
  // clause is actually stored — a dropped clause's id is never cited.
  const bool shrunk = scratch.size() < file_size;

  if (k.inconsistent)
    return; // already refuted; further clauses don't matter

  if (scratch.empty()) { // empty input clause: immediate refutation
    k.inconsistent = true;
    k.input_refuted = true; // the input empty clause is the whole chain
    k.input_refutation_id = id;
  } else if (scratch.size() == 1) {
    const Lit u = scratch[0];
    if (k.values[u] > 0)
      return; // duplicate unit (its id is consumed, never referenced)
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      k.inconsistent = true;
      k.input_refuted = true;
      k.input_refutation_id = id;
      // The partner is read from unit_ids at finish_input, after any
      // restatement has re-pointed it. `id` itself needs none: every copy of
      // `u` in the file form is falsified by the partner, so the checker sees a
      // conflicting rather than propagating antecedent whatever its width.
      k.input_refutation_var = var_of(u);
      return;
    }
    k.unit_ids[var_of(u)] = id;
    if (shrunk)
      restate_input(k, id, scratch, 0);
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
      restate_input(k, id, scratch, 0);
  } else {
    const Ref ref = kernel_add_large_clause(
        k, scratch.data(), static_cast<uint32_t>(scratch.size()), false, 0, id);
    if (shrunk)
      restate_input(k, id, scratch, ref);
  }
}

void finish_input(Kernel &k) {
  // Input ids are now settled: next_clause_id equals the number of input
  // clauses, exactly the checker's numbering, because ingest_clause consumes
  // one id per clause and nothing else consumed any. Derived ids start here.
  // Restatements are non-empty only when a proof was attached before the input;
  // attaching one later cannot be repaired, the input ids are already spent.
  assert(k.input_restatements.empty() || k.proof);
  for (const Kernel::InputRestatement &r : k.input_restatements) {
    const Lit *lits = k.input_restate_lits.data() + r.begin;
    const uint64_t nid = ++k.next_clause_id;
    // One antecedent, the file clause: negating the deduped clause falsifies
    // every literal of the file form (same literal set), so it is RUP by that
    // clause alone, valid for LRAT and DRAT. The original is left live; a
    // checker holding a superset only ever finds RUP easier.
    const uint64_t chain[1] = {r.id};
    k.proof->add_rup(nid, lits, r.size, chain, 1);
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
  if (k.input_refuted) {
    k.input_refuted = false;
    if (k.proof) {
      uint64_t chain[2];
      uint32_t n = 0;
      if (k.input_refutation_var != UINT32_MAX)
        chain[n++] = k.unit_ids[k.input_refutation_var];
      chain[n++] = k.input_refutation_id;
      k.proof->add_empty(++k.next_clause_id, chain, n);
    }
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
