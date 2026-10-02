/*
 * RimRidgeClimbCustFunc.h
 *
 * Hand-written data wiring for the EEZ Studio-generated "RimRidgeClimb" screen (climb view:
 * elevation profile of the climb ahead, coloured by gradient). Same pattern as the other
 * RimRidge*CustFunc files -- outside src/ui_eez/, which every EEZ export replaces.
 *
 * Screen switching (UIFacade): shown automatically when a rated climb starts and left again
 * some seconds after it (Climb::Config::autoShow/autoShowMinRank/showAheadM/hideDelayS);
 * by hand with a tap on the altitude or gradient value of RimRidge (action GoToClimb), back
 * with a swipe (ClimbScreenGesture). A climb swiped away stays away.
 *
 * Widgets (prefix rrclimb_):
 *   cat                   "KAT. 3" / "ANSTIEG" (not rated) / "KEIN ANSTIEG"
 *   rem_val               height left to the summit -- the figure to ride by
 *   total_val, dist_val   height of the whole climb, distance to the summit
 *   profile               box the profile is drawn into (draw event, no canvas buffer): one
 *                         column per pixel, coloured by the gradient of its 25 m step
 *                         (Climb::Config::gradeBandPct -> zone blue/green/yellow/orange/red),
 *                         the part behind the rider dimmed, a marker at the rider and a flag
 *                         on the summit. Shows the climb from 100 m before its foot to 150 m
 *                         behind the summit (to the end of the profile if the summit is not
 *                         in it yet); the whole profile if there is no climb.
 *   summit_alt            altitude of the summit, in the box's corner
 *   ahead_val/_cap        mean gradient of the next lookAheadM of the profile, in its colour
 *   grad_val              the measured gradient (as on RimRidge)
 *   speed_val, hr_val     small
 *   grp_info              one field showing in turn: time, distance, temperature, cadence,
 *                         altitude -- fading in on each change, every infoCycleS
 *   arc                   share of the climb's height that is done
 *   group_rq_mode         the state icon/RQ line group all screens share (RimRidgeCustFunc.cpp)
 *
 * A ">" in front of a figure: the summit is not in the profile yet, so it is "at least".
 * No longer shown: TrailBridge sends a climb up to its summit (Status::summitOpen stays false).
 */

#pragma once

#include <stdint.h>
#include "ClimbProfile.h"

// Once from UIFacade::initDisplay(), after create_screen_rim_ridge_climb().
void ui_RimRidgeClimbInit();

// Fan-out from the same data as RimRidge's speed/HR/gradient.
void ui_RimRidgeClimbUpdateSpeed(float speed);
void ui_RimRidgeClimbUpdateHR(int16_t hr);
void ui_RimRidgeClimbUpdateGrad(float grad);

// For the rotating info field. Values that are missing (NAN, cadence < 0) are skipped.
struct RimRidgeClimbInfo {
	const char* clock;		// "HH:MM"
	uint32_t distM;			// the distance shown on RimRidge
	float temperature;
	int16_t cadence;
	float heightM;			// barometric
};

// Once a second, xUIDrawMutex held. version: of the profile points (ClimbMonitor::poll()) --
// they are fetched from ClimbMonitor when it changed.
void ui_RimRidgeClimbUpdate(const Climb::Status& st, const Climb::Config& cfg, uint32_t version, const RimRidgeClimbInfo& info);
