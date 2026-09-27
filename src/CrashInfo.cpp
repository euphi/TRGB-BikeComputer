/*
 * CrashInfo.cpp
 *
 * See CrashInfo.h.
 */

#include "CrashInfo.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <esp_system.h>
#include <esp_core_dump.h>
#include <esp_partition.h>
#include <esp_app_desc.h>

#include "Singletons.h"
#include "WebPage.h"

namespace CrashInfo {

const char* resetReasonName(int reason) {
	switch ((esp_reset_reason_t)reason) {
	case ESP_RST_POWERON:   return "power on";
	case ESP_RST_EXT:       return "external pin";
	case ESP_RST_SW:        return "software (esp_restart, OTA)";
	case ESP_RST_PANIC:     return "PANIC (exception/abort)";
	case ESP_RST_INT_WDT:   return "INTERRUPT WATCHDOG";
	case ESP_RST_TASK_WDT:  return "TASK WATCHDOG";
	case ESP_RST_WDT:       return "OTHER WATCHDOG";
	case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
	case ESP_RST_BROWNOUT:  return "BROWNOUT (supply voltage)";
	case ESP_RST_SDIO:      return "SDIO";
	case ESP_RST_USB:       return "USB";
	case ESP_RST_JTAG:      return "JTAG";
	case ESP_RST_EFUSE:     return "EFUSE error";
	case ESP_RST_PWR_GLITCH: return "POWER GLITCH";
	case ESP_RST_CPU_LOCKUP: return "CPU LOCKUP";
	default:                return "unknown";
	}
}

bool lastResetWasCrash() {
	switch (esp_reset_reason()) {
	case ESP_RST_PANIC:
	case ESP_RST_INT_WDT:
	case ESP_RST_TASK_WDT:
	case ESP_RST_WDT:
	case ESP_RST_BROWNOUT:
	case ESP_RST_PWR_GLITCH:
	case ESP_RST_CPU_LOCKUP:
		return true;
	default:
		return false;
	}
}

void logBoot() {
	const esp_reset_reason_t r = esp_reset_reason();
	if (!lastResetWasCrash()) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Reset reason: %s", resetReasonName(r));
		return;
	}
	// Only the header: IDF already validated the image at boot (CONFIG_ESP_COREDUMP_CHECK_BOOT).
	size_t addr = 0, size = 0;
	const bool dump = esp_core_dump_image_get(&addr, &size) == ESP_OK && size > 0;
	bclog.logf(BCLogger::Log_Error, BCLogger::TAG_OP, "Reset reason: %s - %s", resetReasonName(r),
	           dump ? "core dump in flash, see /debug/coredump" : "no core dump");
}

static const esp_partition_t* dumpPartition() {
	return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
}

