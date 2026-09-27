/*
 * CrashInfo.h
 *
 * Why did the device restart, and where did it crash? Without USB (built into the bike)
 * this is the only way to find out:
 *
 *   - logBoot(): one log line at boot with esp_reset_reason(), plus the size of the core
 *     dump in flash if the reason was a crash. Deliberately NOT more than that at boot: an
 *     earlier attempt to copy the core dump at boot crashed and looped forever (see the
 *     TODO in BCLogger::setup()) -- without USB that would brick the device.
 *   - registerRoutes(): /debug/coredump shows the dump's summary (task, PC, backtrace),
 *     /debug/coredump.elf downloads the raw image for esp-coredump on the host, and
 *     /debug/coredump/erase clears it. The dump is only read on request, so a broken
 *     image costs one reboot at worst, never a loop.
 *
 * The core dump itself is written by ESP-IDF's panic handler to the "coredump" partition
 * (partitions.csv, CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH); every crash overwrites the last.
 * Decode on the host with the ELF of exactly the firmware that crashed:
 *
 *   esp-coredump info_corefile -t elf -c coredump.elf .pio/build/trgb-esp32-s3/firmware.elf
 */

#pragma once

class AsyncWebServer;

namespace CrashInfo {

const char* resetReasonName(int reason);	// esp_reset_reason_t
bool lastResetWasCrash();
void logBoot();
void registerRoutes(AsyncWebServer& server);

}	// namespace CrashInfo
