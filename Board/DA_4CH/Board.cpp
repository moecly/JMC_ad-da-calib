#include "Board.h"

#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"
#include "at32f425_wk_config.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || \
    defined(CONFIG_CALIB_ROLE_MANUAL)
#include "calib/CalibProc.h"
#endif

Board::Board()
    : m_DACS0(SPI1, DA_SPI_CS0_GPIO_PORT, DA_SPI_CS0_PIN),
      m_DACS1(SPI1, DA_SPI_CS1_GPIO_PORT, DA_SPI_CS1_PIN),
      m_DACS2(SPI1, DA_SPI_CS2_GPIO_PORT, DA_SPI_CS2_PIN),
      m_DACS3(SPI1, DA_SPI_CS3_GPIO_PORT, DA_SPI_CS3_PIN) {
}

void Board::BoardInit() {}

void Board::HandleData() {
    // m_DACS0.SetVal(set->channel0);
    // m_DACS1.SetVal(set->channel1);
    // m_DACS2.SetVal(set->channel2);
    // m_DACS3.SetVal(set->channel3);
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
    return ideal;
}
#endif
