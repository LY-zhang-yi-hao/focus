#pragma once

#include "State.h"
#include "models/FocusTask.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

class TaskListState : public State {
public:
    TaskListState();
    void enter() override;
    void update() override;
    void exit() override;

    // Update task list from JSON / 从 JSON 更新任务列表
    void updateTaskList(const String& jsonTaskList);

    // Get currently selected task / 获取当前选中的任务
    FocusTask* getSelectedTask();

    // 任务列表（public 供 TaskListViewState 只读访问）
    std::vector<FocusTask> pendingTasks;    // 待办任务列表
    std::vector<FocusTask> completedTasks;  // 已完成任务列表

private:
    bool showingCompleted;                 // 当前是否显示"已完成"

    int selectedIndexPending;              // 待办：当前选中索引
    int displayOffsetPending;              // 待办：显示滚动偏移
    int selectedIndexCompleted;            // 已完成：当前选中索引
    int displayOffsetCompleted;            // 已完成：显示滚动偏移

    unsigned long lastActivity;       // Last user interaction time / 最后操作时间

    static const int MAX_VISIBLE_TASKS = 2;  // Max tasks visible on screen / 屏幕可显示任务数
    static const int TASK_TIMEOUT = 30;      // Timeout in seconds / 超时时间（秒）
};
