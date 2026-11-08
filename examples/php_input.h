#pragma once
#include <cstdio>
#include <optional>
#include <vector>

namespace php_example {
using Matrix = std::vector<std::vector<int>>;
std::optional<Matrix> read(FILE *);
}
