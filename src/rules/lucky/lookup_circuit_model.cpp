#include "rules/lucky/lookup_circuit_model.hpp"

#include "rules/lucky/model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace tactix {
namespace {

struct Gate {
  std::vector<Var> inputs;
  std::vector<Var> outputs;
  std::vector<uint32_t> input_external;
  std::vector<uint32_t> output_external;
  std::vector<uint16_t> table;
};

struct Circuit {
  uint32_t variables = 0;
  uint32_t fanin = 0;
  uint32_t fanout = 0;
  uint32_t primary_inputs = 0;
  uint64_t clauses_per_gate = 0;
  std::vector<Var> primary_variables;
  std::vector<Gate> gates;
  std::vector<std::pair<Var, uint8_t>> fixed;
};

uint64_t gate_clause_count(uint32_t fanin, uint32_t fanout) {
  if (!fanin || !fanout || fanin > 8 || fanout > 8)
    return 0;
  return (uint64_t{1} << fanin) * ((uint64_t{1} << fanout) - 1);
}

bool choose_shape(const Kernel &k,
                  const strategy::LookupCircuitModelParameters &parameters,
                  Circuit &circuit) {
  if (!k.input_nonunit_uniform || !k.input_unit_clause_count ||
      !k.input_nonunit_clause_count || !k.input_uniform_nonunit_width)
    return false;
  const uint64_t variables = k.external_to_internal.size();
  if (variables != k.nvars)
    return false;
  bool found = false;
  uint32_t best_score = 0, best_inputs = 0, best_gates = 0;
  for (uint32_t fanin = 1; fanin <= parameters.maximum_fanin; ++fanin)
    for (uint32_t fanout = 1; fanout <= parameters.maximum_fanout; ++fanout) {
      if (fanin + fanout != k.input_uniform_nonunit_width)
        continue;
      const uint64_t per_gate = gate_clause_count(fanin, fanout);
      if (!per_gate || k.input_nonunit_clause_count % per_gate)
        continue;
      const uint64_t gates = k.input_nonunit_clause_count / per_gate;
      if (!gates || gates > UINT32_MAX ||
          gates > variables / fanout)
        continue;
      const uint64_t inputs = variables - gates * fanout;
      if (!inputs || inputs > parameters.maximum_primary_inputs ||
          k.input_unit_clause_count > gates * fanout)
        continue;
      const uint32_t score = fanin > fanout ? fanin - fanout
                                            : fanout - fanin;
      if (!found || score < best_score ||
          (score == best_score && inputs < best_inputs) ||
          (score == best_score && inputs == best_inputs &&
           gates > best_gates)) {
        found = true;
        best_score = score;
        best_inputs = static_cast<uint32_t>(inputs);
        best_gates = static_cast<uint32_t>(gates);
        circuit.fanin = fanin;
        circuit.fanout = fanout;
        circuit.primary_inputs = best_inputs;
        circuit.clauses_per_gate = per_gate;
      }
    }
  if (!found)
    return false;
  circuit.variables = k.nvars;
  circuit.gates.resize(best_gates);
  return true;
}

bool external_variable(const Kernel &k, Lit literal, uint32_t &result) {
  const int external = kernel_externalize(k, literal);
  const int64_t magnitude = external < 0 ? -int64_t(external)
                                         : int64_t(external);
  if (!magnitude || magnitude > UINT32_MAX)
    return false;
  result = static_cast<uint32_t>(magnitude);
  return result <= k.external_to_internal.size();
}

bool map_external(const Kernel &k, uint32_t external, Var &internal) {
  if (!external || external > k.external_to_internal.size())
    return false;
  internal = k.external_to_internal[external - 1];
  return internal != INVALID_VAR && internal < k.nvars;
}

bool reconstruct(Kernel &k, Circuit &circuit) {
  const uint32_t input_rows = uint32_t{1} << circuit.fanin;
  const uint32_t output_rows = uint32_t{1} << circuit.fanout;
  const uint32_t words = (output_rows + 63) / 64;
  const uint64_t total_rows =
      uint64_t{circuit.gates.size()} * input_rows;
  if (total_rows > SIZE_MAX / words)
    return false;
  std::vector<uint64_t> forbidden(static_cast<size_t>(total_rows) * words, 0);
  circuit.primary_variables.resize(circuit.primary_inputs);
  for (uint32_t external = 1; external <= circuit.primary_inputs; ++external)
    if (!map_external(k, external,
                      circuit.primary_variables[external - 1]))
      return false;

  uint64_t nonunit = 0, visited = 0;
  for (Ref ref = 0; ref < k.arena.size();) {
    const Clause *clause = clause_at(k.arena, ref);
    const uint32_t actual_words = clause_actual_words(clause);
    if (!clause->garbage()) {
      if (clause->size != k.input_uniform_nonunit_width ||
          nonunit >= k.input_nonunit_clause_count)
        return false;
      const uint32_t gate_index =
          static_cast<uint32_t>(nonunit / circuit.clauses_per_gate);
      Gate &gate = circuit.gates[gate_index];
      if (gate.inputs.empty()) {
        gate.inputs.resize(circuit.fanin);
        gate.outputs.resize(circuit.fanout);
        gate.input_external.resize(circuit.fanin);
        gate.output_external.resize(circuit.fanout);
        gate.table.assign(input_rows, UINT16_MAX);
        for (uint32_t i = 0; i < circuit.fanin; ++i) {
          uint32_t external = 0;
          if (!external_variable(k, clause->lits[i], external) ||
              !map_external(k, external, gate.inputs[i]))
            return false;
          gate.input_external[i] = external;
        }
        for (uint32_t i = 0; i < circuit.fanout; ++i) {
          uint32_t external = 0;
          if (!external_variable(k, clause->lits[circuit.fanin + i], external) ||
              external != circuit.primary_inputs +
                              gate_index * circuit.fanout + i + 1 ||
              !map_external(k, external, gate.outputs[i]))
            return false;
          gate.output_external[i] = external;
        }
      } else {
        for (uint32_t i = 0; i < circuit.fanin; ++i) {
          uint32_t external = 0;
          if (!external_variable(k, clause->lits[i], external) ||
              external != gate.input_external[i])
            return false;
        }
        for (uint32_t i = 0; i < circuit.fanout; ++i) {
          uint32_t external = 0;
          if (!external_variable(k, clause->lits[circuit.fanin + i], external) ||
              external != gate.output_external[i])
            return false;
        }
      }
      uint32_t input = 0, output = 0;
      for (uint32_t i = 0; i < circuit.fanin; ++i)
        if (negated(clause->lits[i]))
          input |= uint32_t{1} << i;
      for (uint32_t i = 0; i < circuit.fanout; ++i)
        if (negated(clause->lits[circuit.fanin + i]))
          output |= uint32_t{1} << i;
      const uint64_t row = uint64_t{gate_index} * input_rows + input;
      forbidden[static_cast<size_t>(row) * words + output / 64] |=
          uint64_t{1} << (output % 64);
      ++nonunit;
    }
    ref += actual_words;
    if (!(++visited & ((uint64_t{1} << 18) - 1)) &&
        kernel_rule_interrupt(k, k.stats.lookup_circuit_model_interrupted))
      return false;
  }
  if (nonunit != k.input_nonunit_clause_count)
    return false;

  for (uint32_t gate_index = 0; gate_index < circuit.gates.size();
       ++gate_index) {
    Gate &gate = circuit.gates[gate_index];
    if (gate.inputs.size() != circuit.fanin)
      return false;
    for (uint32_t input = 0; input < input_rows; ++input) {
      const uint64_t row = uint64_t{gate_index} * input_rows + input;
      uint32_t missing = UINT32_MAX, missing_count = 0;
      for (uint32_t output = 0; output < output_rows; ++output)
        if (!(forbidden[static_cast<size_t>(row) * words + output / 64] &
              (uint64_t{1} << (output % 64)))) {
          missing = output;
          ++missing_count;
        }
      if (missing_count != 1)
        return false;
      gate.table[input] = static_cast<uint16_t>(missing);
    }
  }

  circuit.fixed.clear();
  for (Var variable = 0; variable < k.nvars; ++variable) {
    const int value = k.value(make_lit(variable, false));
    if (value)
      circuit.fixed.emplace_back(variable, value > 0);
  }
  return circuit.fixed.size() >= k.input_unit_clause_count;
}

bool structural_support(const Circuit &circuit, uint64_t &support) {
  if (!circuit.primary_inputs || circuit.primary_inputs > 64)
    return false;
  std::vector<uint64_t> variable_support(circuit.variables, 0);
  for (uint32_t i = 0; i < circuit.primary_inputs; ++i)
    variable_support[circuit.primary_variables[i]] = uint64_t{1} << i;
  for (const Gate &gate : circuit.gates) {
    uint64_t gate_support = 0;
    const uint32_t first_output =
        *std::min_element(gate.output_external.begin(),
                          gate.output_external.end());
    for (uint32_t i = 0; i < gate.inputs.size(); ++i) {
      if (gate.input_external[i] >= first_output)
        return false;
      gate_support |= variable_support[gate.inputs[i]];
    }
    for (const Var output : gate.outputs)
      variable_support[output] = gate_support;
  }
  support = 0;
  for (const auto &[variable, value] : circuit.fixed) {
    (void)value;
    support |= variable_support[variable];
  }
  return support != 0;
}

void forward_fill(const Circuit &circuit, std::vector<uint8_t> &assignment) {
  for (const Gate &gate : circuit.gates) {
    uint32_t input = 0;
    for (uint32_t i = 0; i < gate.inputs.size(); ++i)
      if (assignment[gate.inputs[i]])
        input |= uint32_t{1} << i;
    const uint32_t output = gate.table[input];
    for (uint32_t i = 0; i < gate.outputs.size(); ++i)
      assignment[gate.outputs[i]] = (output >> i) & 1;
  }
}

enum class EnumerationResult { Unavailable, Exhausted, Found, Interrupted };

#if defined(__x86_64__) || defined(__i386__)
struct alignas(64) LaneValue {
  __m512i value;
};
struct alignas(64) GateTableVectors {
  __m512i part[4];
};

__attribute__((target("avx512f,avx512bw,avx512vbmi,avx512vl")))
EnumerationResult enumerate_avx512(
    Kernel &k, const Circuit &circuit, uint64_t support,
    const strategy::LookupCircuitModelParameters &parameters,
    std::vector<uint8_t> &assignment) {
  const uint32_t support_count = static_cast<uint32_t>(__builtin_popcountll(support));
  if (!support_count || support_count > parameters.maximum_support)
    return EnumerationResult::Unavailable;
  const uint64_t batches = support_count <= 6
                               ? 1
                               : uint64_t{1} << (support_count - 6);
  if (circuit.gates.size() &&
      batches > parameters.maximum_vector_gate_evaluations /
                    circuit.gates.size())
    return EnumerationResult::Unavailable;

  std::vector<uint32_t> support_variables;
  for (uint32_t i = 0; i < circuit.primary_inputs; ++i)
    if (support & (uint64_t{1} << i))
      support_variables.push_back(circuit.primary_variables[i]);

  const uint32_t segments = uint32_t{1} << (circuit.fanin > 6
                                                ? circuit.fanin - 6
                                                : 0);
  std::vector<GateTableVectors> tables(circuit.gates.size());
  alignas(64) uint8_t bytes[64];
  for (uint32_t gate_index = 0; gate_index < circuit.gates.size();
       ++gate_index) {
    for (uint32_t segment = 0; segment < 4; ++segment)
      tables[gate_index].part[segment] = _mm512_setzero_si512();
    for (uint32_t segment = 0; segment < segments; ++segment) {
      for (uint32_t lane = 0; lane < 64; ++lane) {
        const uint32_t input = segment * 64 + lane;
        bytes[lane] = input < circuit.gates[gate_index].table.size()
                          ? static_cast<uint8_t>(
                                circuit.gates[gate_index].table[input])
                          : 0;
      }
      tables[gate_index].part[segment] =
          _mm512_load_si512(static_cast<const void *>(bytes));
    }
  }

  __m512i low_patterns[6];
  for (uint32_t bit = 0; bit < 6; ++bit) {
    for (uint32_t lane = 0; lane < 64; ++lane)
      bytes[lane] = (lane >> bit) & 1;
    low_patterns[bit] =
        _mm512_load_si512(static_cast<const void *>(bytes));
  }
  const __m512i zero = _mm512_setzero_si512();
  const __m512i one = _mm512_set1_epi8(1);
  __m512i output_bits[8];
  for (uint32_t bit = 0; bit < 8; ++bit)
    output_bits[bit] = _mm512_set1_epi8(static_cast<char>(1 << bit));
  const __mmask64 valid = support_count < 6
                              ? (__mmask64{1} << (uint32_t{1} << support_count)) - 1
                              : ~__mmask64{0};
  std::vector<LaneValue> values(circuit.variables);

  for (uint64_t batch = 0; batch < batches; ++batch) {
    if (!(batch & ((uint64_t{1} << 12) - 1)) &&
        kernel_rule_interrupt(k, k.stats.lookup_circuit_model_interrupted))
      return EnumerationResult::Interrupted;
    for (const Var variable : circuit.primary_variables)
      values[variable].value = zero;
    for (uint32_t bit = 0; bit < support_count; ++bit)
      values[support_variables[bit]].value =
          bit < 6 ? low_patterns[bit]
                  : ((batch >> (bit - 6)) & 1 ? one : zero);

    for (uint32_t gate_index = 0; gate_index < circuit.gates.size();
         ++gate_index) {
      const Gate &gate = circuit.gates[gate_index];
      __m512i index = zero;
      for (uint32_t i = 0; i < std::min(circuit.fanin, uint32_t{6}); ++i)
        index = _mm512_or_si512(
            index, _mm512_slli_epi16(values[gate.inputs[i]].value, i));
      __m512i output =
          _mm512_permutexvar_epi8(index, tables[gate_index].part[0]);
      if (circuit.fanin >= 7) {
        const __m512i upper =
            _mm512_permutexvar_epi8(index, tables[gate_index].part[1]);
        const __mmask64 bit6 = _mm512_cmpneq_epi8_mask(
            values[gate.inputs[6]].value, zero);
        output = _mm512_mask_blend_epi8(bit6, output, upper);
      }
      if (circuit.fanin >= 8) {
        const __m512i lower2 =
            _mm512_permutexvar_epi8(index, tables[gate_index].part[2]);
        const __m512i upper2 =
            _mm512_permutexvar_epi8(index, tables[gate_index].part[3]);
        const __mmask64 bit6 = _mm512_cmpneq_epi8_mask(
            values[gate.inputs[6]].value, zero);
        const __m512i half =
            _mm512_mask_blend_epi8(bit6, lower2, upper2);
        const __mmask64 bit7 = _mm512_cmpneq_epi8_mask(
            values[gate.inputs[7]].value, zero);
        output = _mm512_mask_blend_epi8(bit7, output, half);
      }
      for (uint32_t i = 0; i < circuit.fanout; ++i)
        values[gate.outputs[i]].value = _mm512_srli_epi16(
            _mm512_and_si512(output, output_bits[i]), i);
    }
    k.stats.lookup_circuit_model_vector_evaluations += circuit.gates.size();

    __m512i mismatch = zero;
    for (const auto &[variable, expected] : circuit.fixed) {
      const __m512i wanted = expected ? one : zero;
      mismatch = _mm512_or_si512(
          mismatch, _mm512_xor_si512(values[variable].value, wanted));
    }
    const __mmask64 bad = _mm512_cmpneq_epi8_mask(mismatch, zero);
    const __mmask64 good = (~bad) & valid;
    if (!good)
      continue;
    const uint32_t lane = static_cast<uint32_t>(
        __builtin_ctzll(static_cast<uint64_t>(good)));
    const uint64_t index = batch * 64 + lane;
    assignment.assign(circuit.variables, 0);
    for (uint32_t bit = 0; bit < support_count; ++bit)
      assignment[support_variables[bit]] = (index >> bit) & 1;
    forward_fill(circuit, assignment);
    return EnumerationResult::Found;
  }
  return EnumerationResult::Exhausted;
}
#endif

EnumerationResult enumerate_primary(
    Kernel &k, const Circuit &circuit, uint64_t support,
    const strategy::LookupCircuitModelParameters &parameters,
    std::vector<uint8_t> &assignment) {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_cpu_init();
  if (__builtin_cpu_supports("avx512f") &&
      __builtin_cpu_supports("avx512bw") &&
      __builtin_cpu_supports("avx512vbmi") &&
      __builtin_cpu_supports("avx512vl"))
    return enumerate_avx512(k, circuit, support, parameters, assignment);
#else
  (void)k;
  (void)circuit;
  (void)support;
  (void)parameters;
  (void)assignment;
#endif
  return EnumerationResult::Unavailable;
}

class CspSearch {
public:
  CspSearch(Kernel &kernel, const Circuit &circuit)
      : k_(kernel), circuit_(circuit), assignment_(circuit.variables, -1),
        occurrences_(circuit.variables), queued_(circuit.gates.size(), 0) {
    for (uint32_t gate = 0; gate < circuit.gates.size(); ++gate) {
      for (const Var variable : circuit.gates[gate].inputs)
        occurrences_[variable].push_back(gate);
      for (const Var variable : circuit.gates[gate].outputs)
        occurrences_[variable].push_back(gate);
      enqueue(gate);
    }
  }

