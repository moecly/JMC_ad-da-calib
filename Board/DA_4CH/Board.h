#pragma once

#include "BaseBoard.h"
#include "MS5542.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_CHK)
#include "calib/CalibStore.h"
#include "calib/PolyFit.h"
#endif

class Board : public BaseBoard {
public:
    Board();
    void BoardInit() override;
    void HandleData() override;
    void OneMsTask() override;

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_CHK)
    void CalibDaSet(uint8_t ch, uint16_t raw);
    double CalibBuiltinRawFromIdeal(uint16_t ideal) const;
    uint16_t CalibDaRaw(uint8_t ch, uint16_t ideal) const;
#endif

private:
    MS5542 m_DACS0;
    MS5542 m_DACS1;
    MS5542 m_DACS2;
    MS5542 m_DACS3;
};
