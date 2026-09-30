#include "CPPInterface.h"

#include "Application.h"
#include "SoftTimer/SoftTimer.h"

void ApplicationInit() {
    Application::GetInstance().Start();
}

void SwTmrTick(uint32_t tick) {
    SoftTimerTick(tick);
}

