/*
 * UIFacade.h
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include <Ticker.h>
#include "freertos/semphr.h"

#ifdef BC_FL_SUPPORT
#include "ui/uiFLmodel.h"
#endif
#include <Stats/Statistics.h>		//TODO: Move statistics data types to separate class
#include "BikeNavProtocol.h"
#include "ClimbProfile.h"
#include <lvgl.h>

class UIFacade {
public:
	UIFacade();

	void initDisplay();
	void updateHandler();

	void updateSpeed(float speed);		// speed in km/h as float
	void updateCadence(uint16_t cad);	// cadence in revs per min
	void updateHR(uint16_t hr);       	// heartbet in beats per min
	void updateGrad(float grad, float height);	// grad(ient) in permille,
	void updateHeight(float height);	// height only update,

	void setStatMode(Statistics::ESummaryType mode) {statMode = mode;}
	void setStatMode(bool dir) {
		int32_t mode = statMode;
		mode += dir ? 1 : -1;
		if (mode < Statistics::SUM_ESP_TOTAL) mode = Statistics::SUM_ESP_START;
		if (mode > Statistics::SUM_ESP_START) mode = Statistics::SUM_ESP_TOTAL;
		statMode = static_cast<Statistics::ESummaryType>(mode);
		updateData();
	}
	void updateFast() {xSemaphoreGive(xUpdateFast);}

	// WLAN state for the settings screen and the WLAN icon on RimRidge. Values are shared
	// with RRSET_WIFI_* in ui/RimRidgeSettingsCustFunc.h.
	enum WifiUiState : uint8_t {WIFI_UI_OFF = 0, WIFI_UI_CONNECTING = 1, WIFI_UI_ONLINE = 2, WIFI_UI_AP = 3};
	// text: IP address, or a short status ("WLAN aus", "verbinde ...", ...); in WIFI_UI_AP the
	// hotspot password, with `caption` above it ("HOTSPOT <ssid>"). Called by WifiWebserver on
	// every state change, also before initDisplay() (kept until then).
	void updateIP(const String& text, WifiUiState state, const String& caption = String());


	void updateNavi(const String& navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit = 0,
			uint8_t nextManeuver = 0, uint32_t nextManeuverDist = 0, const String& nextStreet = String(),
			uint32_t remainingDist = 0, uint32_t remainingTime = 0);
	void updateNaviDist(uint32_t dist);

	// Manual RimRidgeNav show/hide - tap on rr_nav_pill (RimRidge) / swipe
	// right on the nav screen itself. Independent of updateNavi()'s own
	// auto-popup-on-new-maneuver logic (works even with no active route,
	// e.g. for checking how the screen looks) but shares the same
	// navScreenActive bookkeeping so the two don't fight each other -
	// see updateNavi()'s comment for why that matters.
	void showNavScreen();
	void hideNavScreen();

	// Manual RimRidgeRQ show/hide - tap on the state icon/RQ line group
	// (every screen has one; on RimRidgeRQ itself it goes back) / swipe
	// gesture on the RQ screen. No auto-popup logic here (unlike the Nav
	// screen); opened from RimRidgeNav it counts as dismissing that one.
	void showRQScreen();
	void hideRQScreen();

	// Settings screen (RimRidgeSettings): tap on rr_btn_settings / any swipe on the
	// screen itself. Refreshed once a second while shown (calibration progress).
	void showSettingsScreen();
	void hideSettingsScreen();

	// Climb screen (RimRidgeClimb, elevation profile of the climb ahead -- ClimbMonitor.h).
	// Shown automatically while a rated climb is ridden (see updateClimb()), by hand with a
	// tap on the altitude/gradient value of RimRidge; any swipe on it goes back. A climb
	// swiped away is not shown again by itself.
	void showClimbScreen();
	void hideClimbScreen();

	// Route overview screen (RimRidgeRoute, destination/waypoints/climbs of the GPX route ahead --
	// RouteMonitor.h): a swipe to the left on RimRidgeNav, or "route show"; any swipe goes back to
	// the base screen. Coming from the nav screen counts as dismissing it, like the RQ screen.
	void showRouteScreen();
	void hideRouteScreen();

	// Received (BLE TLV tags 0x0A-0x0D, PROTOCOL.md "Fahrspur-
	// Informationen") and rendered on BOTH RimRidge's compact lane-row
	// chip (rr_lane_row) and RimRidgeNav's own strip (rrnav_lane_row) -
	// see ui_RimRidgeUpdateLanes()/ui_RimRidgeNavUpdateLanes(). Both show
	// purely on laneCount>0, no distance gating (LANE_DISTANCE_M is
	// unused on purpose - unreliable per PROTOCOL.md), and no maneuver
	// code either (2026-09-20 correction: PROTOCOL.md's ACTIVE flag is a
	// per-LANE recommendation, not per-direction - see lane_icon.h's
	// lane_row_get() doc comment for the full story of why this used to
	// take a maneuver code and doesn't anymore). nextLanes is still
	// stored only, no widget for it on either screen yet. lanes/nextLanes
	// may be nullptr with count 0 to clear (e.g. on disconnect or NAV_NONE).
	// laneDist/nextLaneDist are independent of the maneuver distances
	// passed to updateNavi() - the point where a lane choice becomes
	// relevant typically lies well before the maneuver itself (see
	// PROTOCOL.md).
	void updateLanes(const NavLane* lanes, uint8_t laneCount, uint32_t laneDist, const NavLane* nextLanes,
			uint8_t nextLaneCount, uint32_t nextLaneDist);

	void updateBatInt(float voltage, uint8_t batPerc, bool charging);

	float getBatIntVoltageAvg() const {return batIntVoltageAvg;}

	Statistics::ESummaryType getStatMode() const {return statMode;}

	Statistics::EAvgType getStatTimeMode() const {return statTimeMode;}
	void setStatTimeMode(Statistics::EAvgType _statTimeMode) {statTimeMode = _statTimeMode;updateData();}


	enum UIColor {
		UI_ColorNeutral,
		UI_ColorOK,
		UI_ColorWarn,
		UI_ColorCrit
	};

	// rideMode selects the Pause/Start button's glyph together with the state icon (both
	// flip on the same signal) -- see ui_RimRidgeUpdateStateIcon()'s doc comment.
	void updateStateIcon(Statistics::EDrivingState state, UIColor col, bool rideMode);

	// Small GPS-fix status icon on the base screen -- hasFix false hides it entirely,
	// col signals fix quality (see Statistics::updateGpsFixIcon() for the thresholds).
	void updateGpsFix(bool hasFix, UIColor col);

	// Road-surface quality (BMI160, see RoadQuality.h). roadClass 0 = not rated (too slow, no
	// speed, no sensor), 1 = smooth .. 5 = very rough; roughness is the index behind it (NAN
	// if not rated), shockCount the hard hits logged since boot. Called from
	// Statistics::cycle() when something changed, and every 10 s regardless.
	void updateRoadQuality(uint8_t roadClass, float roughness, uint32_t shockCount);

	// Manual road label (RQ-Ride-Screen's surface pills/quality selector/record button) -
	// surface/quality 0 = none, same encoding as I2CSensors::setRoadLabel*(). Refreshes the
	// display from I2CSensors::getRoadLabelState(), not from the tap itself (see
	// ui_RimRidgeRQUpdateLabel()'s comment). Called from Statistics::updateRoadQualityUi(),
	// same cadence as updateRoadQuality() above.
	void updateRoadLabel(uint8_t surface, uint8_t quality, bool capturing);


	// Runs fn with xUIDrawMutex held, for LVGL access from another task (e.g. the debug
	// snapshot in UiDebug.cpp). false if the mutex couldn't be taken within timeoutMs.
	bool runLocked(const std::function<void()>& fn, uint32_t timeoutMs);

	typedef std::function<void(bool ok)> MsgBoxCallBack;
	void showMsgBox(const String& msgText, const MsgBoxCallBack& cb);
	void updateMsgBox(const String& msgText);
	void msgCBFct(bool ok);

	MsgBoxCallBack msgCB = NULL;

	void otaStart();
	void otaProgress(uint8_t perc);

private:
	void updateData();
	void updateClock(const time_t now);
	void updateStats();
	void updateIntBatteryInt();

	// Nav auto-show/hide-back-to-Main state machine (see updateNavi()'s doc
	// comment for the full rule set: 2026-09-20 request - auto-show only
	// within NAV_AUTO_SHOW_DIST_M of the maneuver, auto-hide-back only
	// NAV_AUTO_HIDE_DELAY_MS after a NEW instruction arrives while already
	// showing, and only if that new instruction is more than
	// NAV_AUTO_HIDE_DIST_M away). Assumes xUIDrawMutex is already held -
	// called only from updateNavi()/updateNaviDist() (which hold it) and
	// from updateHandler()'s own 1Hz tick (which bypasses it via
	// isDrawTask(), same as everywhere else in this file).
	void evaluateNaviAutoSwitch();
	// Leaving RimRidgeNav by hand (swipe, or a tap on the state icon/RQ line): the
	// current maneuver counts as dismissed, see hideNavScreen(). xUIDrawMutex held.
	void dismissNavScreen();

	// Once a second, xUIDrawMutex held: fetches the climb state from ClimbMonitor, refreshes
	// RimRidgeClimb and switches to it / back:
	//  - to it when a climb of at least Climb::Config::autoShowMinRank is no further than
	//    showAheadM ahead (autoShow on) -- from the main screen only; from any other screen
	//    it becomes the screen to return to;
	//  - back to the main screen hideDelayS after that is no longer the case (summit passed,
	//    PROFILE_NONE, route left).
	void updateClimb();
	// Where "back" leads: the climb screen while it is on, else the main screen.
	lv_obj_t* baseScreen();
	bool climbScreenActive = false;		// the climb screen is the base screen (shown, or covered by nav/RQ/settings)
	bool climbScreenAuto = false;		// ... because of a climb: it goes away with it (opened by hand without one, it stays)
	uint16_t climbCurrentId = 0;		// Climb::Status::climbId of the climb ahead, 0 = none
	uint16_t climbDismissedId = 0;		// the climb the rider swiped away
	uint32_t climbHideAtMs = 0;			// 0 = no delayed switch back pending
	uint32_t climbLostSinceMs = 0;		// since when there is no climb to show (0 = there is one, or none was shown)
	bool climbOverForScreen(const Climb::Status& st, bool onClimb);


#ifdef BC_FL_SUPPORT
	UiFLModel uifl;
#endif

	Ticker updateTicker;
	Ticker dataTicker;

	//TODO: Use notify/poll mechanism instead of storing data
	int16_t hr = -1, cad = -1;
	float grad = NAN, height = NAN, speed = NAN;

	Statistics::ESummaryType statMode = Statistics::SUM_ESP_START;
	Statistics::EAvgType     statTimeMode = Statistics::AVG_ALL;

	// Model for TRGB specific data
	float batIntVoltage = NAN, batIntVoltageAvg = NAN;
	float batIntVoltageSum = 0;		// of the samples since the last average (updateIntBatteryInt())
	uint8_t batIntSamples = 0;
	int8_t batIntPerc = -1;
	bool batIntCharging = false;

	// Is RimRidgeNav the currently displayed screen? Written by
	// evaluateNaviAutoSwitch() AND by showNavScreen()/hideNavScreen()
	// (manual tap/swipe) - shared so the two mechanisms don't fight each
	// other (see updateNavi()'s comment).
	bool navScreenActive = false;

	// Last WLAN state from WifiWebserver (see updateIP()), guarded by xUIDrawMutex. Shown
	// once displayReady is set - WifiWebserver starts before the UI.
	String wifiText, wifiCaption;
	WifiUiState wifiState = WIFI_UI_OFF;
	bool displayReady = false;
	void applyWifiState();		// xUIDrawMutex held

	// evaluateNaviAutoSwitch()'s state - see its own comment above and
	// updateNavi()'s doc comment for the full rule set.
	uint8_t currentManeuver = NAV_MANEUVER_NONE;    // latest known maneuver (full TLV frames only)
	uint32_t currentManeuverDist = 0;                // latest known distance-to-maneuver - refreshed by
	                                                   // BOTH updateNavi() (full frames) and updateNaviDist()
	                                                   // (wheel-interpolated estimate between frames), so
	                                                   // the distance thresholds react as fast as the speed
	                                                   // sensor allows.
	bool hadActiveNav = false;                        // previous hasActiveNav, to detect the actual
	                                                   // active->NONE edge (route just ended) rather than
	                                                   // re-firing the force-hide on every subsequent call
	                                                   // while already inactive (would kill a manual preview
	                                                   // opened with no active route).
	uint8_t lastManeuverSeen = NAV_MANEUVER_NONE;      // for detecting "a new instruction just arrived"
	uint8_t autoDecidedForManeuver = NAV_MANEUVER_NONE; // maneuver value we've already auto-shown OR the
	                                                   // user has manually dismissed for - suppresses
	                                                   // re-triggering the auto-show every subsequent call
	                                                   // for the SAME still-current maneuver (manual
	                                                   // dismiss must stick until the next real instruction).
	uint32_t pendingHideAtMs = 0;                     // 0 = no pending delayed auto-hide-back check armed
	uint8_t pendingHideForManeuver = NAV_MANEUVER_NONE; // which maneuver the pending check above belongs to -
	                                                   // a newer instruction arriving before it fires
	                                                   // re-arms it instead of acting on stale data.

	// Lane guidance (see updateLanes()) - lanes is rendered on both
	// RimRidge and RimRidgeNav; nextLanes is stored only, no widget for it
	// yet.
	NavLane lanes[NAV_LANES_MAX];
	NavLane nextLanes[NAV_LANES_MAX];
	uint8_t laneCount = 0, nextLaneCount = 0;
	uint32_t laneDist = 0, nextLaneDist = 0;

	// Use (binary) Semaphores as messaging mechanism
	//    give: Signal an information (please update)
	//    take: receive signal
	SemaphoreHandle_t xUpdateFast;
	SemaphoreHandle_t xUpdateSlow;
	SemaphoreHandle_t xUIDrawMutex;
	TaskHandle_t uiTaskHandle = NULL;

	bool isDrawTask(); // returns true if running in draw task


};
