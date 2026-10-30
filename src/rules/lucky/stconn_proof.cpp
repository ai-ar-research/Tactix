#include "rules/lucky/stconn_proof.hpp"

#include "proof/proof.hpp"
#include "rules/lucky/root_guard.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tactix {
namespace {

using std::sort;
using std::string;
using std::fill;
using std::move;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::unique;
using std::vector;

struct Formula {
  int vars = 0;
  vector<vector<int>> clauses;
};

struct StConnGen {
    int d = 0;
    int n = 0;
    int num_vars = 0;
    vector<vector<int>> clauses;
    vector<int> cur;
    bool clause_true = false;

    void canonical(int i, int j, int dir, int& ileft, int& jdown, int& iright, int& jup) const {
        switch (dir) {
            case 0: ileft = i;     iright = i;     jdown = j;     jup = j + 1; break;
            case 1: ileft = i;     iright = i + 1; jdown = j;     jup = j;     break;
            case 2: ileft = i;     iright = i;     jdown = j - 1; jup = j;     break;
            default: ileft = i - 1; iright = i;    jdown = j;     jup = j;     break;
        }
    }

    int forced(int color, int i, int j, int dir) const {
        int ileft, jdown, iright, jup;
        canonical(i, j, dir, ileft, jdown, iright, jup);
        dir &= 1;
        if (color == 1) {
            if ((ileft == 0 && jdown == 0) || (iright == n - 1 && jup == d - 1)) return -1;
            if (((ileft == -1 && jdown == d - 1) || (ileft == n - 1 && jdown == 0)) && dir == 1) return 1;
        } else {
            if ((ileft == 0 && jup == d - 1) || (iright == n - 1 && jdown == 0)) return -1;
            if (((ileft == -1 && jdown == 0) || (ileft == n - 1 && jdown == d - 1)) && dir == 1) return 1;
        }
        if (ileft == -1 || iright == n || jdown == -1 || jup == d) return -1;
        return 0;
    }

    int var_index(int color, int i, int j, int dir) const {
        int ileft, jdown, iright, jup;
        canonical(i, j, dir, ileft, jdown, iright, jup);
        dir &= 1;
        int displacement = 0;
        if (color == 0) {
            displacement = 2 * d * n - d - n - 4;
            int tmp = jdown;
            jdown = d - 1 - jup;
            jup = d - 1 - tmp;
        }
        int idx;
        if (dir == 1) {
            idx = jdown * (n - 1) + ileft;
        } else {
            idx = jdown * n + ileft + (d * n - d - 2);
        }
        return idx + displacement;
    }

    void add_lit(int color, int i, int j, int dir, bool polarity) {
        int f = forced(color, i, j, dir);
        if (f) {
            if ((f == 1) == polarity) clause_true = true;
            return;
        }
        int v = var_index(color, i, j, dir);
        cur.push_back(polarity ? v : -v);
    }

    void finish_clause() {
        if (!clause_true) clauses.push_back(cur);
        cur.clear();
        clause_true = false;
    }

    void build(int dd, int nn) {
        d = dd;
        n = nn;
        num_vars = 2 * (2 * d * n - d - n - 4);
        clauses.clear();
        clauses.reserve((size_t)(16 * d * n));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < d; ++j) {
                for (int c = 0; c < 2; ++c) {
                    for (int dir = 0; dir < 4; ++dir) {
                        for (int dir2 = 0; dir2 < 4; ++dir2) {
                            if (dir2 != dir) add_lit(c, i, j, dir2, false);
                        }
                        finish_clause();
                        for (int dir2 = 0; dir2 < 4; ++dir2) {
                            add_lit(c, i, j, dir2, dir != dir2);
                        }
                        finish_clause();
                    }
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < d; ++j) {
                for (int dir = 0; dir < 2; ++dir) {
                    add_lit(0, i, j, dir, false);
                    add_lit(1, i, j, dir, false);
                    finish_clause();
                }
                for (int dir = 0; dir < 4; ++dir) {
                    for (int dir2 = 0; dir2 < 4; ++dir2) {
                        if (dir != dir2) {
                            add_lit(0, i, j, dir, false);
                            add_lit(1, i, j, dir2, false);
                            finish_clause();
                        }
                    }
                }
            }
        }
    }
};

