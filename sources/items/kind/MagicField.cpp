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
#include "items/kind/MagicField.hpp"

#include "items/Class.hpp"
#include "utility/Utility.hpp"
#include "xml/Xml.hpp"
#include "condition.h"

using namespace ts;
using namespace ts::items;
using namespace ts::items::kind;


LOGGER_DEFINITION(ts::items::kind::MagicField);


MagicField::MagicField(const ClassP& clazz)
	: Kind(std::static_pointer_cast<Kind::ClassT>(clazz)),
	  _damage(0),
	  _damageCount(0),
	  _totalDamage(0),
	  _damageType(DamageType::NONE),
	  _replaceable(true)
{}


bool MagicField::assemble() {
	if (!Kind::assemble()) {
		return false;
	}

	if (_damage != 0 || _damageInterval != DamageInterval::zero() || _damageCount != 0 || _damageType != DamageType::NONE || _totalDamage != 0) {
		if (_damage == 0) {
			LOGe("Magic Field has damage properties but no damage set.");
			return false;
		}
		if (_damageCount == 0 && _totalDamage == 0) {
			LOGe("Magic Field has damage properties but neither damage count nor total damage set.");
			return false;
		}
		if (_damageCount != 0 && _totalDamage != 0) {
			LOGw("Magic Field has both damage count and total damage set. Will unset damage count for you.");
			_damageCount = 0;
		}
		if (_damageType == DamageType::NONE) {
			LOGe("Magic Field has damage properties but no damage count set.");
			return false;
		}

		if (_damageInterval == DamageInterval::zero()) {
			if (_damageCount > 1) {
				LOGe("Magic Field has more than one damage count but no damage interval set.");
				return false;
			}
			if (_totalDamage > 1) {
				LOGe("Magic Field has total damage but no damage interval set.");
				return false;
			}
		}

		ConditionType_t conditionType = CONDITION_NONE;
		switch (_damageType) {
		case DamageType::DEATH:
			conditionType = CONDITION_CURSED;
			break;

		case DamageType::EARTH:
			conditionType = CONDITION_POISON;
			break;

		case DamageType::ENERGY:
			conditionType = CONDITION_ENERGY;
			break;

		case DamageType::FIRE:
			conditionType = CONDITION_FIRE;
			break;

		case DamageType::HOLY:
			conditionType = CONDITION_DAZZLED;
			break;

		case DamageType::ICE:
			conditionType = CONDITION_FREEZING;
			break;

		case DamageType::DROWNING:
			conditionType = CONDITION_DROWN;
			break;

		case DamageType::PHYSICAL:
			conditionType = CONDITION_PHYSICAL;
			break;

		case DamageType::HEALING:
		case DamageType::HEALTH_DRAIN:
		case DamageType::MANA_DRAIN:
		case DamageType::NONE:
		case DamageType::UNDEFINED:
			LOGe("Magic Field has an unsupported damage type set.");
			return false;
		}

		ConditionDamageP condition = std::make_shared<ConditionDamage>(CONDITIONID_COMBAT, conditionType, false, 0);
		if (_totalDamage > 0) {
			for (auto damage : ConditionDamage::generateDamageVector(_totalDamage, _damage)) {
				condition->addDamage(1, _damageInterval.count(), -damage);
			}
		}
		else {
			condition->addDamage(_damageCount, _damageInterval.count(), -_damage);
		}

		if (condition->getTotalDamage() > 0) {
			condition->setParam(CONDITIONPARAM_FORCEUPDATE, true);
		}
	}

	return true;
}


