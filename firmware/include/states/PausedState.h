#pragma once

#include "State.h"

class PausedState : public State
{
public:
    PausedState();
    void enter() override;
    void update() override;
    void exit() override;

    void setPause(int duration, unsigned long elapsedTime);

private:
    int duration;
    unsigned long pauseEnter;
    unsigned long elapsedTime;
};