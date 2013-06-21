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
#include "items.h"

#include "beds.h"
#include "configmanager.h"
#include "condition.h"
#include "depot.h"
#include "fileloader.h"
#include "house.h"
#include "items/Class.hpp"
#include "items/Key.hpp"
#include "mailbox.h"
#include "server.h"
#include "spells.h"
#include "teleport.h"
#include "trashholder.h"
#include "weapons.h"

using namespace items;


ItemKind::ItemKind() :
		stopTime(false),
		showCount(true),
		clientCharges(false),
		stackable(false),
		showDuration(false),
		showCharges(false),
		showAttributes(false),
		allowDistRead(false),
		canReadText(false),
		canWriteText(false),
		forceSerialize(false),
		isVertical(false),
		isHorizontal(false),
		isHangable(false),
		useable(false),
		moveable(true),
		pickupable(false),
		rotable(false),
		replaceable(true),
		lookThrough(false),
		hasHeight(false),
		blockSolid(false),
		blockPickupable(false),
		blockProjectile(false),
		blockPathFind(false),
		allowPickupable(false),
		alwaysOnTop(false),
		floorChange0(false),
		floorChange1(false),
		floorChange2(false),
		floorChange3(false),
		floorChange4(false),
		floorChange5(false),
		floorChange6(false),
		floorChange7(false),
		floorChange8(false),
		floorChange9(false),
		magicEffect(MAGIC_EFFECT_NONE),
		fluidSource(FLUID_NONE),
		weaponType(WEAPON_NONE),
		bedPartnerDir(Direction::NORTH),
		ammoAction(AMMOACTION_NONE),
		combatType(COMBAT_NONE),
		corpseType(RACE_NONE),
		shootType(SHOOT_EFFECT_SPEAR),
		ammoType(AMMO_NONE),
		group(ITEM_GROUP_NONE),
		slotPosition(SLOTP_HAND | SLOTP_AMMO),
		wieldPosition(slots_t::HAND),
		type(ItemType::GENERIC),
		charges(0),
		transformUseTo {0, 0},
		transformToFree(0),
		transformEquipTo(0),
		transformDeEquipTo(0),
		id(0),
		clientId(0),
		maxItems(8), // maximum size if this is a container
		speed(0),
		maxTextLen(0),
		writeOnceItemId(0),
		attack(0),
		extraAttack(0),
		defense(0),
		extraDefense(0),
		armor(0),
		breakChance(-1),
		hitChance(-1),
		maxHitChance(-1),
		runeLevel(0),
		runeMagLevel(0),
		lightLevel(0),
		lightColor(0),
		decayTo(-1),
		rotateTo(0),
		alwaysOnTopOrder(0),
		shootRange(1),
		decayTime(0),
		attackSpeed(0),
		wieldInfo(0),
		minReqLevel(0),
		minReqMagicLevel(0),
		worth(0),
		levelDoor(0),
		condition(nullptr),
		weight(0) // weight of the item, e.g. throwing distance depends on it
{}


ItemKind::~ItemKind() {
	delete condition;
}



LOGGER_DEFINITION(Items);


Items::Items()
	: _defaultRandomizationChance(50),
	  _versionBuild(0),
	  _versionMajor(0),
	  _versionMinor(0)
{}


void Items::addKind(ItemKindP kind, const std::string& fileName) {
	if (_kinds.size() > kind->id && _kinds[kind->id]) {
		LOGw("Item " << kind->id << " is defined multiple times in " << fileName << ".");
		return;
	}

	if (kind->clientId > 0) {
		if (_kindIdsByClientId.find(kind->clientId) != _kindIdsByClientId.end()) {
//			LOGw("Multiple items have the same client ID " << kind->clientId << " in " << fileName << ".");
		}

		_kindIdsByClientId[kind->clientId] = kind->id;
	}

	if (_kinds.size() <= kind->id) {
		if (_kinds.capacity() <= kind->id) {
			_kinds.reserve(kind->id + 5000);
		}

		_kinds.resize(kind->id + 1);
	}
	_kinds[kind->id] = std::move(kind);
}


void Items::addRandomization(uint16_t kindId, int32_t fromId, int32_t toId, int32_t chance, const std::string& fileName) {
	if (_randomizations.find(kindId) != _randomizations.end()) {
		LOGw("Randomization for item " << kindId << " is defined multiple times in " << fileName << ".");
	}

	RandomizationBlock& block = _randomizations[kindId];
	block.chance = chance;
	block.fromRange = fromId;
	block.toRange = toId;
}


ItemKindVector::const_iterator Items::begin() const {
	return _kinds.begin();
}


void Items::clear() {
	_classes.clear();
	_kindIdsByClientId.clear();
	_kinds.clear();
	_randomizations.clear();
	_worth.clear();

	_defaultRandomizationChance = 50;
	_versionBuild = 0;
	_versionMajor = 0;
	_versionMinor = 0;
}


ItemKindVector::const_iterator Items::end() const {
	return _kinds.end();
}


ItemKindPC Items::getKind(uint16_t kindId) const {
	return getMutableKind(kindId);
}


ItemKindPC Items::getKindByClientId(int32_t clientId) const {
	KindIdByClientIdMap::const_iterator it = _kindIdsByClientId.find(clientId);
	if (it == _kindIdsByClientId.end()) {
		return nullptr;
	}

	return getKind(it->second);
}


ItemKindPC Items::getKindByName(const std::string& name) const {
	if (name.empty()) {
		return nullptr;
	}

	for (ItemKindVector::const_iterator it = _kinds.cbegin(); it != _kinds.cend(); ++it) {
		const ItemKindPC& type = *it;
		if (!type) {
			continue;
		}

		if (boost::iequals(type->name, name)) {
			return type;
		}
	}

	return nullptr;
}


uint16_t Items::getKindIdByName(const std::string& name) const {
	ItemKindPC kind = getKindByName(name);
	if (!kind) {
		return 0;
	}

	return kind->id;
}


ItemKindP Items::getMutableKind(uint16_t kindId) const {
	if (kindId == 0 || kindId >= _kinds.size()) {
		return nullptr;
	}

	return _kinds[kindId];
}


