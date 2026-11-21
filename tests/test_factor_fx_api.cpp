#include "unit_test.hpp"
#include "tactix_c_api.hpp"
#include "api/api_internal.hpp"
#include "api/parameter_encoding.hpp"
#include "kernel/kernel.hpp"
#include "policies/default_policy.hpp"
#include "policies/strategy_settings.hpp"
#include "rules/registry.hpp"

using namespace tactix::strategy;

TEST(fx_profile_public_round_trip) {
  int factor = -1;
  for (const auto &rule : tactix_rules())
    if (std::string_view(rule.name) == "Factor") factor = rule.id;
  assert(factor >= 0);
  auto *solver = tactix_init();
  FactorParameters profile;
  public_encoding::Encoded encoded;
  assert(public_encoding::encode(profile, encoded));
  assert(encoded.parameters.kind == TACTIX_ONLINE_PARAMETERS_FACTOR);
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == 0);
  auto &k = api_detail::kernel_for_fixed_testing(solver);
  assert(!k.factor_fx);
  profile.fx = true;
  profile.fx_options = {19, 0, 2, FxClauseScope::Redundant};
  assert(public_encoding::encode(profile, encoded));
  assert(encoded.parameters.kind == TACTIX_ONLINE_PARAMETERS_FACTOR_FX);
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == 0);
  assert(k.factor_fx && k.factor_fx_options.maximum_clause_size == 19);
  assert(k.factor_fx_options.minimum_reduction == 0);
  assert(k.factor_fx_options.gates == 2);
  assert(k.factor_fx_options.scope == FxClauseScope::Redundant);
  tactix_online_parameters readback{};
  assert(tactix_read_parameters(solver, factor, encoded.parameters.kind,
                               TACTIX_PARAMETERS_INSTALLED, &readback) == 0);
  assert(readback.value.factor_fx.maximum_clause_size == 19);
  assert(readback.value.factor_fx.gates == 2);
  assert(readback.value.factor_fx.scope == TACTIX_FX_REDUNDANT);
  for (unsigned invalid : {0u, 4u, 256u}) {
    encoded.parameters.value.factor_fx.gates = invalid;
    assert(tactix_install_parameters(solver, factor, &encoded.parameters) == -1);
    assert(k.factor_fx_options.gates == 2);
  }
  encoded.parameters.value.factor_fx.gates = 2;
  encoded.parameters.value.factor_fx.scope = 3;
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == -1);
  encoded.parameters.value.factor_fx.scope = 0;
  encoded.parameters.value.factor_fx.maximum_clause_size = 2;
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == -1);
  profile.fx = false;
  assert(public_encoding::encode(profile, encoded));
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == 0);
  assert(!k.factor_fx);
  tactix_release(solver);
}

TEST(public_factor_applies_configured_fx) {
  auto *solver = tactix_init();
  for (int context = 4; context <= 9; ++context) {
    const int phase = context & 1 ? -1 : 1;
    for (int lit : {1, -2 * phase, context, 0}) tactix_add(solver, lit);
    for (int lit : {-1, 2 * phase, context, 0}) tactix_add(solver, lit);
  }
  const int factor = tactix_rule_id("Factor");
  FactorParameters profile;
  profile.fx = true;
  profile.fx_options.gates = TACTIX_FX_XOR;
  public_encoding::Encoded encoded;
  assert(public_encoding::encode(profile, encoded));
  assert(tactix_install_parameters(solver, factor, &encoded.parameters) == 0);
  assert(tactix_apply(solver, factor) == 0);
  auto &k = api_detail::kernel_for_fixed_testing(solver);
  assert(k.structural_action_sequence == 1 && k.nvars == 10);
  assert(k.stats.factorings == 1);
  tactix_release(solver);
}

TEST(default_strategy_poses_named_fx_options) {
  const auto check = [](const StrategySettings &settings, bool enabled,
                        FxOptions expected, uint32_t effort) {
    auto *solver = tactix_init();
    tactix_add(solver, 1);
    tactix_add(solver, 0);
    api_detail::complete_input(solver);
    ScopedStrategySettings scoped(&settings);
    DefaultPolicy policy;
    assert(policy.solve(api_detail::kernel_for_fixed_testing(solver)) == 10);
    const auto &config = policy.strategy_config().inprocess;
    const auto &actual = config.factor_parameters.fx_options;
    assert(config.factor_parameters.fx == enabled);
    assert(actual.maximum_clause_size == expected.maximum_clause_size);
    assert(actual.minimum_reduction == expected.minimum_reduction);
    assert(actual.gates == expected.gates && actual.scope == expected.scope);
    assert(config.factor_fx_effort_floor == effort);
    tactix_release(solver);
  };
  check({{"TACTIX_FACTOR_FX", "0"}}, false, {}, 75);
  check({{"TACTIX_FACTOR_FX", "1"},
         {"TACTIX_FACTOR_FX_MAXIMUM_CLAUSE_SIZE", "19"},
         {"TACTIX_FACTOR_FX_MINIMUM_REDUCTION", "0"},
         {"TACTIX_FACTOR_FX_GATES", "2"},
         {"TACTIX_FACTOR_FX_SCOPE", "1"},
         {"TACTIX_FACTOR_FX_EFFORT_FLOOR", "0"}},
        true, {19, 0, 2, FxClauseScope::Redundant}, 0);
  check({{"TACTIX_FACTOR_FX", "1"},
         {"TACTIX_FACTOR_FX_MAXIMUM_CLAUSE_SIZE", "2"},
         {"TACTIX_FACTOR_FX_MINIMUM_REDUCTION", "4294967296"},
         {"TACTIX_FACTOR_FX_GATES", "256"},
         {"TACTIX_FACTOR_FX_SCOPE", "-1"},
         {"TACTIX_FACTOR_FX_EFFORT_FLOOR", "1001"}},
        true, {}, 75);
}
