#pragma once

#include "Board.h"

#include "CuteLogger/CuteLogger.h"
#include "SerialOutStream.h"
#include "GpioLed.h"
#include "BaseTimer.h"
#include "SoftTimer/SoftTimer.h"

class Application : public Board {
public:
    static constexpr int LED_BLINK = 100;

    Application(Logger::LogLevel level=Logger::InfoLevel);
    ~Application();
    static Application& GetInstance();
    void Start();

private:
    void Init();
    void ClockInit();
    void PeriphInit();
    void VersionPrint();
    void DisableIrq();
    void EnableIrq();
    void PinInit();
    void TmrInit();

    SerialOutStream m_serialOutStream;
    GpioLed* m_serialModuleCommLed;
    BaseTimer* m_oneMsTimer;
    SoftTimer::Timer m_oneMsSoftTimer;
    bool m_irqState;
};

void AppOneMsTask();