  bool solve() {
    for (const auto &[variable, value] : circuit_.fixed)
      if (!assign(variable, value))
        return false;
    return dfs();
  }
  bool interrupted() const { return interrupted_; }
  const std::vector<int8_t> &assignment() const { return assignment_; }

private:
  Kernel &k_;
  const Circuit &circuit_;
  std::vector<int8_t> assignment_;
  std::vector<Var> trail_;
  std::vector<std::vector<uint32_t>> occurrences_;
  std::vector<uint32_t> queue_;
  std::vector<uint8_t> queued_;
  size_t queue_head_ = 0;
  bool interrupted_ = false;

  void enqueue(uint32_t gate) {
    if (!queued_[gate]) {
      queued_[gate] = 1;
      queue_.push_back(gate);
    }
  }
  bool charge(uint64_t amount = 1) {
    k_.stats.lookup_circuit_model_csp_work += amount;
    if (!(k_.stats.lookup_circuit_model_csp_work &
          ((uint64_t{1} << 14) - 1)) &&
        kernel_rule_interrupt(k_, k_.stats.lookup_circuit_model_interrupted))
      interrupted_ = true;
    return !interrupted_;
  }
  bool assign(Var variable, uint8_t value) {
    if (assignment_[variable] >= 0)
      return assignment_[variable] == value;
    assignment_[variable] = static_cast<int8_t>(value);
    trail_.push_back(variable);
    for (const uint32_t gate : occurrences_[variable])
      enqueue(gate);
    return true;
  }
  void undo(size_t mark) {
    while (trail_.size() > mark) {
      assignment_[trail_.back()] = -1;
      trail_.pop_back();
    }
  }
  bool candidates(const Gate &gate, std::vector<uint32_t> *masks,
                  uint32_t *input_and, uint32_t *input_or,
                  uint32_t *output_and, uint32_t *output_or,
                  uint32_t *count) {
    uint32_t ia = (uint32_t{1} << circuit_.fanin) - 1, io = 0;
    uint32_t oa = (uint32_t{1} << circuit_.fanout) - 1, oo = 0, found = 0;
    if (masks)
      masks->clear();
    for (uint32_t input = 0; input < (uint32_t{1} << circuit_.fanin);
         ++input) {
      if (!charge())
        return false;
      bool compatible = true;
      for (uint32_t i = 0; i < circuit_.fanin; ++i) {
        const int8_t value = assignment_[gate.inputs[i]];
        if (value >= 0 && value != static_cast<int8_t>((input >> i) & 1)) {
          compatible = false;
          break;
        }
      }
      if (!compatible)
        continue;
      const uint32_t output = gate.table[input];
      for (uint32_t i = 0; i < circuit_.fanout; ++i) {
        const int8_t value = assignment_[gate.outputs[i]];
        if (value >= 0 && value != static_cast<int8_t>((output >> i) & 1)) {
          compatible = false;
          break;
        }
      }
      if (!compatible)
        continue;
      ++found;
      ia &= input;
      io |= input;
      oa &= output;
      oo |= output;
      if (masks)
        masks->push_back(input);
    }
    if (input_and)
      *input_and = ia;
    if (input_or)
      *input_or = io;
    if (output_and)
      *output_and = oa;
    if (output_or)
      *output_or = oo;
    if (count)
      *count = found;
    return found != 0;
  }
  bool propagate() {
    while (queue_head_ < queue_.size()) {
      const uint32_t gate_index = queue_[queue_head_++];
      queued_[gate_index] = 0;
      const Gate &gate = circuit_.gates[gate_index];
      uint32_t ia = 0, io = 0, oa = 0, oo = 0, count = 0;
      if (!candidates(gate, nullptr, &ia, &io, &oa, &oo, &count))
        return false;
      for (uint32_t i = 0; i < circuit_.fanin; ++i)
        if (assignment_[gate.inputs[i]] < 0 &&
            ((ia >> i) & 1) == ((io >> i) & 1) &&
            !assign(gate.inputs[i], (ia >> i) & 1))
          return false;
      for (uint32_t i = 0; i < circuit_.fanout; ++i)
        if (assignment_[gate.outputs[i]] < 0 &&
            ((oa >> i) & 1) == ((oo >> i) & 1) &&
            !assign(gate.outputs[i], (oa >> i) & 1))
          return false;
      if (queue_head_ > 4096 && queue_head_ * 2 > queue_.size()) {
        queue_.erase(queue_.begin(), queue_.begin() + queue_head_);
        queue_head_ = 0;
      }
    }
    return !interrupted_;
  }
  bool complete() const {
    return std::find(assignment_.begin(), assignment_.end(), int8_t{-1}) ==
           assignment_.end();
  }
  bool dfs() {
    const size_t mark = trail_.size();
    if (!propagate()) {
      undo(mark);
      return false;
    }
    if (complete())
      return true;

    uint32_t best = UINT32_MAX, best_count = UINT32_MAX;
    std::vector<uint32_t> best_masks, temporary;
    for (uint32_t reverse = circuit_.gates.size(); reverse; --reverse) {
      const uint32_t gate_index = reverse - 1;
      const Gate &gate = circuit_.gates[gate_index];
      bool open = false;
      for (const Var variable : gate.inputs)
        open |= assignment_[variable] < 0;
      for (const Var variable : gate.outputs)
        open |= assignment_[variable] < 0;
      if (!open)
        continue;
      uint32_t count = 0;
      if (!candidates(gate, &temporary, nullptr, nullptr, nullptr, nullptr,
                      &count)) {
        undo(mark);
        return false;
      }
      if (count > 1 && count < best_count) {
        best = gate_index;
        best_count = count;
        best_masks = temporary;
        if (count == 2)
          break;
      }
    }

    if (best != UINT32_MAX) {
      const Gate &gate = circuit_.gates[best];
      if (circuit_.fanin <= 4 || circuit_.fanin == 8) {
        std::vector<std::pair<int64_t, uint32_t>> scored;
        for (const uint32_t input : best_masks) {
          const size_t lookahead = trail_.size();
          bool compatible = true;
          for (uint32_t i = 0; compatible && i < circuit_.fanin; ++i)
            compatible = assign(gate.inputs[i], (input >> i) & 1);
          const uint32_t output = gate.table[input];
          for (uint32_t i = 0; compatible && i < circuit_.fanout; ++i)
            compatible = assign(gate.outputs[i], (output >> i) & 1);
          if (compatible)
            compatible = propagate();
          scored.emplace_back(compatible
                                  ? static_cast<int64_t>(trail_.size() - lookahead)
                                  : -1,
                              input);
          undo(lookahead);
          if (interrupted_)
            break;
        }
        if (interrupted_) {
          undo(mark);
          return false;
        }
        std::stable_sort(scored.begin(), scored.end(),
                         [](const auto &left, const auto &right) {
                           return left.first > right.first;
                         });
        best_masks.clear();
        for (const auto &[score, input] : scored)
          if (score >= 0)
            best_masks.push_back(input);
      }
      for (const uint32_t input : best_masks) {
        const size_t branch = trail_.size();
        bool compatible = true;
        for (uint32_t i = 0; compatible && i < circuit_.fanin; ++i)
          compatible = assign(gate.inputs[i], (input >> i) & 1);
        const uint32_t output = gate.table[input];
        for (uint32_t i = 0; compatible && i < circuit_.fanout; ++i)
          compatible = assign(gate.outputs[i], (output >> i) & 1);
        if (compatible && dfs())
          return true;
        undo(branch);
        if (interrupted_)
          break;
      }
    } else {
      const auto open = std::find(assignment_.begin(), assignment_.end(),
                                  int8_t{-1});
      if (open != assignment_.end()) {
        const Var variable = static_cast<Var>(open - assignment_.begin());
        for (const uint8_t value : {uint8_t{0}, uint8_t{1}}) {
          const size_t branch = trail_.size();
          if (assign(variable, value) && dfs())
            return true;
          undo(branch);
          if (interrupted_)
            break;
        }
      }
    }
    undo(mark);
    return false;
  }
};

} // namespace

