/*
 * nav_icons.c
 *
 * See nav_icons.h. Icon assets themselves are defined in
 * ui_img_nav_64_allimages.c / ui_img_nav_allimages.c - declared right here
 * (used to pull these in via the now-removed SNavi screen's ui.h, which
 * declared them for its own unrelated reasons; SNavi is gone as of
 * 2026-09-26 but these two shared bitmap-definition files are not, they're
 * genuinely used by RimRidge/RimRidgeNav/RimRidgeRQ via navIcon64()/
 * navIconLarge()).
 */

#include "nav_icons.h"
#include "roundabout-icon.h"

LV_IMG_DECLARE(nav_reserved)
LV_IMG_DECLARE(nav_straight)
LV_IMG_DECLARE(nav_start)
LV_IMG_DECLARE(nav_finish)
LV_IMG_DECLARE(nav_left45)
LV_IMG_DECLARE(nav_left90)
LV_IMG_DECLARE(nav_left135)
LV_IMG_DECLARE(nav_right135)
LV_IMG_DECLARE(nav_right90)
LV_IMG_DECLARE(nav_right45)
LV_IMG_DECLARE(nav_fork_r)
LV_IMG_DECLARE(nav_fork_l)
LV_IMG_DECLARE(nav_uturn)
LV_IMG_DECLARE(nav_Kreisel_3_3)
LV_IMG_DECLARE(nav_nonav)
LV_IMG_DECLARE(nav_64_reserved)
LV_IMG_DECLARE(nav_64_straight)
LV_IMG_DECLARE(nav_64_start)
LV_IMG_DECLARE(nav_64_finish)
LV_IMG_DECLARE(nav_64_left45)
LV_IMG_DECLARE(nav_64_left90)
LV_IMG_DECLARE(nav_64_left135)
LV_IMG_DECLARE(nav_64_right135)
LV_IMG_DECLARE(nav_64_right90)
LV_IMG_DECLARE(nav_64_right45)
LV_IMG_DECLARE(nav_64_fork_r)
LV_IMG_DECLARE(nav_64_fork_l)
LV_IMG_DECLARE(nav_64_uturn)
LV_IMG_DECLARE(nav_64_Kreisel_3_3)
LV_IMG_DECLARE(nav_64_nonav)

const lv_img_dsc_t* navIcon64(uint8_t maneuver, uint8_t roundaboutExit) {
	switch (maneuver) {
	case NAV_MANEUVER_NONE:              return &nav_64_nonav;
	case NAV_MANEUVER_DEPART:            return &nav_64_start;
	case NAV_MANEUVER_ARRIVE:            return &nav_64_finish;
	case NAV_MANEUVER_STRAIGHT:          return &nav_64_straight;
	case NAV_MANEUVER_TURN_SLIGHT_LEFT:  return &nav_64_left45;
	case NAV_MANEUVER_TURN_LEFT:         return &nav_64_left90;
	case NAV_MANEUVER_TURN_SHARP_LEFT:   return &nav_64_left135;
	case NAV_MANEUVER_TURN_SLIGHT_RIGHT: return &nav_64_right45;
	case NAV_MANEUVER_TURN_RIGHT:        return &nav_64_right90;
	case NAV_MANEUVER_TURN_SHARP_RIGHT:  return &nav_64_right135;
	case NAV_MANEUVER_KEEP_LEFT:         return &nav_64_fork_l;
	case NAV_MANEUVER_KEEP_RIGHT:        return &nav_64_fork_r;
	case NAV_MANEUVER_UTURN_LEFT:
	case NAV_MANEUVER_UTURN_RIGHT:       return &nav_64_uturn;		// no separate mirrored asset, same icon for both directions
	case NAV_MANEUVER_ROUNDABOUT: {
		const lv_img_dsc_t* icon = roundabout_icon_get(roundaboutExit, 64);
		return icon ? icon : &nav_64_reserved;
	}
	default:                             return &nav_64_reserved;
	}
}

const lv_img_dsc_t* navIconLarge(uint8_t maneuver, uint8_t roundaboutExit) {
	switch (maneuver) {
	case NAV_MANEUVER_NONE:              return &nav_nonav;
	case NAV_MANEUVER_DEPART:            return &nav_start;
	case NAV_MANEUVER_ARRIVE:            return &nav_finish;
	case NAV_MANEUVER_STRAIGHT:          return &nav_straight;
	case NAV_MANEUVER_TURN_SLIGHT_LEFT:  return &nav_left45;
	case NAV_MANEUVER_TURN_LEFT:         return &nav_left90;
	case NAV_MANEUVER_TURN_SHARP_LEFT:   return &nav_left135;
	case NAV_MANEUVER_TURN_SLIGHT_RIGHT: return &nav_right45;
	case NAV_MANEUVER_TURN_RIGHT:        return &nav_right90;
	case NAV_MANEUVER_TURN_SHARP_RIGHT:  return &nav_right135;
	case NAV_MANEUVER_KEEP_LEFT:         return &nav_fork_l;
	case NAV_MANEUVER_KEEP_RIGHT:        return &nav_fork_r;
	case NAV_MANEUVER_UTURN_LEFT:
	case NAV_MANEUVER_UTURN_RIGHT:       return &nav_uturn;		// no separate mirrored asset, same icon for both directions
	case NAV_MANEUVER_ROUNDABOUT: {
		const lv_img_dsc_t* icon = roundabout_icon_get(roundaboutExit, 192);
		return icon ? icon : &nav_Kreisel_3_3;
	}
	default:                             return &nav_reserved;
	}
}