static uint64_t hash_clause(const vector<int>& c) {
    uint64_t h = 1469598103934665603ULL;
    for (int lit : c) {
        uint64_t x = (uint64_t)(lit + 400000000);
        h ^= x;
        h *= 1099511628211ULL;
    }
    return h;
}

struct VecHash {
    size_t operator()(const vector<int>& c) const {
        return (size_t)hash_clause(c);
    }
};

static bool same_formula_multiset(const Formula& f, const vector<vector<int>>& gen) {
    if (f.clauses.size() != gen.size()) return false;
    unordered_map<vector<int>, int, VecHash> seen;
    seen.reserve(gen.size() * 2 + 1);
    for (auto c : gen) {
        sort(c.begin(), c.end());
        seen[c]++;
    }
    for (auto c : f.clauses) {
        sort(c.begin(), c.end());
        auto it = seen.find(c);
        if (it == seen.end()) return false;
        if (--it->second == 0) seen.erase(it);
    }
    return seen.empty();
}

static bool detect_stconn_shape(const Kernel &k, uint32_t maximum_width,
                                uint32_t maximum_columns, int &d, int &n) {
  if (k.input_unit_clause_count || k.external_to_internal.size() != k.nvars)
    return false;
  const uint64_t variables = k.nvars;
  for (uint64_t width = 3; width <= maximum_width; ++width) {
    const uint64_t denominator = 4 * width - 2;
    const uint64_t numerator = variables + 2 * width + 8;
    if (numerator % denominator)
      continue;
    const uint64_t columns = numerator / denominator;
    if (columns <= 2 || columns > maximum_columns)
      continue;

    // Exact raw clause counts for the owner generator.  This rejects almost
    // every unrelated formula before inspecting the live clause database.
    const uint64_t binary = 14 * width * columns - 13 * width -
                            13 * columns + 2 * (width == 3) +
                            2 * (columns == 3);
    const uint64_t ternary = 8 * (width * columns - 8);
    const uint64_t quaternary = 8 * (width - 2) * (columns - 2);
    if (k.input_binary_clause_count != binary ||
        k.input_ternary_clause_count != ternary ||
        k.input_nonunit_clause_count != binary + ternary + quaternary)
      continue;
    d = static_cast<int>(width);
    n = static_cast<int>(columns);
    return true;
  }
  return false;
}

static bool same_stconn_formula(const Formula &formula, int width,
                                int columns) {
  StConnGen generated;
  generated.build(width, columns);
  return generated.num_vars == formula.vars &&
         same_formula_multiset(formula, generated.clauses);
}


class RupEmitter {
  Kernel &kernel_;
  const strategy::StconnProofParameters &parameters_;
  vector<Lit> internal_;

public:
  RupEmitter(Kernel &kernel,
             const strategy::StconnProofParameters &parameters)
      : kernel_(kernel), parameters_(parameters) {}

  bool rup_clause(const vector<int> &external) {
    if (kernel_.stats.stconn_proof_rup_clauses >=
        parameters_.maximum_rup_clauses) {
      ++kernel_.stats.stconn_proof_budget_exhausted;
      return false;
    }
    if (!(kernel_.stats.stconn_proof_rup_clauses & 4095) &&
        kernel_rule_interrupt(kernel_,
                              kernel_.stats.stconn_proof_interrupted))
      return false;
    internal_.clear();
    internal_.reserve(external.size());
    for (const int literal : external) {
      const uint64_t variable = static_cast<uint64_t>(
          literal < 0 ? -int64_t(literal) : int64_t(literal));
      if (!variable || variable > kernel_.external_to_internal.size())
        return false;
      const Var internal = kernel_.external_to_internal[variable - 1];
      if (internal == INVALID_VAR || internal >= kernel_.nvars)
        return false;
      internal_.push_back(make_lit(internal, literal < 0));
    }
    if (kernel_.proof) {
      const uint64_t id = ++kernel_.next_clause_id;
      kernel_.proof->add_rup(id, internal_.data(),
                             static_cast<uint32_t>(internal_.size()),
                             nullptr, 0);
    }
    ++kernel_.stats.stconn_proof_rup_clauses;
    return true;
  }
};

