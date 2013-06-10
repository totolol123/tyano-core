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
#include "item/Kind.hpp"

#include "item/Class.hpp"
#include "utility/Utility.hpp"
#include "xml/Xml.hpp"
#include "tools.h"


namespace ts { namespace item {

LOGGER_DEFINITION(ts::item::Kind);

const std::string Kind::ATTRIBUTE_AID("aid");
const std::string Kind::ATTRIBUTE_ARMOR("armor");
const std::string Kind::ATTRIBUTE_ARTICLE("article");
const std::string Kind::ATTRIBUTE_ATTACK("attack");
const std::string Kind::ATTRIBUTE_ATTACKSPEED("attackspeed");
const std::string Kind::ATTRIBUTE_CHARGES("charges");
const std::string Kind::ATTRIBUTE_CORPSEOWNER("corpseowner");
const std::string Kind::ATTRIBUTE_DATE("date");
const std::string Kind::ATTRIBUTE_DECAYING("decaying");
const std::string Kind::ATTRIBUTE_DEFENSE("defense");
const std::string Kind::ATTRIBUTE_DESCRIPTION("description");
const std::string Kind::ATTRIBUTE_DURATION("duration");
const std::string Kind::ATTRIBUTE_EXTRAATTACK("extraattack");
const std::string Kind::ATTRIBUTE_EXTRADEFENSE("extradefense");
const std::string Kind::ATTRIBUTE_FLUIDTYPE("fluidtype");
const std::string Kind::ATTRIBUTE_HITCHANCE("hitchance");
const std::string Kind::ATTRIBUTE_NAME("name");
const std::string Kind::ATTRIBUTE_OWNER("owner");
const std::string Kind::ATTRIBUTE_PLURALNAME("pluralname");
const std::string Kind::ATTRIBUTE_SCRIPTPROTECTED("scriptprotected");
const std::string Kind::ATTRIBUTE_SHOOTRANGE("shootrange");
const std::string Kind::ATTRIBUTE_TEXT("text");
const std::string Kind::ATTRIBUTE_UID("uid");
const std::string Kind::ATTRIBUTE_WRITER("writer");


Kind::Kind(const ClassP& clazz, KindId id)
	: _armor(0),
	  _class(clazz),
	  _defense(0),
	  _defenseBonus(0),
	  _healthRegeneration(0),
	  _healthRegenerationInterval(0),
	  _lifetime(0),
	  _lightColor(0),
	  _lightLevel(0),
	  _manaRegeneration(0),
	  _manaRegenerationInterval(0),
	  _requiredLevel(0),
	  _requiredMagicLevel(0),
	  _speed(0),
	  _weight(0),
	  _wieldInfo(0),
	  _worth(0),
	  _charges(0),
	  _clientId(0),
	  _decayedCounterpartId(0),
	  _equipmentCounterpartId(0),
	  _groundSpeed(0),
	  _id(id),
	  _maximumTextLength(0),
	  _rotatedCounterpartId(0),
	  _writtenCounterpartId(0),
	  _floorChange0(FloorChange::NONE),
	  _floorChange1(FloorChange::NONE),
	  _hangableType(HangableType::NONE),
	  _blocksPathfinding(false),
	  _blocksProjectiles(false),
	  _blocksSolids(false),
	  _blocksSolidsExceptPocketables(false),
	  _containedFluid(FLUID_NONE),
	  _descriptionContainsAttributes(false),
	  _descriptionContainsCharges(false),
	  _descriptionContainsLifetime(false),
	  _descriptionContainsText(false),
	  _elevated(false),
	  _equipmentSlot(SLOT_WHEREEVER),
	  _grantsInvisibility(false),
	  _grantsManaShield(false),
	  _grantsRegeneration(false),
	  _movable(true),
	  _opaque(true),
	  _pileOrder(0),
	  _pocketable(false),
	  _persistent(false),
	  _preventsEquipmentLossOnDeath(false),
	  _preventsSkillLossOnDeath(false),
	  _protectsAgainstConditions(CONDITION_NONE),
	  _race(RACE_NONE),
	  _readable(false),
	  _stackable(false),
	  _usable(false),
	  _writable(false)
{
	assert(clazz != nullptr);
	assert(id != 0);
}


Kind::~Kind() {
}


bool Kind::assemble() {
	if (_id == 0) {
		LOGe("Cannot assemble item without ID.");
		return false;
	}

	if (_writable && !_readable) {
		LOGw("Items which are writable must also be readable. I will do that for you.");
		_readable = true;
	}

	return true;
}


void Kind::_checkDeprecatedParameter(const std::string& name, const char* newName) const {
	if (newName != nullptr && name != newName) {
		LOGw("Attribute '" << name << "' is deprecated. Use '" << newName << "'");
	}
}


bool Kind::_setAttributeParameter(const std::string& name, const std::string& value, Attribute attribute, const char* newName) {
	uint16_t increase;
	if (!_setParameter(name, value, increase, newName)) {
		return false;
	}

	if (_attributes.find(attribute) != _attributes.end()) {
		LOGe("Attribute '" << name << "' was set multiple times");
		return false;
	}

	_attributes[attribute] = increase;
	return true;
}


template<typename T>
typename std::enable_if<std::is_same<T,bool>::value,bool>::type
Kind::_setParameter(const std::string& name, const std::string& value, T& target, const char* newName) {
	_checkDeprecatedParameter(name, newName);

	if (!(value >> target)) {
		int8_t integerValue;
		if (value >> integerValue) {
			LOGw("Attribute '" << name << "' is a boolean and should be 'yes' or 'no', not '" << integerValue << "'");
			target = (integerValue != 0);
		}
		else {
			LOGe("Attribute '" << name << "' is not a valid boolean");
			return false;
		}
	}

	return true;
}


template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T,bool>::value,bool>::type
Kind::_setParameter(const std::string& name, const std::string& value, T& target, const char* newName) {
	return _setParameter(name, value, target, std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), newName);
}


template<>
bool Kind::_setParameter<std::string>(const std::string& name, const std::string& value, std::string& target, const char* newName) {
	_checkDeprecatedParameter(name, newName);

	target = value;
	return true;
}


template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
Kind::_setParameter(const std::string& name, const std::string& value, T& target, T min, T max, const char* newName) {
	assert(min <= max);

	_checkDeprecatedParameter(name, newName);

	T numericValue;
	if (!(value >> numericValue)) {
		LOGe("Attribute '" << name << "' is not a valid integer in range " << min << ".." << max);
		return false;
	}

	if (numericValue < min) {
		LOGw("Attribute '" << name << "' is too small: Increased from " << numericValue << " to " << min);
		numericValue = min;
	}
	else if (numericValue > max) {
		LOGw("Attribute '" << name << "' is too large: Decreased from " << numericValue << " to " << max);
		numericValue = max;
	}

	target = numericValue;
	return true;
}


bool Kind::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
#warning add to worth map & check duplicates

