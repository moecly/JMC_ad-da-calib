#include "Board.h"

#include <cstdint>

#include "AD7689Drv.h"
#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_MANUAL)
#include "calib/CalibProc.h"
#endif

void Board::BoardInit() {
    m_ad7689Drv = std::make_unique<AD7689Drv>(SPI1);
    m_ad7689Drv->Init();
}

void Board::HandleData() {
    const std::array<uint16_t, AD7689Drv::NUM_CHANNELS> adVal = m_ad7689Drv->GetAdData();
    // get->channel0 = adVal[0];
    // get->channel1 = adVal[1];
    // get->channel2 = adVal[2];
    // get->channel3 = adVal[3];
}

void Board::OneMsTask() {
    if (m_ad7689Drv) {
        m_ad7689Drv->Sync();
    }
#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_MANUAL)
    CalibProcTick();
#endif
}

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_MANUAL)
uint16_t Board::CalibAdRaw(uint8_t ch) const { return m_ad7689Drv->GetAdData()[ch]; }

float Board::CalibAdIdeal(uint8_t ch, uint16_t raw) const {
    static CalibRecord rec;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (!CalibStoreLoad(&rec)) rec.degree = 0;
    }
    if (rec.degree == POLY_DEGREE && (rec.validMask & (1u << ch)))
        return (float)PolyEval(rec.coeff[ch], (double)raw);
    return (float)raw;
}
#endif

