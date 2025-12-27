#pragma once

#include "controllers/DisplayController.h"
#include "controllers/LedController.h"
#include "controllers/InputController.h"
#include "controllers/NetworkController.h"
#include <Preferences.h>

// Declare global controller instances / 声明全局控制器实例
extern DisplayController displayController;
extern LEDController ledController;
extern InputController inputController;
extern NetworkController networkController;
extern Preferences preferences;
