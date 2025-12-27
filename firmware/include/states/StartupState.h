#pragma once

#include "State.h"

class StartupState : public State
{
public:
    StartupState();
    void enter() override;
    void update() override;
    void exit() override;

private:
    unsigned long startEnter;
};
