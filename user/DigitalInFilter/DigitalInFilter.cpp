#include "DigitalInFilter.h"

DigitalInFilter::DigitalInFilter(int ms)
 : m_filterTimeMs(ms),
   m_cnt(0) {
}

void DigitalInFilter::Push(bool val) {
    if (val != m_currVal) {
        m_cnt++;
        if (m_cnt >= m_filterTimeMs) {
            m_currVal = val;
        }
    } else {
        m_cnt = 0;
    }
}

bool DigitalInFilter::Get() {
    return m_currVal;
}

void DigitalInFilter::SetFilterTime(int ms) {
    m_filterTimeMs = ms;
}