	if (name == "armor") {
		_setParameter(name, value, _armor);
	}
	else if (name == "blocksProjectiles" || name == "blockprojectile") {
		_setParameter(name, value, _blocksProjectiles, "blocksProjectiles");
	}
	else if (name == "blocksSolidsExceptPocketables" || name == "allowpickupable") {
		_setParameter(name, value, _blocksSolidsExceptPocketables, "_blocksSolidsExceptPocketables");
	}
	else if (name == "charges") {
		_setParameter(name, value, _charges);
	}
	else if (name == "containedFluid" || name == "fluidsource") {
		_checkDeprecatedParameter(name, "containedFluid");

		FluidTypes_t fluid = getFluidType(value);
		if (fluid != FLUID_NONE) {
			_containedFluid = fluid;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid fluid (" << getFluidNames() << ")");
		}
	}
	else if (name == "decayedCounterpart" || name == "decayto") {
		_setParameter(name, value, _decayedCounterpartId, "decayedCounterpart");
	}
	else if (name == "defense") {
		_setParameter(name, value, _defense);
	}
	else if (name == "defenseBonus" || name == "extradefense" || name == "extradef") {
		_setParameter(name, value, _defenseBonus, "defenseBonus");
	}
	else if (name == "description") {
		_setParameter(name, value, _description);
	}
	else if (name == "descriptionContainsAttributes" || name == "showattributes") {
		_setParameter(name, value, _descriptionContainsAttributes, "descriptionContainsAttributes");
	}
	else if (name == "descriptionContainsCharges" || name == "showcharges") {
		_setParameter(name, value, _descriptionContainsCharges, "descriptionContainsCharges");
	}
	else if (name == "descriptionContainsLifetime" || name == "showduration") {
		_setParameter(name, value, _descriptionContainsLifetime, "descriptionContainsLifetime");
	}
	else if (name == "equipmentCounterpart" || name == "transformequipto" || name == "transformdeequipto") {
		_setParameter(name, value, _equipmentCounterpartId, "equipmentCounterpart");
	}
	else if (name == "equipmentSlot" || name == "slottype") {
		_checkDeprecatedParameter(name, "equipmentSlot");

		if (_equipmentSlot != SLOT_WHEREEVER) {
			LOGe("Attribute '" << name << "' cannot be set multiple times");
			return true;
		}

		if (value == "ammo") {
			_equipmentSlot = SLOT_AMMO;
		}
		else if (value == "body") {
			_equipmentSlot = SLOT_ARMOR;
		}
		else if (value == "backpack") {
			_equipmentSlot = SLOT_BACKPACK;
		}
		else if (value == "feet") {
			_equipmentSlot = SLOT_FEET;
		}
		else if (value == "head") {
			_equipmentSlot = SLOT_HEAD;
		}
		else if (value == "legs") {
			_equipmentSlot = SLOT_LEGS;
		}
		else if (value == "necklace") {
			_equipmentSlot = SLOT_NECKLACE;
		}
		else if (value == "ring") {
			_equipmentSlot = SLOT_RING;
		}
		else if (value == "shield") {
			_equipmentSlot = SLOT_RIGHT;
		}
		else if (value == "weapon") {
			_equipmentSlot = SLOT_LEFT;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid equipment slot (ammo, body, backpack, feet, head, legs, necklace, ring, shield, weapon)");
		}
	}
	else if (name == "grantsInvisibility" || name == "invisible") {
		_setParameter(name, value, _grantsInvisibility, "grantsInvisibility");
	}
	else if (name == "grantsManaShield" || name == "manashield") {
		_setParameter(name, value, _grantsManaShield, "grantsManaShield");
	}
	else if (name == "healthRegeneration" || name == "healthgain") {
		_setParameter(name, value, _healthRegeneration, "healthRegeneration");
	}
	else if (name == "healthRegenerationInterval" || name == "healthticks") {
		_setParameter<std::chrono::milliseconds>(name, value, _healthRegenerationInterval, "healthRegenerationInterval");
	}
	else if (name == "lifetime" || name == "duration") {
		_setParameter<std::chrono::milliseconds>(name, value, _lifetime, "lifetime");
	}
	else if (name == "manaRegeneration" || name == "managain") {
		_setParameter(name, value, _manaRegeneration, "manaRegeneration");
	}
	else if (name == "manaRegenerationInterval" || name == "manaticks") {
		_setParameter<std::chrono::milliseconds>(name, value, _manaRegenerationInterval, "manaRegenerationInterval");
	}
	else if (name == "maxhealthpoints" || name == "maxhitpoints") {
		_setAttributeParameter(name, value, Attribute::HEALTH, "<attributes/health>");
	}
	else if (name == "maximumTextLength" || name == "maxtextlen" || name == "maxtextlength") {
		_setParameter(name, value, _maximumTextLength);
	}
	else if (name == "maxmanapoints") {
		_setAttributeParameter(name, value, Attribute::MANA, "<attributes/mana>");
	}
	else if (name == "movable" || name == "moveable") {
		_setParameter(name, value, _movable, "movable");
	}
	else if (name == "nameArticle" || name == "article") {
		_setParameter(name, value, _nameArticle, "nameArticle");
	}
	else if (name == "name") {
		_setParameter(name, value, _name);
	}
	else if (name == "persistent" || name == "forceserialize" || name == "forceserialization" || name == "forcesave") {
		_setParameter(name, value, _persistent, "persistent");
	}
	else if (name == "pluralName" || name == "plural") {
		_setParameter(name, value, _pluralName, "pluralName");
	}
	else if (name == "preventsEquipmentLossOnDeath" || name == "preventdrop") {
		_setParameter(name, value, _preventsEquipmentLossOnDeath, "preventsEquipmentLossOnDeath");
	}
	else if (name == "preventsSkillLossOnDeath" || name == "preventloss") {
		_setParameter(name, value, _preventsSkillLossOnDeath, "preventsSkillLossOnDeath");
	}
	else if (name == "race" || name == "corpsetype") {
		_checkDeprecatedParameter(name, "race");

		if (value == "blood") {
			_race = RACE_BLOOD;
		}
		else if (value == "energy") {
			_race = RACE_ENERGY;
		}
		else if (value == "fire") {
			_race = RACE_FIRE;
		}
		else if (value == "undead") {
			_race = RACE_UNDEAD;
		}
		else if (value == "venom") {
			_race = RACE_VENOM;
		}
		else {
			LOGe("Attribute '" << name << "' must be a valid race (blood, energy, fire, undead or venom)");
		}
	}
	else if (name == "readable") {
		 _setParameter(name, value, _readable, nullptr);
	}
	else if (name == "rotatedCounterpart" || name == "rotateto") {
		_setParameter(name, value, _rotatedCounterpartId, "rotatedCounterpart");
	}
	else if (name == "runeSpellName" || name == "runespellname") {
		_setParameter(name, value, _runeSpellName, "runeSpellName");
	}
	else if (name == "skillaxe") {
		_setAttributeParameter(name, value, Attribute::AXE_FIGHTING, "<attributes/axeFighting>");
	}
	else if (name == "skillclub") {
		_setAttributeParameter(name, value, Attribute::CLUB_FIGHTING, "<attributes/clubFighting>");
	}
	else if (name == "skilldist") {
		_setAttributeParameter(name, value, Attribute::DISTANCE_FIGHTING, "<attributes/distanceFighting>");
	}
	else if (name == "skillfish") {
		_setAttributeParameter(name, value, Attribute::FISHING, "<attributes/fishing>");
	}
	else if (name == "skillfist") {
		_setAttributeParameter(name, value, Attribute::FIST_FIGHTING, "<attributes/fistFighting>");
	}
	else if (name == "skillshield") {
		_setAttributeParameter(name, value, Attribute::SHIELD_HANDLING, "<attributes/shieldHandling>");
	}
	else if (name == "skillsword") {
		_setAttributeParameter(name, value, Attribute::SWORD_FIGHTING, "<attributes/swordFighting>");
	}
	else if (name == "soulpoints") {
		_setAttributeParameter(name, value, Attribute::SOUL, "<attributes/soul>");
	}
	else if (name == "speed") {
		_setParameter(name, value, _speed);
	}
	else if (name == "weight") {
		if (deprecatedNode != nullptr) {
			LOGi("Please note that the weight is given in oz now, i.e. <itemClass weight=\"12.34\" .../> instead of <attribute key=\"weight\" value=\"1234\"/>.");

			int32_t weight;
			if (!(value >> weight)) {
				LOGe("Attribute '" << name << "' is not a valid integer");
				return false;
			}

			_weight = weight / 100.0f;
		}
		else {
			_setParameter(name, value, _weight);
		}
	}
	else if (name == "worth") {
		_setParameter(name, value, _worth, nullptr);
	}
	else if (name == "writable" || name == "writeable") {
		_setParameter(name, value, _writable, "writable");
	}
	else if (name == "writtenCounterpart" || name == "writeonceitemid") {
		_setParameter(name, value, _writtenCounterpartId, "writtenCounterpart");
	}
	else {
		return false;
	}