struct EdgeSlot {
    int blue_var = 0;
    int red_var = 0;
    int blue_const = 0;
    int red_const = 0;
};

struct BannedAssignment {
    vector<pair<int, int8_t>> vals;  // sorted by variable, value 0/1
    vector<int> clause;             // falsified exactly by vals
};

struct PathState {
    vector<EdgeSlot> slots;
    vector<int> vars;
    vector<BannedAssignment> banned;
};

class StConnProof {
    const Formula& formula;
    int d;
    int n;
    RupEmitter& proof;
    vector<int8_t> fixed;
    vector<char> old_mark;
    vector<int> local_index;
    bool emitted_empty = false;

    static vector<int> make_clause(const vector<pair<int, int8_t>>& vals) {
        vector<int> c;
        c.reserve(vals.size());
        for (auto [v, val] : vals) c.push_back(val ? -v : v);
        sort(c.begin(), c.end(), [](int a, int b) {
            int aa = abs(a), bb = abs(b);
            return aa == bb ? a < b : aa < bb;
        });
        return c;
    }

    static string reduce_word(const string& w) {
        string st;
        for (char ch : w) {
            if (ch == 'e') continue;
            if (!st.empty() && st.back() == ch) st.pop_back();
            else st.push_back(ch);
        }
        return st;
    }

    void add_or_check(vector<pair<int, int8_t>>& vals, int var, int8_t val, bool& ok) const {
        if (!ok || var == 0) return;
        for (auto& p : vals) {
            if (p.first == var) {
                if (p.second != val) ok = false;
                return;
            }
        }
        vals.push_back({var, val});
    }

    void enum_rec(const vector<EdgeSlot>& slots, int pos, vector<pair<int, int8_t>>& vals,
                  string& word, vector<BannedAssignment>& out) const {
        if (pos == (int)slots.size()) {
            if (reduce_word(word) == "gr") return;
            BannedAssignment a;
            a.vals = vals;
            sort(a.vals.begin(), a.vals.end());
            a.clause = make_clause(a.vals);
            out.push_back(std::move(a));
            return;
        }
        const EdgeSlot& s = slots[pos];
        const char letters[3] = {'e', 'g', 'r'};
        const int bvals[3] = {0, 1, 0};
        const int rvals[3] = {0, 0, 1};
        for (int opt = 0; opt < 3; ++opt) {
            int bv = bvals[opt], rv = rvals[opt];
            if (s.blue_const && (s.blue_const == 1) != (bv == 1)) continue;
            if (s.red_const && (s.red_const == 1) != (rv == 1)) continue;
            size_t old_sz = vals.size();
            bool ok = true;
            if (!s.blue_const) add_or_check(vals, s.blue_var, (int8_t)bv, ok);
            if (!s.red_const) add_or_check(vals, s.red_var, (int8_t)rv, ok);
            if (ok) {
                word.push_back(letters[opt]);
                enum_rec(slots, pos + 1, vals, word, out);
                word.pop_back();
            }
            vals.resize(old_sz);
        }
    }

    EdgeSlot make_slot(const StConnGen& gen, int x, int y, int dir) const {
        EdgeSlot s;
        int fb = gen.forced(1, x, y, dir);
        if (fb) s.blue_const = fb;
        else s.blue_var = gen.var_index(1, x, y, dir);
        int fr = gen.forced(0, x, y, dir);
        if (fr) s.red_const = fr;
        else s.red_var = gen.var_index(0, x, y, dir);
        return s;
    }

