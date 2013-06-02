////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#include "otpch.h"
#include "creatures/Player.hpp"

namespace ts {
namespace creatures {


Attribute getAttributeWithId(const std::string& id) {
	if (id == "axeFighting") {
		return Attribute::AXE_FIGHTING;
	}
	if (id == "clubFighting") {
		return Attribute:: CLUB_FIGHTING;
	}
	if (id == "distanceFighting") {
		return Attribute::DISTANCE_FIGHTING;
	}
	if (id == "fishing") {
		return Attribute::FISHING;
	}
	if (id == "fistFighting") {
		return Attribute::FIST_FIGHTING;
	}
	if (id == "health") {
		return Attribute::HEALTH;
	}
	if (id == "mana") {
		return Attribute::MANA;
	}
	if (id == "shieldHandling") {
		return Attribute::SHIELD_HANDLING;
	}
	if (id == "soul") {
		return Attribute::SOUL;
	}
	if (id == "swordFighting") {
		return Attribute::SWORD_FIGHTING;
	}

	return Attribute::NONE;
}

} // namespace creatures
} // namespace ts
