#pragma once

#include <cstdint>

void CalibLinkInit();
int CalibLinkSend(const uint8_t* payload, uint8_t len);
int CalibLinkRecv(uint8_t* payload, uint8_t* len);