uint16_t Items::getRandomizedKindId(uint16_t kindId) const {
	if (!server.configManager().getBool(ConfigManager::RANDOMIZE_TILES)) {
		return kindId;
	}

	RandomizationMap::const_iterator it = _randomizations.find(kindId);
	if (it == _randomizations.end()) {
		return kindId;
	}

	const RandomizationBlock& block = it->second;
	if (block.chance > 0 && random_range(0, 100) <= block.chance) {
		kindId = random_range(block.fromRange, block.toRange);
	}

	return kindId;
}


uint32_t Items::getVersionBuild() const {
	return _versionBuild;
}


uint32_t Items::getVersionMajor() const {
	return _versionMajor;
}


uint32_t Items::getVersionMinor() const {
	return _versionMinor;
}


const Items::WorthMap& Items::getWorth() const {
	return _worth;
}


void Items::loadKindFromXmlNode(xmlNodePtr root, uint16_t kindId, const std::string& filePath) {
	// TODO refactor

	int32_t intValue;
	std::string strValue;

	if (kindId > 20000 && kindId < 20100) {
		// TODO check this
		kindId -= 20000;
	}

	ItemKindP kind = getMutableKind(kindId);
	if (!kind) {
		LOGe("<item> node for non-existent item " << kindId << " in " << filePath << ".");
		return;
	}

//	if(!type->name.empty() && (!readXMLString(node, "override", strValue) || !booleanString(strValue)))
//		LOGw("[Items::loadFromXml] Duplicate registered item with kindId " << kindId);

	if(readXMLString(root, "name", strValue))
		kind->name = strValue;

	if(readXMLString(root, "article", strValue))
		kind->article = strValue;

	if(readXMLString(root, "plural", strValue))
		kind->pluralName = strValue;

	for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if(readXMLString(node, "key", strValue))
		{
			std::string tmpStrValue = asLowerCaseString(strValue);
			if(tmpStrValue == "type")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "container")
					{
						kind->type = ItemType::CONTAINER;
						kind->group = ITEM_GROUP_CONTAINER;
					}
					else if(tmpStrValue == "generic")
						kind->type = ItemType::GENERIC;
					else if(tmpStrValue == "key")
						kind->type = ItemType::KEY;
					else if(tmpStrValue == "magicfield")
						kind->type = ItemType::MAGICFIELD;
					else if(tmpStrValue == "depot")
						kind->type = ItemType::DEPOT;
					else if(tmpStrValue == "mailbox")
						kind->type = ItemType::MAILBOX;
					else if(tmpStrValue == "trashholder")
						kind->type = ItemType::TRASHHOLDER;
					else if(tmpStrValue == "teleport")
						kind->type = ItemType::TELEPORT;
					else if(tmpStrValue == "door")
						kind->type = ItemType::DOOR;
					else if(tmpStrValue == "bed")
						kind->type = ItemType::BED;
					else {
						LOGe("<item> <kind> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
						continue;
					}

					auto clazzIt = _classes.find(kind->type);
					if (clazzIt == _classes.cend()) {
						LOGe("Cannot update item kind of unsupported type " << static_cast<uint32_t>(kind->type));
						continue;
					}

					kind->_class = clazzIt->second;
				}
			}
			else if(tmpStrValue == "name")
			{
				if(readXMLString(node, "value", strValue))
					kind->name = strValue;
			}
			else if(tmpStrValue == "article")
			{
				if(readXMLString(node, "value", strValue))
					kind->article = strValue;
			}
			else if(tmpStrValue == "plural")
			{
				if(readXMLString(node, "value", strValue))
					kind->pluralName = strValue;
			}
			else if(tmpStrValue == "description")
			{
				if(readXMLString(node, "value", strValue))
					kind->description = strValue;
			}
			else if(tmpStrValue == "runespellname")
			{
				if(readXMLString(node, "value", strValue))
					kind->runeSpellName = strValue;
			}
			else if(tmpStrValue == "weight")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->weight = intValue / 100.f;
			}
			else if(tmpStrValue == "showcount")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->showCount = (intValue != 0);
			}
			else if(tmpStrValue == "armor")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->armor = intValue;
			}
			else if(tmpStrValue == "defense")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->defense = intValue;
			}
			else if(tmpStrValue == "extradefense" || tmpStrValue == "extradef")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->extraDefense = intValue;
			}
			else if(tmpStrValue == "attack")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->attack = intValue;
			}
			else if(tmpStrValue == "extraattack" || tmpStrValue == "extraatk")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->extraAttack = intValue;
			}
			else if(tmpStrValue == "attackspeed")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->attackSpeed = intValue;
			}
			else if(tmpStrValue == "rotateto")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->rotateTo = intValue;
			}
			else if(tmpStrValue == "moveable" || tmpStrValue == "movable")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->moveable = (intValue != 0);
			}
			else if(tmpStrValue == "blockprojectile")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->blockProjectile = (intValue != 0);
			}
			else if(tmpStrValue == "allowpickupable")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->allowPickupable = (intValue != 0);
			}
			else if(tmpStrValue == "floorchange")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "down")
						kind->setFloorChange(CHANGE_DOWN, true);
					else if(tmpStrValue == "north")
						kind->setFloorChange(CHANGE_NORTH, true);
					else if(tmpStrValue == "south")
						kind->setFloorChange(CHANGE_SOUTH, true);
					else if(tmpStrValue == "west")
						kind->setFloorChange(CHANGE_WEST, true);
					else if(tmpStrValue == "east")
						kind->setFloorChange(CHANGE_EAST, true);
					else if(tmpStrValue == "northex")
						kind->setFloorChange(CHANGE_NORTH_EX, true);
					else if(tmpStrValue == "southex")
						kind->setFloorChange(CHANGE_SOUTH_EX, true);
					else if(tmpStrValue == "westex")
						kind->setFloorChange(CHANGE_WEST_EX, true);
					else if(tmpStrValue == "eastex")
						kind->setFloorChange(CHANGE_EAST_EX, true);
				}
			}
			else if(tmpStrValue == "corpsetype")
			{
				tmpStrValue = asLowerCaseString(strValue);
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "venom")
						kind->corpseType = RACE_VENOM;
					else if(tmpStrValue == "blood")
						kind->corpseType = RACE_BLOOD;
					else if(tmpStrValue == "undead")
						kind->corpseType = RACE_UNDEAD;
					else if(tmpStrValue == "fire")
						kind->corpseType = RACE_FIRE;
					else if(tmpStrValue == "energy")
						kind->corpseType = RACE_ENERGY;
					else
						LOGw("<item> <corpsetype> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "containersize")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->maxItems = intValue;
			}
			else if(tmpStrValue == "fluidsource")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					FluidTypes_t fluid = getFluidType(tmpStrValue);
					if(fluid != FLUID_NONE)
						kind->fluidSource = fluid;
					else
						LOGw("<item> <fluidsource> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "writeable" || tmpStrValue == "writable")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->canWriteText = (intValue != 0);
					kind->canReadText = (intValue != 0);
				}
			}
			else if(tmpStrValue == "readable")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->canReadText = (intValue != 0);
			}
			else if(tmpStrValue == "maxtextlen" || tmpStrValue == "maxtextlength")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->maxTextLen = intValue;
			}
			else if(tmpStrValue == "writeonceitemid")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->writeOnceItemId = intValue;
			}
			else if(tmpStrValue == "worth")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					if(_worth.find(intValue) != _worth.end() && (!readXMLString(root, "override", strValue) || !booleanString(strValue)))
						LOGw("Multiple items are worth " << intValue << " in " << filePath << ".");
					else
					{
						_worth[intValue] = kindId;
						kind->worth = intValue;
					}
				}
			}
			else if(tmpStrValue == "forceserialize" || tmpStrValue == "forceserialization" || tmpStrValue == "forcesave")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->forceSerialize = (intValue != 0);
			}
			else if(tmpStrValue == "leveldoor")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->levelDoor = intValue;
			}
			else if(tmpStrValue == "weapontype")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "sword")
						kind->weaponType = WEAPON_SWORD;
					else if(tmpStrValue == "club")
						kind->weaponType = WEAPON_CLUB;
					else if(tmpStrValue == "axe")
						kind->weaponType = WEAPON_AXE;
					else if(tmpStrValue == "shield")
						kind->weaponType = WEAPON_SHIELD;
					else if(tmpStrValue == "distance")
						kind->weaponType = WEAPON_DIST;
					else if(tmpStrValue == "wand" || tmpStrValue == "rod")
						kind->weaponType = WEAPON_WAND;
					else if(tmpStrValue == "ammunition")
						kind->weaponType = WEAPON_AMMO;
					else if(tmpStrValue == "fist")
						kind->weaponType = WEAPON_FIST;
					else
						LOGw("<item> <weapontype> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "slottype")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "head")
					{
						kind->slotPosition |= SLOTP_HEAD;
						kind->wieldPosition = slots_t::HEAD;
					}
					else if(tmpStrValue == "body")
					{
						kind->slotPosition |= SLOTP_ARMOR;
						kind->wieldPosition = slots_t::ARMOR;
					}
					else if(tmpStrValue == "legs")
					{
						kind->slotPosition |= SLOTP_LEGS;
						kind->wieldPosition = slots_t::LEGS;
					}
					else if(tmpStrValue == "feet")
					{
						kind->slotPosition |= SLOTP_FEET;
						kind->wieldPosition = slots_t::FEET;
					}
					else if(tmpStrValue == "backpack")
					{
						kind->slotPosition |= SLOTP_BACKPACK;
						kind->wieldPosition = slots_t::BACKPACK;
					}
					else if(tmpStrValue == "two-handed")
					{
						kind->slotPosition |= SLOTP_TWO_HAND;
						kind->wieldPosition = slots_t::TWO_HAND;
					}
					else if(tmpStrValue == "necklace")
					{
						kind->slotPosition |= SLOTP_NECKLACE;
						kind->wieldPosition = slots_t::NECKLACE;
					}
					else if(tmpStrValue == "ring")
					{
						kind->slotPosition |= SLOTP_RING;
						kind->wieldPosition = slots_t::RING;
					}
					else if(tmpStrValue == "ammo")
						kind->wieldPosition = slots_t::AMMO;
					else if(tmpStrValue == "hand")
						kind->wieldPosition = slots_t::HAND;
					else
						LOGw("<item> <slottype> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "ammotype")
			{
				if(readXMLString(node, "value", strValue))
				{
					kind->ammoType = getAmmoType(strValue);
					if(kind->ammoType == AMMO_NONE)
						LOGw("<item> <ammotype> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "shoottype")
			{
				if(readXMLString(node, "value", strValue))
				{
					ShootEffect_t shoot = getShootType(strValue);
					if(shoot != SHOOT_EFFECT_UNKNOWN)
						kind->shootType = shoot;
					else
						LOGw("<item> <shoottype> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "effect")
			{
				if(readXMLString(node, "value", strValue))
				{
					MagicEffect_t effect = getMagicEffect(strValue);
					if(effect != MAGIC_EFFECT_UNKNOWN)
						kind->magicEffect = effect;
					else
						LOGw("<item> <effect> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "range")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->shootRange = intValue;
			}
			else if(tmpStrValue == "stopduration")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->stopTime = (intValue != 0);
			}
			else if(tmpStrValue == "decayto")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->decayTo = intValue;
			}
			else if(tmpStrValue == "transformequipto")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->transformEquipTo = intValue;
			}
			else if(tmpStrValue == "transformdeequipto")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->transformDeEquipTo = intValue;
			}
			else if(tmpStrValue == "duration")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->decayTime = std::max((int32_t)0, intValue);
			}
			else if(tmpStrValue == "showduration")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->showDuration = (intValue != 0);
			}
			else if (tmpStrValue == "charges") {
				if (readXMLInteger(node, "value", intValue)) {
					if (intValue < 0) {
						LOGw("<item> cannot have " << intValue << " charges. Must be between 0 and " << std::numeric_limits<uint16_t>::max() << ".");
						intValue = 0;
					}
					else if (intValue > std::numeric_limits<uint16_t>::max()) {
						LOGw("<item> cannot have " << intValue << " charges. Must be between 0 and " << std::numeric_limits<uint16_t>::max() << ".");
						intValue = std::numeric_limits<uint16_t>::max();
					}

					kind->charges = intValue;
				}
			}
			else if(tmpStrValue == "showcharges")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->showCharges = (intValue != 0);
			}
			else if(tmpStrValue == "showattributes")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->showAttributes = (intValue != 0);
			}
			else if(tmpStrValue == "breakchance")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->breakChance = std::max(0, std::min(100, intValue));
			}
			else if(tmpStrValue == "ammoaction")
			{
				if(readXMLString(node, "value", strValue))
				{
					AmmoAction_t ammo = getAmmoAction(strValue);
					if(ammo != AMMOACTION_NONE)
						kind->ammoAction = ammo;
					else
						LOGw("<item> <ammoaction> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
				}
			}
			else if(tmpStrValue == "hitchance")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->hitChance = std::max(0, std::min(100, intValue));
			}
			else if(tmpStrValue == "maxhitchance")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->maxHitChance = std::max(0, std::min(100, intValue));
			}
			else if(tmpStrValue == "preventloss")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.preventLoss = (intValue != 0);
			}
			else if(tmpStrValue == "preventdrop")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.preventDrop = (intValue != 0);
			}
			else if(tmpStrValue == "invisible")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.invisible = (intValue != 0);
			}
			else if(tmpStrValue == "speed")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.speed = intValue;
			}
			else if(tmpStrValue == "healthgain")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.regeneration = true;
					kind->abilities.healthGain = intValue;
				}
			}
			else if(tmpStrValue == "healthticks")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.regeneration = true;
					kind->abilities.healthTicks = intValue;
				}
			}
			else if(tmpStrValue == "managain")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.regeneration = true;
					kind->abilities.manaGain = intValue;
				}
			}
			else if(tmpStrValue == "manaticks")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.regeneration = true;
					kind->abilities.manaTicks = intValue;
				}
			}
			else if(tmpStrValue == "manashield")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.manaShield = (intValue != 0);
			}
			else if(tmpStrValue == "skillsword")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_SWORD] = intValue;
			}
			else if(tmpStrValue == "skillaxe")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_AXE] = intValue;
			}
			else if(tmpStrValue == "skillclub")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_CLUB] = intValue;
			}
			else if(tmpStrValue == "skilldist")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_DIST] = intValue;
			}
			else if(tmpStrValue == "skillfish")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_FISH] = intValue;
			}
			else if(tmpStrValue == "skillshield")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_SHIELD] = intValue;
			}
			else if(tmpStrValue == "skillfist")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.skills[SKILL_FIST] = intValue;
			}
			else if(tmpStrValue == "maxhealthpoints" || tmpStrValue == "maxhitpoints")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.stats[STAT_MAXHEALTH] = intValue;
			}
			else if(tmpStrValue == "maxhealthpercent" || tmpStrValue == "maxhitpointspercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.statsPercent[STAT_MAXHEALTH] = intValue;
			}
			else if(tmpStrValue == "maxmanapoints")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.stats[STAT_MAXMANA] = intValue;
			}
			else if(tmpStrValue == "maxmanapercent" || tmpStrValue == "maxmanapointspercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.statsPercent[STAT_MAXMANA] = intValue;
			}
			else if(tmpStrValue == "soulpoints")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.stats[STAT_SOUL] = intValue;
			}
			else if(tmpStrValue == "soulpercent" || tmpStrValue == "soulpointspercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.statsPercent[STAT_SOUL] = intValue;
			}
			else if(tmpStrValue == "magiclevelpoints" || tmpStrValue == "magicpoints")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.stats[STAT_MAGICLEVEL] = intValue;
			}
			else if(tmpStrValue == "magiclevelpercent" || tmpStrValue == "magicpointspercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.statsPercent[STAT_MAGICLEVEL] = intValue;
			}
			else if(tmpStrValue == "increasemagicvalue")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.increment[MAGIC_VALUE] = intValue;
			}
			else if(tmpStrValue == "increasemagicpercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.increment[MAGIC_PERCENT] = intValue;
			}
			else if(tmpStrValue == "increasehealingvalue")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.increment[HEALING_VALUE] = intValue;
			}
			else if(tmpStrValue == "increasehealingpercent")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.increment[HEALING_PERCENT] = intValue;
			}
			else if(tmpStrValue == "absorbpercentall")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					for(uint32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
						kind->abilities.setAbsorb((CombatType_t)i, intValue);
				}
			}
			else if(tmpStrValue == "absorbpercentelements")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setAbsorb(COMBAT_ENERGYDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_FIREDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_EARTHDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_ICEDAMAGE, intValue);
				}
			}
			else if(tmpStrValue == "absorbpercentmagic")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setAbsorb(COMBAT_ENERGYDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_FIREDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_EARTHDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_ICEDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_HOLYDAMAGE, intValue);
					kind->abilities.setAbsorb(COMBAT_DEATHDAMAGE, intValue);
				}
			}
			else if(tmpStrValue == "absorbpercentenergy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_ENERGYDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentfire")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_FIREDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentpoison" ||	tmpStrValue == "absorbpercentearth")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_EARTHDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentice")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_ICEDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentholy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_HOLYDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentdeath")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_DEATHDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentlifedrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_LIFEDRAIN, intValue);
			}
			else if(tmpStrValue == "absorbpercentmanadrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_MANADRAIN, intValue);
			}
			else if(tmpStrValue == "absorbpercentdrown")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_DROWNDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercentphysical")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_PHYSICALDAMAGE, intValue);
			}
			else if(tmpStrValue == "absorbpercenthealing")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_HEALING, intValue);
			}
			else if(tmpStrValue == "absorbpercentundefined")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setAbsorb(COMBAT_UNDEFINEDDAMAGE, intValue);
			}
			else if(tmpStrValue == "reflectpercentall")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
						kind->abilities.setReflect((CombatType_t)i, REFLECT_PERCENT, intValue);
				}
			}
			else if(tmpStrValue == "reflectpercentelements")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_PERCENT, intValue);
				}
			}
			else if(tmpStrValue == "reflectpercentmagic")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_HOLYDAMAGE, REFLECT_PERCENT, intValue);
					kind->abilities.setReflect(COMBAT_DEATHDAMAGE, REFLECT_PERCENT, intValue);
				}
			}
			else if(tmpStrValue == "reflectpercentenergy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentfire")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentpoison" ||	tmpStrValue == "reflectpercentearth")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentice")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentholy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_HOLYDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentdeath")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_DEATHDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentlifedrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_LIFEDRAIN, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentmanadrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_MANADRAIN, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentdrown")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_DROWNDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentphysical")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_PHYSICALDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercenthealing")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_HEALING, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectpercentundefined")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_UNDEFINEDDAMAGE, REFLECT_PERCENT, intValue);
			}
			else if(tmpStrValue == "reflectchanceall")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
						kind->abilities.setReflect((CombatType_t)i, REFLECT_CHANCE, intValue);
				}
			}
			else if(tmpStrValue == "reflectchanceelements")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_CHANCE, intValue);
				}
			}
			else if(tmpStrValue == "reflectchancemagic")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_HOLYDAMAGE, REFLECT_CHANCE, intValue);
					kind->abilities.setReflect(COMBAT_DEATHDAMAGE, REFLECT_CHANCE, intValue);
				}
			}
			else if(tmpStrValue == "reflectchanceenergy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_ENERGYDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancefire")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_FIREDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancepoison" ||	tmpStrValue == "reflectchanceearth")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_EARTHDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchanceice")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_ICEDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchanceholy")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_HOLYDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancedeath")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_DEATHDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancelifedrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_LIFEDRAIN, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancemanadrain")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_MANADRAIN, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancedrown")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_DROWNDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancephysical")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_PHYSICALDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchancehealing")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_HEALING, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "reflectchanceundefined")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->abilities.setReflect(COMBAT_UNDEFINEDDAMAGE, REFLECT_CHANCE, intValue);
			}
			else if(tmpStrValue == "suppressshock" || tmpStrValue == "suppressenergy")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_ENERGY;
			}
			else if(tmpStrValue == "suppressburn" || tmpStrValue == "suppressfire")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_FIRE;
			}
			else if(tmpStrValue == "suppresspoison" || tmpStrValue == "suppressearth")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_POISON;
			}
			else if(tmpStrValue == "suppressfreeze" || tmpStrValue == "suppressice")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_FREEZING;
			}
			else if(tmpStrValue == "suppressdazzle" || tmpStrValue == "suppressholy")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_DAZZLED;
			}
			else if(tmpStrValue == "suppresscurse" || tmpStrValue == "suppressdeath")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_CURSED;
			}
			else if(tmpStrValue == "suppressdrown")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_DROWN;
			}
			else if(tmpStrValue == "suppressphysical")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_PHYSICAL;
			}
			else if(tmpStrValue == "suppresshaste")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_HASTE;
			}
			else if(tmpStrValue == "suppressparalyze")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_PARALYZE;
			}
			else if(tmpStrValue == "suppressdrunk")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_DRUNK;
			}
			else if(tmpStrValue == "suppressregeneration")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_REGENERATION;
			}
			else if(tmpStrValue == "suppresssoul")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_SOUL;
			}
			else if(tmpStrValue == "suppressoutfit")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_OUTFIT;
			}
			else if(tmpStrValue == "suppressinvisible")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_INVISIBLE;
			}
			else if(tmpStrValue == "suppressinfight")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_INFIGHT;
			}
			else if(tmpStrValue == "suppressexhaust")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_EXHAUST;
			}
			else if(tmpStrValue == "suppressmuted")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_MUTED;
			}
			else if(tmpStrValue == "suppresspacified")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_PACIFIED;
			}
			else if(tmpStrValue == "suppresslight")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_LIGHT;
			}
			else if(tmpStrValue == "suppressattributes")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_ATTRIBUTES;
			}
			else if(tmpStrValue == "suppressmanashield")
			{
				if(readXMLInteger(node, "value", intValue) && intValue != 0)
					kind->abilities.conditionSuppressions |= CONDITION_MANASHIELD;
			}
			else if(tmpStrValue == "field")
			{
				kind->group = ITEM_GROUP_MAGICFIELD;
				kind->type = ItemType::MAGICFIELD;
				CombatType_t combatType = COMBAT_NONE;
				ConditionDamage* conditionDamage = nullptr;

				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "fire")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_FIRE, false, 0);
						combatType = COMBAT_FIREDAMAGE;
					}
					else if(tmpStrValue == "energy")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_ENERGY, false, 0);
						combatType = COMBAT_ENERGYDAMAGE;
					}
					else if(tmpStrValue == "earth" || tmpStrValue == "poison")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_POISON, false, 0);
						combatType = COMBAT_EARTHDAMAGE;
					}
					else if(tmpStrValue == "ice" || tmpStrValue == "freezing")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_FREEZING, false, 0);
						combatType = COMBAT_ICEDAMAGE;
					}
					else if(tmpStrValue == "holy" || tmpStrValue == "dazzled")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_DAZZLED, false, 0);
						combatType = COMBAT_HOLYDAMAGE;
					}
					else if(tmpStrValue == "death" || tmpStrValue == "cursed")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_CURSED, false, 0);
						combatType = COMBAT_DEATHDAMAGE;
					}
					else if(tmpStrValue == "drown")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_DROWN, false, 0);
						combatType = COMBAT_DROWNDAMAGE;
					}
					else if(tmpStrValue == "physical")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_PHYSICAL, false, 0);
						combatType = COMBAT_PHYSICALDAMAGE;
					}
					else
						LOGw("<item> <field> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");

					if(combatType != COMBAT_NONE)
					{
						kind->combatType = combatType;
						kind->condition = conditionDamage;
						uint32_t ticks = 0;
						int32_t damage = 0, start = 0, count = 1;

						xmlNodePtr fieldAttributesNode = node->children;
						while(fieldAttributesNode)
						{
							if(readXMLString(fieldAttributesNode, "key", strValue))
							{
								tmpStrValue = asLowerCaseString(strValue);
								if(tmpStrValue == "ticks")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										ticks = std::max(0, intValue);
								}

								if(tmpStrValue == "count")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										count = std::max(1, intValue);
								}

								if(tmpStrValue == "start")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										start = std::max(0, intValue);
								}

								if(tmpStrValue == "damage")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
									{
										damage = -intValue;
										if(start > 0)
										{
											std::list<int32_t> damageList;
											ConditionDamage::generateDamageList(damage, start, damageList);

											for(std::list<int32_t>::iterator it = damageList.begin(); it != damageList.end(); ++it)
												conditionDamage->addDamage(1, ticks, -*it);

											start = 0;
										}
										else
											conditionDamage->addDamage(count, ticks, damage);
									}
								}
							}

							fieldAttributesNode = fieldAttributesNode->next;
						}

						if(conditionDamage->getTotalDamage() > 0)
							kind->condition->setParam(CONDITIONPARAM_FORCEUPDATE, true);
					}
				}
			}
			else if(tmpStrValue == "elementphysical")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_PHYSICALDAMAGE;
				}
			}
			else if(tmpStrValue == "elementfire")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_FIREDAMAGE;
				}
			}
			else if(tmpStrValue == "elementenergy")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_ENERGYDAMAGE;
				}
			}
			else if(tmpStrValue == "elementearth")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_EARTHDAMAGE;
				}
			}
			else if(tmpStrValue == "elementice")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_ICEDAMAGE;
				}
			}
			else if(tmpStrValue == "elementholy")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_HOLYDAMAGE;
				}
			}
			else if(tmpStrValue == "elementdeath")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_DEATHDAMAGE;
				}
			}
			else if(tmpStrValue == "elementlifedrain")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_LIFEDRAIN;
				}
			}
			else if(tmpStrValue == "elementmanadrain")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_MANADRAIN;
				}
			}
			else if(tmpStrValue == "elementhealing")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_HEALING;
				}
			}
			else if(tmpStrValue == "elementundefined")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->abilities.elementDamage = intValue;
					kind->abilities.elementType = COMBAT_UNDEFINEDDAMAGE;
				}
			}
			else if(tmpStrValue == "replaceable" || tmpStrValue == "replacable")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->replaceable = (intValue != 0);
			}
			else if(tmpStrValue == "partnerdirection")
			{
				if(readXMLString(node, "value", strValue))
					kind->bedPartnerDir = getDirection(strValue);
			}
			else if(tmpStrValue == "maletransformto")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->transformUseTo[PLAYERSEX_MALE] = intValue;

					ItemKindP targetKind = getMutableKind(intValue);
					if (targetKind) {
						if(!targetKind->transformToFree)
							targetKind->transformToFree = kind->id;

						if(!kind->transformUseTo[PLAYERSEX_FEMALE])
							kind->transformUseTo[PLAYERSEX_FEMALE] = intValue;
					}
					else {
						LOGw("<item> <maletransformto> node refers to unknown item ID " << intValue << " for item " << kind->id << " in " << filePath << ".");
					}
				}
			}
			else if(tmpStrValue == "femaletransformto")
			{
				if(readXMLInteger(node, "value", intValue))
				{
					kind->transformUseTo[PLAYERSEX_FEMALE] = intValue;

					ItemKindP targetKind = getMutableKind(intValue);
					if (targetKind) {
						if(!targetKind->transformToFree)
							targetKind->transformToFree = kind->id;

						if(!kind->transformUseTo[PLAYERSEX_MALE])
							kind->transformUseTo[PLAYERSEX_MALE] = intValue;
					}
					else {
						LOGw("<item> <femaletransformto> node refers to unknown item ID " << intValue << " for item " << kind->id << " in " << filePath << ".");
					}
				}
			}
			else if(tmpStrValue == "transformto")
			{
				if(readXMLInteger(node, "value", intValue))
					kind->transformToFree = intValue;
			}
			else
				LOGw("<item> node contains unknown node <" << strValue << "> for item " << kind->id << " in " << filePath << ".");
		}
	}

	if(kind->pluralName.empty() && !kind->name.empty())
	{
		kind->pluralName = (kind->showCount ? (kind->name + "s") : kind->name);
	}
}


