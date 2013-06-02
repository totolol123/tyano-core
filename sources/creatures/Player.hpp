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

#ifndef _CREATURES_PLAYER_HPP
#define _CREATURES_PLAYER_HPP

namespace ts {
namespace creatures {

	enum class Attribute : uint8_t {
		NONE = 0,
		AXE_FIGHTING,
		CLUB_FIGHTING,
		DISTANCE_FIGHTING,
		FISHING,
		FIST_FIGHTING,
		HEALTH,
		MANA,
		SHIELD_HANDLING,
		SOUL,
		SWORD_FIGHTING,
	};


	Attribute getAttributeWithId(const std::string& id);

} // namespace creatures
} // namespace ts



namespace std {

	template <>
	struct hash<ts::creatures::Attribute> : function<size_t(const ts::creatures::Attribute)> {
		typedef std::underlying_type<argument_type>::type  underlying_type;

		result_type operator()(const argument_type& type) const {
			return std::hash<underlying_type>()(static_cast<underlying_type>(type));
		}
	};

} // namespace std

#endif // _CREATURES_PLAYER_HPP
