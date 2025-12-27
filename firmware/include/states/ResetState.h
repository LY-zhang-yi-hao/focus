#pragma once

#include "State.h"

class ResetState : public State
{
public:
    void enter() override;
    void update() override;
    void exit() override;

    unsigned long resetStartTime = 0;
};