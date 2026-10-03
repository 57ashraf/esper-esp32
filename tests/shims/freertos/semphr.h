#pragma once
#include "FreeRTOS.h"
#include <mutex>
#include <chrono>
using SemaphoreHandle_t = std::timed_mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::timed_mutex; }
inline int xSemaphoreTake(SemaphoreHandle_t m, unsigned t) {
    if (!m) return pdFALSE;
    if (t == portMAX_DELAY) { m->lock(); return pdTRUE; }
    return m->try_lock_for(std::chrono::milliseconds(t)) ? pdTRUE : pdFALSE;
}
inline void xSemaphoreGive(SemaphoreHandle_t m) { m->unlock(); }
