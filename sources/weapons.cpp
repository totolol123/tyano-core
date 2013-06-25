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
#include "weapons.h"

#include "game.h"
#include "configmanager.h"
#include "items.h"
#include "player.h"
#include "server.h"
#include "tools.h"
#include "vocation.h"


LOGGER_DEFINITION(Weapons);


Weapons::Weapons()
	: m_interface("Weapon Interface")
{}


WeaponPC Weapons::getWeapon(const Item* item) const
{
	if(!item)
		return nullptr;

	WeaponMap::const_iterator it = weapons.find(item->getId());
	if(it != weapons.end())
		return it->second;

	return nullptr;
}

void Weapons::clear()
{
	weapons.clear();
	m_interface.reInitState();
}

bool Weapons::loadDefaults()
{
	Items& items = server.items();

	for (Items::const_iterator it = items.begin(); it != items.end(); ++it) {
		const ItemKindPC& kind = *it;
		if (!kind || weapons.find(kind->id) != weapons.end())
			continue;

		if(kind->weaponType != WEAPON_NONE)
		{
			switch(kind->weaponType)
			{
				case WEAPON_AXE:
				case WEAPON_SWORD:
				case WEAPON_CLUB:
				case WEAPON_FIST:
				{
					WeaponMeleeP weapon = std::make_shared<WeaponMelee>(&m_interface);
					weapon->configureWeapon(*it);
					weapons[kind->id] = weapon;

					break;
				}

				case WEAPON_AMMO:
				case WEAPON_DIST:
				{
					if(kind->weaponType == WEAPON_DIST && kind->ammoType != AMMO_NONE)
						continue;

					WeaponDistanceP weapon = std::make_shared<WeaponDistance>(&m_interface);
					weapon->configureWeapon(*it);
					weapons[kind->id] = weapon;

					break;
				}

				default:
					break;
			}
		}
	}

	return true;
}

WeaponP Weapons::getEvent(const std::string& nodeName)
{
	std::string tmpNodeName = asLowerCaseString(nodeName);
	if(tmpNodeName == "melee")
		return std::make_shared<WeaponMelee>(&m_interface);

	if(tmpNodeName == "distance" || tmpNodeName == "ammunition")
		return std::make_shared<WeaponDistance>(&m_interface);

	if(tmpNodeName == "wand" || tmpNodeName == "rod")
		return std::make_shared<WeaponWand>(&m_interface);

	return nullptr;
}

bool Weapons::registerEvent(const WeaponP& weapon, xmlNodePtr p, bool override)
{
	WeaponMap::iterator it = weapons.find(weapon->getID());
	if(it == weapons.end())
	{
		weapons[weapon->getID()] = weapon;
		return true;
	}

	if(override)
	{
		it->second = weapon;
		return true;
	}

	LOGw("[Weapons::registerEvent] Duplicate registered item with id: " << weapon->getID());
	return false;
}

int32_t Weapons::getMaxMeleeDamage(int32_t attackSkill, int32_t attackValue)
{
	return (int32_t)std::ceil((attackSkill * (attackValue * 0.05)) + (attackValue * 0.5));
}

int32_t Weapons::getMaxWeaponDamage(int32_t level, int32_t attackSkill, int32_t attackValue, float attackFactor)
{
	return (int32_t)std::ceil((2 * (attackValue * (attackSkill + 5.8) / 25 + (level - 1) / 10.)) / attackFactor);
}



LOGGER_DEFINITION(Weapon);


Weapon::Weapon(LuaScriptInterface* _interface):
	Event(_interface)
{
	id = 0;
	level = 0;
	magLevel = 0;
	mana = 0;
	manaPercent = 0;
	soul = 0;
	exhaustion = 0;
	enabled = true;
	wieldUnproperly = false;
	swing = true;
	ammoAction = AMMOACTION_NONE;
	params.blockedByArmor = true;
	params.blockedByShield = true;
	params.combatType = COMBAT_PHYSICALDAMAGE;
}

Weapon::~Weapon()
{
	//
}

