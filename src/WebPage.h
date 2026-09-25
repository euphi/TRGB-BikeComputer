/*
 * WebPage.h
 *
 * Shared shell for the HTML pages this firmware generates (/dev/, /sensor/, /system,
 * /logfiles/, /debug/...). Before this, every one of them hand-rolled its own <html>
 * head -- with different charsets, different (or missing) viewport tags and no way back
 * except the browser button.
 *
 * Styling comes from /stylesheet.css, which belongs to the web UI and is never written
 * to from here: pages use the classes already defined there (container, table, badge-*,
 * btn, btn-ghost, eyebrow, row). Anything a page needs beyond that goes into its own
 * <style> block via begin()'s extraCss.
 */

#pragma once

#include <Arduino.h>

namespace WebPage {

// Emits everything up to and including <h2>title</h2> inside the page container.
// extraCss is placed in this page's own <style> block (may be nullptr).
void begin(String& out, const char* title, const char* extraCss = nullptr);

// Closes the container and the document: a Back button, the toast element, and the
// script backing both. extraScript is appended after the shared helpers, so it can use
// toast() and req() (may be nullptr).
//
// Shared helpers available to extraScript:
//   toast(msg, isError)      show a message that fades after 4s and can be clicked away
//   req(url, okMsg, then)    fetch url, toast the outcome, run then() on success
void end(String& out, const char* extraScript = nullptr);

}	// namespace WebPage
