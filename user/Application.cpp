#include "Application.h"

#include "DrvInit.h"
#include "FWConfig.h"
#include "at32f425_board.h"
#include "at32f425_clock.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_CHK) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
#include "calib/CalibProc.h"
#endif

Application::Application(Logger::LogLevel level)
    : m_oneMsTimer(new BaseTimer(TMR1)),
      m_irqState(true) {
    GetLoggerInstance().RegisterOutStream(&m_serialOutStream);
    GetLoggerInstance().SetLogLevel(level);
    DisableIrq();
}

Application::~Application() {
    delete m_serialModuleCommLed;
}

Application& Application::GetInstance() {
    // static Application instance(Logger::InfoLevel);
    static Application instance(Logger::DebugLevel);
    return instance;
}

void Application::Start() {
    Init();
    while (true) {
#if defined(DEBUG)
        m_serialOutStream.Loop();
#endif
        SoftTimerLoop();
        HandleData();
    }
}

void Application::Init() {
    DisableIrq();
    VersionPrint();
    ClockInit();
    PeriphInit();
    BoardInit();
    TmrInit();
    EnableIrq();
#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_CHK) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
    CalibProcInit();
#endif
    LogInfo << "Init success";
}

void Application::ClockInit() {
#if 0  // main函数初始化时钟
    system_clock_config();

    /* gpio clock */
    DrvInit::ClockEnable(CRM_GPIOA_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_GPIOB_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_GPIOC_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_GPIOD_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_GPIOF_PERIPH_CLOCK);

    /* usart */
    DrvInit::ClockEnable(CRM_USART2_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_USART3_PERIPH_CLOCK);
    DrvInit::ClockEnable(CRM_USART4_PERIPH_CLOCK);

    /* dma */
    DrvInit::ClockEnable(CRM_DMA1_PERIPH_CLOCK);
#endif
    LogInfo << "ClockInit success";
}

void Application::PeriphInit() {
    PinInit();
    m_oneMsTimer->Stop();
    m_oneMsTimer->SetTimerPr(1, BaseTimer::MS);
    m_oneMsTimer->Start();
    LogInfo << "PeriphInit success";
}

void Application::VersionPrint() {
    if (CONFIG_MINOR_VERSION < 10) {
        LogInfo << "Device: " << CONFIG_DEVICE_NAME << ", App Version: v" << CONFIG_MAJOR_VERSION << ".0"
                << CONFIG_MINOR_VERSION;
    } else {
        LogInfo << "Device: " << CONFIG_DEVICE_NAME << ", App Version: v" << CONFIG_MAJOR_VERSION << "."
                << CONFIG_MINOR_VERSION;
    }
}

void Application::DisableIrq() {
    if (m_irqState == true) {
        __disable_irq();
        __asm volatile("isb");
        m_irqState = false;
    }
}

void Application::EnableIrq() {
    if (m_irqState == false) {
        __enable_irq();
        __asm volatile("isb");
        m_irqState = true;
    }
}

void Application::PinInit() {
    // // serial module comm
    // // rx
    // DrvInit::PinInit(GPIOA, GPIO_PINS_2, GPIO_OUTPUT_PUSH_PULL, GPIO_PULL_UP, GPIO_MODE_MUX,
    // GPIO_DRIVE_STRENGTH_STRONGER); DrvInit::Pinmux(GPIOA, GPIO_PINS_SOURCE2, GPIO_MUX_1);
    // // tx
    // DrvInit::PinInit(GPIOA, GPIO_PINS_3, GPIO_OUTPUT_PUSH_PULL, GPIO_PULL_UP, GPIO_MODE_MUX,
    // GPIO_DRIVE_STRENGTH_STRONGER); DrvInit::Pinmux(GPIOA, GPIO_PINS_SOURCE3, GPIO_MUX_1);
    // // rts
    // DrvInit::PinInit(GPIOA, GPIO_PINS_1, GPIO_OUTPUT_PUSH_PULL, GPIO_PULL_UP, GPIO_MODE_MUX,
    // GPIO_DRIVE_STRENGTH_STRONGER); DrvInit::Pinmux(GPIOA, GPIO_PINS_SOURCE1, GPIO_MUX_1);
}

void Application::TmrInit() {
    m_oneMsSoftTimer.mode = SoftTimer::PERIOD;
    m_oneMsSoftTimer.timeout = 1;
    m_oneMsSoftTimer.func = AppOneMsTask;
    SoftTimerPush(&m_oneMsSoftTimer);
}

void AppOneMsTask() { Application::GetInstance().OneMsTask(); }
