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

#include "ui/uiFLmodel.h"
#include <Stats/Statistics.h>		//TODO: Move statistics data types to separate class
#include "BikeNavProtocol.h"

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

	void updateIP(const String& ipStr);
	void updateSSIDList(const String& ssidStr);
	void updateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, uint8_t apStaCount);


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

	// Manual RimRidgeRQ show/hide - tap on rr_line_rq (the road-quality
	// indicator, RimRidge) / swipe gesture on the RQ screen itself. No
	// auto-popup logic here (unlike the Nav screen) - this screen only
	// ever opens on a deliberate tap, so no extra bookkeeping is needed
	// to keep it from fighting anything else.
	void showRQScreen();
	void hideRQScreen();

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

	void setChartArray(int16_t a[], uint8_t idx);
	void setChartPosFirst(uint16_t pos, uint8_t idx);
	void updateChart();

	enum UIColor {
		UI_ColorNeutral,
		UI_ColorOK,
		UI_ColorWarn,
		UI_ColorCrit
	};

	void updateStateIcon(Statistics::EDrivingState state, UIColor col);

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


	UiFLModel uifl;

	Ticker updateTicker;
	Ticker dataTicker;

	//TODO: Use notify/poll mechanism instead of storing data
	int16_t hr = -1, cad = -1;
	float grad = NAN, height = NAN, speed = NAN;

	Statistics::ESummaryType statMode = Statistics::SUM_ESP_START;
	Statistics::EAvgType     statTimeMode = Statistics::AVG_ALL;

	// Model for TRGB specific data
	float batIntVoltage = NAN, batIntVoltageAvg = NAN;
	int8_t batIntPerc = -1;
	bool batIntCharging = false;

	// Is RimRidgeNav the currently displayed screen? Written by
	// evaluateNaviAutoSwitch() AND by showNavScreen()/hideNavScreen()
	// (manual tap/swipe) - shared so the two mechanisms don't fight each
	// other (see updateNavi()'s comment).
	bool navScreenActive = false;

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