static void summaryHtml(String& html) {
	html += F("<table class=\"table\">\n<tr><td>Last reset</td><td>");
	html += resetReasonName(esp_reset_reason());
	html += F("</td></tr>\n");

	size_t addr = 0, size = 0;
	esp_err_t err = esp_core_dump_image_get(&addr, &size);
	if (err != ESP_OK || size == 0) {
		html += F("<tr><td>Core dump</td><td>none in flash (");
		html += esp_err_to_name(err);
		html += F(")</td></tr>\n</table>\n");
		return;
	}
	html += F("<tr><td>Core dump</td><td>");
	html += size;
	html += F(" byte</td></tr>\n");
	err = esp_core_dump_image_check();
	if (err != ESP_OK) {
		html += F("<tr><td>Check</td><td>corrupt (");
		html += esp_err_to_name(err);
		html += F(")</td></tr>\n</table>\n");
		return;
	}
	esp_core_dump_summary_t* s = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
	if (!s) {
		html += F("</table>\n<p>out of memory</p>\n");
		return;
	}
	err = esp_core_dump_get_summary(s);
	if (err != ESP_OK) {
		html += F("<tr><td>Summary</td><td>");
		html += esp_err_to_name(err);
		html += F("</td></tr>\n</table>\n");
		free(s);
		return;
	}
	char buf[160];
	char running[APP_ELF_SHA256_SZ] = {};
	esp_app_get_elf_sha256(running, sizeof(running));
	const bool same = strncmp(running, (const char*)s->app_elf_sha256, sizeof(running) - 1) == 0;
	snprintf(buf, sizeof(buf), "<tr><td>Task</td><td><b>%.16s</b></td></tr>\n", s->exc_task);
	html += buf;
	snprintf(buf, sizeof(buf), "<tr><td>PC</td><td>0x%08lx</td></tr>\n", (unsigned long)s->exc_pc);
	html += buf;
	snprintf(buf, sizeof(buf), "<tr><td>Cause / vaddr</td><td>%lu / 0x%08lx</td></tr>\n",
	         (unsigned long)s->ex_info.exc_cause, (unsigned long)s->ex_info.exc_vaddr);
	html += buf;
	snprintf(buf, sizeof(buf), "<tr><td>Firmware</td><td>ELF %.*s %s</td></tr>\n", (int)sizeof(running) - 1,
	         (const char*)s->app_elf_sha256, same ? "(= running firmware)" : "(<b>not</b> the running firmware)");
	html += buf;
	html += F("<tr><td>Backtrace</td><td style=\"font-family:monospace\">");
	String bt;
	for (uint32_t i = 0; i < s->exc_bt_info.depth && i < 16; i++) {
		snprintf(buf, sizeof(buf), "0x%08lx ", (unsigned long)s->exc_bt_info.bt[i]);
		bt += buf;
	}
	html += bt;
	if (s->exc_bt_info.corrupted) html += F(" (corrupted)");
	html += F("</td></tr>\n</table>\n<p class=\"eyebrow\">Resolve on the host with the ELF of that firmware:<br>"
	          "<code>xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf ");
	html += bt;
	html += F("</code><br>or the whole dump: <code>esp-coredump info_corefile -t elf -c coredump.elf firmware.elf</code></p>\n");
	free(s);
}

void registerRoutes(AsyncWebServer& server) {
	// More specific paths first: AsyncWebServer matches "/debug/coredump" as a prefix too.
	server.on("/debug/coredump.elf", HTTP_GET, [](AsyncWebServerRequest* request) {
		size_t addr = 0, size = 0;
		const esp_partition_t* part = dumpPartition();
		if (!part || esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0 || size > part->size) {
			request->send(404, "text/plain", "no core dump in flash");
			return;
		}
		AsyncWebServerResponse* resp = request->beginResponse("application/octet-stream", size,
			[part, size](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
				if (index >= size) return 0;
				size_t n = size - index < maxLen ? size - index : maxLen;
				if (n > 1024) n = 1024;			// short flash reads: cache is off meanwhile
				return esp_partition_read(part, index, buf, n) == ESP_OK ? n : 0;
			});
		resp->addHeader("Content-Disposition", "attachment; filename=\"coredump.elf\"");
		request->send(resp);
	});
	server.on("/debug/coredump/erase", HTTP_GET, [](AsyncWebServerRequest* request) {
		const esp_err_t err = esp_core_dump_image_erase();
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Core dump erased: %s", esp_err_to_name(err));
		request->send(err == ESP_OK ? 200 : 500, "text/plain", err == ESP_OK ? "Core dump erased" : esp_err_to_name(err));
	});
	server.on("/debug/coredump", HTTP_GET, [](AsyncWebServerRequest* request) {
		String html;
		html.reserve(2048);
		WebPage::begin(html, "Core dump");
		html += F("<p class=\"eyebrow\">Written by the panic handler on a crash; each crash overwrites the last. "
		          "Read only on this page, never at boot.</p>\n");
		summaryHtml(html);
		html += F("<p><a class=\"btn\" href=\"/debug/coredump.elf\">Download coredump.elf</a> "
		          "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"if(confirm('Erase the core dump?'))"
		          "req('/debug/coredump/erase','Erased',()=>location.reload());return false;\">Erase</a></p>\n");
		WebPage::end(html);
		request->send(200, "text/html", html);
	});
}

}	// namespace CrashInfo
