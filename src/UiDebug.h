/*
 * UiDebug.h
 *
 * Remote UI testing over the web server, without touching the device: a screenshot of
 * the active screen and synthetic touch input (tap, long press, swipe). Made for
 * checking EEZ screens on the real hardware, see doc/DEBUG.md "Remote UI testing".
 *
 *   GET /debug/ui/touch?x=..&y=..[&x2=..&y2=..][&ms=..][&wait=..]  press at (x,y) for ms
 *        (default 100), moving linearly to (x2,y2) if given -- ms=800 is a long press, a
 *        200 px move in 300 ms a swipe. wait delays it (ms), e.g. to tap while WiFi is off.
 *        409 while the previous touch is still pending or running.
 *   GET /debug/ui/snap        render the active screen into a PSRAM buffer (async, ~0.3 s)
 *   GET /debug/ui/snap.raw    that buffer: RGB565 little endian, X-Width/X-Height headers;
 *                             409 until a snapshot is ready. Tools/uishot.py makes a PNG.
 *   GET /debug/ui/screen      name of the active screen
 */

#pragma once

namespace UiDebug {
	// From UIFacade::initDisplay(), after the screens exist: registers the virtual touch
	// input with LVGL and the web routes.
	void setup();
}