	return true;
}


bool Kind::setParameter(const std::string& name, xmlNode& node) {
	if (name == "absorptions") {
		_setAbsorptions(name, node);
	}
	else if (name == "attributes") {
		_setAttributes(name, node);
	}
	else if (name == "floorChanges") {
		_setFloorChanges(name, node);
	}
	else if (name == "protectsAgainstConditions") {
		_setProtectsAgainstConditions(name, node);
	}
	else if (name == "reflections") {
		_setReflections(name, node);
	}

	return false;
}


void Kind::_setAbsorptions(const std::string& name, xmlNode& node) {
	for (xmlNodePtr child = node.children; child != nullptr; child = child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string id = xml::name(node);

		DamageType damageType;
		if (id == "all") {
			if (!_absorptions.empty()) {
				LOGe("Attribute <" << name << "> cannot mix damage types with 'all'");
				continue;
			}

			damageType = DamageType::NONE;
		}
		else {
			damageType = combat::getDamageTypeWithId(id);
			switch (damageType) {
			case DamageType::NONE:
				LOGe("Attribute <" << name << "> contains unknown damage type '" << id << "'");
				continue;

			case DamageType::DROWNING:
			case DamageType::HEALING:
				LOGe("Attribute <" << name << "> cannot contain absorptions for damage type '" << id << "'");
				continue;

			case DamageType::DEATH:
			case DamageType::EARTH:
			case DamageType::ENERGY:
			case DamageType::FIRE:
			case DamageType::HOLY:
			case DamageType::ICE:
			case DamageType::HEALTH_DRAIN:
			case DamageType::MANA_DRAIN:
			case DamageType::PHYSICAL:
			case DamageType::UNDEFINED:
				// okay
				break;
			}

			if (_absorptions.find(damageType) != _absorptions.end()) {
				LOGe("Attribute <" << name << "> contains multiple entries for damage type '" << id << "'");
				continue;
			}
		}

		xmlAttrPtr percentageAttribute = xml::attribute(*child, "percentage");
		if (percentageAttribute == nullptr) {
			LOGe("Damage type '" << id << "' in <" << name << "> has no attribute 'percentage'");
			continue;
		}

		uint8_t percentage;
		if (!xml::attribute(*percentageAttribute, percentage)) {
			LOGe("'percentage' of damage type '" << id << "' in <" << name << "> is not a valid integer (0..100)");
			continue;
		}

		if (percentage > 100) {
			LOGw("'percentage' of damage type '" << id << "' in <" << name << "> is too large: Decreased from " << percentage << " to 100");
			percentage = 100;
		}

		if (percentage == 0) {
			continue;
		}

		if (damageType == DamageType::NONE) {
			_absorptions[DamageType::DEATH] = percentage;
			_absorptions[DamageType::EARTH] = percentage;
			_absorptions[DamageType::ENERGY] = percentage;
			_absorptions[DamageType::FIRE] = percentage;
			_absorptions[DamageType::HOLY] = percentage;
			_absorptions[DamageType::ICE] = percentage;
			_absorptions[DamageType::HEALTH_DRAIN] = percentage;
			_absorptions[DamageType::MANA_DRAIN] = percentage;
			_absorptions[DamageType::PHYSICAL] = percentage;
			_absorptions[DamageType::UNDEFINED] = percentage;
		}
		else {
			_absorptions[damageType] = percentage;
		}
	}
}


