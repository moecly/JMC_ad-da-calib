#pragma once

class Led {
public:
    enum LedState {
        ON,
        OFF,
    };

    Led() = default;
    virtual ~Led() = default;
    virtual void On() = 0;
    virtual void Off() = 0;
    virtual void Toggle() = 0;
    virtual LedState GetState() = 0;
};
