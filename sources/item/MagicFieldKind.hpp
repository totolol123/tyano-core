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

#ifndef _ITEM_MAGICFIELDKIND_HPP
#define _ITEM_MAGICFIELDKIND_HPP

#include "item/Kind.hpp"

class ConditionDamage;

typedef std::shared_ptr<ConditionDamage>  ConditionDamageP;


namespace ts { namespace item {

class MagicFieldKind;

typedef std::shared_ptr<MagicFieldKind>        MagicFieldKindP;
typedef std::shared_ptr<const MagicFieldKind>  MagicFieldKindPC;


class MagicFieldKind : public Kind {

public:

	typedef const ConcreteClass<MagicFieldKind>  ClassT;
	typedef std::shared_ptr<const ClassT>        ClassPC;


	MagicFieldKind(const ClassPC& clazz, KindId id);

	        const ConditionDamage* getDamageCondition () const;
	        DamageType             getDamageType      () const;
	        bool                   isReplaceable      () const;
	virtual ItemP                  newItem            () const;


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
	Duration   _damageInterval;  // XML/field
	DamageType _damageType;      // XML/field
	bool       _replaceable;     // XML/replaceable

};

}} // namespace ts::item

#endif // _ITEM_MAGICFIELDKIND_HPP