bool Items::loadKindsFromOtb() {
	std::string filePath = getFilePath(FileType::OTHER, "items/items.otb");

	FileLoader loader;
	if (!loader.openFile(filePath.c_str(), false, true)) {
		return loader.getError();
	}

	PropStream properties;

	uint32_t nodeType;
	const NodeStruct* node = loader.getChildNode(NO_NODE, nodeType);

	if (loader.getProps(node, properties)) {
		uint32_t flags;
		if (!properties.GET_ULONG(flags)) {
			LOGe("Cannot load flags from " << filePath << ".");
			return false;
		}

		attribute_t attribute;
		if (!properties.GET_VALUE(attribute)) {
			LOGe("Cannot load attribute from " << filePath << ".");
			return false;
		}

		if (attribute == ROOT_ATTR_VERSION) {
			datasize_t length = 0;
			if (!properties.GET_VALUE(length)) {
				LOGe("Cannot load version attribute length from " << filePath << ".");
				return false;
			}
			if (length != sizeof(VERSIONINFO)) {
				LOGe("Invalid version attribute in " << filePath << ".");
				return false;
			}

			VERSIONINFO* version;
			if (!properties.GET_STRUCT(version)) {
				LOGe("Cannot load version attribute from " << filePath << ".");
				return false;
			}

			_versionBuild = version->dwBuildNumber;
			_versionMajor = version->dwMajorVersion; // otb format file version
			_versionMinor = version->dwMinorVersion; // client version
		}
	}

	if (_versionMajor == 0xFFFFFFFF) {
		LOGw("Generic file version used by " << filePath << ".");
	}
	else if (_versionMajor < 3) {
		LOGe("File format used by " << filePath << " is too old.");
		return false;
	}
	else if (_versionMajor > 3) {
		LOGe("File format used by " << filePath << " is too old.");
		return false;
	}

	_kinds.reserve(std::numeric_limits<uint16_t>::max());

	for (node = loader.getChildNode(node, nodeType); node != NO_NODE; node = loader.getNextNode(node, nodeType)) {
		if (!loader.getProps(node, properties)) {
			LOGe("Cannot load item node properties from " << filePath << ".");
			return false;
		}

		ItemKindP kind = std::make_shared<ItemKind>();
		kind->group = static_cast<itemgroup_t>(nodeType);

		flags_t flags;
		switch (nodeType) {
			case ITEM_GROUP_CONTAINER:
				kind->type = ItemType::CONTAINER;
				break;

			case ITEM_GROUP_DOOR: // unused
				kind->type = ItemType::DOOR;
				break;

			case ITEM_GROUP_MAGICFIELD: // unused
				kind->type = ItemType::MAGICFIELD;
				break;

			case ITEM_GROUP_TELEPORT: // unused
				kind->type = ItemType::TELEPORT;
				break;

			case ITEM_GROUP_NONE:
			case ITEM_GROUP_GROUND:
			case ITEM_GROUP_SPLASH:
			case ITEM_GROUP_FLUID:
			case ITEM_GROUP_CHARGES:
			case ITEM_GROUP_DEPRECATED:
				break;

			default:
				LOGe("Item node has unknown type " << nodeType << " in " << filePath << ".");
				return false;
		}

		if (!properties.GET_VALUE(flags)) {
			LOGe("Cannot load item flags from " << filePath << ".");
			return false;
		}

		kind->blockSolid = hasBitSet(FLAG_BLOCK_SOLID, flags);
		kind->blockProjectile = hasBitSet(FLAG_BLOCK_PROJECTILE, flags);
		kind->blockPathFind = hasBitSet(FLAG_BLOCK_PATHFIND, flags);
		kind->hasHeight = hasBitSet(FLAG_HAS_HEIGHT, flags);
		kind->useable = hasBitSet(FLAG_USEABLE, flags);
		kind->pickupable = hasBitSet(FLAG_PICKUPABLE, flags);
		kind->moveable = hasBitSet(FLAG_MOVEABLE, flags);
		kind->stackable = hasBitSet(FLAG_STACKABLE, flags);

		kind->alwaysOnTop = hasBitSet(FLAG_ALWAYSONTOP, flags);
		kind->isVertical = hasBitSet(FLAG_VERTICAL, flags);
		kind->isHorizontal = hasBitSet(FLAG_HORIZONTAL, flags);
		kind->isHangable = hasBitSet(FLAG_HANGABLE, flags);
		kind->allowDistRead = hasBitSet(FLAG_ALLOWDISTREAD, flags);
		kind->rotable = hasBitSet(FLAG_ROTABLE, flags);
		kind->canReadText = hasBitSet(FLAG_READABLE, flags);
		kind->clientCharges = hasBitSet(FLAG_CLIENTCHARGES, flags);
		kind->lookThrough = hasBitSet(FLAG_LOOKTHROUGH, flags);

		attribute_t attribute;
		while (properties.GET_VALUE(attribute)) {
			datasize_t length = 0;
			if (!properties.GET_VALUE(length)) {
				LOGe("Cannot load item attribute length from " << filePath << ".");
				return false;
			}

			switch (attribute) {
				case ITEM_ATTR_SERVERID: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item server ID attribute length in " << filePath << ".");
						return false;
					}

					uint16_t serverId;
					if (!properties.GET_USHORT(serverId)) {
						LOGe("Cannot load item server ID from " << filePath << ".");
						return false;
					}

					if (serverId > 20000 && serverId < 20100) {
						// TODO check this
						serverId -= 20000;
					}

					kind->id = serverId;
					break;
				}

				case ITEM_ATTR_CLIENTID: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item client ID attribute length in " << filePath << ".");
						return false;
					}

					uint16_t clientId;
					if (!properties.GET_USHORT(clientId)) {
						LOGe("Cannot load item client ID from " << filePath << ".");
						return false;
					}

					kind->clientId = clientId;
					break;
				}

				case ITEM_ATTR_SPEED: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item speed attribute length in " << filePath << ".");
						return false;
					}

					uint16_t speed;
					if (!properties.GET_USHORT(speed)) {
						LOGe("Cannot load item speed from " << filePath << ".");
						return false;
					}

					kind->speed = speed;
					break;
				}

				case ITEM_ATTR_LIGHT2: {
					if (length != sizeof(lightBlock2)) {
						LOGe("Invalid item light attribute length in " << filePath << ".");
						return false;
					}

					lightBlock2* block;
					if (!properties.GET_STRUCT(block)) {
						LOGe("Cannot load item light from " << filePath << ".");
						return false;
					}

					kind->lightLevel = block->lightLevel;
					kind->lightColor = block->lightColor;
					break;
				}

				case ITEM_ATTR_TOPORDER: {
					if (length != sizeof(uint8_t)) {
						LOGe("Invalid item always-on-top order attribute length in " << filePath << ".");
						return false;
					}

					uint8_t topOrder;
					if (!properties.GET_UCHAR(topOrder)) {
						LOGe("Cannot load item always-on-top order from " << filePath << ".");
						return false;
					}

					kind->alwaysOnTopOrder = topOrder;
					break;
				}

				case ITEM_ATTR_SPRITEHASH:
				case ITEM_ATTR_MINIMAPCOLOR:
				case ITEM_ATTR_07:
				case ITEM_ATTR_08:
					if (!properties.SKIP_N(length)) {
						LOGe("Cannot skip useless item attribute in " << filePath << ".");
						return false;
					}
					break;

				default: {
					if (!properties.SKIP_N(length)) {
						LOGe("Cannot skip unknown item attribute in " << filePath << ".");
						return false;
					}

					LOGw("Skipped unknown item attribute " << static_cast<uint32_t>(attribute) << " in " << filePath << ".");
					break;
				}
			}
		}

		auto clazzIt = _classes.find(kind->type);
		if (clazzIt == _classes.cend()) {
			LOGe("Cannot add item kind of unsupported type " << static_cast<uint32_t>(kind->type));
			continue;
		}

		kind->_class = clazzIt->second;

		addKind(kind, filePath);
	}

	_kinds.shrink_to_fit();

	return true;
}