void Kind::_setAttributes(const std::string& name, xmlNode& node) {
	for (xmlNodePtr child = node.children; child != nullptr; child = child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string id = xml::name(node);

		Attribute attribute = creature::getAttributeWithId(id);
		switch (attribute) {
		case Attribute::NONE:
			LOGe("Attribute <" << name << "> contains unknown character attribute '" << id << "'");
			continue;

		case Attribute::AXE_FIGHTING:
		case Attribute::CLUB_FIGHTING:
		case Attribute::DISTANCE_FIGHTING:
		case Attribute::FISHING:
		case Attribute::FIST_FIGHTING:
		case Attribute::HEALTH:
		case Attribute::MANA:
		case Attribute::SHIELD_HANDLING:
		case Attribute::SOUL:
		case Attribute::SWORD_FIGHTING:
			// okay
			break;
		}

		if (_attributes.find(attribute) != _attributes.end()) {
			LOGe("Attribute <" << name << "> contains multiple entries for character attribute '" << id << "'");
			continue;
		}

		xmlAttrPtr increaseAttribute = xml::attribute(*child, "increase");
		if (increaseAttribute == nullptr) {
			LOGe("Character attribute '" << id << "' in <" << name << "> has no attribute 'increase'");
			continue;
		}

		uint16_t increase;
		if (!xml::attribute(*increaseAttribute, increase)) {
			LOGe("'increase' of character attribute '" << id << "' in <" << name << "> is not a valid integer (0.." << std::numeric_limits<uint16_t>::max() << ")");
			continue;
		}

		if (increase == 0) {
			continue;
		}

		_attributes[attribute] = increase;
	}
}


