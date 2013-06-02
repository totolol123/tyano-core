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

#ifndef _ITEMS_KIND_MAGICFIELD_HPP
#define _ITEMS_KIND_MAGICFIELD_HPP

#include "items/Kind.hpp"

class ConditionDamage;

typedef std::shared_ptr<ConditionDamage>  ConditionDamageP;


namespace ts {
namespace items {
namespace kind {

	class MagicField : public Kind {

	public:

		typedef const Class<MagicField>         ClassT;
		typedef std::shared_ptr<ClassT>         ClassP;
		typedef std::chrono::duration<uint8_t>  DamageInterval;


		MagicField(const ClassP& clazz);


	protected:

		virtual bool assemble     ();
		virtual bool setParameter (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);


	private:

		LOGGER_DECLARATION;


		// 4+ bytes
		uint32_t         _damage;          // XML/field
		ConditionDamageP _damageCondition; // XML/field
		uint32_t         _damageCount;     // XML/field
		uint32_t         _totalDamage;     // XML/field

		// 1 byte
		DamageInterval _damageInterval;  // XML/field
		DamageType     _damageType;      // XML/field
		bool           _replaceable;     // XML/replaceable

	};

} // namespace kind
} // namespace items
} // namespace ts

#endif // _ITEMS_KIND_MAGICFIELD_HPP
