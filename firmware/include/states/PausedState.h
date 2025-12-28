#pragma once

#include "State.h"
#include <Arduino.h>

class PausedState : public State
{
public:
    PausedState();
    void enter() override;
    void update() override;
    void exit() override;

    void setPause(int duration,
                  unsigned long elapsedTime,
                  const String& taskId = "",
                  const String& taskName = "",
                  const String& sessionId = "",
                  const String& taskDisplayName = "");

private:
    int duration;
    unsigned long pauseEnter;
    unsigned long elapsedTime;
    String taskId;
    String taskName;
    String sessionId;
    String taskDisplayName;
};
