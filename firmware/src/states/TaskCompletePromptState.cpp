#include "StateMachine.h"
#include "Controllers.h"
#include "states/TaskCompletePromptState.h"
#include <ArduinoJson.h>

// 判断字符串是否为可打印 ASCII（用于在小字库下避免中文渲染为空白）/ ASCII printable check
static bool isAsciiPrintable(const String& text) {
    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

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

    // 优先显示 display_name（ASCII/拼音/简称），否则回退 name；若仍非 ASCII 则显示兜底标识 / Prefer display_name
    String nameToShow = taskDisplayName;
    if (nameToShow.isEmpty()) {
        nameToShow = taskName;
    }
    if (nameToShow.isEmpty() || !isAsciiPrintable(nameToShow)) {
        String suffix = taskId;
        if (suffix.length() > 4) {
            suffix = suffix.substring(suffix.length() - 4);
        }
        if (!suffix.isEmpty() && isAsciiPrintable(suffix)) {
            nameToShow = "TASK " + suffix;
        } else {
            nameToShow = "TASK";
        }
    }

    displayController.drawTaskCompletePromptScreen(nameToShow, markDoneSelected, isCanceled);
}

void TaskCompletePromptState::exit()
{
    Serial.println("Exiting TaskCompletePrompt State / 离开任务结束确认状态");
    inputController.releaseHandlers();
    ledController.turnOff();
}
