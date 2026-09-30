#pragma once

#include "BaseBoard.h"
#include "MS5542.h"

class Board : public BaseBoard {
public:
    Board();
    void BoardInit() override;
    void HandleData() override;
    void OneMsTask() override;

private:
    MS5542 m_DACS0;
    MS5542 m_DACS1;
    MS5542 m_DACS2;
    MS5542 m_DACS3;
};