void Kind::_setFloorChanges(const std::string& name, xmlNode& node) {
	_floorChange0 = FloorChange::NONE;
	_floorChange1 = FloorChange::NONE;

	for (xmlNodePtr child = node.children; child != nullptr; child = child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (!xmlStrcmp(child->name, reinterpret_cast<const xmlChar*>("floorChange"))) {
			LOGw("Attribute <" << name << "> contains unknown child node '" << reinterpret_cast<const char*>(child->name) << "'");
			continue;
		}

		std::string value;
		if (!xml::attribute(*child, "direction", value)) {
			LOGe("Missing attribute 'direction' in <floorChange>");
			continue;
		}

		FloorChange floorChange;

		if (value == "down") {
			floorChange = FloorChange::DOWN;
		}
		else if (value == "north") {
			floorChange = FloorChange::NORTH;
		}
		else if (value == "south") {
			floorChange = FloorChange::SOUTH;
		}
		else if (value == "west") {
			floorChange = FloorChange::WEST;
		}
		else if (value == "east") {
			floorChange = FloorChange::EAST;
		}
		else if (value == "northex") {
			floorChange = FloorChange::NORTH_EX;
		}
		else if (value == "southex") {
			floorChange = FloorChange::SOUTH_EX;
		}
		else if (value == "westex") {
			floorChange = FloorChange::WEST_EX;
		}
		else if (value == "eastex") {
			floorChange = FloorChange::EAST_EX;
		}
		else {
			LOGe("Attribute 'direction' in <floorChange> must be a direction change (down, east, eastex, north, northex, south, southex, west or westex)");
			continue;
		}

		if (_floorChange0 == FloorChange::NONE) {
			_floorChange0 = floorChange;
		}
		else if (_floorChange1 == FloorChange::NONE) {
			_floorChange1 = floorChange;
		}
		else {
			LOGe("Item cannot have more than two floor change entries");
			break;
		}
	}
}