bool Weapon::configureEvent(xmlNodePtr p)
{
	int32_t intValue, wieldInfo = 0;
	std::string strValue;
	if(!readXMLInteger(p, "id", intValue))
	{
		LOGe("[Weapon::configureEvent] Weapon without id.");
		return false;
	}

	id = intValue;
	if(readXMLInteger(p, "lv", intValue) || readXMLInteger(p, "lvl", intValue) || readXMLInteger(p, "level", intValue))
	{
	 	level = intValue;
		if(level > 0)
			wieldInfo |= WIELDINFO_LEVEL;
	}

	if(readXMLInteger(p, "maglv", intValue) || readXMLInteger(p, "maglvl", intValue) || readXMLInteger(p, "maglevel", intValue))
	{
	 	magLevel = intValue;
		if(magLevel > 0)
			wieldInfo |= WIELDINFO_MAGLV;
	}

	if(readXMLInteger(p, "mana", intValue))
	 	mana = intValue;

	if(readXMLInteger(p, "manapercent", intValue))
	 	manaPercent = intValue;

	if(readXMLInteger(p, "soul", intValue))
	 	soul = intValue;

	if(readXMLInteger(p, "exhaust", intValue) || readXMLInteger(p, "exhaustion", intValue))
		exhaustion = intValue;

	if(readXMLString(p, "enabled", strValue))
		enabled = booleanString(strValue);

	if(readXMLString(p, "unproperly", strValue))
		wieldUnproperly = booleanString(strValue);

	if(readXMLString(p, "swing", strValue))
		swing = booleanString(strValue);

	if(readXMLString(p, "type", strValue))
		params.combatType = getCombatType(strValue);

	std::string error;
	StringVector vocStringVec;

	xmlNodePtr vocationNode = p->children;
	while(vocationNode)
	{
		if(!parseVocationNode(vocationNode, vocWeaponMap, vocStringVec, error))
			LOGw("[Weapon::configureEvent] " << error);

		vocationNode = vocationNode->next;
	}

	if(!vocWeaponMap.empty())
		wieldInfo |= WIELDINFO_VOCREQ;

	auto kind = server.items()[getID()];
	if (kind == nullptr) {
		LOGe("Weapon event for non-existent item " << getID() << ".");
		return false;
	}

	if(wieldInfo)
	{
		ItemKindP mutableKind = server.items().getMutableKind(id);
		if (mutableKind) {
			mutableKind->wieldInfo = wieldInfo;
			mutableKind->minReqLevel = getReqLevel();
			mutableKind->minReqMagicLevel = getReqMagLv();
			mutableKind->vocationString = parseVocationString(vocStringVec);
		}
	}

	return configureWeapon(kind);
}

bool Weapon::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "internalloadweapon" || tmpFunctionName == "default")
	{
		m_scripted = EVENT_SCRIPT_FALSE;
		return configureWeapon(server.items()[getID()]);
	}

	return false;
}

bool Weapon::configureWeapon(const ItemKindPC& kind)
{
	id = kind->id;
	return true;
}

int32_t Weapon::playerWeaponCheck(Player* player, Creature* target) const
{
	const Position& playerPos = player->getPosition();
	const Position& targetPos = target->getPosition();
	if(playerPos.z != targetPos.z)
		return 0;

	ItemKindPC kind = server.items()[getID()];
	if (!kind) {
		return 0;
	}

	int32_t range;
	if(kind->weaponType == WEAPON_AMMO)
		range = player->getShootRange();
	else
		range = kind->shootRange;

	if(std::max(std::abs(playerPos.x - targetPos.x), std::abs(playerPos.y - targetPos.y)) > range)
		return 0;

	if(player->hasFlag(PlayerFlag_IgnoreEquipCheck))
		return 100;

	if(!enabled)
		return 0;

	if(player->getMana() < getManaCost(player))
		return 0;

	if(player->getSoul() < soul)
		return 0;

	if(!vocWeaponMap.empty() && vocWeaponMap.find(player->getVocationId()) == vocWeaponMap.end())
		return 0;

	int32_t damageModifier = 100;
	if(player->getLevel() < getReqLevel())
		damageModifier = (isWieldedUnproperly() ? damageModifier / 2 : 0);

	if(player->getMagicLevel() < getReqMagLv())
		damageModifier = (isWieldedUnproperly() ? damageModifier / 2 : 0);

	return damageModifier;
}

