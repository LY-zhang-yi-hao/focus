#pragma once

#include <Arduino.h>

// FocusTask / 专注任务数据结构
//
// 用于承载从 Home Assistant 下发的任务列表（来源：TickTick #focus 等），并在设备端展示与选择。
struct FocusTask {
    String id;                 // 任务 ID（由 HA 生成/透传，可为复合 ID）/ Task ID
    String name;               // 任务名称 / Task name
    String displayName;        // 设备显示名（可选，ASCII/拼音/简称），用于中文不可渲染时替代显示 / Optional device display name
    int estimatedDuration = 25;     // 建议单次番茄时长（分钟）/ Recommended session duration (min)
    uint32_t spentTodaySeconds = 0; // 今日累计专注用时（秒）/ Spent today (sec)
};
