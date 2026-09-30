#pragma once

class OutStreamAbstract {
public:
    OutStreamAbstract() = default;
    virtual ~OutStreamAbstract() = default;
    virtual void Write(const char *data) = 0;
};

