#pragma once
#include "tactix.h"
#ifdef __cplusplus
extern "C" {
#endif
struct php_input;
struct php_input *php_input_read(FILE *);
int php_input_holes(const struct php_input *);
int *php_input_matrix(const struct php_input *);
int php_input_collisions(tactix *, const struct php_input *, uint64_t *derived);
void php_input_release(struct php_input *);
#ifdef __cplusplus
}
#endif