bool Weapon::useWeapon(Player* player, Item* item, Creature* target) const
{
	int32_t damageModifier = playerWeaponCheck(player, target);
	if(!damageModifier)
		return false;

	return internalUseWeapon(player, item, target, damageModifier);
}

bool Weapon::useFist(Player* player, Creature* target)
{
	const Position& playerPos = player->getPosition();
	const Position& targetPos = target->getPosition();
	if(!Position::areInRange<1,1>(playerPos, targetPos))
		return false;

	float attackFactor = player->getAttackFactor();
	int32_t attackSkill = player->getSkill(SKILL_FIST, SKILL_LEVEL);
	int32_t attackValue = 7;

	double maxDamage = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);
	if(random_range(1, 100) <= server.configManager().getNumber(ConfigManager::CRITICAL_HIT_CHANCE))
	{
		maxDamage = std::pow(maxDamage, server.configManager().getDouble(ConfigManager::CRITICAL_HIT_MUL));
		player->sendCritical();
	}

	Vocation* vocation = player->getVocation();
	if(vocation && vocation->getMultiplier(MULTIPLIER_MELEE) != 1.0)
		maxDamage *= vocation->getMultiplier(MULTIPLIER_MELEE);

	maxDamage = std::floor(maxDamage);
	int32_t damage = -random_range(0, (int32_t)maxDamage, DISTRO_NORMAL);

	CombatParams fist;
	fist.blockedByArmor = true;
	fist.blockedByShield = true;
	fist.combatType = COMBAT_PHYSICALDAMAGE;

	Combat::doCombatHealth(player, target, damage, damage, fist);
	if(!player->hasFlag(PlayerFlag_NotGainSkill) && player->getAddAttackSkill())
		player->addSkillAdvance(SKILL_FIST, 1);

	return true;
}

bool Weapon::internalUseWeapon(Player* player, Item* item, Creature* target, int32_t damageModifier) const
{
	if(isScripted())
	{
		LuaVariant var;
		var.type = VARIANT_NUMBER;
		var.number = target->getID();
		executeUseWeapon(player, var);
	}
	else
	{
		int32_t damage = (getWeaponDamage(player, target, item) * damageModifier) / 100;
		Combat::doCombatHealth(player, target, damage, damage, params);
	}

	onUsedAmmo(player, item, target->getTile());
	onUsedWeapon(player, item, target->getTile());
	return true;
}

bool Weapon::internalUseWeapon(Player* player, Item* item, Tile* tile) const
{
	if(isScripted())
	{
		LuaVariant var;
		var.type = VARIANT_TARGETPOSITION;
		var.pos = StackPosition(tile->getPosition(), 0);
		executeUseWeapon(player, var);
	}
	else
	{
		Combat::postCombatEffects(player, tile->getPosition(), params);
		server.game().addMagicEffect(tile->getPosition(), MAGIC_EFFECT_POFF);
	}

	onUsedAmmo(player, item, tile);
	onUsedWeapon(player, item, tile);
	return true;
}

void Weapon::onUsedWeapon(Player* player, Item* item, Tile* destTile) const
{
	if(!player->hasFlag(PlayerFlag_NotGainSkill))
	{
		skills_t skillType;
		uint32_t skillPoint = 0;
		if(getSkillType(player, item, skillType, skillPoint))
			player->addSkillAdvance(skillType, skillPoint);
	}

	if(!player->hasFlag(PlayerFlag_HasNoExhaustion) && exhaustion > 0)
		player->addExhaust(exhaustion, EXHAUST_COMBAT);

	int32_t manaCost = getManaCost(player);
	if(manaCost > 0)
	{
		player->changeMana(-manaCost);
		if(!player->hasFlag(PlayerFlag_NotGainMana) && (player->getZone() != ZONE_PVP
			|| !server.configManager().getBool(ConfigManager::PVPZONE_ADDMANASPENT)))
			player->addManaSpent(manaCost);
	}

	if(!player->hasFlag(PlayerFlag_HasInfiniteSoul) && soul > 0)
		player->changeSoul(-soul);
}

