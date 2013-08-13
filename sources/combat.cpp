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
#include "const.h"

#include "combat.h"
#include "tools.h"

#include "game.h"
#include "configmanager.h"
#include "map.h"

#include "condition.h"
#include "creature.h"
#include "creatureevent.h"
#include "items.h"
#include "monster.h"
#include "player.h"
#include "weapons.h"
#include "vocation.h"

#include "server.h"


LOGGER_DEFINITION(Combat);


Combat::Combat()
{
	params.valueCallback = nullptr;
	params.tileCallback = nullptr;
	params.targetCallback = nullptr;
	area = nullptr;

	formulaType = FORMULA_UNDEFINED;
	mina = minb = maxa = maxb = minl = maxl = minm = maxm = minc = maxc = 0;
}

Combat::~Combat()
{
	for(std::list<const Condition*>::iterator it = params.conditionList.begin(); it != params.conditionList.end(); ++it)
		delete (*it);

	params.conditionList.clear();
	delete area;

	delete params.valueCallback;
	delete params.tileCallback;
	delete params.targetCallback;
}

bool Combat::getMinMaxValues(const CreatureP& creature, const CreatureP& target, int32_t& min, int32_t& max) const
{
	if(creature)
	{
		if(creature->getCombatValues(min, max))
			return true;

		if(Player* player = creature->getPlayer())
		{
			if(params.valueCallback)
			{
				params.valueCallback->getMinMaxValues(player, min, max, params.useCharges);
				return true;
			}

			min = max = 0;
			switch(formulaType)
			{
				case FORMULA_LEVELMAGIC:
				{
					min = (int32_t)((player->getLevel() / minl + player->getMagicLevel() * minm) * 1. * mina + minb);
					max = (int32_t)((player->getLevel() / maxl + player->getMagicLevel() * maxm) * 1. * maxa + maxb);
					if(minc && std::abs(min) < std::abs(minc))
						min = minc;

					if(maxc && std::abs(max) < std::abs(maxc))
						max = maxc;

					player->increaseCombatValues(min, max, params.useCharges, true);
					return true;
				}

				case FORMULA_SKILL:
				{
					Item* tool = player->getWeapon();
					if(WeaponPC weapon = server.weapons().getWeapon(tool))
					{
						max = (int32_t)(weapon->getWeaponDamage(*player, target.get(), tool, true) * maxa + maxb);
						if(params.useCharges && tool->hasCharges() && server.configManager().getBool(ConfigManager::REMOVE_WEAPON_CHARGES))
							server.game().transformItem(tool, tool->getId(), std::max((int32_t)0, ((int32_t)tool->getCharges()) - 1));
					}
					else
						max = (int32_t)maxb;

					min = (int32_t)minb;
					if(maxc && std::abs(max) < std::abs(maxc))
						max = maxc;

					return true;
				}

				case FORMULA_VALUE:
				{
					min = (int32_t)minb;
					max = (int32_t)maxb;
					return true;
				}

				default:
					break;
			}

			return false;
		}
	}

	if(formulaType != FORMULA_VALUE)
		return false;

	min = (int32_t)mina;
	max = (int32_t)maxa;
	return true;
}

void Combat::getCombatArea(const Position& centerPos, const Position& targetPos, const CombatArea* area, std::list<Tile*>& list)
{
	if(area)
		area->getList(centerPos, targetPos, list);
	else if(targetPos.z < MAP_MAX_LAYERS)
	{
		Tile* tile = server.game().getTile(targetPos);
		if (tile != nullptr) {
			list.push_back(tile);
		}
	}
}

CombatType_t Combat::ConditionToDamageType(ConditionType_t type)
{
	switch(type)
	{
		case CONDITION_FIRE:
			return COMBAT_FIREDAMAGE;

		case CONDITION_ENERGY:
			return COMBAT_ENERGYDAMAGE;

		case CONDITION_POISON:
			return COMBAT_EARTHDAMAGE;

		case CONDITION_FREEZING:
			return COMBAT_ICEDAMAGE;

		case CONDITION_DAZZLED:
			return COMBAT_HOLYDAMAGE;

		case CONDITION_CURSED:
			return COMBAT_DEATHDAMAGE;

		case CONDITION_DROWN:
			return COMBAT_DROWNDAMAGE;

		case CONDITION_PHYSICAL:
			return COMBAT_PHYSICALDAMAGE;

		default:
			break;
	}

	return COMBAT_NONE;
}

ConditionType_t Combat::DamageToConditionType(CombatType_t type)
{
	switch(type)
	{
		case COMBAT_FIREDAMAGE:
			return CONDITION_FIRE;

		case COMBAT_ENERGYDAMAGE:
			return CONDITION_ENERGY;

		case COMBAT_EARTHDAMAGE:
			return CONDITION_POISON;

		case COMBAT_ICEDAMAGE:
			return CONDITION_FREEZING;

		case COMBAT_HOLYDAMAGE:
			return CONDITION_DAZZLED;

		case COMBAT_DEATHDAMAGE:
			return CONDITION_CURSED;

		case COMBAT_PHYSICALDAMAGE:
			return CONDITION_PHYSICAL;

		default:
			break;
	}

	return CONDITION_NONE;
}

