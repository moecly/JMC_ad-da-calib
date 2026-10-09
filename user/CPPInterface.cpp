#include "CPPInterface.h"

#include "Application.h"
#include "Rtt.h"
#include "SoftTimer/SoftTimer.h"

void ApplicationInit() {
    Application::GetInstance().Start();
}

void SwTmrTick(uint32_t tick) {
    SoftTimerTick(tick);
}

void RttWriteRaw(const char* data, uint32_t len) { RttWrite(data, len); }

