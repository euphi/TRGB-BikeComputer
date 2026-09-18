// Forwarding shim: EEZ Studio's generated code (src/ui_eez/*) includes
// <lvgl/lvgl.h> (its ESP-IDF-component-style convention), but this project
// is PlatformIO/Arduino and only exposes the library root as <lvgl.h>.
// Lives in include/ (a stable PlatformIO include root) so it survives every
// EEZ Studio re-export, which only touches src/ui_eez/.
#include <lvgl.h>