ReturnValue Combat::canDoCombat(const CreatureP& caster, Tile* tile, bool isAggressive)
{
	if(tile->hasProperty(BLOCKPROJECTILE) || tile->floorChange() || tile->getTeleporter())
		return RET_NOTENOUGHROOM;

	if(caster)
	{
		bool success = true;
		CreatureEventList combatAreaEvents = caster->getCreatureEvents(CREATURE_EVENT_COMBAT_AREA);
		for(CreatureEventList::iterator it = combatAreaEvents.begin(); it != combatAreaEvents.end(); ++it)
		{
			if(!(*it)->executeCombatArea(caster, tile, isAggressive) && success)
				success = false;
		}

		if(!success)
			return RET_NOTPOSSIBLE;

		if(caster->getPosition().z < tile->getPosition().z)
			return RET_FIRSTGODOWNSTAIRS;

		if(caster->getPosition().z > tile->getPosition().z)
			return RET_FIRSTGOUPSTAIRS;

		if(!isAggressive)
			return RET_NOERROR;

		const Player* player = caster->getPlayer();
		if(player && player->hasFlag(PlayerFlag_IgnoreProtectionZone))
			return RET_NOERROR;
	}

	return isAggressive && tile->hasFlag(TILESTATE_PROTECTIONZONE) ?
		RET_ACTIONNOTPERMITTEDINPROTECTIONZONE : RET_NOERROR;
}

ReturnValue Combat::canDoCombat(const CreatureP& attacker, const CreatureP& target)
{
	if(!attacker)
		return RET_NOERROR;

	bool success = true;
	CreatureEventList combatEvents = attacker->getCreatureEvents(CREATURE_EVENT_COMBAT);
	for(CreatureEventList::iterator it = combatEvents.begin(); it != combatEvents.end(); ++it)
	{
		if(!(*it)->executeCombat(attacker, target) && success)
			success = false;
	}

	if(!success)
		return RET_NOTPOSSIBLE;

	const Monster* attackerMonster = attacker->getMonster();

	bool checkZones = false;
	if(const Player* targetPlayer = target->getPlayer())
	{
		if(!targetPlayer->isAttackable())
			return RET_YOUMAYNOTATTACKTHISPLAYER;

		const Player* attackerPlayer = nullptr;
		if((attackerPlayer = attacker->getPlayer()) || (attackerMonster != nullptr && attackerMonster->getMaster()
			&& (attackerPlayer = attackerMonster->getMaster()->getPlayer())))
		{
			checkZones = true;
			if((server.game().getWorldType() == WORLD_TYPE_NO_PVP && !Combat::isInPvpZone(*attacker, *target)) ||
				isProtected(const_cast<Player*>(attackerPlayer), const_cast<Player*>(targetPlayer))
				|| (server.configManager().getBool(ConfigManager::CANNOT_ATTACK_SAME_LOOKFEET) &&
				attackerPlayer->getDefaultOutfit().lookFeet == targetPlayer->getDefaultOutfit().lookFeet)
				|| !attackerPlayer->canSeeCreature(targetPlayer))
				return RET_YOUMAYNOTATTACKTHISPLAYER;
		}
	}
	else if(const Monster* targetMonster = target->getMonster())
	{
		if(!target->isAttackable())
			return RET_YOUMAYNOTATTACKTHISCREATURE;

		if (attackerMonster != nullptr && !attackerMonster->hasController() && !targetMonster->hasController()) {
			return RET_YOUMAYNOTATTACKTHISCREATURE;
		}

		const Player* attackerPlayer = nullptr;
		if((attackerPlayer = attacker->getPlayer()) || (attackerMonster != nullptr && attackerMonster->getMaster()
			&& (attackerPlayer = attackerMonster->getMaster()->getPlayer())))
		{
			if(attackerPlayer->hasFlag(PlayerFlag_CannotAttackMonster))
				return RET_YOUMAYNOTATTACKTHISCREATURE;

			const Monster* targetMonster = target->getMonster();
			if(targetMonster != nullptr && targetMonster->getMaster() && targetMonster->getMaster()->getPlayer())
			{
				checkZones = true;
				if(server.game().getWorldType() == WORLD_TYPE_NO_PVP && !Combat::isInPvpZone(*attacker, *target))
					return RET_YOUMAYNOTATTACKTHISCREATURE;
			}
		}
	}

	return checkZones && (target->getTile()->hasFlag(TILESTATE_NOPVPZONE) ||
		(attacker->getTile()->hasFlag(TILESTATE_NOPVPZONE)
		&& !target->getTile()->hasFlag(TILESTATE_NOPVPZONE) &&
		!target->getTile()->hasFlag(TILESTATE_PROTECTIONZONE))) ?
		RET_ACTIONNOTPERMITTEDINANOPVPZONE : RET_NOERROR;
}

ReturnValue Combat::canTargetCreature(const PlayerP& player, const CreatureP& target)
{
	if(player == target)
		return RET_YOUMAYNOTATTACKTHISPLAYER;

	CreatureEventList targetEvents = player->getCreatureEvents(CREATURE_EVENT_TARGET);

	bool deny = false;
	for(CreatureEventList::iterator it = targetEvents.begin(); it != targetEvents.end(); ++it)
	{
		if(!(*it)->executeTarget(player, target))
			deny = true;
	}

	if(deny)
		return RET_DONTSHOWMESSAGE;

	if(!player->hasFlag(PlayerFlag_IgnoreProtectionZone))
	{
		if(player->getZone() == ZONE_PROTECTION)
			return RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE;

		if(target->getZone() == ZONE_PROTECTION)
			return RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE;

		const Monster* targetMonster = target->getMonster();
		if(target->getPlayer() || (targetMonster != nullptr && targetMonster->getMaster() && targetMonster->getMaster()->getPlayer()))
		{
			if(player->getZone() == ZONE_NOPVP)
				return RET_ACTIONNOTPERMITTEDINANOPVPZONE;

			if(target->getZone() == ZONE_NOPVP)
				return RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE;
		}
	}

	if(player->hasFlag(PlayerFlag_CannotUseCombat) || !target->isAttackable())
		return target->getPlayer() ? RET_YOUMAYNOTATTACKTHISPLAYER : RET_YOUMAYNOTATTACKTHISCREATURE;

	if(target->getPlayer() && !Combat::isInPvpZone(*player, *target) && player->getSkullClient(target->getPlayer()) == SKULL_NONE)
	{
		if(player->getSecureMode() == SECUREMODE_ON)
			return RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS;

		if(player->getSkull() == SKULL_BLACK)
			return RET_YOUMAYNOTATTACKTHISPLAYER;
	}

	return canDoCombat(player, target);
}

