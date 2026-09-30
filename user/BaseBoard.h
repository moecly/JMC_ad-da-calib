#pragma once

class BaseBoard {
public: 
    BaseBoard() = default;
    virtual ~BaseBoard() = default;
    virtual void BoardInit() = 0;
    virtual void HandleData() = 0;
    virtual void OneMsTask() = 0;
};
