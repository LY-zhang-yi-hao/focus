#pragma once

#include "State.h"

class TimerState : public State
{
public:
    TimerState();

    void enter() override;
    void update() override;
    void exit() override;

    void setTimer(int duration, unsigned long elapsedTime);

private:
    int duration;
    unsigned long startTime;
    unsigned long elapsedTime;
};