bool Combat::isInPvpZone(const Creature& attacker, const Creature& target)
{
	return attacker.getZone() == ZONE_PVP && target.getZone() == ZONE_PVP;
}

bool Combat::isProtected(Player* attacker, Player* target)
{
	if(attacker->hasFlag(PlayerFlag_CannotAttackPlayer) || !target->isAttackable())
		return true;

	if(attacker->getZone() == ZONE_PVP && target->getZone() == ZONE_PVP && server.configManager().getBool(ConfigManager::PVP_TILE_IGNORE_PROTECTION))
		return false;

	if(attacker->hasCustomFlag(PlayerCustomFlag_IsProtected) || target->hasCustomFlag(PlayerCustomFlag_IsProtected))
		return true;

	uint32_t protectionLevel = server.configManager().getNumber(ConfigManager::PROTECTION_LEVEL);
	if(target->getLevel() < protectionLevel || attacker->getLevel() < protectionLevel)
		return true;

	if(!attacker->getVocation()->isAttackable() || !target->getVocation()->isAttackable())
		return true;

	return attacker->checkLoginDelay(target->getID());
}

void Combat::setPlayerCombatValues(formulaType_t _type, double _mina, double _minb, double _maxa, double _maxb, double _minl, double _maxl, double _minm, double _maxm, int32_t _minc, int32_t _maxc)
{
	formulaType = _type; mina = _mina; minb = _minb; maxa = _maxa; maxb = _maxb;
	minl = _minl; maxl = _maxl; minm = _minm; maxm = _maxm; minc = _minc; maxc = _maxc;
}

bool Combat::setParam(CombatParam_t param, uint32_t value)
{
	switch(param)
	{
		case COMBATPARAM_COMBATTYPE:
			params.combatType = (CombatType_t)value;
			return true;

		case COMBATPARAM_EFFECT:
			params.effects.impact = (MagicEffect_t)value;
			return true;

		case COMBATPARAM_DISTANCEEFFECT:
			params.effects.distance = (ShootEffect_t)value;
			return true;

		case COMBATPARAM_BLOCKEDBYARMOR:
			params.blockedByArmor = (value != 0);
			return true;

		case COMBATPARAM_BLOCKEDBYSHIELD:
			params.blockedByShield = (value != 0);
			return true;

		case COMBATPARAM_TARGETCASTERORTOPMOST:
			params.targetCasterOrTopMost = (value != 0);
			return true;

		case COMBATPARAM_TARGETPLAYERSORSUMMONS:
			params.targetPlayersOrSummons = (value != 0);
			return true;

		case COMBATPARAM_DIFFERENTAREADAMAGE:
			params.differentAreaDamage = (value != 0);
			return true;

		case COMBATPARAM_CREATEITEM:
			params.itemId = value;
			return true;

		case COMBATPARAM_AGGRESSIVE:
			params.isAggressive = (value != 0);
			return true;

		case COMBATPARAM_DISPEL:
			params.dispelType = (ConditionType_t)value;
			return true;

		case COMBATPARAM_USECHARGES:
			params.useCharges = (value != 0);
			return true;

		case COMBATPARAM_HITEFFECT:
			params.effects.hit = (MagicEffect_t)value;
			return true;

		case COMBATPARAM_HITCOLOR:
			params.effects.color = (TextColor_t)value;
			return true;

		default:
			break;
	}

	return false;
}

bool Combat::setCallback(CallBackParam_t key)
{
	switch(key)
	{
		case CALLBACKPARAM_LEVELMAGICVALUE:
		{
			delete params.valueCallback;
			params.valueCallback = new ValueCallback(FORMULA_LEVELMAGIC);
			return true;
		}

		case CALLBACKPARAM_SKILLVALUE:
		{
			delete params.valueCallback;
			params.valueCallback = new ValueCallback(FORMULA_SKILL);
			return true;
		}

		case CALLBACKPARAM_TARGETTILECALLBACK:
		{
			delete params.tileCallback;
			params.tileCallback = new TileCallback();
			break;
		}

		case CALLBACKPARAM_TARGETCREATURECALLBACK:
		{
			delete params.targetCallback;
			params.targetCallback = new TargetCallback();
			break;
		}

		default:
			LOGe("Combat::setCallback - Unknown callback type: " << (uint32_t)key);
			break;
	}

	return false;
}

CallBack* Combat::getCallback(CallBackParam_t key)
{
	switch(key)
	{
		case CALLBACKPARAM_LEVELMAGICVALUE:
		case CALLBACKPARAM_SKILLVALUE:
			return params.valueCallback;

		case CALLBACKPARAM_TARGETTILECALLBACK:
			return params.tileCallback;

		case CALLBACKPARAM_TARGETCREATURECALLBACK:
			return params.targetCallback;

		default:
			break;
	}

	return nullptr;
}

bool Combat::CombatHealthFunc(const CreatureP& caster, const CreatureP& target, const CombatParams& params, void* data)
{
	int32_t change = 0;
	if(Combat2Var* var = (Combat2Var*)data)
	{
		change = var->change;
		if(!change)
			change = random_range(var->minChange, var->maxChange, DISTRO_NORMAL);
	}

	if(server.game().combatBlockHit(params.combatType, caster, target, change, params.blockedByShield, params.blockedByArmor))
		return false;

	if(change < 0 && caster && caster->getPlayer() && target->getPlayer() && target->getPlayer()->getSkull() != SKULL_BLACK)
		change = change / 2;

	if(!server.game().combatChangeHealth(params.combatType, caster, target, change, params.effects.hit, params.effects.color))
		return false;

	CombatConditionFunc(caster, target, params, nullptr);
	CombatDispelFunc(caster, target, params, nullptr);
	return true;
}

