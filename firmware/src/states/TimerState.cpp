#include "StateMachine.h"
#include "Controllers.h"
#include <ArduinoJson.h>

static String generateSessionId()
{
    // 简单且足够唯一的会话 ID：millis + micros / Simple session id: millis + micros
    return String((uint32_t)millis()) + "-" + String((uint32_t)micros());
}

TimerState::TimerState()
    : duration(0),
      elapsedTime(0),
      startTime(0),
      taskId(""),
      taskName(""),
      sessionId(""),
      taskDisplayName("")
{
}

void TimerState::enter()
{
    Serial.println("Entering Timer State / 进入计时状态");

    // Start time based on the elapsed time / 根据已消耗时间校准起点
    startTime = millis() - (elapsedTime * 1000);

    displayController.drawTimerScreen(duration * 60);
    uint32_t remainingSeconds = 0;
    if ((unsigned long)(duration * 60) > elapsedTime)
    {
        remainingSeconds = (duration * 60) - elapsedTime;
    }
    ledController.startFillAndDecay(RED, remainingSeconds * 1000);

    // Ensure session id / 确保会话 ID
    if (sessionId.isEmpty())
    {
        sessionId = generateSessionId();
    }

    // Register state-specific handlers / 注册状态回调
    inputController.onPressHandler([this]()
                                   {
                                       Serial.println("Timer State: Button Pressed / 计时状态：按键按下");

                                       // Update elapsed time / 更新已用时间
                                       this->elapsedTime = (millis() - this->startTime) / 1000;

                                       // Send 'Stop' webhook (pause) / 发送暂停事件
                                       DynamicJsonDocument doc(768);
                                       doc["action"] = "stop";
                                       doc["event"] = "focus_paused";
                                       doc["session_id"] = this->sessionId;
                                       doc["task_id"] = this->taskId;
                                       doc["task_name"] = this->taskName;
                                       doc["task_display_name"] = this->taskDisplayName;
                                       doc["elapsed_seconds"] = (uint32_t)this->elapsedTime;
                                       doc["count_time"] = false;

                                       String payload;
                                       serializeJson(doc, payload);
                                       networkController.sendWebhookPayload(payload);

                                       displayController.showTimerPause();

                                       // Transition to PausedState and set elapsed time
                                       StateMachine::pausedState.setPause(
                                           this->duration,
                                           this->elapsedTime,
                                           this->taskId,
                                           this->taskName,
                                           this->sessionId,
                                           this->taskDisplayName);
                                       stateMachine.changeState(&StateMachine::pausedState); // Transition to Paused State / 切换到暂停状态
                                   });

    inputController.onDoublePressHandler([this]()
                                         {
                                             Serial.println("Timer State: Double press - cancel / 计时状态：双击取消");

                                             // Update elapsed time / 更新已用时间
                                             this->elapsedTime = (millis() - this->startTime) / 1000;

                                             // Send 'Stop' webhook (canceled) / 发送取消事件
                                             DynamicJsonDocument doc(768);
                                             doc["action"] = "stop";
                                             doc["event"] = "focus_canceled";
                                             doc["session_id"] = this->sessionId;
                                             doc["task_id"] = this->taskId;
                                             doc["task_name"] = this->taskName;
                                             doc["task_display_name"] = this->taskDisplayName;
                                             doc["elapsed_seconds"] = (uint32_t)this->elapsedTime;
                                             doc["count_time"] = false;

                                             String payload;
                                             serializeJson(doc, payload);
                                             networkController.sendWebhookPayload(payload);

                                             displayController.showCancel();

                                             // 如果存在任务，则进入"是否完成任务"确认 / If task exists, ask whether to mark done
                                             if (!this->taskId.isEmpty())
                                             {
                                                 StateMachine::taskCompletePromptState.setContext(
                                                     this->taskId,
                                                     this->taskName,
                                                     this->sessionId,
                                                     (uint32_t)this->elapsedTime,
                                                     false,
                                                     true,
                                                     this->taskDisplayName);
                                                 stateMachine.changeState(&StateMachine::taskCompletePromptState);
                                             }
                                             else
                                             {
                                                 stateMachine.changeState(&StateMachine::idleState); // Transition to IdleState / 返回空闲
                                             }
                                         });

    // 旋转查看任务列表 / Rotate to view task list
    inputController.onEncoderRotateHandler([this](int delta)
                                           {
                                               Serial.println("Timer State: Encoder rotate - view task list / 计时状态：旋转查看任务列表");

                                               // 更新已用时间（用于返回时恢复）/ Update elapsed time for restoration
                                               this->elapsedTime = (millis() - this->startTime) / 1000;

                                               // 进入只读任务列表查看状态（不暂停计时、不触发webhook）
                                               // Enter read-only task list view (no pause, no webhook)
                                               StateMachine::taskListViewState.setTimerContext(
                                                   this->duration,
                                                   this->elapsedTime,
                                                   this->taskId,
                                                   this->taskName,
                                                   this->sessionId,
                                                   this->taskDisplayName);
                                               stateMachine.changeState(&StateMachine::taskListViewState);
                                           });

    // 长按也可以取消计时（保留作为备用）/ Long press also cancels timer (kept as backup)
    inputController.onLongPressHandler([this]()
                                       {
                                           Serial.println("Timer State: Long press - cancel / 计时状态：长按取消");

                                           // Update elapsed time / 更新已用时间
                                           this->elapsedTime = (millis() - this->startTime) / 1000;

                                           // Send 'Stop' webhook (canceled) / 发送取消事件
                                           DynamicJsonDocument doc(768);
                                           doc["action"] = "stop";
                                           doc["event"] = "focus_canceled";
                                           doc["session_id"] = this->sessionId;
                                           doc["task_id"] = this->taskId;
                                           doc["task_name"] = this->taskName;
                                           doc["task_display_name"] = this->taskDisplayName;
                                           doc["elapsed_seconds"] = (uint32_t)this->elapsedTime;
                                           doc["count_time"] = false;

                                           String payload;
                                           serializeJson(doc, payload);
                                           networkController.sendWebhookPayload(payload);

                                           displayController.showCancel();

                                           // 如果存在任务，则进入"是否完成任务"确认 / If task exists, ask whether to mark done
                                           if (!this->taskId.isEmpty())
                                           {
                                               StateMachine::taskCompletePromptState.setContext(
                                                   this->taskId,
                                                   this->taskName,
                                                   this->sessionId,
                                                   (uint32_t)this->elapsedTime,
                                                   false,
                                                   true,
                                                   this->taskDisplayName);
                                               stateMachine.changeState(&StateMachine::taskCompletePromptState);
                                           }
                                           else
                                           {
                                               stateMachine.changeState(&StateMachine::idleState); // Transition to IdleState / 返回空闲
                                           }
                                       });

    networkController.startBluetooth();

    // Send start/resume webhook / 发送开始/恢复事件
    DynamicJsonDocument doc(768);
    doc["action"] = "start";
    doc["event"] = (elapsedTime > 0) ? "focus_resumed" : "focus_started";
    doc["session_id"] = sessionId;
    doc["task_id"] = taskId;
    doc["task_name"] = taskName;
    doc["task_display_name"] = taskDisplayName;
    doc["duration_minutes"] = duration;

    String payload;
    serializeJson(doc, payload);
    networkController.sendWebhookPayload(payload);
}

