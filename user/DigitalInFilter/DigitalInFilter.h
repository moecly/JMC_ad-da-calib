#pragma once

#include <cstdint>

class DigitalInFilter {
public:
    DigitalInFilter(int ms=1);
    ~DigitalInFilter() = default;
    void Push(bool val); // 1ms调用一次
    bool Get();
    void SetFilterTime(int ms);

private:
    int m_filterTimeMs;
    int m_cnt;
    bool m_currVal;
};