bool Combat::CombatManaFunc(const CreatureP& caster, const CreatureP& target, const CombatParams& params, void* data)
{
	int32_t change = 0;
	if(Combat2Var* var = (Combat2Var*)data)
	{
		change = var->change;
		if(!change)
			change = random_range(var->minChange, var->maxChange, DISTRO_NORMAL);
	}

	if(change < 0 && caster && caster->getPlayer() && target->getPlayer() && target->getPlayer()->getSkull() != SKULL_BLACK)
		change = change / 2;

	if(!server.game().combatChangeMana(caster, target, change))
		return false;

	CombatConditionFunc(caster, target, params, nullptr);
	CombatDispelFunc(caster, target, params, nullptr);
	return true;
}

bool Combat::CombatConditionFunc(const CreatureP& caster, const CreatureP& target, const CombatParams& params, void* data)
{
	if(params.conditionList.empty())
		return false;

	bool result = true;
	for(std::list<const Condition*>::const_iterator it = params.conditionList.begin(); it != params.conditionList.end(); ++it)
	{
		if(caster != target && target->isImmune((*it)->getType()))
			continue;

		Condition* tmp = (*it)->clone();
		if(caster)
			tmp->setParam(CONDITIONPARAM_OWNER, caster->getID());

		//TODO: infight condition until all aggressive conditions has ended
		if(!target->addCombatCondition(tmp) && result)
			result = false;
	}

	return result;
}

bool Combat::CombatDispelFunc(const CreatureP& caster, const CreatureP& target, const CombatParams& params, void* data)
{
	if(!target->hasCondition(params.dispelType))
		return false;

	target->removeCondition(caster, params.dispelType);
	return true;
}

bool Combat::CombatNullFunc(const CreatureP& caster, const CreatureP& target, const CombatParams& params, void* data)
{
	CombatConditionFunc(caster, target, params, nullptr);
	CombatDispelFunc(caster, target, params, nullptr);
	return true;
}

void Combat::combatTileEffects(const SpectatorList& list, const CreatureP& caster, Tile* tile, const CombatParams& params)
{
	if(params.itemId)
	{
		Player* player = nullptr;
		if(caster)
		{
			const Monster* casterMonster = caster->getMonster();

			if(caster->getPlayer())
				player = caster->getPlayer();
			else if(casterMonster != nullptr && casterMonster->hasMaster())
				player = casterMonster->getMaster()->getPlayer();
		}

		uint32_t itemId = params.itemId;
		if(player)
		{
			bool pzLock = false;
			if(server.game().getWorldType() == WORLD_TYPE_NO_PVP || tile->hasFlag(TILESTATE_NOPVPZONE))
			{
				switch(itemId)
				{
					case ITEM_FIREFIELD:
						itemId = ITEM_FIREFIELD_SAFE;
						break;
					case ITEM_POISONFIELD:
						itemId = ITEM_POISONFIELD_SAFE;
						break;
					case ITEM_ENERGYFIELD:
						itemId = ITEM_ENERGYFIELD_SAFE;
						break;
					case ITEM_MAGICWALL:
						itemId = ITEM_MAGICWALL_SAFE;
						break;
					case ITEM_WILDGROWTH:
						itemId = ITEM_WILDGROWTH_SAFE;
						break;
					default:
						break;
				}
			}
			else if(params.isAggressive) {
				ItemKindPC itemKind = server.items()[itemId];
				if (itemKind && itemKind->blockPathFind) {
					pzLock = true;
				}
			}

			player->addInFightTicks(pzLock);
		}

		if(boost::intrusive_ptr<Item> item = Item::CreateItem(itemId))
		{
			if(caster)
				item->setOwner(caster->getID());

			if(server.game().internalAddItem(caster.get(), tile, item.get()) == RET_NOERROR)
				server.game().startDecay(item.get());
		}
	}

	if(params.tileCallback)
		params.tileCallback->onTileCombat(caster, tile);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(list, tile->getPosition(), params.effects.impact);
}

void Combat::postCombatEffects(const CreatureP& caster, const Position& pos, const CombatParams& params)
{
	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), pos, params.effects.distance);
}

void Combat::addDistanceEffect(const CreatureP& caster, const Position& fromPos, const Position& toPos, ShootEffect_t effect)
{
	if(effect == SHOOT_EFFECT_WEAPONTYPE)
	{
		switch(caster->getWeaponType())
		{
			case WEAPON_AXE:
				effect = SHOOT_EFFECT_WHIRLWINDAXE;
				break;

			case WEAPON_SWORD:
				effect = SHOOT_EFFECT_WHIRLWINDSWORD;
				break;

			case WEAPON_CLUB:
				effect = SHOOT_EFFECT_WHIRLWINDCLUB;
				break;

			case WEAPON_FIST:
				effect = SHOOT_EFFECT_LARGEROCK;
				break;

			default:
				effect = SHOOT_EFFECT_NONE;
				break;
		}
	}

	if(caster && effect != SHOOT_EFFECT_NONE)
		server.game().addDistanceEffect(fromPos, toPos, effect);
}