void Weapon::onUsedAmmo(Player* player, Item* item, Tile* destTile) const
{
	if(!server.configManager().getBool(ConfigManager::REMOVE_WEAPON_AMMO))
		return;

	switch(ammoAction)
	{
		case AMMOACTION_REMOVECOUNT:
			server.game().transformItem(item, item->getId(), std::max((int32_t)0, ((int32_t)item->getItemCount()) - 1));
			break;

		case AMMOACTION_REMOVECHARGE:
			server.game().transformItem(item, item->getId(), std::max((int32_t)0, ((int32_t)item->getCharges()) - 1));
			break;

		case AMMOACTION_MOVE:
			server.game().internalMoveItem(player, item->getParent(), destTile, INDEX_WHEREEVER, item, 1, nullptr, FLAG_NOLIMIT);
			break;

		case AMMOACTION_MOVEBACK:
			break;

		default:
			if(item->hasCharges())
				server.game().transformItem(item, item->getId(), std::max((int32_t)0, ((int32_t)item->getCharges()) - 1));

			break;
	}
}

int32_t Weapon::getManaCost(const Player* player) const
{
	if(mana != 0)
		return mana;

	if(manaPercent != 0)
	{
		int32_t maxMana = player->getMaxMana();
		int32_t manaCost = (maxMana * manaPercent) / 100;
		return manaCost;
	}

	return 0;
}

bool Weapon::executeUseWeapon(Player* player, const LuaVariant& var) const
{
	//onUseWeapon(cid, var)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			env->streamVariant(scriptstream, "var", var);

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			m_interface->pushVariant(L, var);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[Weapon::executeUseWeapon] Call stack overflow");
		return false;
	}
}

WeaponMelee::WeaponMelee(LuaScriptInterface* _interface):
	Weapon(_interface)
{
	elementType = COMBAT_NONE;
	elementDamage = 0;
}

bool WeaponMelee::configureEvent(xmlNodePtr p)
{
	return Weapon::configureEvent(p);
}

bool WeaponMelee::configureWeapon(const ItemKindPC& kind)
{
	elementType = kind->abilities.elementType;
	elementDamage = kind->abilities.elementDamage;

	return Weapon::configureWeapon(kind);
}

bool WeaponMelee::useWeapon(Player* player, Item* item, Creature* target) const
{
	if(!Weapon::useWeapon(player, item, target))
		return false;

	if(elementDamage && elementType != COMBAT_NONE)
	{
		CombatParams element;
		element.combatType = elementType;

		int32_t damage = getElementDamage(player, item);
		Combat::doCombatHealth(player, target, damage, damage, element);
	}

	return true;
}

void WeaponMelee::onUsedWeapon(Player* player, Item* item, Tile* destTile) const
{
	Weapon::onUsedWeapon(player, item, destTile);
}

void WeaponMelee::onUsedAmmo(Player* player, Item* item, Tile* destTile) const
{
	Weapon::onUsedAmmo(player, item, destTile);
}

bool WeaponMelee::getSkillType(const Player* player, const Item* item,
	skills_t& skill, uint32_t& skillpoint) const
{
	skillpoint = 0;
	if(player->getAddAttackSkill())
	{
		switch(player->getLastAttackBlockType())
		{
			case BLOCK_ARMOR:
			case BLOCK_NONE:
				skillpoint = 1;
				break;

			case BLOCK_DEFENSE:
			default:
				skillpoint = 0;
				break;
		}
	}

	switch(item->getWeaponType())
	{
		case WEAPON_SWORD:
		{
			skill = SKILL_SWORD;
			return true;
		}

		case WEAPON_CLUB:
		{
			skill = SKILL_CLUB;
			return true;
		}

		case WEAPON_AXE:
		{
			skill = SKILL_AXE;
			return true;
		}

		case WEAPON_FIST:
		{
			skill = SKILL_FIST;
			return true;
		}

		default:
			break;
	}

	return false;
}