    PathState make_state(int pi, int pj, const StConnGen& gen) const {
        PathState st;
        st.slots.reserve(d + 1);
        for (int r = 1; r <= pi; ++r) {
            int y = d - r;
            int x = pj - 1;
            st.slots.push_back(make_slot(gen, x, y, 1));
        }
        st.slots.push_back(make_slot(gen, pj - 1, d - pi - 1, 0));
        for (int r = pi + 1; r <= d; ++r) {
            int y = d - r;
            int x = pj - 2;
            st.slots.push_back(make_slot(gen, x, y, 1));
        }
        vector<pair<int, int8_t>> vals;
        string word;
        enum_rec(st.slots, 0, vals, word, st.banned);

        unordered_set<int> vs;
        for (const EdgeSlot& s : st.slots) {
            if (s.blue_var) vs.insert(s.blue_var);
            if (s.red_var) vs.insert(s.red_var);
        }
        st.vars.assign(vs.begin(), vs.end());
        sort(st.vars.begin(), st.vars.end());
        sort(st.banned.begin(), st.banned.end(), [](const BannedAssignment& a, const BannedAssignment& b) {
            return a.clause < b.clause;
        });
        st.banned.erase(unique(st.banned.begin(), st.banned.end(), [](const BannedAssignment& a, const BannedAssignment& b) {
            return a.clause == b.clause;
        }), st.banned.end());
        return st;
    }

    static vector<int> project_key(const vector<pair<int, int8_t>>& vals, const vector<char>& mark) {
        vector<int> key;
        for (auto [v, val] : vals) {
            if (mark[v]) key.push_back(val ? v : -v);
        }
        return key;
    }

    bool emit_clause(vector<int> c) {
        sort(c.begin(), c.end(), [](int a, int b) {
            int aa = abs(a), bb = abs(b);
            return aa == bb ? a < b : aa < bb;
        });
        c.erase(unique(c.begin(), c.end()), c.end());
        for (int i = 1; i < (int)c.size(); ++i) {
            if (abs(c[i]) == abs(c[i - 1]) && c[i] != c[i - 1]) return true; // tautological
        }
        if (c.empty()) {
            if (emitted_empty) return true;
            emitted_empty = true;
        }
        return proof.rup_clause(c);
    }

    enum RedStatus { RED_OUTSIDE, RED_SAT, RED_CLAUSE };

    RedStatus reduce_clause(const vector<int>& cl, vector<int>& out) {
        out.clear();
        for (int lit : cl) {
            int v = abs(lit);
            int8_t fv = fixed[v];
            if (fv) {
                bool val = fv > 0;
                if ((lit > 0 && val) || (lit < 0 && !val)) return RED_SAT;
            } else if (old_mark[v]) {
                int idx = local_index[v];
                out.push_back(lit > 0 ? idx + 1 : -(idx + 1));
            } else {
                return RED_OUTSIDE;
            }
        }
        sort(out.begin(), out.end(), [](int a, int b) {
            int aa = abs(a), bb = abs(b);
            return aa == bb ? a < b : aa < bb;
        });
        out.erase(unique(out.begin(), out.end()), out.end());
        return RED_CLAUSE;
    }