void Combat::CombatFunc(const CreatureP& caster, const Position& pos, const CombatArea* area,
	const CombatParams& params, COMBATFUNC func, void* data)
{
	std::list<Tile*> tileList;
	if(caster)
		getCombatArea(caster->getPosition(), pos, area, tileList);
	else
		getCombatArea(pos, pos, area, tileList);

	Combat2Var* var = (Combat2Var*)data;
	if(var && !params.differentAreaDamage)
		var->change = random_range(var->minChange, var->maxChange, DISTRO_NORMAL);

	uint32_t maxX = 0, maxY = 0, diff;
	//calculate the max viewable range
	for(std::list<Tile*>::iterator it = tileList.begin(); it != tileList.end(); ++it)
	{
		diff = std::abs((*it)->getPosition().x - pos.x);
		if(diff > maxX)
			maxX = diff;

		diff = std::abs((*it)->getPosition().y - pos.y);
		if(diff > maxY)
			maxY = diff;
	}

	SpectatorList list;
	server.game().getSpectators(list, pos, false, true, maxX + Map::maxViewportX, maxX + Map::maxViewportX,
		maxY + Map::maxViewportY, maxY + Map::maxViewportY);

	MonsterP monsterCaster = caster->getMonster();

	Tile* tile = nullptr;
	for(std::list<Tile*>::iterator it = tileList.begin(); it != tileList.end(); ++it)
	{
		if(!(tile = (*it)) || canDoCombat(caster, (*it), params.isAggressive) != RET_NOERROR)
			continue;

		bool skip = true;

		if (tile->getCreatures() != nullptr) {
			auto creatures = *tile->getCreatures();
			for (auto cit = creatures.begin(), cend = creatures.end(); skip && cit != cend; ++cit)
			{
				if(params.targetPlayersOrSummons && !(*cit)->getPlayer() && !(*cit)->hasController())
					continue;

				if(params.targetCasterOrTopMost)
				{
					if(caster && caster->getTile() == tile)
					{
						if((*cit) == caster)
							skip = false;
					}
					else if((*cit) == tile->getTopCreature())
						skip = false;

					if(skip)
						continue;
				}

				if(!params.isAggressive || (caster != (*cit) && Combat::canDoCombat(caster, (*cit).get()) == RET_NOERROR))
				{
					func(caster, *cit, params, (void*)var);
					if(params.targetCallback)
						params.targetCallback->onTargetCombat(caster, *cit);
				}
			}
		}

		combatTileEffects(list, caster, tile, params);
	}

	postCombatEffects(caster, pos, params);
}

void Combat::doCombat(const CreatureP& caster, const CreatureP& target) const
{
	//target combat callback function
	if(params.combatType != COMBAT_NONE)
	{
		int32_t minChange = 0, maxChange = 0;
		getMinMaxValues(caster, target, minChange, maxChange);
		if(params.combatType != COMBAT_MANADRAIN)
			doCombatHealth(caster, target, minChange, maxChange, params);
		else
			doCombatMana(caster, target, minChange, maxChange, params);
	}
	else
		doCombatDefault(caster, target, params);
}

void Combat::doCombat(const CreatureP& caster, const Position& pos) const
{
	//area combat callback function
	if(params.combatType != COMBAT_NONE)
	{
		int32_t minChange = 0, maxChange = 0;
		getMinMaxValues(caster, nullptr, minChange, maxChange);
		if(params.combatType != COMBAT_MANADRAIN)
			doCombatHealth(caster, pos, area, minChange, maxChange, params);
		else
			doCombatMana(caster, pos, area, minChange, maxChange, params);
	}
	else
		CombatFunc(caster, pos, area, params, CombatNullFunc, nullptr);
}

void Combat::doCombatHealth(const CreatureP& caster, const CreatureP& target, int32_t minChange, int32_t maxChange, const CombatParams& params)
{
	if(params.isAggressive && (caster == target || Combat::canDoCombat(caster, target) != RET_NOERROR))
		return;

	Combat2Var var;
	var.minChange = minChange;
	var.maxChange = maxChange;

	CombatHealthFunc(caster, target, params, (void*)&var);
	if(params.targetCallback)
		params.targetCallback->onTargetCombat(caster, target);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(target->getPosition(), params.effects.impact);

	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), target->getPosition(), params.effects.distance);
}

void Combat::doCombatHealth(const CreatureP& caster, const Position& pos, const CombatArea* area,
	int32_t minChange, int32_t maxChange, const CombatParams& params)
{
	Combat2Var var;
	var.minChange = minChange;
	var.maxChange = maxChange;
	CombatFunc(caster, pos, area, params, CombatHealthFunc, (void*)&var);
}

void Combat::doCombatMana(const CreatureP& caster, const CreatureP& target, int32_t minChange, int32_t maxChange, const CombatParams& params)
{
	if(params.isAggressive && (caster == target || Combat::canDoCombat(caster, target) != RET_NOERROR))
		return;

	Combat2Var var;
	var.minChange = minChange;
	var.maxChange = maxChange;

	CombatManaFunc(caster, target, params, (void*)&var);
	if(params.targetCallback)
		params.targetCallback->onTargetCombat(caster, target);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(target->getPosition(), params.effects.impact);

	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), target->getPosition(), params.effects.distance);
}

void Combat::doCombatMana(const CreatureP& caster, const Position& pos, const CombatArea* area,
	int32_t minChange, int32_t maxChange, const CombatParams& params)
{
	Combat2Var var;
	var.minChange = minChange;
	var.maxChange = maxChange;
	CombatFunc(caster, pos, area, params, CombatManaFunc, (void*)&var);
}

void Combat::doCombatCondition(const CreatureP& caster, const Position& pos, const CombatArea* area,
	const CombatParams& params)
{
	CombatFunc(caster, pos, area, params, CombatConditionFunc, nullptr);
}

void Combat::doCombatCondition(const CreatureP& caster, const CreatureP& target, const CombatParams& params)
{
	if(params.isAggressive && (caster == target || Combat::canDoCombat(caster, target) != RET_NOERROR))
		return;

	CombatConditionFunc(caster, target, params, nullptr);
	if(params.targetCallback)
		params.targetCallback->onTargetCombat(caster, target);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(target->getPosition(), params.effects.impact);

	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), target->getPosition(), params.effects.distance);
}