void Kind::_setProtectsAgainstConditions(const std::string& name, xmlNode& node) {
	for (xmlNodePtr child = node.children; child != nullptr; child = child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string id = xml::name(node);

		ConditionType_t conditionType = combat::getConditionTypeWithId(id);
		switch (conditionType) {
		case CONDITION_NONE:
			LOGe("Attribute <" << name << "> contains unknown damage type '" << id << "'");
			continue;

		case CONDITION_GAMEMASTER:
		case CONDITION_HUNTING:
		case CONDITION_MUTED:
			LOGe("Attribute <" << name << "> cannot have protection against condition type '" << id << "'");
			continue;

		case CONDITION_ATTRIBUTES:
		case CONDITION_CURSED:
		case CONDITION_DAZZLED:
		case CONDITION_DROWN:
		case CONDITION_DRUNK:
		case CONDITION_ENERGY:
		case CONDITION_EXHAUST:
		case CONDITION_FIRE:
		case CONDITION_HASTE:
		case CONDITION_FREEZING:
		case CONDITION_INFIGHT:
		case CONDITION_INVISIBLE:
		case CONDITION_LIGHT:
		case CONDITION_MANASHIELD:
		case CONDITION_OUTFIT:
		case CONDITION_PACIFIED:
		case CONDITION_PARALYZE:
		case CONDITION_PHYSICAL:
		case CONDITION_POISON:
		case CONDITION_REGENERATION:
		case CONDITION_SOUL:
			// okay
			break;
		}

		if ((_protectsAgainstConditions & conditionType) != 0) {
			LOGw("Attribute <" << name << "> contains multiple entries for protection again condition type '" << id << "'");
			continue;
		}

		_protectsAgainstConditions = static_cast<ConditionType_t>(_protectsAgainstConditions|conditionType);
	}
}


