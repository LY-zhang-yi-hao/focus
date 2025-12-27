#pragma once

#include "State.h"

class DoneState : public State
{
public:
    DoneState();
    void enter() override;
    void update() override;
    void exit() override;

private:
    unsigned long doneEnter;
};