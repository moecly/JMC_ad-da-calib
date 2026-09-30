#pragma once

#include "CuteLogger/OutStreamAbstract.h"
#include <cstdint>

class SerialOutStream : public OutStreamAbstract {
public:
    constexpr static int MAX_BUF_NUM = 10;
    constexpr static int MAX_BUF_SIZE = 200;
    
    SerialOutStream();
    ~SerialOutStream();
    void Write(const char* message) override;
    void Loop();

private:
    char* m_buf[MAX_BUF_NUM];
    int m_head;
    int m_tail;
};
