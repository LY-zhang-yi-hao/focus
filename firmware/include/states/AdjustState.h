#pragma once

#include "State.h"

class AdjustState : public State
{
public:
    void enter() override;
    void update() override;
    void exit() override;
    void adjustTimer(int duration);

private:
    int adjustDuration;
    unsigned long lastActivity;
};