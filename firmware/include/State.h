#pragma once

#include "Config.h"

// Base class / 状态基类
class State {
public:
    virtual ~State() {}

    virtual void enter() = 0; 
    virtual void update() = 0;
    virtual void exit() = 0;
};
