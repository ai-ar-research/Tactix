// S4 SwitchMode (stable ⇄ focused) — plan §5.2, kissat/src/mode.c.
#pragma once

#include "rules/registry.hpp"

namespace tactix {

bool switchmode_guard(const Kernel &);
ApplyResult switchmode_apply(Kernel &);

} // namespace tactix
