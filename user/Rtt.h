#pragma once

#include <cstdint>

void RttWrite(const char* data, uint32_t len);
int RttRead(char* out, uint32_t maxLen);