int32_t WeaponMelee::getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool maxDamage /*= false*/) const
{
	int32_t attackSkill = player->getWeaponSkill(item);
	int32_t attackValue = std::max((int32_t)0, (int32_t(item->getAttack() + item->getExtraAttack()) - elementDamage));
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);
	if(random_range(1, 100) <= server.configManager().getNumber(ConfigManager::CRITICAL_HIT_CHANCE))
	{
		maxValue = std::pow(maxValue, server.configManager().getDouble(ConfigManager::CRITICAL_HIT_MUL));
		player->sendCritical();
	}

	Vocation* vocation = player->getVocation();
	if(vocation && vocation->getMultiplier(MULTIPLIER_MELEE) != 1.0)
		maxValue *= vocation->getMultiplier(MULTIPLIER_MELEE);

	int32_t ret = (int32_t)std::floor(maxValue);
	if(maxDamage)
		return -ret;

	return -random_range(0, ret, DISTRO_NORMAL);
}

int32_t WeaponMelee::getElementDamage(const Player* player, const Item* item) const
{
	int32_t attackSkill = player->getWeaponSkill(item);
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, elementDamage, attackFactor);
	if(random_range(1, 100) <= server.configManager().getNumber(ConfigManager::CRITICAL_HIT_CHANCE))
	{
		maxValue = std::pow(maxValue, server.configManager().getDouble(ConfigManager::CRITICAL_HIT_MUL));
		player->sendCritical();
	}

	Vocation* vocation = player->getVocation();
	if(vocation && vocation->getMultiplier(MULTIPLIER_MELEE) != 1.0)
		maxValue *= vocation->getMultiplier(MULTIPLIER_MELEE);

	return -random_range(0, (int32_t)std::floor(maxValue), DISTRO_NORMAL);
}

WeaponDistance::WeaponDistance(LuaScriptInterface* _interface):
	Weapon(_interface)
{
	hitChance = -1;
	maxHitChance = breakChance = ammoAttackValue = 0;
	swing = params.blockedByShield = false;
}

bool WeaponDistance::configureEvent(xmlNodePtr p)
{
	return Weapon::configureEvent(p);
}

bool WeaponDistance::configureWeapon(const ItemKindPC& kind)
{
	if(kind->ammoType != AMMO_NONE) //hit chance on two-handed weapons is limited to 90%
		maxHitChance = 90;
	else //one-handed is set to 75%
		maxHitChance = 75;

	if(kind->hitChance > 0)
		hitChance = kind->hitChance;

	if(kind->maxHitChance > 0)
		maxHitChance = kind->maxHitChance;

	if(kind->breakChance > 0)
		breakChance = kind->breakChance;

	if(kind->ammoAction != AMMOACTION_NONE)
		ammoAction = kind->ammoAction;

	params.effects.distance = kind->shootType;
	ammoAttackValue = kind->attack;

	return Weapon::configureWeapon(kind);
}

int32_t WeaponDistance::playerWeaponCheck(Player* player, Creature* target) const
{
	ItemKindPC kind = server.items()[id];
	if(kind && kind->weaponType == WEAPON_AMMO)
	{
		if(Item* bow = player->getWeapon(true))
		{
			WeaponPC boWeapon = server.weapons().getWeapon(bow);
			if(boWeapon)
				return boWeapon->playerWeaponCheck(player, target);
		}
	}

	return Weapon::playerWeaponCheck(player, target);
}

