#pragma once
// The public C API for C++ tests. Its handle type `tactix` collides with
// namespace tactix, so C++ code names the handle tactix_c_handle.
#define tactix tactix_c_handle
#include "tactix.h"
#undef tactix
