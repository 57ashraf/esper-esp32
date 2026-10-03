#pragma once
#include <cstdint>
using TickType_t = uint32_t;
constexpr int pdTRUE=1, pdFALSE=0;
constexpr unsigned portMAX_DELAY=0xffffffff;
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(x) (x)
inline void vTaskDelay(unsigned) {}