bool WeaponDistance::useWeapon(Player* player, Item* item, Creature* target) const
{
	int32_t damageModifier = playerWeaponCheck(player, target);
	if(damageModifier == 0)
		return false;

	int32_t chance;
	if(hitChance == -1)
	{
		//hit chance is based on distance to target and distance skill
		uint32_t skill = player->getSkill(SKILL_DIST, SKILL_LEVEL);
		const Position& playerPos = player->getPosition();
		const Position& targetPos = target->getPosition();
		uint32_t distance = std::max(std::abs(playerPos.x - targetPos.x), std::abs(playerPos.y - targetPos.y));

		if(maxHitChance == 75)
		{
			//chance for one-handed weapons
			switch(distance)
			{
				case 1:
					chance = (uint32_t)((float)std::min(skill, (uint32_t)74)) + 1;
					break;
				case 2:
					chance = (uint32_t)((float)2.4 * std::min(skill, (uint32_t)28)) + 8;
					break;
				case 3:
					chance = (uint32_t)((float)1.55 * std::min(skill, (uint32_t)45)) + 6;
					break;
				case 4:
					chance = (uint32_t)((float)1.25 * std::min(skill, (uint32_t)58)) + 3;
					break;
				case 5:
					chance = (uint32_t)((float)std::min(skill, (uint32_t)74)) + 1;
					break;
				case 6:
					chance = (uint32_t)((float)0.8 * std::min(skill, (uint32_t)90)) + 3;
					break;
				case 7:
					chance = (uint32_t)((float)0.7 * std::min(skill, (uint32_t)104)) + 2;
					break;
				default:
					chance = hitChance;
					break;
			}
		}
		else if(maxHitChance == 90)
		{
			//formula for two-handed weapons
			switch(distance)
			{
				case 1:
					chance = (uint32_t)((float)1.2 * std::min(skill, (uint32_t)74)) + 1;
					break;
				case 2:
					chance = (uint32_t)((float)3.2 * std::min(skill, (uint32_t)28));
					break;
				case 3:
					chance = (uint32_t)((float)2.0 * std::min(skill, (uint32_t)45));
					break;
				case 4:
					chance = (uint32_t)((float)1.55 * std::min(skill, (uint32_t)58));
					break;
				case 5:
					chance = (uint32_t)((float)1.2 * std::min(skill, (uint32_t)74)) + 1;
					break;
				case 6:
					chance = (uint32_t)((float)1.0 * std::min(skill, (uint32_t)90));
					break;
				case 7:
					chance = (uint32_t)((float)1.0 * std::min(skill, (uint32_t)90));
					break;
				default:
					chance = hitChance;
					break;
			}
		}
		else if(maxHitChance == 100)
		{
			switch(distance)
			{
				case 1:
					chance = (uint32_t)((float)1.35 * std::min(skill, (uint32_t)73)) + 1;
					break;
				case 2:
					chance = (uint32_t)((float)3.2 * std::min(skill, (uint32_t)30)) + 4;
					break;
				case 3:
					chance = (uint32_t)((float)2.05 * std::min(skill, (uint32_t)48)) + 2;
					break;
				case 4:
					chance = (uint32_t)((float)1.5 * std::min(skill, (uint32_t)65)) + 2;
					break;
				case 5:
					chance = (uint32_t)((float)1.35 * std::min(skill, (uint32_t)73)) + 1;
					break;
				case 6:
					chance = (uint32_t)((float)1.2 * std::min(skill, (uint32_t)87)) - 4;
					break;
				case 7:
					chance = (uint32_t)((float)1.1 * std::min(skill, (uint32_t)90)) + 1;
					break;
				default:
					chance = hitChance;
					break;
			}
		}
		else
			chance = maxHitChance;
	}
	else
		chance = hitChance;

	if(item->getWeaponType() == WEAPON_AMMO)
	{
		Item* bow = player->getWeapon(true);
		if(bow && bow->getHitChance() > 0)
			chance += bow->getHitChance();
	}

	if(chance < random_range(1, 100))
	{
		//we failed attack, miss!
		Tile* destTile = target->getTile();
		if(!Position::areInRange<1,1,0>(player->getPosition(), target->getPosition()))
		{
			std::vector<std::pair<int32_t, int32_t> > destList;
			destList.push_back(std::make_pair(-1, -1));
			destList.push_back(std::make_pair(-1, 0));
			destList.push_back(std::make_pair(-1, 1));
			destList.push_back(std::make_pair(0, -1));
			destList.push_back(std::make_pair(0, 0));
			destList.push_back(std::make_pair(0, 1));
			destList.push_back(std::make_pair(1, -1));
			destList.push_back(std::make_pair(1, 0));
			destList.push_back(std::make_pair(1, 1));
			std::random_shuffle(destList.begin(), destList.end());

			Position destPos = target->getPosition();
			Tile* tmpTile = nullptr;
			for(std::vector<std::pair<int32_t, int32_t> >::iterator it = destList.begin(); it != destList.end(); ++it)
			{
				if((tmpTile = server.game().getTile(destPos.x + it->first, destPos.y + it->second, destPos.z))
					&& !tmpTile->hasProperty(IMMOVABLEBLOCKSOLID) && tmpTile->ground)
				{
					destTile = tmpTile;
					break;
				}
			}
		}

		Weapon::internalUseWeapon(player, item, destTile);
	}
	else
		Weapon::internalUseWeapon(player, item, target, damageModifier);

	return true;
}