void Combat::doCombatDispel(const CreatureP& caster, const Position& pos, const CombatArea* area,
	const CombatParams& params)
{
	CombatFunc(caster, pos, area, params, CombatDispelFunc, nullptr);
}

void Combat::doCombatDispel(const CreatureP& caster, const CreatureP& target, const CombatParams& params)
{
	if(params.isAggressive && (caster == target || Combat::canDoCombat(caster, target) != RET_NOERROR))
		return;

	CombatDispelFunc(caster, target, params, nullptr);
	if(params.targetCallback)
		params.targetCallback->onTargetCombat(caster, target);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(target->getPosition(), params.effects.impact);

	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), target->getPosition(), params.effects.distance);
}

void Combat::doCombatDefault(const CreatureP& caster, const CreatureP& target, const CombatParams& params)
{
	if(params.isAggressive && (caster == target || Combat::canDoCombat(caster, target) != RET_NOERROR))
		return;

	const SpectatorList& list = server.game().getSpectators(target->getTile()->getPosition());
	CombatNullFunc(caster, target, params, nullptr);

	combatTileEffects(list, caster, target->getTile(), params);
	if(params.targetCallback)
		params.targetCallback->onTargetCombat(caster, target);

	if(params.effects.impact != MAGIC_EFFECT_NONE && (!caster || !caster->isGhost()
		|| server.configManager().getBool(ConfigManager::GHOST_SPELL_EFFECTS)))
		server.game().addMagicEffect(target->getPosition(), params.effects.impact);

	if(caster && params.effects.distance != SHOOT_EFFECT_NONE)
		addDistanceEffect(caster, caster->getPosition(), target->getPosition(), params.effects.distance);
}

//**********************************************************


LOGGER_DEFINITION(ValueCallback);


void ValueCallback::getMinMaxValues(Player* player, int32_t& min, int32_t& max, bool useCharges) const
{
	//"onGetPlayerMinMaxValues"(cid, ...)
	if(!m_interface->reserveEnv())
	{
		LOGe("[ValueCallback::getMinMaxValues] Callstack overflow.");
		return;
	}

	ScriptEnviroment* env = m_interface->getEnv();
	if(!env->setCallbackId(m_scriptId, m_interface))
		return;

	m_interface->pushFunction(m_scriptId);
	lua_State* L = m_interface->getState();
	lua_pushnumber(L, env->addThing(player));

	int32_t parameters = 1;
	switch(type)
	{
		case FORMULA_LEVELMAGIC:
		{
			//"onGetPlayerMinMaxValues"(cid, level, magLevel)
			lua_pushnumber(L, player->getLevel());
			lua_pushnumber(L, player->getMagicLevel());

			parameters += 2;
			break;
		}

		case FORMULA_SKILL:
		{
			//"onGetPlayerMinMaxValues"(cid, level, skill, attack, factor)
			Item* tool = player->getWeapon();
			lua_pushnumber(L, player->getLevel());
			lua_pushnumber(L, player->getWeaponSkill(tool));

			int32_t attack = 7;
			if(tool)
			{
				attack = tool->getAttack();
				if(useCharges && tool->hasCharges() && server.configManager().getBool(ConfigManager::REMOVE_WEAPON_CHARGES))
					server.game().transformItem(tool, tool->getId(), std::max(0, tool->getCharges() - 1));
			}

			lua_pushnumber(L, attack);
			lua_pushnumber(L, player->getAttackFactor());

			parameters += 4;
			break;
		}

		default:
		{
			LOGe("[ValueCallback::getMinMaxValues] Unknown callback type");
			return;
		}
	}

	int32_t params = lua_gettop(L);
	if(!lua_pcall(L, parameters, 2, 0))
	{
		min = LuaScriptInterface::popNumber(L);
		max = LuaScriptInterface::popNumber(L);
		player->increaseCombatValues(min, max, useCharges, type != FORMULA_SKILL);
	}
	else
		LuaScriptInterface::error(nullptr, std::string(LuaScriptInterface::popString(L)));

	if((lua_gettop(L) + parameters + 1) != params)
		LuaScriptInterface::error(__FUNCTION__, "Stack size changed!");

	env->resetCallback();
	m_interface->releaseEnv();
}

//**********************************************************

LOGGER_DEFINITION(TileCallback);


void TileCallback::onTileCombat(const CreatureP& creature, Tile* tile) const
{
	//"onTileCombat"(cid, pos)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(!env->setCallbackId(m_scriptId, m_interface))
			return;

		m_interface->pushFunction(m_scriptId);
		lua_State* L = m_interface->getState();

		lua_pushnumber(L, creature ? env->addThing(creature) : 0);
		m_interface->pushPosition(L, tile->getPosition());

		m_interface->callFunction(2);
		env->resetCallback();
		m_interface->releaseEnv();
	}
	else
		LOGe("[TileCallback::onTileCombat] Call stack overflow.");
}

//**********************************************************

LOGGER_DEFINITION(TargetCallback);


void TargetCallback::onTargetCombat(const CreatureP& creature, const CreatureP& target) const
{
	//"onTargetCombat"(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(!env->setCallbackId(m_scriptId, m_interface))
			return;

		uint32_t cid = 0;
		if(creature)
			cid = env->addThing(creature);

		m_interface->pushFunction(m_scriptId);
		lua_State* L = m_interface->getState();

		lua_pushnumber(L, cid);
		lua_pushnumber(L, env->addThing(target));

		int32_t size = lua_gettop(L);
		if(lua_pcall(L, 2, 0 /*nReturnValues*/, 0) != 0)
			LuaScriptInterface::error(nullptr, std::string(LuaScriptInterface::popString(L)));

		if((lua_gettop(L) + 2 /*nParams*/ + 1) != size)
			LuaScriptInterface::error(__FUNCTION__, "Stack size changed!");

		env->resetCallback();
		m_interface->releaseEnv();
	}
	else
	{
		LOGe("[TargetCallback::onTargetCombat] Call stack overflow.");
		return;
	}
}

