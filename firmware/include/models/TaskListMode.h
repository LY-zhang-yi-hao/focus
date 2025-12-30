#pragma once

#include <stdint.h>

// 任务列表模式：待办 / 已完成 / 项目选择
enum class TaskListMode : uint8_t {
    Pending = 0,
    Completed = 1,
    Projects = 2,
};