bool MagicField::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
	if (Kind::setParameter(name, value, deprecatedNode)) {
		return true;
	}

	if (name == "damage") {
		_setParameter(name, value, _damage);
	}
	else if (name == "damageCount") {
		_setParameter(name, value, _damageCount);
	}
	else if (name == "damageInterval") {
		uint8_t numericValue;
		if (!(value >> numericValue)) {
			LOGe("Attribute '" << name << "' is not a valid integer in range 0.." << std::numeric_limits<uint8_t>::max());
			return true;
		}

		_damageInterval = DamageInterval(numericValue);
	}
	else if (name == "damageType") {
		DamageType damageType = combat::getDamageTypeWithId(value);
		switch (damageType) {
		case DamageType::NONE:
			LOGe("Attribute '" << name << "' must be a valid damage type");
			return true;

		case DamageType::HEALING:
		case DamageType::HEALTH_DRAIN:
		case DamageType::MANA_DRAIN:
		case DamageType::UNDEFINED:
			LOGe("Attribute '" << name << "' uses unsupported damage type '" << value << "'");
			return true;

		case DamageType::DEATH:
		case DamageType::EARTH:
		case DamageType::ENERGY:
		case DamageType::FIRE:
		case DamageType::HOLY:
		case DamageType::ICE:
		case DamageType::DROWNING:
		case DamageType::PHYSICAL:
			// okay
			break;
		}

		_damageType = damageType;
	}
	else if (name == "field" && deprecatedNode != nullptr) {
		LOGw("The dedicated 'field' <attribute> is deprecated and will be replaced by <magicField damage=\"123\" damageType=\"fire\" damageInterval=\"5\" damageCount=\"5\" (or totalDamage=\"1000\")/>.");

		// old code
		DamageType damageType = combat::getDamageTypeWithId(value);
		switch (damageType) {
		case DamageType::NONE:
			LOGe("Attribute '" << name << "' must be a valid damage type");
			return true;

		case DamageType::HEALING:
		case DamageType::HEALTH_DRAIN:
		case DamageType::MANA_DRAIN:
		case DamageType::UNDEFINED:
			LOGe("Attribute '" << name << "' uses unsupported damage type '" << value << "'");
			return true;

		case DamageType::DEATH:
		case DamageType::EARTH:
		case DamageType::ENERGY:
		case DamageType::FIRE:
		case DamageType::HOLY:
		case DamageType::ICE:
		case DamageType::DROWNING:
		case DamageType::PHYSICAL:
			// okay
			break;
		}

		uint32_t start = 0;
		uint32_t damage = 0;

		for (xmlNodePtr child = deprecatedNode->children; child != nullptr; child = child->next) {
			if (child->type != XML_ENTITY_NODE) {
				continue;
			}

			std::string name = xml::name(*child);
			if (name != "attribute") {
				LOGw("Unknown <" << name << "> node in 'field' node detected");
				continue;
			}

			if (!xml::attribute(*child, "key", name)) {
				LOGe("Defect <attribute> node in 'field' node detected");
				continue;
			}

			std::string value;
			if (!xml::attribute(*child, "value", value)) {
				LOGe("Attribute '" << name << "' in 'field' has no value");
				continue;
			}

			if (name == "count") {
				if (!(value >> _damageCount)) {
					LOGe("'field' parameter 'count' is not a valid integer in range 0.." << std::numeric_limits<uint32_t>::max());
				}
				continue;
			}
			else if (name == "damage") {
				if (!(value >> damage)) {
					LOGe("'field' parameter 'damage' is not a valid integer in range 0.." << std::numeric_limits<uint32_t>::max());
				}
				continue;
			}
			else if (name == "ticks") {
				uint16_t interval;
				if (!(value >> interval)) {
					LOGe("'field' parameter 'ticks' is not a valid integer in range 0.." << std::numeric_limits<uint16_t>::max());
					continue;
				}

				_damageInterval = DamageInterval(interval);
				continue;
			}
			else if (name == "start") {
				if (!(value >> start)) {
					LOGe("'field' parameter 'start' is not a valid integer in range 0.." << std::numeric_limits<uint32_t>::max());
				}
				continue;
			}
			else {
				LOGe("Unknown 'field' parameter '" << name << "'");
			}
		}

		if (start > 0) {
			_damage = start;
			_totalDamage = damage;
		}
		else {
			_damage = damage;
			_totalDamage = 0;
		}
	}
	else if (name == "replaceable" || name == "replacable") {
		_setParameter(name, value, _replaceable, "replaceable");
	}
	else if (name == "totalDamage") {
		_setParameter(name, value, _totalDamage);
	}
	else {
		return false;
	}

	return true;
}
