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

#ifndef _ITEM_WEAPONKIND_HPP
#define _ITEM_WEAPONKIND_HPP

#include "item/Kind.hpp"


namespace ts { namespace item {

class WeaponKind;

typedef std::shared_ptr<WeaponKind>        WeaponKindP;
typedef std::shared_ptr<const WeaponKind>  WeaponKindPC;


class WeaponKind : public Kind {

public:

	typedef const ConcreteClass<WeaponKind>  ClassT;
	typedef std::shared_ptr<const ClassT>    ClassPC;


	WeaponKind(const ClassPC& clazz, KindId id);

	virtual ItemP newItem () const;


protected:

	virtual bool setParameter (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);


private:

	LOGGER_DECLARATION;

	// 4 bytes
	AmmoAction_t  _ammoConsumption;  // XML/ammoaction
	Ammo_t        _ammoType;         // XML/ammotype
	int32_t       _attack;           // XML/attack
	int32_t       _attackBonus;      // XML/extraattack
	uint32_t      _attackSpeed;      // XML/attackspeed
	uint32_t      _range;            // XML/range
	ShootEffect_t _shootEffect;      // XML/shoottype
	WeaponType_t  _weaponClass;      // XML/weapontype

	// 2 bytes
	int16_t  _damage;  // XML/damage

	// 1 byte
	int8_t     _breakChance;       // XML/breakchance
	DamageType _damageType;        // XML/element*
	int8_t     _hitChance;         // XML/hitchance
	int8_t     _maximumHitChance;  // XML/maxhitchance

};

}} // namespace ts::item

#endif // _ITEM_WEAPONKIND_HPP
