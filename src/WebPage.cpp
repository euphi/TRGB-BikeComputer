/*
 * WebPage.cpp
 *
 * See WebPage.h.
 */

#include "WebPage.h"

namespace WebPage {

void begin(String& out, const char* title, const char* extraCss) {
	// Reserve past CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL (4096) so the buffer starts in
	// PSRAM instead of ramping there through many small reallocs of the scarce internal
	// heap. Pages that know they are larger reserve more themselves afterwards.
	out.reserve(6144);
	out += F("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
	         "<meta charset=\"UTF-8\">\n"
	         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
	         "<link rel=\"stylesheet\" href=\"/stylesheet.css\">\n"
	         "<title>");
	out += title;
	out += F("</title>\n<style>"
	         // Only what the shared stylesheet does not provide. Tokens fall back so a
	         // page stays readable even if /stylesheet.css fails to load.
	         "#toast{position:fixed;left:50%;bottom:26px;transform:translateX(-50%);max-width:88vw;"
	         "padding:10px 16px;border-radius:10px;border:1px solid var(--rr-line,#3A362E);"
	         "background:var(--rr-panel,#1E252B);color:var(--rr-parchment,#E7E2D6);font-size:0.84rem;"
	         "box-shadow:0 6px 20px rgba(0,0,0,.45);opacity:0;pointer-events:none;"
	         "transition:opacity .18s;z-index:50;cursor:pointer}"
	         "#toast.show{opacity:1;pointer-events:auto}"
	         "#toast.err{border-color:var(--rr-err,#C1604A)}"
	         "td a.badge{margin-right:6px;text-decoration:none}"
	         "td{vertical-align:middle}");
	if (extraCss) out += extraCss;
	out += F("</style>\n</head>\n<body>\n<div class=\"container\" style=\"max-width:820px;\">\n<h2>");
	out += title;
	out += F("</h2>\n");
}

void end(String& out, const char* extraScript) {
	// history.back() keeps the menu's scroll position; the href is the fallback for a
	// page opened directly from a bookmark, where there is no history to go back to.
	out += F("<div class=\"row\" style=\"margin-top:20px;\">"
	         "<a class=\"btn btn-ghost\" href=\"/\" onclick=\"if(history.length>1){history.back();return false;}\">Back</a>"
	         "</div>\n</div>\n"
	         "<div id=\"toast\" onclick=\"this.classList.remove('show')\"></div>\n"
	         "<script>\n"
	         "let _t;\n"
	         "function toast(m,e){const o=document.getElementById('toast');o.textContent=m;"
	         "o.className='show'+(e?' err':'');clearTimeout(_t);"
	         "_t=setTimeout(()=>o.classList.remove('show'),4000);}\n"
	         "async function req(u,ok,then){try{const r=await fetch(u);"
	         "toast(r.ok?ok:'Failed ('+r.status+')',!r.ok);if(r.ok&&then)then();return r.ok;}"
	         "catch(x){toast('Request failed',true);return false;}}\n");
	if (extraScript) out += extraScript;
	out += F("</script>\n</body>\n</html>");
}

}	// namespace WebPage
