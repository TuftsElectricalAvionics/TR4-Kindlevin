#include "logger.h"

#include <chrono>

#include "utils.h"

using namespace std::chrono_literals;

namespace seds {
    StackType_t LOGGER_TASK_STACK[LOGGER_TASK_STACK_SIZE];
    StaticTask_t LOGGER_TASK;

    void logger_task_impl(void* args) {
        (void) args;

        while (true) {
            seds::delay(50ms);
        }
    }
}
