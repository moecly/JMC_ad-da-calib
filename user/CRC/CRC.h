#pragma once

#include <cstdint>

class CRCCheck {
public:
    CRCCheck(uint32_t init=0xffffffff, uint32_t poly=0xedb88320);
    ~CRCCheck() = default;
    uint32_t Calculate(const char* data, int length);

private:
    uint32_t m_init;
    uint32_t m_poly;
    uint32_t m_table[256];
};
