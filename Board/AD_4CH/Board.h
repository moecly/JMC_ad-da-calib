#pragma once

#include <memory>
#include <cstdint>
#include <optional>

#include "AD7689Drv.h"
#include "BaseBoard.h"


class Board : public BaseBoard {
public:
    static constexpr uint16_t CURRENT_RES_VAL = 249;

    virtual ~Board() = default;
    void BoardInit() override;
    void HandleData() override;
    void OneMsTask() override;

private:
    // AD7689Drv* m_ad7689Drv;
    std::unique_ptr<AD7689Drv> m_ad7689Drv;
};
