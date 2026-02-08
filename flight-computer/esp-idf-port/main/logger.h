#pragma once
#include "freertos/FreeRTOS.h"

namespace seds {
    constexpr size_t LOGGER_TASK_STACK_SIZE = 0x2000;
    constexpr uint32_t LOGGER_TASK_PRIORITY = 2; // Higher than any "idle" tasks, but still fairly low
    extern StackType_t LOGGER_TASK_STACK[LOGGER_TASK_STACK_SIZE];
    extern StaticTask_t LOGGER_TASK;

    void logger_task_impl(void* args);
}
