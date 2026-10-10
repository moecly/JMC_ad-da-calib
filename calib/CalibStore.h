#pragma once

#include <cstdint>

#include "CRC/CRC.h"
#include "calib/PolyFit.h"

constexpr uint32_t CALIB_BASE = 0x0800F000;

inline CRCCheck& CalibCrc() {
    static CRCCheck crc;
    return crc;
}

struct CalibRecord {
    uint16_t board;
    uint16_t degree;
    uint16_t channelCount;
    uint16_t validMask;
    double coeff[4][POLY_COEFF_NUM];
};

bool CalibStoreLoad(CalibRecord* out);
bool CalibStoreSave(const CalibRecord* in);