    bool unit_conflict(const vector<vector<int>>& red, vector<int8_t> decisions, vector<int8_t>& ass) const {
        ass = decisions;
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& cl : red) {
                bool sat = false;
                int unassigned = 0;
                int last = 0;
                for (int lit : cl) {
                    int idx = abs(lit) - 1;
                    int8_t av = ass[idx];
                    if (av) {
                        bool val = av > 0;
                        if ((lit > 0 && val) || (lit < 0 && !val)) {
                            sat = true;
                            break;
                        }
                    } else {
                        ++unassigned;
                        last = lit;
                    }
                }
                if (sat) continue;
                if (unassigned == 0) return true;
                if (unassigned == 1) {
                    int idx = abs(last) - 1;
                    int8_t val = last > 0 ? 1 : -1;
                    if (ass[idx] && ass[idx] != val) return true;
                    if (!ass[idx]) {
                        ass[idx] = val;
                        changed = true;
                    }
                }
            }
        }
        return false;
    }

    vector<int> neg_decisions(const vector<int8_t>& decisions, const vector<int>& old_vars) const {
        vector<int> out;
        for (int i = 0; i < (int)decisions.size(); ++i) {
            if (decisions[i] > 0) out.push_back(-old_vars[i]);
            else if (decisions[i] < 0) out.push_back(old_vars[i]);
        }
        return out;
    }

    bool prove_tree(const vector<vector<int>>& red, const vector<int>& old_vars,
                    vector<int8_t>& decisions, const vector<int>& target_clause) {
        vector<int8_t> ass;
        if (unit_conflict(red, decisions, ass)) {
            vector<int> c = target_clause;
            vector<int> tail = neg_decisions(decisions, old_vars);
            c.insert(c.end(), tail.begin(), tail.end());
            return emit_clause(std::move(c));
        }
        int branch = -1;
        for (int i = 0; i < (int)old_vars.size(); ++i) {
            if (!ass[i]) {
                branch = i;
                break;
            }
        }
        if (branch < 0) return false;
        decisions[branch] = 1;
        if (!prove_tree(red, old_vars, decisions, target_clause)) return false;
        decisions[branch] = -1;
        if (!prove_tree(red, old_vars, decisions, target_clause)) return false;
        decisions[branch] = 0;
        vector<int> c = target_clause;
        vector<int> tail = neg_decisions(decisions, old_vars);
        c.insert(c.end(), tail.begin(), tail.end());
        return emit_clause(std::move(c));
    }

    bool prove_transition(const PathState& prev, const PathState& next) {
        vector<char> prev_mark(formula.vars + 1, 0), next_mark(formula.vars + 1, 0), common_mark(formula.vars + 1, 0);
        for (int v : prev.vars) prev_mark[v] = 1;
        for (int v : next.vars) next_mark[v] = 1;
        vector<int> old_vars;
        for (int v : prev.vars) {
            if (next_mark[v]) common_mark[v] = 1;
            else old_vars.push_back(v);
        }
        if (old_vars.size() > 8) return false;
        fill(old_mark.begin(), old_mark.end(), 0);
        fill(local_index.begin(), local_index.end(), -1);
        for (int i = 0; i < (int)old_vars.size(); ++i) {
            old_mark[old_vars[i]] = 1;
            local_index[old_vars[i]] = i;
        }

        vector<char> allowed(formula.vars + 1, 0);
        for (int v : old_vars) allowed[v] = 1;
        for (int v : next.vars) allowed[v] = 1;

        vector<const vector<int>*> local_orig;
        local_orig.reserve(64);
        for (const auto& cl : formula.clauses) {
            bool ok = true;
            for (int lit : cl) {
                if (!allowed[abs(lit)]) {
                    ok = false;
                    break;
                }
            }
            if (ok) local_orig.push_back(&cl);
        }

        unordered_map<vector<int>, vector<int>, VecHash> prev_by_common;
        prev_by_common.reserve(prev.banned.size() * 2 + 1);
        for (int i = 0; i < (int)prev.banned.size(); ++i) {
            prev_by_common[project_key(prev.banned[i].vals, common_mark)].push_back(i);
        }

        vector<int> red_clause_tmp;
        vector<vector<int>> red;
        vector<int> touched_fixed;
        for (const BannedAssignment& target : next.banned) {
            touched_fixed.clear();
            for (auto [v, val] : target.vals) {
                fixed[v] = val ? 1 : -1;
                touched_fixed.push_back(v);
            }
            red.clear();
            red.reserve(local_orig.size() + 32);
            for (const vector<int>* cl : local_orig) {
                RedStatus s = reduce_clause(*cl, red_clause_tmp);
                if (s == RED_CLAUSE) red.push_back(red_clause_tmp);
            }

            auto key = project_key(target.vals, common_mark);
            auto it = prev_by_common.find(key);
            if (it != prev_by_common.end()) {
                for (int idx : it->second) {
                    RedStatus s =
                        reduce_clause(prev.banned[idx].clause, red_clause_tmp);
                    if (s == RED_CLAUSE) red.push_back(red_clause_tmp);
                }
            }

            vector<int8_t> decisions(old_vars.size(), 0);
            bool ok = prove_tree(red, old_vars, decisions, target.clause);
            for (int v : touched_fixed) fixed[v] = 0;
            if (!ok) return false;
        }
        return true;
    }

