#pragma once

#include <cstdint>

class TaskLock {
public:
    TaskLock();
    ~TaskLock();
    void Lock();
    void UnLock();
private:
    volatile uint8_t m_lockFlag;
};
