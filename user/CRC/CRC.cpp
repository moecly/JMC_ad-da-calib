#include "CRC.h"

CRCCheck::CRCCheck(uint32_t init, uint32_t poly) 
 : m_init(init),
   m_poly(poly) {
    uint32_t crc;
    for (int i = 0; i < sizeof(m_table) / sizeof(uint32_t); i++) {
        // LogDebug << "CRC test i = " << i;
        crc = i;
        for (int j = 8; j > 0; j--) {
            if (crc & 1) {
                crc = (crc >> 1) ^ m_poly;
            } else {
                crc >>= 1;
            }
        }
        m_table[i] = crc;
    }
}

uint32_t CRCCheck::Calculate(const char* data, int length) {
    uint8_t* uData = reinterpret_cast<uint8_t*>(const_cast<char*>(data));
    uint32_t crc = m_init;
    for (int i = 0; i < length; i++) {
        crc = (crc >> 8) ^ m_table[(crc ^ uData[i]) & 0xFF];
    }
    return crc ^ m_init;
}