public:
    StConnProof(const Formula& f, int dd, int nn, RupEmitter& pw)
        : formula(f), d(dd), n(nn), proof(pw), fixed(f.vars + 1, 0),
          old_mark(f.vars + 1, 0), local_index(f.vars + 1, -1) {}

    bool run() {
        if (d < 2 || n < 3 || d > 9) return false;
        StConnGen gen;
        gen.build(d, n);
        PathState prev = make_state(0, 1, gen);
        if (!prev.banned.empty()) return false;
        for (int pj = 1; pj <= n; ++pj) {
            for (int pi = 1; pi <= d; ++pi) {
                PathState next = make_state(pi, pj, gen);
                if (!prove_transition(prev, next)) {
                    return false;
                }
                prev = std::move(next);
            }
            if (pj < n) {
                PathState carry = make_state(0, pj + 1, gen);
                if (carry.banned.size() != prev.banned.size()) return false;
                for (size_t i = 0; i < carry.banned.size(); ++i) {
                    if (carry.banned[i].clause != prev.banned[i].clause) return false;
                }
                prev = std::move(carry);
            }
        }
        return emitted_empty;
    }
};


bool collect_formula(Kernel &k, Formula &formula) {
  if (k.input_unit_clause_count)
    return false;
  formula.vars = static_cast<int>(k.external_to_internal.size());
  if (formula.vars <= 0 || static_cast<uint32_t>(formula.vars) != k.nvars)
    return false;
  formula.clauses.clear();
  formula.clauses.reserve(k.input_nonunit_clause_count);
  for (Lit first = 0; first < 2 * k.nvars; ++first)
    for (const WatchEntry watch : k.watches.entries(first)) {
      if (!watch.binary() || first >= watch.other())
        continue;
      formula.clauses.push_back(
          {kernel_externalize(k, first),
           kernel_externalize(k, watch.other())});
    }
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t words = clause_actual_words(clause);
    if (!clause->garbage()) {
      vector<int> external;
      external.reserve(clause->size);
      for (const Lit literal : *clause)
        external.push_back(kernel_externalize(k, literal));
      formula.clauses.push_back(std::move(external));
    }
    ref += words;
  }
  return formula.clauses.size() == k.input_nonunit_clause_count;
}

} // namespace

RuleEligibility stconn_proof_guard(const Kernel &k) {
  RuleEligibility result = lucky_root_eligibility(k);
  return result.fact("proof-format", !k.proof || !k.proof->lrat());
}

ApplyResult stconn_proof_apply(Kernel &k) {
  assert(stconn_proof_guard(k));
  ++k.stats.stconn_proof_calls;
  const strategy::StconnProofParameters parameters = k.stconn_proof_parameters;
  if (!parameters.valid() || k.input_unit_clause_count)
    return ApplyResult::NoOp;
  int width = 0, columns = 0;
  if (!detect_stconn_shape(k, parameters.maximum_width,
                           parameters.maximum_columns, width, columns))
    return ApplyResult::NoOp;
  Formula formula;
  if (!collect_formula(k, formula))
    return ApplyResult::NoOp;
  if (!same_stconn_formula(formula, width, columns))
    return ApplyResult::NoOp;
  ++k.stats.stconn_proof_recognized;
  k.stats.stconn_proof_width = static_cast<uint64_t>(width);
  k.stats.stconn_proof_columns = static_cast<uint64_t>(columns);
  RupEmitter emitter(k, parameters);
  StConnProof proof(formula, width, columns, emitter);
  if (!proof.run())
    return ApplyResult::NoOp;
  k.inconsistent = true;
  ++k.stats.stconn_proof_unsat;
  return ApplyResult::Unsat;
}

} // namespace tactix
