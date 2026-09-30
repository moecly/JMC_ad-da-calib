#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>


void ApplicationInit();
void SwTmrTick(uint32_t tick);

#ifdef __cplusplus
}
#endif