void TimerState::update()
{
    inputController.update();
    ledController.update();
    networkController.update();

    unsigned long currentTime = millis();
    elapsedTime = (currentTime - startTime) / 1000;

    int remainingSeconds = duration * 60 - elapsedTime;

    displayController.drawTimerScreen(remainingSeconds);

    // Check if the timer is done / 检查计时是否完成
    if (remainingSeconds <= 0)
    {
        Serial.println("Timer State: Done / 计时状态：完成");

        // Send completion webhook (count_time=true) / 发送完成事件（计入今日统计）
        DynamicJsonDocument doc(768);
        doc["action"] = "stop";
        doc["event"] = "focus_completed";
        doc["session_id"] = sessionId;
        doc["task_id"] = taskId;
        doc["task_name"] = taskName;
        doc["task_display_name"] = taskDisplayName;
        doc["elapsed_seconds"] = (uint32_t)(duration * 60);
        doc["count_time"] = true;

        String payload;
        serializeJson(doc, payload);
        networkController.sendWebhookPayload(payload);

        displayController.showTimerDone();

        // 如果存在任务，则进入“是否完成任务”确认 / If task exists, ask whether to mark done
        if (!taskId.isEmpty())
        {
            StateMachine::taskCompletePromptState.setContext(
                taskId,
                taskName,
                sessionId,
                (uint32_t)(duration * 60),
                true,
                false,
                taskDisplayName);
            stateMachine.changeState(&StateMachine::taskCompletePromptState);
        }
        else
        {
            stateMachine.changeState(&StateMachine::doneState); // Transition to Done State / 切换到完成状态
        }
    }
}

void TimerState::exit()
{
    inputController.releaseHandlers();
    networkController.stopBluetooth();
    ledController.turnOff();
    Serial.println("Exiting Timer State / 离开计时状态");
}

void TimerState::setTimer(int duration,
                          unsigned long elapsedTime,
                          const String& taskId,
                          const String& taskName,
                          const String& sessionId,
                          const String& taskDisplayName)
{
    this->duration = duration;
    this->elapsedTime = elapsedTime;
    this->taskId = taskId;
    this->taskName = taskName;
    this->sessionId = sessionId;
    this->taskDisplayName = taskDisplayName;
}
