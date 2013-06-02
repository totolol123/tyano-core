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
#include "items/kind/Weapon.hpp"

#include "items/Class.hpp"
#include "tools.h"

using namespace ts;
using namespace ts::items;
using namespace ts::items::kind;

LOGGER_DEFINITION(ts::items::kind::Weapon);


Weapon::Weapon(const ClassP& clazz)
	: Kind(std::static_pointer_cast<Kind::ClassT>(clazz)),
	  _ammoConsumption(AMMOACTION_NONE),
	  _ammoType(AMMO_NONE),
	  _attack(0),
	  _attackBonus(0),
	  _attackSpeed(0),
	  _range(1),
	  _shootEffect(SHOOT_EFFECT_UNKNOWN),
	  _weaponClass(WEAPON_NONE),
	  _damage(0),
	  _breakChance(-1),
	  _damageType(DamageType::NONE),
	  _hitChance(-1),
	  _maximumHitChance(-1)
{}


bool Weapon::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
	if (Kind::setParameter(name, value, deprecatedNode)) {
		return true;
	}

	if (name == "ammoConsumption" || name == "ammoaction") {
		_checkDeprecatedParameter(name, "ammoConsumption");

		AmmoAction_t ammoConsumption = getAmmoAction(value);
		if (ammoConsumption != AMMOACTION_NONE) {
			_ammoConsumption = ammoConsumption;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid ammunition consumption (" << getAmmoConsumptionNames() << ")");
		}
	}
	else if (name == "ammoType" || name == "ammotype") {
		_checkDeprecatedParameter(name, "ammoType");

		Ammo_t ammoType = getAmmoType(value);
		if (ammoType != AMMO_NONE) {
			_ammoType = ammoType;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid ammunition type (" << getAmmoTypeNames() << ")");
		}
	}
	else if (name == "attack") {
		_setParameter(name, value, _attack);
	}
	else if (name == "attackBonus" || name == "extraattack" || name == "extraatk") {
		_setParameter(name, value, _attackBonus, "attackBonus");
	}
	else if (name == "attackSpeed" || name == "attackspeed") {
		_setParameter(name, value, _attackSpeed, "attackSpeed");
	}
	else if (name == "breakChance" || name == "breakchance") {
		_setParameter(name, value, _breakChance, X_INT8(0), X_INT8(100), "breakChance");
	}
	else if (name == "damage") {
		_setParameter(name, value, _damage);
	}
	else if (name == "damageType") {
		DamageType damageType = combat::getDamageTypeWithId(value);
		if (damageType != DamageType::NONE) {
			_damageType = damageType;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid damage type");
		}
	}
	else if (name == "elementenergy") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::ENERGY;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementphysical") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::PHYSICAL;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementfire") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::FIRE;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementearth") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::EARTH;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementice") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::ICE;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementholy") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::HOLY;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementdeath") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::DEATH;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementlifedrain") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::HEALTH_DRAIN;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementmanadrain") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::MANA_DRAIN;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementhealing") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::HEALING;
		_setParameter(name, value, _damage);
	}
	else if (name == "elementundefined") {
		_checkDeprecatedParameter(name, "damage");
		_checkDeprecatedParameter(name, "damageType");

		_damageType = DamageType::UNDEFINED;
		_setParameter(name, value, _damage);
	}
	else if (name == "hitChance" || name == "hitchance") {
		_setParameter(name, value, _hitChance, X_INT8(0), X_INT8(100), "hitChance");
	}
	else if (name == "maximumHitChance" || name == "maxhitchance") {
		_setParameter(name, value, _maximumHitChance, X_INT8(0), X_INT8(100), "maximumHitChance");
	}
	else if (name == "range") {
		_setParameter(name, value, _range);
	}
	else if (name == "shootEffect" || name == "shoottype") {
		_checkDeprecatedParameter(name, "shootEffect");

		ShootEffect_t shootEffect = getShootType(value);
		if (shootEffect != SHOOT_EFFECT_UNKNOWN) {
			_shootEffect = shootEffect;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid shoot effect (" << getShootEffectNames() << ")");
		}
	}
	else if (name == "weaponClass" || name == "weapontype") {
		_checkDeprecatedParameter(name, "weaponClass");

		if (value == "ammunition") {
			_weaponClass = WEAPON_AMMO;
		}
		else if (value == "axe") {
			_weaponClass = WEAPON_AXE;
		}
		else if (value == "club") {
			_weaponClass = WEAPON_CLUB;
		}
		else if (value == "distance") {
			_weaponClass = WEAPON_DIST;
		}
		else if (value == "fist") {
			_weaponClass = WEAPON_FIST;
		}
		else if (value == "shield") {
			_weaponClass = WEAPON_SHIELD;
		}
		else if (value == "sword") {
			_weaponClass = WEAPON_SWORD;
		}
		else if (value == "wand") {
			_weaponClass = WEAPON_WAND;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid weapon class (ammunition, axe, club, distance, fist, shield, sword or wand)");
		}
	}
	else {
		return false;
	}

	return true;
}
