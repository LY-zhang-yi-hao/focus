#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskCompletePromptState.h"
#include <ArduinoJson.h>

TaskCompletePromptState::TaskCompletePromptState()
    : elapsedSeconds(0),
      countTime(false),
      isCanceled(false),
      markDoneSelected(false)
{
}

void TaskCompletePromptState::setContext(const String& taskId,
                                         const String& taskName,
                                         const String& sessionId,
                                         uint32_t elapsedSeconds,
                                         bool countTime,
                                         bool isCanceled,
                                         const String& taskDisplayName)
{
    this->taskId = taskId;
    this->taskName = taskName;
    this->sessionId = sessionId;
    this->taskDisplayName = taskDisplayName;
    this->elapsedSeconds = elapsedSeconds;
    this->countTime = countTime;
    this->isCanceled = isCanceled;

    // 默认选项：正常结束→YES；取消→NO / Default: completed -> YES, canceled -> NO
    markDoneSelected = !isCanceled;
}

void TaskCompletePromptState::enter()
{
    Serial.println("Entering TaskCompletePrompt State / 进入任务结束确认状态");

    ledController.setBreath(isCanceled ? MAGENTA : GREEN, -1, true, 5);

    // 旋钮选择：顺时针=YES，逆时针=NO / Encoder: CW=YES, CCW=NO
    inputController.onEncoderRotateHandler([this](int delta) {
        if (delta > 0) {
            markDoneSelected = true;
        } else if (delta < 0) {
            markDoneSelected = false;
        }
    });

    auto confirm = [this]() {
        DynamicJsonDocument doc(512);
        doc["action"] = "focus_result";
        doc["event"] = "task_done_decision";
        doc["session_id"] = sessionId;
        doc["task_id"] = taskId;
        doc["task_name"] = taskName;
        doc["task_display_name"] = taskDisplayName;
        doc["mark_task_done"] = markDoneSelected;
        doc["end_type"] = isCanceled ? "canceled" : "completed";
        // 添加专注时长，用于 HA 统计
        doc["elapsed_seconds"] = elapsedSeconds;
        doc["count_time"] = countTime;

        String payload;
        serializeJson(doc, payload);
        networkController.sendWebhookPayload(payload);

        stateMachine.changeState(&StateMachine::idleState);
    };

    // 按键确认 / Press to confirm
    inputController.onPressHandler(confirm);

    // 长按也视为确认当前选择 / Long press also confirms current selection
    inputController.onLongPressHandler(confirm);
}

void TaskCompletePromptState::update()
{
    inputController.update();
    ledController.update();
    networkController.update();

    // 优先显示中文任务名（taskName），displayName 仅做兼容兜底
    String nameToShow = taskName;
    if (nameToShow.isEmpty()) {
        nameToShow = taskDisplayName;
    }
    if (nameToShow.isEmpty()) {
        String suffix = taskId;
        if (suffix.length() > 4) {
            suffix = suffix.substring(suffix.length() - 4);
        }
        nameToShow = suffix.isEmpty() ? "未命名任务" : ("任务 " + suffix);
    }

    displayController.drawTaskCompletePromptScreen(nameToShow, markDoneSelected, isCanceled);
}

void TaskCompletePromptState::exit()
{
    Serial.println("Exiting TaskCompletePrompt State / 离开任务结束确认状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}