bool Items::loadKindsFromXml() {
	std::string filePath = getFilePath(FileType::OTHER, "items/items.xml");

	xmlDocP document(xmlParseFile(filePath.c_str()));
	if (!document) {
		LOGe("Cannot load items file " << filePath << ": " << getLastXMLError());
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(document.get());
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("items")) != 0) {
		LOGe("No <items> root node in " << filePath << ".");
		return false;
	}

	IntegerVec intVector, endVector;
	std::string strValue, endValue;
	StringVector strVector;
	int32_t intValue;

	for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("item")) != 0) {
			LOGw("Unexpected <" << reinterpret_cast<const char*>(node->name) << "> node (expected <item>) in " << filePath << ".");
			continue;
		}

		if (readXMLInteger(node, "id", intValue)) {
			loadKindFromXmlNode(node, intValue, filePath);
		}
		else if (readXMLString(node, "fromid", strValue) && readXMLString(node, "toid", endValue)) {
			intVector = vectorAtoi(explodeString(strValue, ";"));
			endVector = vectorAtoi(explodeString(endValue, ";"));
			if (intVector[0] && endVector[0] && intVector.size() == endVector.size()) {
				size_t size = intVector.size();
				for (size_t i = 0; i < size; ++i) {
					loadKindFromXmlNode(node, intVector[i], filePath);
					while (intVector[i] < endVector[i]) {
						loadKindFromXmlNode(node, ++intVector[i], filePath);
					}
				}
			}
			else {
				LOGw("Malformed <item> node with ID range from '" << strValue << "' to '" << endValue << "' in " << filePath << ".");
			}
		}
		else {
			LOGw("Cannot parse <item> node without ID in " << filePath << ".");
		}
	}

	for (ItemKindVector::const_iterator it = _kinds.begin(); it != _kinds.end(); ++it) {
		const ItemKindP& type = *it;
		if (!type) {
			continue;
		}

		if ((type->transformToFree || type->transformUseTo[PLAYERSEX_FEMALE] || type->transformUseTo[PLAYERSEX_MALE]) && type->type != ItemType::BED) {
			LOGw("Item " << type->id << " is not set as a bed-type in " << filePath << ".");
		}
	}

	return true;
}


