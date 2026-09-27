#pragma once
// Single-threaded host test: a mutex that just records whether it is held, to catch
// double takes (which would deadlock the non-recursive FreeRTOS mutex on the device).
#include <cstdio>
#include <cstdlib>
struct MockMutex {bool held = false;};
typedef MockMutex* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() {return new MockMutex;}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t) {
	if (m->held) {printf("  FAIL: mutex taken twice (deadlock on the device)\n"); exit(1);}
	m->held = true;
	return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t m) {m->held = false; return pdTRUE;}
