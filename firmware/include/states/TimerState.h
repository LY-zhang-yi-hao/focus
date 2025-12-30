#pragma once

#include "State.h"
#include <Arduino.h>

class TimerState : public State
{
public:
    TimerState();

    void enter() override;
    void update() override;
    void exit() override;

    void setTimer(int duration,
                  unsigned long elapsedTime,
                  const String& taskId = "",
                  const String& taskName = "",
                  const String& sessionId = "",
                  const String& taskDisplayName = "",
                  const String& taskProjectId = "");

private:
    int duration;
    unsigned long startTime;
    unsigned long elapsedTime;
    String taskProjectId;
    String taskId;
    String taskName;
    String sessionId;
    String taskDisplayName;
};