void Kind::_setReflections(const std::string& name, xmlNode& node) {
	for (xmlNodePtr child = node.children; child != nullptr; child = child->next) {
		if (child->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string id = xml::name(node);

		DamageType damageType;
		if (id == "all") {
			if (!_reflections.empty()) {
				LOGe("Attribute <" << name << "> cannot mix damage types with 'all'");
				continue;
			}

			damageType = DamageType::NONE;
		}
		else {
			damageType = combat::getDamageTypeWithId(id);
			switch (damageType) {
			case DamageType::NONE:
				LOGe("Attribute <" << name << "> contains unknown damage type '" << id << "'");
				continue;

			case DamageType::DROWNING:
			case DamageType::HEALING:
				LOGe("Attribute <" << name << "> cannot contain reflections for damage type '" << id << "'");
				continue;

			case DamageType::DEATH:
			case DamageType::EARTH:
			case DamageType::ENERGY:
			case DamageType::FIRE:
			case DamageType::HOLY:
			case DamageType::ICE:
			case DamageType::HEALTH_DRAIN:
			case DamageType::MANA_DRAIN:
			case DamageType::PHYSICAL:
			case DamageType::UNDEFINED:
				// okay
				break;
			}

			if (_reflections.find(damageType) != _reflections.end()) {
				LOGe("Attribute <" << name << "> contains multiple entries for damage type '" << id << "'");
				continue;
			}
		}

		xmlAttrPtr chanceAttribute = xml::attribute(*child, "chance");
		if (chanceAttribute == nullptr) {
			LOGe("Damage type '" << id << "' in <" << name << "> has no attribute 'chance'");
			continue;
		}

		uint8_t chance;
		if (!xml::attribute(*chanceAttribute, chance)) {
			LOGe("'chance' of damage type '" << id << "' in <" << name << "> is not a valid integer (0..100)");
			continue;
		}

		if (chance > 100) {
			LOGw("'chance' of damage type '" << id << "' in <" << name << "> is too large: Decreased from " << chance << " to 100");
			chance = 100;
		}

		xmlAttrPtr percentageAttribute = xml::attribute(*child, "percentage");
		if (percentageAttribute == nullptr) {
			LOGe("Damage type '" << id << "' in <" << name << "> has no attribute 'percentage'");
			continue;
		}

		uint8_t percentage;
		if (!xml::attribute(*percentageAttribute, percentage)) {
			LOGe("'percentage' of damage type '" << id << "' in <" << name << "> is not a valid integer (0..100)");
			continue;
		}

		if (percentage > 100) {
			LOGw("'percentage' of damage type '" << id << "' in <" << name << "> is too large: Decreased from " << percentage << " to 100");
			percentage = 100;
		}

		if (chance == 0 || percentage == 0) {
			continue;
		}

		Reflection reflection(chance, percentage);

		if (damageType == DamageType::NONE) {
			_reflections[DamageType::DEATH] = reflection;
			_reflections[DamageType::EARTH] = reflection;
			_reflections[DamageType::ENERGY] = reflection;
			_reflections[DamageType::FIRE] = reflection;
			_reflections[DamageType::HOLY] = reflection;
			_reflections[DamageType::ICE] = reflection;
			_reflections[DamageType::HEALTH_DRAIN] = reflection;
			_reflections[DamageType::MANA_DRAIN] = reflection;
			_reflections[DamageType::PHYSICAL] = reflection;
			_reflections[DamageType::UNDEFINED] = reflection;
		}
		else {
			_reflections[damageType] = reflection;
		}
	}
}




ClassDescriptor<Kind>::DynamicAttributesP ClassDescriptor<Kind>::getDynamicAttributes() {
	using attributes::Type;

	auto attributes = DynamicAttributesP(new DynamicAttributes);
	attributes->emplace(Kind::ATTRIBUTE_AID, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_ARMOR, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_ARTICLE, Type::STRING);
	attributes->emplace(Kind::ATTRIBUTE_ATTACK, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_ATTACKSPEED, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_CHARGES, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_CORPSEOWNER, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_DATE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_DECAYING, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_DEFENSE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_DESCRIPTION, Type::STRING);
	attributes->emplace(Kind::ATTRIBUTE_DURATION, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_EXTRAATTACK, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_EXTRADEFENSE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_FLUIDTYPE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_HITCHANCE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_NAME, Type::STRING);
	attributes->emplace(Kind::ATTRIBUTE_OWNER, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_PLURALNAME, Type::STRING);
	attributes->emplace(Kind::ATTRIBUTE_SCRIPTPROTECTED, Type::BOOLEAN);
	attributes->emplace(Kind::ATTRIBUTE_SHOOTRANGE, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_TEXT, Type::STRING);
	attributes->emplace(Kind::ATTRIBUTE_UID, Type::INTEGER);
	attributes->emplace(Kind::ATTRIBUTE_WRITER, Type::STRING);

	return attributes;
}




template bool Kind::_setParameter<bool>(const std::string& name, const std::string& value, bool& target, const char* newName);
template bool Kind::_setParameter<int8_t>(const std::string& name, const std::string& value, int8_t& target, const char* newName);
template bool Kind::_setParameter<int16_t>(const std::string& name, const std::string& value, int16_t& target, const char* newName);
template bool Kind::_setParameter<int32_t>(const std::string& name, const std::string& value, int32_t& target, const char* newName);
template bool Kind::_setParameter<int64_t>(const std::string& name, const std::string& value, int64_t& target, const char* newName);
template bool Kind::_setParameter<uint8_t>(const std::string& name, const std::string& value, uint8_t& target, const char* newName);
template bool Kind::_setParameter<uint16_t>(const std::string& name, const std::string& value, uint16_t& target, const char* newName);
template bool Kind::_setParameter<uint32_t>(const std::string& name, const std::string& value, uint32_t& target, const char* newName);
template bool Kind::_setParameter<uint64_t>(const std::string& name, const std::string& value, uint64_t& target, const char* newName);
template bool Kind::_setParameter<float>(const std::string& name, const std::string& value, float& target, const char* newName);
template bool Kind::_setParameter<double>(const std::string& name, const std::string& value, double& target, const char* newName);
template bool Kind::_setParameter<std::string>(const std::string& name, const std::string& value, std::string& target, const char* newName);

template bool Kind::_setParameter<int8_t>(const std::string& name, const std::string& value, int8_t& target, int8_t min, int8_t max, const char* newName);
template bool Kind::_setParameter<int16_t>(const std::string& name, const std::string& value, int16_t& target, int16_t min, int16_t max, const char* newName);
template bool Kind::_setParameter<int32_t>(const std::string& name, const std::string& value, int32_t& target, int32_t min, int32_t max, const char* newName);
template bool Kind::_setParameter<int64_t>(const std::string& name, const std::string& value, int64_t& target, int64_t min, int64_t max, const char* newName);
template bool Kind::_setParameter<uint8_t>(const std::string& name, const std::string& value, uint8_t& target, uint8_t min, uint8_t max, const char* newName);
template bool Kind::_setParameter<uint16_t>(const std::string& name, const std::string& value, uint16_t& target, uint16_t min, uint16_t max, const char* newName);
template bool Kind::_setParameter<uint32_t>(const std::string& name, const std::string& value, uint32_t& target, uint32_t min, uint32_t max, const char* newName);
template bool Kind::_setParameter<uint64_t>(const std::string& name, const std::string& value, uint64_t& target, uint64_t min, uint64_t max, const char* newName);
template bool Kind::_setParameter<float>(const std::string& name, const std::string& value, float& target, float min, float max, const char* newName);
template bool Kind::_setParameter<double>(const std::string& name, const std::string& value, double& target, double min, double max, const char* newName);

}} // namespace ts::item
