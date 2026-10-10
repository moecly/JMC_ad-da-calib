#include "Board.h"

#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"
#include "at32f425_wk_config.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
#include "calib/CalibProc.h"
#endif

// code_to_code 仅由 ch0 实测标定，其余通道无有效换算
constexpr uint8_t kRefCh = 0;

static inline uint16_t code_to_code(uint16_t code)
{
    float x = (float)code;
    float y = (((0.00000000000037393660707901615 * x + -0.000000042660788724369936) * x + 0.9947796026082614) * x + 34.00608779723021);
    if (y < 0.0f) y = 0.0f;
    if (y > 65535.0f) y = 65535.0f;
    return (uint16_t)(y + (y >= 0.0f ? 0.5f : -0.5f));
}

Board::Board()
    : m_DACS0(SPI1, DA_SPI_CS0_GPIO_PORT, DA_SPI_CS0_PIN),
      m_DACS1(SPI1, DA_SPI_CS1_GPIO_PORT, DA_SPI_CS1_PIN),
      m_DACS2(SPI1, DA_SPI_CS2_GPIO_PORT, DA_SPI_CS2_PIN),
      m_DACS3(SPI1, DA_SPI_CS3_GPIO_PORT, DA_SPI_CS3_PIN) {
}

void Board::BoardInit() {}

void Board::HandleData() {
    // m_DACS0.SetVal(code_to_code(set->channel0));
    // m_DACS1.SetVal(set->channel1);
    // m_DACS2.SetVal(set->channel2);
    // m_DACS3.SetVal(set->channel3);
    // static int cnt = 0;
    // cnt++;
    // if (cnt >= 100) {
    //     LogInfo << "val0 = " << set->channel0 << ", val1 = " << set->channel1 << ", val 2 = " << set->channel2 << ",
    //     val3 = " << set->channel3; cnt = 0;
    // }
}

void Board::OneMsTask() {
#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
    CalibProcTick();
#endif
}

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
void Board::CalibDaSet(uint8_t ch, uint16_t raw) {
    switch (ch) {
        case 0: m_DACS0.SetVal(raw); break;
        case 1: m_DACS1.SetVal(raw); break;
        case 2: m_DACS2.SetVal(raw); break;
        default: m_DACS3.SetVal(raw); break;
    }
}

double Board::CalibBuiltinRawFromIdeal(uint16_t ideal) const { return (double)code_to_code(ideal); }

uint16_t Board::CalibDaRaw(uint8_t ch, uint16_t ideal) const {
    static CalibRecord rec;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (!CalibStoreLoad(&rec)) rec.degree = 0;
    }
    if (rec.degree == POLY_DEGREE && (rec.validMask & (1u << ch))) {
        double y = PolyEval(rec.coeff[ch], (double)ideal);
        if (y < 0.0) y = 0.0;
        if (y > 65535.0) y = 65535.0;
        return (uint16_t)(y + 0.5);
    }
    return ch == kRefCh ? code_to_code(ideal) : ideal;
}
#endif