void WeaponDistance::onUsedWeapon(Player* player, Item* item, Tile* destTile) const
{
	Weapon::onUsedWeapon(player, item, destTile);
}

void WeaponDistance::onUsedAmmo(Player* player, Item* item, Tile* destTile) const
{
	if(ammoAction == AMMOACTION_MOVEBACK && breakChance > 0 && random_range(1, 100) < breakChance)
		server.game().transformItem(item, item->getId(), std::max(0, item->getItemCount() - 1));
	else
		Weapon::onUsedAmmo(player, item, destTile);
}

int32_t WeaponDistance::getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool maxDamage /*= false*/) const
{
	int32_t attackValue = ammoAttackValue;
	if(item->getWeaponType() == WEAPON_AMMO)
	{
		Item* bow = const_cast<Player*>(player)->getWeapon(true);
		if(bow)
			attackValue += bow->getAttack() + bow->getExtraAttack();
	}

	int32_t attackSkill = player->getSkill(SKILL_DIST, SKILL_LEVEL);
	float attackFactor = player->getAttackFactor();

	double maxValue = Weapons::getMaxWeaponDamage(player->getLevel(), attackSkill, attackValue, attackFactor);
	if(random_range(1, 100) <= server.configManager().getNumber(ConfigManager::CRITICAL_HIT_CHANCE))
	{
		maxValue = std::pow(maxValue, server.configManager().getDouble(ConfigManager::CRITICAL_HIT_MUL));
		player->sendCritical();
	}

	Vocation* vocation = player->getVocation();
	if(vocation && vocation->getMultiplier(MULTIPLIER_DISTANCE) != 1.0)
		maxValue *= vocation->getMultiplier(MULTIPLIER_DISTANCE);

	int32_t ret = (int32_t)std::floor(maxValue);
	if(maxDamage)
		return -ret;

	int32_t minValue = 0;
	if(target)
	{
		if(target->getPlayer())
			minValue = (int32_t)std::ceil(player->getLevel() * 0.1);
		else
			minValue = (int32_t)std::ceil(player->getLevel() * 0.2);
	}

	return -random_range(minValue, ret, DISTRO_NORMAL);
}

bool WeaponDistance::getSkillType(const Player* player, const Item* item,
	skills_t& skill, uint32_t& skillpoint) const
{
	skill = SKILL_DIST;
	skillpoint = 0;

	if(player->getAddAttackSkill())
	{
		switch(player->getLastAttackBlockType())
		{
			case BLOCK_NONE:
				skillpoint = 2;
				break;

			case BLOCK_ARMOR:
				skillpoint = 1;
				break;

			case BLOCK_DEFENSE:
			default:
				skillpoint = 0;
				break;
		}
	}
	return true;
}

WeaponWand::WeaponWand(LuaScriptInterface* _interface):
	Weapon(_interface)
{
	minChange = 0;
	maxChange = 0;
	params.blockedByArmor = false;
	params.blockedByShield = false;
}

bool WeaponWand::configureEvent(xmlNodePtr p)
{
	if(!Weapon::configureEvent(p))
		return false;

	int32_t intValue;
	if(readXMLInteger(p, "min", intValue))
		minChange = intValue;

	if(readXMLInteger(p, "max", intValue))
		maxChange = intValue;

	return true;
}

bool WeaponWand::configureWeapon(const ItemKindPC& kind)
{
	params.effects.distance = kind->shootType;

	return Weapon::configureWeapon(kind);
}

int32_t WeaponWand::getWeaponDamage(const Player* player, const Creature* target, const Item* item, bool maxDamage /* = false*/) const
{
	int32_t attack = item->getAttack();

	float multiplier = 1.0f;
	if(Vocation* vocation = player->getVocation())
		multiplier = vocation->getMultiplier(MULTIPLIER_WAND);

	int32_t maxValue = (int32_t)((maxChange + attack) * multiplier);
	if(maxDamage)
	{
		player->sendCritical();
		return -maxValue;
	}

	int32_t minValue = (int32_t)((minChange + attack) * multiplier);
	return random_range(-minValue, -maxValue, DISTRO_NORMAL);
}
