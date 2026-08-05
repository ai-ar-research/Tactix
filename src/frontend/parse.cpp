#include "frontend/parse.hpp"

#include "proof/proof.hpp"

#include <algorithm>

namespace tactix {

// N2 and the input path: the checker reads the ORIGINAL FILE and numbers its
// clauses 1..N by position (lrat-check and cake_lpr both ignore the header
// count and count what they read), so a chain citing id `i` asks it to
// propagate the clause the FILE has at position `i`. Everything ingest_clause
// does to an input clause is therefore a potential divergence. The complete
// enumeration, each with its verdict:
//   - REPEATED LITERAL — the one real breach, fixed here by restatement.
//     Checkers treat a clause as a literal LIST, not a set: `-1 -1` is not
//     unit for them, so citing it as a propagating antecedent draws
//     "multiple literals unassigned" / "not empty or singleton after
//     reduction". (A repeat that is FALSIFIED at check time is harmless,
//     which is why only some of these ever showed up.)
//   - TAUTOLOGY — dropped, and its id is consumed but never recorded
//     anywhere, so no chain can cite it. No divergence.
//   - LITERAL ORDER — we sort; checkers are order-insensitive within a
//     clause, and the order we store is never communicated. No divergence.
//   - DUPLICATE UNIT / DUPLICATE BINARY — the later copy is dropped and its
//     id never recorded; the first copy's id still names a clause the checker
//     holds verbatim. No divergence. (A repeated LARGE clause is not deduped
//     at all: both copies are stored under their own ids.)
//   - EMPTY INPUT CLAUSE / CONTRADICTING INPUT UNITS — the refutation itself
//     is fine, but emitting it mid-input consumed an id the checker had
//     already given to a LATER input clause. Fixed here by deferral (it was
//     accepted in practice only because it is always the proof's last line).
//   - CLAUSES AFTER A REFUTATION — dropped, ids still consumed, nothing
//     recorded. No divergence.
//   - A LYING HEADER — the declared clause count is advisory for us and for
//     both LRAT checkers, and neither side uses it to number clauses, so a
//     wrong count cannot misalign ids. (This is why the fix does NOT reserve
//     the input id range from the header the way CaDiCaL does; see below.)
//
// Keeping input ids aligned is what makes the fix non-trivial: the restated
// clause needs an id ABOVE every input id, and N is not known until the input
// ends. CaDiCaL solves it by trusting the header — reserve_ids(nCls) up front,
// then `original_id < reserved_ids ? ++original_id : ++clause_id`
// (src/internal.cpp), so a restatement can take a derived id mid-parse
// (add_new_original_clause, src/clause.cpp: when `original.size() > size` it
// emits the shrunk clause under a fresh id with the original as its single
// antecedent, then deletes the original). We take the same lemma shape but not
// the same id scheme: deferring to end-of-input needs no header at all, and it
// fixes the refutation-id collision above for free.

// Record "the checker's clause `id` is NOT the clause we just stored" — the
// file form repeated a literal and we stored the deduped one. finish_input
// turns this into a lemma and re-points the stored id. The COUNT is
// unconditional (it is a property of the instance, and `c input` is the
// reachability probe for this whole path); the restatement itself only
// matters with a proof, since without one no id names anything.
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          Ref ref) {
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
  // every input clause consumes one proof id in input order — including
  // tautologies/duplicates that are never stored (standard LRAT input
  // numbering)
  const uint64_t id = ++k.next_clause_id;

  // grow the kernel to cover every mentioned variable
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty())
    kernel_reserve_vars(k, maxv + 1);

  // set semantics: dedupe; tautologies contribute nothing
  const size_t file_size = scratch.size();
  std::sort(scratch.begin(), scratch.end());
  scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
  for (size_t i = 1; i < scratch.size(); i++)
    if (scratch[i] == negate(scratch[i - 1]))
      return; // tautology
  // N2: the checker holds the FILE clause under `id`, repeats and all, so a
  // shrunk clause may not keep it. Restate at finish_input — but only if we
  // actually STORE this one; a dropped clause's id is never cited.
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
      // The partner is read from unit_ids at finish_input, AFTER any
      // restatement has re-pointed it. `id` itself needs none: every copy of
      // `u` in the file form is falsified by the partner, so the checker sees
      // a conflicting — not propagating — antecedent whatever its width.
      k.input_refutation_var = var_of(u);
      return;
    }
    k.unit_ids[var_of(u)] = id;
    if (shrunk)
      restate_input(k, id, scratch, 0);
    kernel_assign_unit(k, u);
  } else if (scratch.size() == 2) {
    // duplicate binary: id consumed, never stored — the duplicate-unit
    // precedent above (the checker keeps its never-referenced input copy;
    // no proof lines). Nothing is deleted during parse, so presence in
    // binary_ids ⟺ a live stored copy. kissat's dense mode likewise
    // dedups before its inprocessing ever runs; duplicated input pairs
    // otherwise inflate Factor's watch-score seeds and block BVE via the
    // singular-delete hazard. On an instance whose input carries six figures
    // of duplicated pairs, this deduplication alone is worth an order of
    // magnitude.
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
  // Input ids are now settled: next_clause_id == the number of input clauses,
  // exactly the checker's numbering, because ingest_clause consumes one id per
  // clause and nothing else consumed any. Derived ids start here.
  // non-empty only if a proof was attached BEFORE the input (see restate_input
  // — attaching one later cannot be repaired, the input ids are already spent)
  assert(k.input_restatements.empty() || k.proof);
  for (const Kernel::InputRestatement &r : k.input_restatements) {
    const Lit *lits = k.input_restate_lits.data() + r.begin;
    const uint64_t nid = ++k.next_clause_id;
    // one antecedent: the file clause. Negating the deduped clause falsifies
    // every literal of the file form (same literal set), so it is RUP by that
    // clause alone — valid for LRAT and for DRAT. The original is left live;
    // a checker holding a superset only ever finds RUP easier.
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
    // skip whitespace
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