// **********************************************************

LOGGER_DEFINITION(CombatArea);

void CombatArea::clear()
{
	for(CombatAreas::iterator it = areas.begin(); it != areas.end(); ++it)
		delete it->second;

	areas.clear();
}

CombatArea::CombatArea(const CombatArea& rhs)
{
	hasExtArea = rhs.hasExtArea;
	for(CombatAreas::const_iterator it = rhs.areas.begin(); it != rhs.areas.end(); ++it)
		areas[it->first] = new MatrixArea(*it->second);
}


bool CombatArea::getList(const Position& origin, const Position& destination, std::list<Tile*>& list) const {
	Game& game = server.game();

	const MatrixArea* area = getArea(origin, destination);
	if (area == nullptr) {
		return false;
	}

	uint16_t sizeX = area->getCols();
	if (sizeX == 0) {
		return true;
	}

	uint16_t sizeY = area->getRows();
	if (sizeY == 0) {
		return true;
	}

	uint16_t centerX;
	uint16_t centerY;
	area->getCenter(centerY, centerX);

	int32_t maxOffsetX = std::min(sizeX - centerX - 1, Position::MAX_X - destination.x);
	int32_t maxOffsetY = std::min(sizeY - centerY - 1, Position::MAX_Y - destination.y);
	int32_t minOffsetX = -std::min(centerX, destination.x);
	int32_t minOffsetY = -std::min(centerY, destination.y);

	for (auto offsetY = minOffsetY; offsetY <= maxOffsetY; ++offsetY) {
		for (auto offsetX = minOffsetX; offsetX <= maxOffsetX; ++offsetX) {
			if (area->getValue(centerY + offsetY, centerX + offsetX) == 0) {
				continue;
			}

			Position position(destination.x + offsetX, destination.y + offsetY, destination.z);
			if (!game.isSightClear(destination, position, true)) {
				continue;
			}

			Tile* tile = game.getTile(position);
			if (tile == nullptr) {
				continue;
			}

			list.push_back(tile);
		}
	}

	return true;
}


void CombatArea::copyArea(const MatrixArea* input, MatrixArea* output, MatrixOperation_t op) const
{
	uint16_t centerY, centerX;
	input->getCenter(centerY, centerX);
	if(op == MATRIXOPERATION_COPY)
	{
		for(uint32_t y = 0; y < input->getRows(); ++y)
		{
			for(uint32_t x = 0; x < input->getCols(); ++x)
				(*output)[y][x] = (*input)[y][x];
		}

		output->setCenter(centerY, centerX);
	}
	else if(op == MATRIXOPERATION_MIRROR)
	{
		for(uint32_t y = 0; y < input->getRows(); ++y)
		{
			int32_t rx = 0;
			for(int32_t x = input->getCols() - 1; x >= 0; --x)
				(*output)[y][rx++] = (*input)[y][x];
		}

		output->setCenter(centerY, (input->getRows() - 1) - centerX);
	}
	else if(op == MATRIXOPERATION_FLIP)
	{
		for(uint32_t x = 0; x < input->getCols(); ++x)
		{
			int32_t ry = 0;
			for(int32_t y = input->getRows() - 1; y >= 0; --y)
				(*output)[ry++][x] = (*input)[y][x];
		}

		output->setCenter((input->getCols() - 1) - centerY, centerX);
	}
	else //rotation
	{
		uint16_t centerX, centerY;
		input->getCenter(centerY, centerX);

		int32_t rotateCenterX = (output->getCols() / 2) - 1, rotateCenterY = (output->getRows() / 2) - 1, angle = 0;
		switch(op)
		{
			case MATRIXOPERATION_ROTATE90:
				angle = 90;
				break;

			case MATRIXOPERATION_ROTATE180:
				angle = 180;
				break;

			case MATRIXOPERATION_ROTATE270:
				angle = 270;
				break;

			default:
				angle = 0;
				break;
		}

		double angleRad = 3.1416 * angle / 180.0;
		float a = std::cos(angleRad), b = -std::sin(angleRad);
		float c = std::sin(angleRad), d = std::cos(angleRad);

		for(int32_t x = 0; x < (long)input->getCols(); ++x)
		{
			for(int32_t y = 0; y < (long)input->getRows(); ++y)
			{
				//calculate new coordinates using rotation center
				int32_t newX = x - centerX, newY = y - centerY,
					rotatedX = round(newX * a + newY * b),
					rotatedY = round(newX * c + newY * d);
				//write in the output matrix using rotated coordinates
				(*output)[rotatedY + rotateCenterY][rotatedX + rotateCenterX] = (*input)[y][x];
			}
		}

		output->setCenter(rotateCenterY, rotateCenterX);
	}
}

MatrixArea* CombatArea::createArea(const std::list<uint32_t>& list, uint32_t rows)
{
	uint32_t cols = list.size() / rows;
	MatrixArea* area = new MatrixArea(rows, cols);

	uint16_t x = 0, y = 0;
	for(std::list<uint32_t>::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if(*it == 1 || *it == 3)
			area->setValue(y, x, true);

		if(*it == 2 || *it == 3)
			area->setCenter(y, x);

		++x;
		if(cols != x)
			continue;

		x = 0;
		++y;
	}

	return area;
}

void CombatArea::setupArea(const std::list<uint32_t>& list, uint32_t rows)
{
	//NORTH
	MatrixArea* area = createArea(list, rows);
	areas[Direction::NORTH] = area;
	uint32_t maxOutput = std::max(area->getCols(), area->getRows()) * 2;

	//SOUTH
	MatrixArea* southArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(area, southArea, MATRIXOPERATION_ROTATE180);
	areas[Direction::SOUTH] = southArea;

	//EAST
	MatrixArea* eastArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(area, eastArea, MATRIXOPERATION_ROTATE90);
	areas[Direction::EAST] = eastArea;

	//WEST
	MatrixArea* westArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(area, westArea, MATRIXOPERATION_ROTATE270);
	areas[Direction::WEST] = westArea;
}

