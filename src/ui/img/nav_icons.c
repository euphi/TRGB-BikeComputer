/*
 * nav_icons.c
 *
 * See nav_icons.h. Icon assets themselves are declared (LV_IMG_DECLARE) in
 * ui/Screens/SNavi/ui.h and defined in ui_img_nav_64_allimages.c / ui_img_nav_allimages.c.
 */

#include "nav_icons.h"
#include "ui/Screens/SNavi/ui.h"
#include "roundabout-icon.h"

const lv_img_dsc_t* navIcon64(uint8_t maneuver, uint8_t roundaboutExit) {
	switch (maneuver) {
	case NAV_MANEUVER_NONE:              return &nav_64_nonav;
	case NAV_MANEUVER_DEPART:            return &nav_64_start;
	case NAV_MANEUVER_ARRIVE:            return &nav_64_finish;
	case NAV_MANEUVER_STRAIGHT:          return &nav_64_straight;
	case NAV_MANEUVER_TURN_SLIGHT_LEFT:  return &nav_64_left45;
	case NAV_MANEUVER_TURN_LEFT:         return &nav_64_left90;
	case NAV_MANEUVER_TURN_SHARP_LEFT:   return &nav_64_left135;
	case NAV_MANEUVER_TURN_SLIGHT_RIGHT: return &nav_64_reserved;	// TODO: no nav_64_right45 asset compiled yet (doc/Resources/Images/nav_64_righ45.png was never converted)
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
