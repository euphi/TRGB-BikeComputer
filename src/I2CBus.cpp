/*
 * I2CBus.cpp
 */

#include "I2CBus.h"

#include <Arduino.h>
#include <lvgl.h>

namespace {

SemaphoreHandle_t busMutex = xSemaphoreCreateRecursiveMutex();

void (*libTouchRead)(lv_indev_drv_t*, lv_indev_data_t*) = nullptr;

void guardedTouchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
	I2CBus::Guard guard;
	libTouchRead(drv, data);
}

}

void I2CBus::lock() {
	xSemaphoreTakeRecursive(busMutex, portMAX_DELAY);
}

void I2CBus::unlock() {
	xSemaphoreGiveRecursive(busMutex);
}

bool I2CBus::guardTouch() {
	if (libTouchRead) return true;
	for (lv_indev_t* indev = lv_indev_get_next(nullptr); indev; indev = lv_indev_get_next(indev)) {
		if (indev->driver->type != LV_INDEV_TYPE_POINTER || !indev->driver->read_cb) continue;
		libTouchRead = indev->driver->read_cb;
		indev->driver->read_cb = guardedTouchRead;
		return true;
	}
	return false;
}