void CombatArea::setupArea(int32_t length, int32_t spread)
{
	std::list<uint32_t> list;
	uint32_t rows = length;

	int32_t cols = 1;
	if(spread != 0)
		cols = ((length - length % spread) / spread) * 2 + 1;

	int32_t colSpread = cols;
	for(uint32_t y = 1; y <= rows; ++y)
	{
		int32_t mincol = cols - colSpread + 1, maxcol = cols - (cols - colSpread);
		for(int32_t x = 1; x <= cols; ++x)
		{
			if(y == rows && x == ((cols - cols % 2) / 2) + 1)
				list.push_back(3);
			else if(x >= mincol && x <= maxcol)
				list.push_back(1);
			else
				list.push_back(0);
		}

		if(spread > 0 && y % spread == 0)
			--colSpread;
	}

	setupArea(list, rows);
}

void CombatArea::setupArea(int32_t radius)
{
	const int32_t area[13][13] =
	{
		{0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 8, 8, 7, 8, 8, 0, 0, 0, 0},
		{0, 0, 0, 8, 7, 6, 6, 6, 7, 8, 0, 0, 0},
		{0, 0, 8, 7, 6, 5, 5, 5, 6, 7, 8, 0, 0},
		{0, 8, 7, 6, 5, 4, 4, 4, 5, 6, 7, 8, 0},
		{0, 8, 6, 5, 4, 3, 2, 3, 4, 5, 6, 8, 0},
		{8, 7, 6, 5, 4, 2, 1, 2, 4, 5, 6, 7, 8},
		{0, 8, 6, 5, 4, 3, 2, 3, 4, 5, 6, 8, 0},
		{0, 8, 7, 6, 5, 4, 4, 4, 5, 6, 7, 8, 0},
		{0, 0, 8, 7, 6, 5, 5, 5, 6, 7, 8, 0, 0},
		{0, 0, 0, 8, 7, 6, 6, 6, 7, 8, 0, 0, 0},
		{0, 0, 0, 0, 8, 8, 7, 8, 8, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0}
	};

	std::list<uint32_t> list;
	for(int32_t y = 0; y < 13; ++y)
	{
		for(int32_t x = 0; x < 13; ++x)
		{
			if(area[y][x] == 1)
				list.push_back(3);
			else if(area[y][x] > 0 && area[y][x] <= radius)
				list.push_back(1);
			else
				list.push_back(0);
		}
	}

	setupArea(list, 13);
}

void CombatArea::setupExtArea(const std::list<uint32_t>& list, uint32_t rows)
{
	if(list.empty())
		return;

	//NORTH-WEST
	MatrixArea* area = createArea(list, rows);
	areas[Direction::NORTH_WEST] = area;
	uint32_t maxOutput = std::max(area->getCols(), area->getRows()) * 2;

	//NORTH-EAST
	MatrixArea* neArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(area, neArea, MATRIXOPERATION_MIRROR);
	areas[Direction::NORTH_EAST] = neArea;

	//SOUTH-WEST
	MatrixArea* swArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(area, swArea, MATRIXOPERATION_FLIP);
	areas[Direction::SOUTH_WEST] = swArea;

	//SOUTH-EAST
	MatrixArea* seArea = new MatrixArea(maxOutput, maxOutput);
	copyArea(swArea, seArea, MATRIXOPERATION_MIRROR);
	areas[Direction::SOUTH_EAST] = seArea;

	hasExtArea = true;
}

// **********************************************************

MagicField::ClassAttributesP MagicField::getClassAttributes() {
	return Item::getClassAttributes();
}


const std::string& MagicField::getClassName() {
	static const std::string name("Magic Field");
	return name;
}


bool MagicField::isBlocking(const Creature* creature) const
{
	if(getId() != ITEM_MAGICWALL_SAFE && getId() != ITEM_WILDGROWTH_SAFE)
		return Item::isBlocking(creature);

	return !creature || !creature->getPlayer();
}

void MagicField::onStepInField(const CreatureP& creature)
{
	if(getId() == ITEM_MAGICWALL_SAFE || getId() == ITEM_WILDGROWTH_SAFE || isBlocking(creature.get()))
	{
		if(!creature->isGhost())
			server.game().internalRemoveItem(creature.get(), this, 1);

		return;
	}

	if(!getKind()->condition)
		return;

	Condition* condition = getKind()->condition->clone();
	uint32_t ownerId = getOwner();
	if(ownerId && !getTile()->hasFlag(TILESTATE_PVPZONE))
	{
		if(Creature* owner = server.game().getCreatureByID(ownerId))
		{
			bool harmful = true;
			if((server.game().getWorldType() == WORLD_TYPE_NO_PVP || getTile()->hasFlag(TILESTATE_NOPVPZONE))
				&& (owner->getPlayer() || owner->hasController()))
				harmful = false;
			else if(Player* targetPlayer = creature->getPlayer())
			{
				if(owner->getPlayer() && Combat::isProtected(owner->getPlayer(), targetPlayer))
					harmful = false;
			}

			if(!harmful || (OTSYS_TIME() - createTime) <= (uint32_t)server.configManager().getNumber(
				ConfigManager::FIELD_OWNERSHIP) || creature->hasBeenAttacked(ownerId))
				condition->setParam(CONDITIONPARAM_OWNER, ownerId);
		}
	}

	creature->addCondition(condition);
}


bool MagicField::isReplaceable() const {return getKind()->replaceable;}


CombatType_t MagicField::getCombatType() const
{
	return getKind()->combatType;
}
