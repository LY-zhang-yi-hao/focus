#pragma once

#include <Arduino.h>
#include <vector>

// FocusSubtask / 子任务（ChecklistItem）
struct FocusSubtask {
    String id;
    String title;
    bool isCompleted = false;
};

// FocusTask / 专注任务数据结构
//
// 用于承载从 Home Assistant 下发的任务列表（来源：TickTick #focus 等），并在设备端展示与选择。
struct FocusTask {
    String id;                 // 任务 ID（由 HA 生成/透传，可为复合 ID）/ Task ID
    String projectId;          // 所属 TickTick 项目（清单）ID / Project ID
    String name;               // 任务名称 / Task name
    String displayName;        // 设备显示名（可选，兼容字段）/ Optional device display name (compat)
    bool isCompleted = false;  // 是否已完成（用于区分“待办/已完成”列表）/ Completed flag
    int estimatedDuration = 25;     // 建议单次番茄时长（分钟）/ Recommended session duration (min)
    uint32_t spentTodaySeconds = 0; // 今日累计专注用时（秒）/ Spent today (sec)
    String completedAt;             // 完成日期（MM.DD，仅已完成任务有效）/ Completed date (MM.DD)
    uint32_t completedSpentSeconds = 0; // 完成当天累计专注用时（秒，仅已完成任务有效）/ Spent on completed day (sec)

    // 展示字段（来自 TickTick Open API）
    int priority = 0;          // 0/1/3/5
    String priorityFlag;       // H/M/L/-
    String dueMmdd;            // 截止日期（MM.DD）
    bool hasRepeat = false;    // 是否重复
    bool hasReminder = false;  // 是否提醒

    int subtasksTotal = 0;     // 子任务总数
    int subtasksDone = 0;      // 已完成子任务数
    std::vector<FocusSubtask> subtasks;  // 子任务列表（仅当 tasks.kind=CHECKLIST 或 items 非空时）
};
