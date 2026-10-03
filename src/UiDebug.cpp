/*
 * UiDebug.cpp
 *
 * See UiDebug.h.
 */

#include "UiDebug.h"

#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "Singletons.h"
#include "ui_eez/screens.h"

namespace {

// ---------------- synthetic touch ----------------
// A second LVGL pointer device next to the real touch controller. Its read callback runs
// in the UI task (LVGL's indev timer); the web handler only hands over a request.
struct Touch {
	int16_t x0, y0, x1, y1;
	uint32_t durationMs;
	uint32_t notBeforeMs;		// millis() - for a touch that has to happen while WiFi is off
};
portMUX_TYPE touchMux = portMUX_INITIALIZER_UNLOCKED;
Touch pendingTouch;				// guarded by touchMux
bool touchPending = false;		// guarded by touchMux
volatile bool touchBusy = false;	// pending or running, for the 409 answer

Touch activeTouch;				// UI task only
bool touchActive = false;
uint32_t touchStartMs = 0;
lv_point_t lastPoint = {0, 0};
lv_indev_drv_t touchDrv;

void touchRead(lv_indev_drv_t* drv, lv_indev_data_t* data) {
	(void) drv;
	if (!touchActive) {
		portENTER_CRITICAL(&touchMux);
		if (touchPending && (int32_t)(millis() - pendingTouch.notBeforeMs) >= 0) {
			activeTouch = pendingTouch;
			touchPending = false;
			touchActive = true;
			touchStartMs = millis();
		}
		portEXIT_CRITICAL(&touchMux);
	}
	if (touchActive) {
		const uint32_t t = millis() - touchStartMs;
		if (t < activeTouch.durationMs) {
			const float f = (float) t / activeTouch.durationMs;
			lastPoint.x = activeTouch.x0 + (lv_coord_t)((activeTouch.x1 - activeTouch.x0) * f);
			lastPoint.y = activeTouch.y0 + (lv_coord_t)((activeTouch.y1 - activeTouch.y0) * f);
			data->point = lastPoint;
			data->state = LV_INDEV_STATE_PRESSED;
			return;
		}
		touchActive = false;
		touchBusy = false;
		lastPoint.x = activeTouch.x1;
		lastPoint.y = activeTouch.y1;
	}
	data->point = lastPoint;
	data->state = LV_INDEV_STATE_RELEASED;
}

// ---------------- snapshot ----------------
// 480x480 RGB565 = 450 KB, in PSRAM, allocated on first use (never before trgb.init(),
// see doc/PITFALLS.md) and only touched on request.
uint8_t* snapBuf = nullptr;
uint32_t snapBufSize = 0;
lv_img_dsc_t snapDsc;
volatile uint8_t snapState = 0;		// 0 none, 1 running, 2 ready, 3 failed

void takeSnapshot() {
	lv_obj_t* scr = lv_scr_act();
	const uint32_t need = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);
	if (!snapBuf) {
		snapBuf = (uint8_t*) heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
		snapBufSize = snapBuf ? need : 0;
	}
	snapState = (snapBuf && need <= snapBufSize
			&& lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR, &snapDsc, snapBuf, snapBufSize) == LV_RES_OK) ? 2 : 3;
}

// Own short-lived task: rendering a whole screen needs more stack than the UI task
// (< 1 KB spare) or async_tcp should give up; xUIDrawMutex makes it safe.
void snapTask(void*) {
	if (!ui.runLocked(takeSnapshot, 2000)) snapState = 3;
	vTaskDelete(NULL);
}

const char* screenName(lv_obj_t* scr) {
	if (scr == objects.rim_ridge) return "rim_ridge";
	if (scr == objects.rim_ridge_nav) return "rim_ridge_nav";
	if (scr == objects.rim_ridge_rq) return "rim_ridge_rq";
	if (scr == objects.rim_ridge_settings) return "rim_ridge_settings";
	if (scr == objects.rim_ridge_settings_wifi) return "rim_ridge_settings_wifi";
	if (scr == objects.rim_ridge_settings_imu) return "rim_ridge_settings_imu";
	if (scr == objects.rim_ridge_settings_alt) return "rim_ridge_settings_alt";
	if (scr == objects.rim_ridge_settings_dev) return "rim_ridge_settings_dev";
	if (scr == objects.rim_ridge_settings_num) return "rim_ridge_settings_num";
	if (scr == objects.rim_ridge_climb) return "rim_ridge_climb";
	if (scr == objects.rim_ridge_wifi) return "rim_ridge_wifi";
	if (scr == objects.rim_ridge_wifi_pw) return "rim_ridge_wifi_pw";
	return "other";
}

int16_t param(AsyncWebServerRequest* request, const char* name, int16_t fallback) {
	return request->hasParam(name) ? (int16_t) request->getParam(name)->value().toInt() : fallback;
}

} // namespace

void UiDebug::setup() {
	lv_indev_drv_init(&touchDrv);
	touchDrv.type = LV_INDEV_TYPE_POINTER;
	touchDrv.read_cb = touchRead;
	lv_indev_drv_register(&touchDrv);

	AsyncWebServer& server = webserver.getServer();
	server.on("/debug/ui/touch", HTTP_GET, [](AsyncWebServerRequest* request) {
		if (!request->hasParam("x") || !request->hasParam("y")) {
			request->send(400, "text/plain", "x and y required");
			return;
		}
		if (touchBusy) {
			request->send(409, "text/plain", "previous touch still running");
			return;
		}
		Touch t;
		t.x0 = param(request, "x", 0);
		t.y0 = param(request, "y", 0);
		t.x1 = param(request, "x2", t.x0);
		t.y1 = param(request, "y2", t.y0);
		t.durationMs = (uint32_t) param(request, "ms", 100);
		if (t.durationMs < 40) t.durationMs = 40;		// at least one indev read period
		t.notBeforeMs = millis() + (uint32_t) param(request, "wait", 0);
		touchBusy = true;
		portENTER_CRITICAL(&touchMux);
		pendingTouch = t;
		touchPending = true;
		portEXIT_CRITICAL(&touchMux);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI, "Debug touch (%d,%d)->(%d,%d) %u ms", t.x0, t.y0, t.x1, t.y1, t.durationMs);
		request->send(200, "text/plain", "ok");
	});
	server.on("/debug/ui/snap.raw", HTTP_GET, [](AsyncWebServerRequest* request) {
		if (snapState != 2) {
			request->send(409, "text/plain", snapState == 1 ? "snapshot running" : "no snapshot - GET /debug/ui/snap first");
			return;
		}
		AsyncWebServerResponse* response = request->beginResponse(200, "application/octet-stream", snapBuf, snapDsc.data_size);
		response->addHeader("X-Width", String(snapDsc.header.w));
		response->addHeader("X-Height", String(snapDsc.header.h));
		request->send(response);
	});
	server.on("/debug/ui/snap", HTTP_GET, [](AsyncWebServerRequest* request) {
		if (snapState == 1) {
			request->send(409, "text/plain", "snapshot running");
			return;
		}
		snapState = 1;
		if (xTaskCreate(snapTask, "UiSnap", 8192, nullptr, 5, nullptr) != pdPASS) {
			snapState = 3;
			request->send(503, "text/plain", "no memory for the snapshot task");
			return;
		}
		request->send(202, "text/plain", "snapshot started");
	});
	server.on("/debug/ui/screen", HTTP_GET, [](AsyncWebServerRequest* request) {
		request->send(200, "text/plain", screenName(lv_scr_act()));
	});
}
