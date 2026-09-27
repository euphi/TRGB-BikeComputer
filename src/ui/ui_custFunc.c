/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui.h"
#include <math.h> // to initialize float to NAN

lv_obj_t* msgBox = NULL;


static void msg_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_current_target(e);
    bool ok = lv_msgbox_get_active_btn(obj) == 0;
    msgCppCB(ok);
    lv_msgbox_close(obj);
    msgBox = 0;
}

void ui_MsgBox(const char* str) {
    static const char * btns[] = {"Apply", "Close", ""};
    if (msgBox) lv_msgbox_close(msgBox);
    msgBox = lv_msgbox_create(NULL, "Msg", str, btns, true);
    lv_obj_add_event_cb(msgBox, msg_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(msgBox);
}

void ui_MsgBoxUpdate(const char* str) {
	if (!msgBox) return;
	lv_label_set_text(lv_msgbox_get_text(msgBox), str);
}

