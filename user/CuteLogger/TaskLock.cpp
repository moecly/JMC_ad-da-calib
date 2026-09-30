#include "TaskLock.h"

TaskLock::TaskLock()
 : m_lockFlag(false) {
}

TaskLock::~TaskLock() {}

void TaskLock::Lock() {
    // __disable_irq();
    while (m_lockFlag) {
    }
    m_lockFlag = true;
}

void TaskLock::UnLock() {
    if (m_lockFlag == false) return;
    m_lockFlag = false;
    // __enable_irq();
}
