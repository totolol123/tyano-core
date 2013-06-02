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

#ifndef _COMBAT_COMBAT_HPP
#define _COMBAT_COMBAT_HPP

#include "const.h"


namespace ts {
namespace combat {

	enum class DamageType : uint8_t {
		NONE = 0,
		DEATH,
		DROWNING,
		EARTH,
		ENERGY,
		FIRE,
		HEALING,
		HOLY,
		ICE,
		HEALTH_DRAIN,
		MANA_DRAIN,
		PHYSICAL,
		UNDEFINED,
	};



	ConditionType_t getConditionTypeWithId (const std::string& id);
	DamageType      getDamageTypeWithId    (const std::string& id, bool includeDeprecated = false);

} // namespace combat
} // namespace ts



namespace std {

	template <>
	struct hash<ts::combat::DamageType> : function<size_t(const ts::combat::DamageType)> {
		typedef std::underlying_type<argument_type>::type  underlying_type;

		result_type operator()(const argument_type& type) const {
			return std::hash<underlying_type>()(static_cast<underlying_type>(type));
		}
	};

} // namespace std

#endif // _COMBAT_COMBAT_HPP