bool Items::loadRandomizationFromXml() {
	std::string filePath = getFilePath(FileType::OTHER, "items/randomization.xml");

	xmlDocP document(xmlParseFile(filePath.c_str()));
	if (!document) {
		LOGe("Cannot load items file " << filePath << ": " << getLastXMLError());
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(document.get());
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("randomization")) != 0) {
		LOGe("No <randomization> root node in " << filePath << ".");
		return false;
	}

	IntegerVec intVector, endVector;
	std::string strValue, endValue;
	StringVector strVector;

	int32_t intValue, kindId = 0, endId = 0, fromId = 0, toId = 0;

	for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("config")) == 0) {
			if (readXMLInteger(node, "chance", intValue) || readXMLInteger(node, "defaultChance", intValue)) {
				if (intValue > 100) {
					intValue = 100;
					LOGw("<config> node 'defaultChance' cannot be higher than 100 in " << filePath << ".");
				}

				_defaultRandomizationChance = intValue;
			}
		}
		else if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("palette")) == 0) {
			if (readXMLString(node, "randomize", strValue)) {
				std::vector<int32_t> itemList = vectorAtoi(explodeString(strValue, ";"));
				if (itemList.size() == 2 && itemList[0] < itemList[1]) {
					fromId = itemList[0];
					toId = itemList[1];
				}
				else {
					LOGw("<palette> node randomize value '" << strValue << "' must be in format 'fromId;toId' where fromId is less than toId in " << filePath << ".");
					continue;
				}

				int32_t chance = _defaultRandomizationChance;
				if (readXMLInteger(node, "chance", intValue)) {
					if (intValue > 100) {
						intValue = 100;
						LOGw("<palette> node 'chance' cannot be higher than 100 in " << filePath << ".");
					}

					chance = intValue;
				}

				if (readXMLInteger(node, "itemid", kindId)) {
					addRandomization(kindId, fromId, toId, chance, filePath);
				}
				else if (readXMLInteger(node, "fromid", kindId) && readXMLInteger(node, "toid", endId)) {
					addRandomization(kindId, fromId, toId, chance, filePath);
					while (kindId < endId) {
						addRandomization(++kindId, fromId, toId, chance, filePath);
					}
				}
				else {
					LOGw("<palette> nodes must specify either attribute 'itemid' or attributes 'fromid' and 'toid' in " << filePath << ".");
				}
			}
		}
		else {
			LOGw("Unexpected <" << reinterpret_cast<const char*>(node->name) << "> node (expected <config> or <palette>) in " << filePath << ".");
		}
	}

	return true;
}


bool Items::reload() {
	// TODO proper error handling
	// TODO reload move events & weapons?
	// TODO reuse previous ItemKinds

	clear();

	setupClasses();

	if (!loadKindsFromOtb()) {
		return false;
	}
	if (!loadKindsFromXml()) {
		return false;
	}
	if (!loadRandomizationFromXml()) {
		return false;
	}

	return true;
}


void Items::setupClasses() {
	_classes[ItemType::BED] = makeClass<BedItem>();
	_classes[ItemType::CONTAINER] = makeClass<Container>();
	_classes[ItemType::DEPOT] = makeClass<Depot>();
	_classes[ItemType::DOOR] = makeClass<Door>();
	_classes[ItemType::MAGICFIELD] = makeClass<MagicField>();
	_classes[ItemType::MAILBOX] = makeClass<Mailbox>();
	_classes[ItemType::GENERIC] = makeClass<Item>();
	_classes[ItemType::TELEPORT] = makeClass<Teleport>();
	_classes[ItemType::TRASHHOLDER] = makeClass<TrashHolder>();
	_classes[ItemType::KEY] = makeClass<Key>();
}


ItemKindPC Items::operator[](uint16_t typeId) const {
	return getKind(typeId);
}
