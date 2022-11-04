/*
 * mcu-app - Microcontroller application framework
 * Copyright 2022  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#define pdTRUE 1
#define pdFALSE 0

typedef unsigned int BaseType_t;
typedef uint64_t TickType_t;

#define portTICK_PERIOD_MS 1000
#define portYIELD_FROM_ISR(x)

class SemaphoreHandle {
public:
    SemaphoreHandle(void *ptr) : ok(ptr != nullptr) {}

    operator bool() { return ok; }

    bool ok;
    std::mutex mutex;
    std::condition_variable cv;
    bool value{0};
};

using SemaphoreHandle_t = std::shared_ptr<SemaphoreHandle>;

static inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    return std::make_shared<SemaphoreHandle>((void*)1);
}

static inline void vSemaphoreDelete(SemaphoreHandle_t &sem) {
    sem.reset();
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t &sem) {
    std::lock_guard<std::mutex> lock{sem->mutex};

    if (sem->value) {
        return pdFALSE;
    } else {
        sem->value = true;
        sem->cv.notify_all();
        return pdTRUE;
    }
}

static inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t &sem,
        BaseType_t *xHigherPriorityTaskWoken) {
    return xSemaphoreGive(sem);
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t &sem, TickType_t ticks) {
    std::unique_lock<std::mutex> lock{sem->mutex};

    if (!sem->value)
        sem->cv.wait_for(lock, std::chrono::microseconds(ticks));

    if (!sem->value) {
        return pdFALSE;
    } else {
        sem->value = false;
        return pdTRUE;
    }
}
