/*
 * I2CBus.h
 *
 * One lock for everything on Wire: touch controller (UI task), BME280 (esp_timer task and
 * the task that delivers the wheel revolutions) and BMI160 (ImuTask).
 *
 * TwoWire's own lock only covers the bus transfer. The receive buffer (rxLength/rxIndex)
 * is read after it is released, by available()/read() and by the return value of
 * requestFrom(). If another task starts a transfer in between, the first one gets the
 * other device's bytes, a wrong length, or -- once rxIndex is ahead of rxLength -- an
 * available() that stays true while read() returns nothing: loops like
 * "while (Wire.available()) Wire.read();" (SparkFun BME280) then never end. That was the
 * watchdog reset of 2026-10-02 (doc/PITFALLS.md).
 *
 * So: hold a Guard from the start of a transfer until its bytes are taken out of Wire.
 * Library calls that do both internally are wrapped as a whole. Recursive, so a wrapped
 * call may use a helper that takes the lock itself.
 */

#pragma once

namespace I2CBus {

void lock();
void unlock();

class Guard {
public:
	Guard() {lock();}
	~Guard() {unlock();}
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;
};

// Puts the lock around the touch controller's LVGL read callback, which lives in
// TRGBArduinoSupport. Call once after trgb.init(). false if there is no pointer device.
bool guardTouch();

}
