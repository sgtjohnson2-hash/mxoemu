#ifndef MXOEMU_DOJOSPAWN_H
#define MXOEMU_DOJOSPAWN_H

#include "LocationVector.h"
#include <cmath>

// Training-dojo bot placement. World units are 100 per metre and "forward" for a
// player at heading rot is (-sin(rot), -cos(rot)) - the convention
// PlayerObject::GoAhead() walks with. The returned position is distM metres from the
// player along (heading + angleOffsetRad), and its rot turns it to face the player
// (so forward points back at the player).
inline LocationVector DojoPlaceInFront(const LocationVector& player, float distM, float angleOffsetRad)
{
	const double heading = player.rot + angleOffsetRad;
	const double bx = player.x - std::sin(heading) * distM * 100.0;
	const double bz = player.z - std::cos(heading) * distM * 100.0;
	LocationVector out(bx, player.y, bz);
	out.rot = std::atan2(-(player.x - bx), -(player.z - bz));
	return out;
}

#endif
