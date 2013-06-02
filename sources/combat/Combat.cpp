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
#include "combat/Combat.hpp"


namespace ts {
namespace combat {

ConditionType_t getConditionTypeWithId(const std::string& id) {
	if (id == "attributes") {
		return CONDITION_ATTRIBUTES;
	}
	if (id == "cursed") {
		return CONDITION_CURSED;
	}
	if (id == "dazzled") {
		return CONDITION_DAZZLED;
	}
	if (id == "drown") {
		return CONDITION_DROWN;
	}
	if (id == "drunk") {
		return CONDITION_DRUNK;
	}
	if (id == "energy") {
		return CONDITION_ENERGY;
	}
	if (id == "exhaust") {
		return CONDITION_EXHAUST;
	}
	if (id == "fire") {
		return CONDITION_FIRE;
	}
	if (id == "freezing") {
		return CONDITION_FREEZING;
	}
	if (id == "gamemaster") {
		return CONDITION_GAMEMASTER;
	}
	if (id == "haste") {
		return CONDITION_HASTE;
	}
	if (id == "hunting") {
		return CONDITION_HUNTING;
	}
	if (id == "inFight") {
		return CONDITION_INFIGHT;
	}
	if (id == "invisible") {
		return CONDITION_INVISIBLE;
	}
	if (id == "light") {
		return CONDITION_LIGHT;
	}
	if (id == "manaShield") {
		return CONDITION_MANASHIELD;
	}
	if (id == "muted") {
		return CONDITION_MUTED;
	}
	if (id == "outfit") {
		return CONDITION_OUTFIT;
	}
	if (id == "pacified") {
		return CONDITION_PACIFIED;
	}
	if (id == "paralyze") {
		return CONDITION_PARALYZE;
	}
	if (id == "physical") {
		return CONDITION_PHYSICAL;
	}
	if (id == "poison") {
		return CONDITION_POISON;
	}
	if (id == "regeneration") {
		return CONDITION_REGENERATION;
	}
	if (id == "soul") {
		return CONDITION_SOUL;
	}

	return CONDITION_NONE;
}


DamageType getDamageTypeWithId(const std::string& id, bool includeDeprecated) {
	if (id == "death") {
		return DamageType::DEATH;
	}
	if (id == "drowning") {
		return DamageType::DROWNING;
	}
	if (id == "earth") {
		return DamageType::EARTH;
	}
	if (id == "energy") {
		return DamageType::ENERGY;
	}
	if (id == "fire") {
		return DamageType::FIRE;
	}
	if (id == "healing") {
		return DamageType::HEALING;
	}
	if (id == "holy") {
		return DamageType::HOLY;
	}
	if (id == "ice") {
		return DamageType::ICE;
	}
	if (id == "healthDrain") {
		return DamageType::HEALTH_DRAIN;
	}
	if (id == "manaDrain") {
		return DamageType::MANA_DRAIN;
	}
	if (id == "physical") {
		return DamageType::PHYSICAL;
	}
	if (id == "undefined") {
		return DamageType::UNDEFINED;
	}

	if (includeDeprecated) {
		if (id == "cursed") {
			return DamageType::DEATH;
		}
		if (id == "dazzled") {
			return DamageType::HOLY;
		}
		if (id == "drown") {
			return DamageType::DROWNING;
		}
		if (id == "freezing") {
			return DamageType::ICE;
		}
		if (id == "poison") {
			return DamageType::EARTH;
		}
	}

	return DamageType::NONE;
}

} // namespace combat
} // namespace ts
