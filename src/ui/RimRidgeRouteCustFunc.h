/*
 * RimRidgeRouteCustFunc.h
 *
 * Hand-written data wiring for the EEZ Studio-generated "RimRidgeRoute" screen (route overview:
 * what is still ahead on TrailBridge's GPX route -- destination, waypoints, climbs). Same pattern
 * as the other RimRidge*CustFunc files -- outside src/ui_eez/, which every EEZ export replaces.
 * The data side is RouteMonitor.h (BLE service, anchors -> distances and times).
 *
 * Screen switching (UIFacade): a swipe to the left on RimRidgeNav opens it (counts as dismissing
 * the nav screen, like the RQ screen); any swipe goes back (RouteScreenGesture); also "route show".
 *
 * Widgets (prefix rrroute_):
 *   dest                  "Ziel 36.0 km  15:42": distance to the destination and the arrival time
 *                         (the clock if it is set, else the time left)
 *   list                  frame of the list; the rows are created here, nearest first, waypoints
 *                         and climbs mixed in the order the rider reaches them:
 *                           waypoint  name .................. distance
 *                                     arrival 14:32 (18 min)
 *                           climb     Anstieg 2/7 ............. distance to the foot
 *                                     450 Hm, 8.2 km, 5.5 %     (in it: "noch <to the summit>")
 *                         the stripe at the left: brass for a waypoint, for a climb the colour of its
 *                         mean gradient (as the climb screen's profile)
 *   empty, empty2         "Keine Strecke" / nothing ahead
 *   foot                  waypoints ahead, "Liste gekürzt" if TrailBridge or the firmware cut the list
 * A once-a-second lv_timer refreshes the screen while it is shown (distances move with the wheel).
 * At most MAX_ROWS rows exist; the list is nearest first, so the far end is what is cut.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Once, after create_screen_rim_ridge_route(): the canvas' example content out, the refresh timer.
void ui_RimRidgeRouteInit();

// Refreshes the screen now (UIFacade::showRouteScreen(), so the first second is not stale). xUIDrawMutex held.
void ui_RimRidgeRouteRefresh();

#ifdef __cplusplus
} /*extern "C"*/
#endif