RuleEligibility lookup_circuit_model_guard(const Kernel &k) {
  RuleEligibility result;
  return result.fact("consistent", !k.inconsistent)
      .fact("no-conflict", !k.conflict_valid)
      .fact("no-pending", !k.pending.valid)
      .fact("root-level", k.level == 0)
      .fact("propagated", k.propagate_head == k.trail.size())
      .fact("sparse-mode", !k.dense)
      .fact("no-probe-transaction", !k.probing.recording)
      .fact("large-watches-connected", k.large_watches_connected);
}

ApplyResult lookup_circuit_model_apply(Kernel &k) {
  assert(lookup_circuit_model_guard(k));
  ++k.stats.lookup_circuit_model_calls;
  const strategy::LookupCircuitModelParameters parameters =
      k.lookup_circuit_model_parameters;
  if (!parameters.valid())
    return ApplyResult::NoOp;
  Circuit circuit;
  if (!choose_shape(k, parameters, circuit) || !reconstruct(k, circuit))
    return ApplyResult::NoOp;
  ++k.stats.lookup_circuit_model_recognized;
  k.stats.lookup_circuit_model_gates = circuit.gates.size();

  uint64_t support = 0;
  if (!structural_support(circuit, support))
    return ApplyResult::NoOp;
  k.stats.lookup_circuit_model_support =
      static_cast<uint64_t>(__builtin_popcountll(support));

  std::vector<uint8_t> assignment;
  const EnumerationResult enumeration =
      enumerate_primary(k, circuit, support, parameters, assignment);
  if (enumeration == EnumerationResult::Interrupted)
    return ApplyResult::NoOp;
  if (enumeration == EnumerationResult::Found &&
      lucky_model_valid(k, assignment)) {
    lucky_publish_model(k, assignment);
    ++k.stats.lookup_circuit_model_sat;
    return ApplyResult::Sat;
  }

  CspSearch search(k, circuit);
  if (!search.solve() || search.interrupted())
    return ApplyResult::NoOp;
  assignment.assign(search.assignment().begin(), search.assignment().end());
  for (uint8_t &value : assignment)
    if (value > 1)
      value = 0;
  if (!lucky_model_valid(k, assignment))
    return ApplyResult::NoOp;
  lucky_publish_model(k, assignment);
  ++k.stats.lookup_circuit_model_sat;
  return ApplyResult::Sat;
}

} // namespace tactix
