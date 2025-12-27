#pragma once

#include "State.h"

class ProvisionState : public State
{
public:
    void enter() override;
    void update() override;
    void exit() override;
};