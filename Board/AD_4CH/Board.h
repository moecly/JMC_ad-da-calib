#pragma once

#include <memory>
#include <cstdint>
#include <optional>

#include "AD7689Drv.h"
#include "BaseBoard.h"

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_MANUAL)
#include "calib/CalibStore.h"
#include "calib/PolyFit.h"
#endif

class Board : public BaseBoard {
public:
    static constexpr uint16_t CURRENT_RES_VAL = 249;

    virtual ~Board() = default;
    void BoardInit() override;
    void HandleData() override;
    void OneMsTask() override;

#if defined(CONFIG_CALIB_ROLE_REF) || defined(CONFIG_CALIB_ROLE_DUT) || defined(CONFIG_CALIB_ROLE_MANUAL)
    uint16_t CalibAdRaw(uint8_t ch) const;
    float CalibAdIdeal(uint8_t ch, uint16_t raw) const;
#endif

private:
    // AD7689Drv* m_ad7689Drv;
    std::unique_ptr<AD7689Drv> m_ad7689Drv;
};
