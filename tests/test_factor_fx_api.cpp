#include "unit_test.hpp"
#include "tactix_c_api.hpp"
#include "api/api_internal.hpp"
#include "api/parameter_encoding.hpp"
#include "kernel/kernel.hpp"
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
