#pragma once
#include "tactix.h"
#ifdef __cplusplus
extern "C" {
#endif
int php_cubic(tactix *,const int *matrix,int holes,uint64_t *additions,
              tactix_clausal_effort *effort);
#ifdef __cplusplus
}
#endif
