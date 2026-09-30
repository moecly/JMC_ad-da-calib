#include "Board.h"

#include <cstdint>

#include "AD7689Drv.h"
#include "CuteLogger/CuteLogger.h"
#include "DrvInit.h"

static inline uint16_t code_to_code(uint16_t raw)
{
    float x = (float)raw;
    float y = (((-0.000000000000371769659693692 * x + 0.000000045203336050454734) * x + 1.0266435420767652) * x + -32.826379360537395);
    if (y < 0.0f) y = 0.0f;
    if (y > 65535.0f) y = 65535.0f;
    return (uint16_t)(y + (y >= 0.0f ? 0.5f : -0.5f));
}

void Board::BoardInit() {
    m_ad7689Drv = std::make_unique<AD7689Drv>(SPI1);
    m_ad7689Drv->Init();
}

void Board::HandleData() {
    const std::array<uint16_t, AD7689Drv::NUM_CHANNELS> adVal = m_ad7689Drv->GetAdData();
    // get->channel0 = code_to_code(adVal[0]);
    // get->channel1 = adVal[1];
    // get->channel2 = adVal[2];
    // get->channel3 = adVal[3];
}

void Board::OneMsTask() {
    if (m_ad7689Drv) {
        m_ad7689Drv->Sync();
    }
}

