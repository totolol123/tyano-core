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

#include "creature.h"
#include "player.h"
#include "npc.h"
#include "monster.h"

#include "condition.h"
#include "combat.h"

#include "container.h"

#include "configmanager.h"
#include "creatureevent.h"
#include "dispatcher.h"
#include "game.h"
#include "items.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"

boost::recursive_mutex AutoId::lock;
uint32_t AutoId::count = 1000;
AutoId::List AutoId::list;


LOGGER_DEFINITION(Creature);


Creature::Creature()
{
	id = 0;
	_tile = nullptr;
	direction = SOUTH;
	master = nullptr;
	lootDrop = LOOT_DROP_FULL;
	skillLoss = true;
	hideName = hideHealth = cannotMove = false;
	speakType = SPEAK_CLASS_NONE;
	skull = SKULL_NONE;
	partyShield = SHIELD_NONE;

	health = 1000;
	healthMax = 1000;
	mana = 0;
	manaMax = 0;

	lastStep = 0;
	lastStepCost = 1;
	baseSpeed = 220;
	varSpeed = 0;

	masterRadius = -1;
	masterPosition = Position();

	followCreature = nullptr;
	hasFollowPath = false;
	removed = false;
	eventWalk = 0;
	forceUpdateFollowPath = false;
	isUpdatingPath = false;
	checked = false;

	attackedCreature = nullptr;
	lastHitCreature = 0;
	lastDamageSource = COMBAT_NONE;
	blockCount = 0;
	blockTicks = 0;
	walkUpdateTicks = 0;
	checkVector = -1;

	scriptEventsBitField = 0;
	onIdleStatus();
}

Creature::~Creature() {
	attackedCreature = nullptr;

	for(CreatureList::iterator cit = summons.begin(); cit != summons.end(); ++cit)
	{
		(*cit)->setAttackedCreature(nullptr);
		(*cit)->setMaster(nullptr);
	}

	for(ConditionList::iterator it = conditions.begin(); it != conditions.end(); ++it) {
		delete *it;
	}
}

bool Creature::canSee(const Position& myPos, const Position& pos, uint32_t viewRangeX, uint32_t viewRangeY)
{
	if(myPos.z <= 7)
	{
		//we are on ground level or above (7 -> 0)
		//view is from 7 -> 0
		if(pos.z > 7)
			return false;
	}
	else if(myPos.z >= 8)
	{
		//we are underground (8 -> 15)
		//view is +/- 2 from the floor we stand on
		if(std::abs(myPos.z - pos.z) > 2)
			return false;
	}

	int32_t offsetz = myPos.z - pos.z;
	return (((uint32_t)pos.x >= myPos.x - viewRangeX + offsetz) && ((uint32_t)pos.x <= myPos.x + viewRangeX + offsetz) &&
		((uint32_t)pos.y >= myPos.y - viewRangeY + offsetz) && ((uint32_t)pos.y <= myPos.y + viewRangeY + offsetz));
}

bool Creature::canSee(const Position& pos) const
{
	return canSee(getPosition(), pos, Map::maxViewportX, Map::maxViewportY);
}

bool Creature::canSeeCreature(const Creature* creature) const
{
	return creature == this || (!creature->isGhost() && (!creature->isInvisible() || canSeeInvisibility()));
}

int64_t Creature::getTimeSinceLastMove() const
{
	if(lastStep)
		return OTSYS_TIME() - lastStep;

	return 0x7FFFFFFFFFFFFFFFLL;
}

int32_t Creature::getWalkDelay(Direction dir) const
{
	if(lastStep)
		return getStepDuration(dir) - (OTSYS_TIME() - lastStep);

	return 0;
}

int32_t Creature::getWalkDelay() const
{
	if(lastStep)
		return getStepDuration() - (OTSYS_TIME() - lastStep);

	return 0;
}

void Creature::onThink(uint32_t interval)
{
	if(followCreature && master != followCreature && !canSeeCreature(followCreature))
		internalCreatureDisappear(followCreature, false);

	if(attackedCreature && master != attackedCreature && !canSeeCreature(attackedCreature))
		internalCreatureDisappear(attackedCreature, false);

	blockTicks += interval;
	if(blockTicks >= 1000)
	{
		blockCount = std::min((uint32_t)blockCount + 1, (uint32_t)2);
		blockTicks = 0;
	}

	if(followCreature)
	{
		walkUpdateTicks += interval;
		if(forceUpdateFollowPath || walkUpdateTicks >= 2000)
		{
			walkUpdateTicks = 0;
			forceUpdateFollowPath = false;
			isUpdatingPath = true;
		}
	}

	if(isUpdatingPath)
	{
		isUpdatingPath = false;
		getPathToFollowCreature();
	}

	onAttacking(interval);
	executeConditions(interval);

	CreatureEventList thinkEvents = getCreatureEvents(CREATURE_EVENT_THINK);
	for(CreatureEventList::iterator it = thinkEvents.begin(); it != thinkEvents.end(); ++it)
		(*it)->executeThink(this, interval);
}

void Creature::onAttacking(uint32_t interval)
{
	if(!attackedCreature)
		return;

	CreatureEventList attackEvents = getCreatureEvents(CREATURE_EVENT_ATTACK);
	for(CreatureEventList::iterator it = attackEvents.begin(); it != attackEvents.end(); ++it)
	{
		if(!(*it)->executeAttack(this, attackedCreature) && attackedCreature)
			setAttackedCreature(nullptr);
	}

	if(!attackedCreature)
		return;

	onAttacked();
	attackedCreature->onAttacked();
	if(server.game().isSightClear(getPosition(), attackedCreature->getPosition(), true))
		doAttacking(interval);
}

void Creature::onWalk()
{
	if(getWalkDelay() <= 0)
	{
		Direction dir;
		uint32_t flags = FLAG_IGNOREFIELDDAMAGE;
		if(getNextStep(dir, flags) && server.game().internalMoveCreature(this, dir, flags) != RET_NOERROR)
			forceUpdateFollowPath = true;
	}

	if(listWalkDir.empty())
		onWalkComplete();

	if(eventWalk)
	{
		eventWalk = 0;
		addEventWalk();
	}
}

void Creature::onWalk(Direction& dir)
{
	if(!hasCondition(CONDITION_DRUNK))
		return;

	uint32_t r = random_range(0, 16);
	if(r > 4)
		return;

	switch(r)
	{
		case 0:
			dir = NORTH;
			break;
		case 1:
			dir = WEST;
			break;
		case 3:
			dir = SOUTH;
			break;
		case 4:
			dir = EAST;
			break;
	}

	server.game().internalCreatureSay(this, SPEAK_MONSTER_SAY, "Hicks!", isGhost());
}

bool Creature::getNextStep(Direction& dir, uint32_t& flags)
{
	if(listWalkDir.empty())
		return false;

	dir = listWalkDir.front();
	listWalkDir.pop_front();
	onWalk(dir);
	return true;
}

bool Creature::startAutoWalk(std::list<Direction>& listDir)
{
	if(getPlayer() && getPlayer()->getNoMove())
	{
		getPlayer()->sendCancelWalk();
		return false;
	}

	listWalkDir = listDir;
	addEventWalk();
	return true;
}

void Creature::addEventWalk()
{
	if(eventWalk)
		return;

	int64_t ticks = getEventStepTicks();
	if(ticks > 0)
		eventWalk = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(ticks),
			std::bind(&Game::checkCreatureWalk, &server.game(), getID())));
}

void Creature::stopEventWalk()
{
	if(!eventWalk)
		return;

	server.scheduler().cancelTask(eventWalk);
	eventWalk = 0;
	if(!listWalkDir.empty())
	{
		listWalkDir.clear();
		onWalkAborted();
	}
}

void Creature::internalCreatureDisappear(const Creature* creature, bool isLogout)
{
	if(attackedCreature == creature)
	{
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(isLogout);
	}

	if(followCreature == creature)
	{
		setFollowCreature(nullptr);
		onFollowCreatureDisappear(isLogout);
	}
}


void Creature::onAddTileItem(const Tile* tile, const Position& pos, const Item* item)
{
}

void Creature::onUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem,
		const ItemKindPC& oldType, const Item* newItem, const ItemKindPC& newType)
{
}

void Creature::onRemoveTileItem(const Tile* tile, const Position& pos, const ItemKindPC& iType, const Item* item)
{
}

void Creature::onCreatureAppear(const Creature* creature)
{
}

void Creature::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	internalCreatureDisappear(creature, true);
}

void Creature::onRemovedCreature()
{
	setRemoved();
	removeList();
	if(master && !master->isRemoved())
		master->removeSummon(this);
}


void Creature::setParent(Cylinder* cylinder) {
	assert(!removed);

	Thing::setParent(cylinder);

	_tile = dynamic_cast<Tile*>(getParent());
}


void Creature::setRemoved() {
	assert(!removed);
	assert(_tile == nullptr);

	removed = true;
}


void Creature::onChangeZone(ZoneType_t zone)
{
	if(attackedCreature && zone == ZONE_PROTECTION)
		internalCreatureDisappear(attackedCreature, false);
}

void Creature::onAttackedCreatureChangeZone(ZoneType_t zone)
{
	if(zone == ZONE_PROTECTION)
		internalCreatureDisappear(attackedCreature, false);
}

void Creature::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	if(creature == this)
	{
		lastStep = OTSYS_TIME();
		lastStepCost = 1;

		setLastPosition(oldPos);
		if(!teleport)
		{
			if(oldPos.z != newPos.z || (std::abs(newPos.x - oldPos.x) >= 1 && std::abs(newPos.y - oldPos.y) >= 1))
				lastStepCost = 2;
		}
		else
			stopEventWalk();

		if(!summons.empty())
		{
			CreatureList::iterator cit;
			std::list<Creature*> despawnList;
			for(cit = summons.begin(); cit != summons.end(); ++cit)
			{
				const Position pos = (*cit)->getPosition();
				if((std::abs(pos.z - newPos.z) > 2) || (std::max(std::abs((
					newPos.x) - pos.x), std::abs((newPos.y - 1) - pos.y)) > 30))
					despawnList.push_back((*cit).get());
			}

			for(std::list<Creature*>::iterator cit = despawnList.begin(); cit != despawnList.end(); ++cit)
				server.game().removeCreature(*cit, true);
		}

		if(newTile->getZone() != oldTile->getZone())
			onChangeZone(getZone());
	}

	if(creature == followCreature || (creature == this && followCreature))
	{
		if(hasFollowPath)
		{
			isUpdatingPath = true;
			server.dispatcher().addTask(Task::create(
				std::bind(&Game::updateCreatureWalk, &server.game(), getID())));
		}

		if(newPos.z != oldPos.z || !canSee(followCreature->getPosition()))
			internalCreatureDisappear(followCreature, false);
	}

	if(creature == attackedCreature || (creature == this && attackedCreature))
	{
		if(newPos.z == oldPos.z && canSee(attackedCreature->getPosition()))
		{
			if(hasExtraSwing()) //our target is moving lets see if we can get in hit
				server.dispatcher().addTask(Task::create(
					std::bind(&Game::checkCreatureAttack, &server.game(), getID())));

			if(newTile->getZone() != oldTile->getZone())
				onAttackedCreatureChangeZone(attackedCreature->getZone());
		}
		else
			internalCreatureDisappear(attackedCreature, false);
	}
}

bool Creature::onDeath()
{
	DeathList deathList = getKillers();
	bool deny = false;

	CreatureEventList prepareDeathEvents = getCreatureEvents(CREATURE_EVENT_PREPAREDEATH);
	for(CreatureEventList::iterator it = prepareDeathEvents.begin(); it != prepareDeathEvents.end(); ++it)
	{
		if(!(*it)->executePrepareDeath(this, deathList) && !deny)
			deny = true;
	}

	if(deny)
		return false;

	int32_t i = 0, size = deathList.size(), limit = server.configManager().getNumber(ConfigManager::DEATH_ASSISTS) + 1;
	if(limit > 0 && size > limit)
		size = limit;

	Creature* tmp = nullptr;
	CreatureVector justifyVec;
	for(DeathList::iterator it = deathList.begin(); it != deathList.end(); ++it, ++i)
	{
		if(it->isNameKill())
			continue;

		bool lastHit = it == deathList.begin();
		uint32_t flags = KILLFLAG_NONE;
		if(lastHit)
			flags |= (uint32_t)KILLFLAG_LASTHIT;

		if(i < size)
		{
			if(it->getKillerCreature()->getPlayer())
				tmp = it->getKillerCreature();
			else if(it->getKillerCreature()->getPlayerMaster())
				tmp = it->getKillerCreature()->getMaster();
		}

		if(tmp)
		{
			if(std::find(justifyVec.begin(), justifyVec.end(), tmp) == justifyVec.end())
			{
				flags |= (uint32_t)KILLFLAG_JUSTIFY;
				justifyVec.push_back(tmp);
			}

			tmp = nullptr;
		}

		if(!it->getKillerCreature()->onKilledCreature(this, flags) && lastHit)
			return false;

		if(hasBitSet((uint32_t)KILLFLAG_UNJUSTIFIED, flags))
			it->setUnjustified(true);
	}

	for(CountMap::iterator it = damageMap.begin(); it != damageMap.end(); ++it)
	{
		if((tmp = server.game().getCreatureByID(it->first)))
			tmp->onAttackedCreatureKilled(this);
	}

	dropCorpse(deathList);
	if(master)
		master->removeSummon(this);

	return true;
}

void Creature::dropCorpse(DeathList deathList)
{
	boost::intrusive_ptr<Item> corpse = createCorpse(deathList);
	if(corpse)
		corpse->setParent(&VirtualCylinder::virtualCylinder);

	bool deny = false;
	CreatureEventList deathEvents = getCreatureEvents(CREATURE_EVENT_DEATH);
	for(CreatureEventList::iterator it = deathEvents.begin(); it != deathEvents.end(); ++it)
	{
		if(!(*it)->executeDeath(this, corpse.get(), deathList) && !deny)
			deny = true;
	}

	if(!corpse)
		return;

	corpse->setParent(nullptr);
	if(deny)
		return;

	Tile* tile = getTile();
	if(!tile)
		return;

	boost::intrusive_ptr<Item> splash;
	switch(getRace())
	{
		case RACE_VENOM:
			splash = Item::CreateItem(ITEM_FULLSPLASH, FLUID_GREEN);
			break;

		case RACE_BLOOD:
			splash = Item::CreateItem(ITEM_FULLSPLASH, FLUID_BLOOD);
			break;

		default:
			break;
	}

	if(splash)
	{
		server.game().internalAddItem(nullptr, tile, splash.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
		server.game().startDecay(splash.get());
	}

	server.game().internalAddItem(nullptr, tile, corpse.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
	dropLoot(corpse->getContainer());
	server.game().startDecay(corpse.get());
}

DeathList Creature::getKillers()
{
	DeathList list;
	Creature* lhc = nullptr;
	if(!(lhc = server.game().getCreatureByID(lastHitCreature)))
		list.push_back(DeathEntry(getCombatName(lastDamageSource), 0));
	else
		list.push_back(DeathEntry(lhc, 0));

	int32_t requiredTime = server.configManager().getNumber(ConfigManager::DEATHLIST_REQUIRED_TIME);
	int64_t now = OTSYS_TIME();

	CountBlock_t cb;
	for(CountMap::const_iterator it = damageMap.begin(); it != damageMap.end(); ++it)
	{
		cb = it->second;
		if((now - cb.ticks) > requiredTime)
			continue;

		Creature* mdc = server.game().getCreatureByID(it->first);
		if(!mdc || mdc == lhc || (lhc && (mdc->getMaster() == lhc || lhc->getMaster() == mdc)))
			continue;

		bool deny = false;
		for(DeathList::iterator fit = list.begin(); fit != list.end(); ++fit)
		{
			if(fit->isNameKill())
				continue;

			Creature* tmp = fit->getKillerCreature();
			if(!(mdc->getName() == tmp->getName() && mdc->getMaster() == tmp->getMaster()) &&
				(!mdc->getMaster() || (mdc->getMaster() != tmp && mdc->getMaster() != tmp->getMaster()))
				&& (mdc->getSummonCount() <= 0 || tmp->getMaster() != mdc))
				continue;

			deny = true;
			break;
		}

		if(!deny)
			list.push_back(DeathEntry(mdc, cb.total));
	}

	if(list.size() > 1)
		std::sort(list.begin() + 1, list.end(), DeathLessThan());

	return list;
}

bool Creature::hasBeenAttacked(uint32_t attackerId) const
{
	CountMap::const_iterator it = damageMap.find(attackerId);
	if(it != damageMap.end())
		return (OTSYS_TIME() - it->second.ticks) <= server.configManager().getNumber(ConfigManager::PZ_LOCKED);

	return false;
}

boost::intrusive_ptr<Item> Creature::createCorpse(DeathList deathList)
{
	return Item::CreateItem(getLookCorpse());
}

void Creature::changeHealth(int32_t healthChange)
{
	if(healthChange > 0)
		health += std::min(healthChange, getMaxHealth() - health);
	else
		health = std::max((int32_t)0, health + healthChange);

	server.game().addCreatureHealth(this);
}

void Creature::changeMana(int32_t manaChange)
{
	if(manaChange > 0)
		mana += std::min(manaChange, getMaxMana() - mana);
	else
		mana = std::max((int32_t)0, mana + manaChange);
}

bool Creature::getStorage(const uint32_t key, std::string& value) const
{
	StorageMap::const_iterator it = storageMap.find(key);
	if(it != storageMap.end())
	{
		value = it->second;
		return true;
	}

	value = "-1";
	return false;
}

bool Creature::setStorage(const uint32_t key, const std::string& value)
{
	storageMap[key] = value;
	return true;
}

void Creature::gainHealth(Creature* caster, int32_t healthGain)
{
	if(healthGain > 0)
	{
		int32_t prevHealth = getHealth();
		changeHealth(healthGain);

		int32_t effectiveGain = getHealth() - prevHealth;
		if(caster)
			caster->onTargetCreatureGainHealth(this, effectiveGain);
	}
	else
		changeHealth(healthGain);
}

void Creature::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	lastDamageSource = combatType;
	onAttacked();

	changeHealth(-damage);
	if(attacker)
		attacker->onAttackedCreatureDrainHealth(this, damage);
}

void Creature::drainMana(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	lastDamageSource = combatType;
	onAttacked();

	changeMana(-damage);
	if(attacker)
		attacker->onAttackedCreatureDrainMana(this, damage);
}

BlockType_t Creature::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense/* = false*/, bool checkArmor/* = false*/)
{
	BlockType_t blockType = BLOCK_NONE;
	if(isImmune(combatType))
	{
		damage = 0;
		blockType = BLOCK_IMMUNITY;
	}
	else if(checkDefense || checkArmor)
	{
		bool hasDefense = false;
		if(blockCount > 0)
		{
			--blockCount;
			hasDefense = true;
		}

		if(checkDefense && hasDefense)
		{
			int32_t maxDefense = getDefense(), minDefense = maxDefense / 2;
			damage -= random_range(minDefense, maxDefense);
			if(damage <= 0)
			{
				damage = 0;
				blockType = BLOCK_DEFENSE;
				checkArmor = false;
			}
		}

		if(checkArmor)
		{
			int32_t armorValue = getArmor(), minArmorReduction = 0,
				maxArmorReduction = 0;
			if(armorValue > 1)
			{
				minArmorReduction = (int32_t)std::ceil(armorValue * 0.475);
				maxArmorReduction = (int32_t)std::ceil(
					((armorValue * 0.475) - 1) + minArmorReduction);
			}
			else if(armorValue == 1)
			{
				minArmorReduction = 1;
				maxArmorReduction = 1;
			}

			damage -= random_range(minArmorReduction, maxArmorReduction);
			if(damage <= 0)
			{
				damage = 0;
				blockType = BLOCK_ARMOR;
			}
		}

		if(hasDefense && blockType != BLOCK_NONE)
			onBlockHit(blockType);
	}

	if(attacker)
	{
		attacker->onAttackedCreature(this);
		attacker->onAttackedCreatureBlockHit(this, blockType);
	}

	onAttacked();
	return blockType;
}

bool Creature::setAttackedCreature(Creature* creature)
{
	if(creature)
	{
		const Position& creaturePos = creature->getPosition();
		if(creaturePos.z != getPosition().z || !canSee(creaturePos))
		{
			attackedCreature = nullptr;
			return false;
		}
	}

	attackedCreature = creature;
	if(attackedCreature)
	{
		onAttackedCreature(attackedCreature);
		attackedCreature->onAttacked();
	}

	for(CreatureList::iterator cit = summons.begin(); cit != summons.end(); ++cit)
		(*cit)->setAttackedCreature(creature);

	return true;
}

void Creature::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	fpp.fullPathSearch = !hasFollowPath;
	fpp.clearSight = true;
	fpp.maxSearchDist = 12;
	fpp.minTargetDist = fpp.maxTargetDist = 1;
}

void Creature::getPathToFollowCreature()
{
	if(followCreature)
	{
		FindPathParams fpp;
		getPathSearchParams(followCreature, fpp);
		if(server.game().getPathToEx(this, followCreature->getPosition(), listWalkDir, fpp))
		{
			hasFollowPath = true;
			startAutoWalk(listWalkDir);
		}
		else
			hasFollowPath = false;
	}

	onFollowCreatureComplete(followCreature);
}


Position Creature::getPosition() const {
	if (_tile == nullptr) {
		return lastPosition;
	}

	return _tile->getPosition();
}


bool Creature::setFollowCreature(Creature* creature, bool fullPathSearch /*= false*/)
{
	if(creature)
	{
		if(followCreature == creature)
			return true;

		const Position& creaturePos = creature->getPosition();
		if(creaturePos.z != getPosition().z || !canSee(creaturePos))
		{
			followCreature = nullptr;
			return false;
		}

		if(!listWalkDir.empty())
		{
			listWalkDir.clear();
			onWalkAborted();
		}

		hasFollowPath = forceUpdateFollowPath = false;
		followCreature = creature;
		isUpdatingPath = true;
	}
	else
	{
		isUpdatingPath = false;
		followCreature = nullptr;
	}

	onFollowCreature(creature);
	return true;
}

double Creature::getDamageRatio(Creature* attacker) const
{
	double totalDamage = 0, attackerDamage = 0;
	for(CountMap::const_iterator it = damageMap.begin(); it != damageMap.end(); ++it)
	{
		totalDamage += it->second.total;
		if(it->first == attacker->getID())
			attackerDamage += it->second.total;
	}

	return attackerDamage / totalDamage;
}

void Creature::addDamagePoints(Creature* attacker, int32_t damagePoints)
{
	uint32_t attackerId = 0;
	if(attacker)
		attackerId = attacker->getID();

	CountMap::iterator it = damageMap.find(attackerId);
	if(it != damageMap.end())
	{
		it->second.ticks = OTSYS_TIME();
		if(damagePoints > 0)
			it->second.total += damagePoints;
	}
	else
		damageMap[attackerId] = CountBlock_t(damagePoints);

	if(damagePoints > 0)
		lastHitCreature = attackerId;
}

void Creature::addHealPoints(Creature* caster, int32_t healthPoints)
{
	if(healthPoints <= 0)
		return;

	uint32_t casterId = 0;
	if(caster)
		casterId = caster->getID();

	CountMap::iterator it = healMap.find(casterId);
	if(it != healMap.end())
	{
		it->second.ticks = OTSYS_TIME();
		it->second.total += healthPoints;
	}
	else
		healMap[casterId] = CountBlock_t(healthPoints);
}

void Creature::onAddCondition(ConditionType_t type, bool hadCondition)
{
	if(type == CONDITION_INVISIBLE)
	{
		if(!hadCondition)
			server.game().internalCreatureChangeVisible(this, VISIBLE_DISAPPEAR);
	}
	else if(type == CONDITION_PARALYZE)
	{
		if(hasCondition(CONDITION_HASTE))
			removeCondition(CONDITION_HASTE);
	}
	else if(type == CONDITION_HASTE)
	{
		if(hasCondition(CONDITION_PARALYZE))
			removeCondition(CONDITION_PARALYZE);
	}
}

void Creature::onEndCondition(ConditionType_t type)
{
	if(type == CONDITION_INVISIBLE && !hasCondition(CONDITION_INVISIBLE, -1, false))
		server.game().internalCreatureChangeVisible(this, VISIBLE_APPEAR);
}

void Creature::onTickCondition(ConditionType_t type, int32_t interval, bool& _remove)
{
	if(const MagicField* field = getTile()->getFieldItem())
	{
		switch(type)
		{
			case CONDITION_FIRE:
				_remove = field->getCombatType() != COMBAT_FIREDAMAGE;
				break;
			case CONDITION_ENERGY:
				_remove = field->getCombatType() != COMBAT_ENERGYDAMAGE;
				break;
			case CONDITION_POISON:
				_remove = field->getCombatType() != COMBAT_EARTHDAMAGE;
				break;
			case CONDITION_DROWN:
				_remove = field->getCombatType() != COMBAT_DROWNDAMAGE;
				break;
			default:
				break;
		}
	}
}

void Creature::onCombatRemoveCondition(const Creature* attacker, Condition* condition)
{
	removeCondition(condition);
}

void Creature::onIdleStatus()
{
	if(getHealth() > 0)
	{
		healMap.clear();
		damageMap.clear();
	}
}

void Creature::onAttackedCreatureDrainHealth(Creature* target, int32_t points)
{
	onAttackedCreatureDrain(target, points);
}

void Creature::onAttackedCreatureDrainMana(Creature* target, int32_t points)
{
	onAttackedCreatureDrain(target, points);
}

void Creature::onAttackedCreatureDrain(Creature* target, int32_t points)
{
	target->addDamagePoints(this, points);
}

void Creature::onTargetCreatureGainHealth(Creature* target, int32_t points)
{
	target->addHealPoints(this, points);
}

void Creature::onAttackedCreatureKilled(Creature* target)
{
	if(target == this)
		return;

	double gainExp = target->getGainedExperience(this);
	onGainExperience(gainExp, !target->getPlayer(), false);
}

bool Creature::onKilledCreature(Creature* target, uint32_t& flags)
{
	bool ret = true;
	if(master)
		ret = master->onKilledCreature(target, flags);

	CreatureEventList killEvents = getCreatureEvents(CREATURE_EVENT_KILL);
	if(!hasBitSet((uint32_t)KILLFLAG_LASTHIT, flags))
	{
		for(CreatureEventList::iterator it = killEvents.begin(); it != killEvents.end(); ++it)
			(*it)->executeKill(this, target, false);

		return true;
	}

	for(CreatureEventList::iterator it = killEvents.begin(); it != killEvents.end(); ++it)
	{
		if(!(*it)->executeKill(this, target, true) && ret)
			ret = false;
	}

	return ret;
}

void Creature::onGainExperience(double& gainExp, bool fromMonster, bool multiplied)
{
	if(gainExp <= 0)
		return;

	if(master)
	{
		gainExp = gainExp / 2;
		master->onGainExperience(gainExp, fromMonster, multiplied);
	}
	else if(!multiplied)
		gainExp *= server.configManager().getDouble(ConfigManager::RATE_EXPERIENCE);

	int16_t color = server.configManager().getNumber(ConfigManager::EXPERIENCE_COLOR);
	if(color < 0)
		color = random_range(0, 255);

	std::stringstream ss;
	ss << (uint64_t)gainExp;
	server.game().addAnimatedText(getPosition(), (uint8_t)color, ss.str());
}

void Creature::onGainSharedExperience(double& gainExp, bool fromMonster, bool multiplied)
{
	if(gainExp <= 0)
		return;

	if(master)
	{
		gainExp = gainExp / 2;
		master->onGainSharedExperience(gainExp, fromMonster, multiplied);
	}
	else if(!multiplied)
		gainExp *= server.configManager().getDouble(ConfigManager::RATE_EXPERIENCE);

	int16_t color = server.configManager().getNumber(ConfigManager::EXPERIENCE_COLOR);
	if(color < 0)
		color = random_range(0, 255);

	std::stringstream ss;
	ss << (uint64_t)gainExp;
	server.game().addAnimatedText(getPosition(), (uint8_t)color, ss.str());
}

void Creature::addSummon(Creature* creature)
{
	creature->setDropLoot(LOOT_DROP_NONE);
	creature->setLossSkill(false);

	creature->setMaster(this);
	summons.push_back(creature);
}

void Creature::removeSummon(const Creature* creature)
{
	CreatureList::iterator it = std::find(summons.begin(), summons.end(), creature);
	if(it == summons.end())
		return;

	(*it)->setMaster(nullptr);
	server.game().autorelease(*it);
	summons.erase(it);
}

void Creature::destroySummons()
{
	for(CreatureList::iterator it = summons.begin(); it != summons.end(); ++it)
	{
		(*it)->setAttackedCreature(nullptr);
		(*it)->changeHealth(-(*it)->getHealth());

		(*it)->setMaster(nullptr);

		server.game().autorelease(*it);
	}

	summons.clear();
}

bool Creature::addCondition(Condition* condition)
{
	if(!condition)
		return false;

	bool hadCondition = hasCondition(condition->getType(), -1, false);
	if(Condition* previous = getCondition(condition->getType(), condition->getId(), condition->getSubId()))
	{
		previous->addCondition(this, condition);
		delete condition;
		return true;
	}

	if(condition->startCondition(this))
	{
		conditions.push_back(condition);
		onAddCondition(condition->getType(), hadCondition);
		return true;
	}

	delete condition;
	return false;
}

bool Creature::addCombatCondition(Condition* condition)
{
	bool hadCondition = hasCondition(condition->getType(), -1, false);
	//Caution: condition variable could be deleted after the call to addCondition
	ConditionType_t type = condition->getType();
	if(!addCondition(condition))
		return false;

	onAddCombatCondition(type, hadCondition);
	return true;
}

void Creature::removeCondition(ConditionType_t type)
{
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end();)
	{
		if((*it)->getType() != type)
		{
			++it;
			continue;
		}

		Condition* condition = *it;
		it = conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(condition->getType());
		delete condition;
	}
}

void Creature::removeCondition(ConditionType_t type, ConditionId_t id)
{
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end();)
	{
		if((*it)->getType() != type || (*it)->getId() != id)
		{
			++it;
			continue;
		}

		Condition* condition = *it;
		it = conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(condition->getType());
		delete condition;
	}
}

void Creature::removeCondition(Condition* condition)
{
	ConditionList::iterator it = std::find(conditions.begin(), conditions.end(), condition);
	if(it != conditions.end())
	{
		Condition* condition = *it;
		it = conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_ABORT);
		onEndCondition(condition->getType());
		delete condition;
	}
}

void Creature::removeCondition(const Creature* attacker, ConditionType_t type)
{
	ConditionList tmpList = conditions;
	for(ConditionList::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
	{
		if((*it)->getType() == type)
			onCombatRemoveCondition(attacker, *it);
	}
}

void Creature::removeConditions(ConditionEnd_t reason, bool onlyPersistent/* = true*/)
{
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end();)
	{
		if(onlyPersistent && !(*it)->isPersistent())
		{
			++it;
			continue;
		}

		Condition* condition = *it;
		it = conditions.erase(it);

		condition->endCondition(this, reason);
		onEndCondition(condition->getType());
		delete condition;
	}
}

Condition* Creature::getCondition(ConditionType_t type, ConditionId_t id, uint32_t subId/* = 0*/) const
{
	for(ConditionList::const_iterator it = conditions.begin(); it != conditions.end(); ++it)
	{
		if((*it)->getType() == type && (*it)->getId() == id && (*it)->getSubId() == subId)
			return *it;
	}

	return nullptr;
}

void Creature::executeConditions(uint32_t interval)
{
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end();)
	{
		if((*it)->executeCondition(this, interval))
		{
			++it;
			continue;
		}

		Condition* condition = *it;
		it = conditions.erase(it);

		condition->endCondition(this, CONDITIONEND_TICKS);
		onEndCondition(condition->getType());
		delete condition;
	}
}

bool Creature::hasCondition(ConditionType_t type, int32_t subId/* = 0*/, bool checkTime/* = true*/) const
{
	if(isSuppress(type))
		return false;

	for(ConditionList::const_iterator it = conditions.begin(); it != conditions.end(); ++it)
	{
		if((*it)->getType() != type || (subId != -1 && (*it)->getSubId() != (uint32_t)subId))
			continue;

		if(!checkTime || server.configManager().getBool(ConfigManager::OLD_CONDITION_ACCURACY)
			|| !(*it)->getEndTime() || (*it)->getEndTime() >= OTSYS_TIME())
			return true;
	}

	return false;
}

bool Creature::isImmune(CombatType_t type) const
{
	return ((getDamageImmunities() & (uint32_t)type) == (uint32_t)type);
}

bool Creature::isImmune(ConditionType_t type) const
{
	return ((getConditionImmunities() & (uint32_t)type) == (uint32_t)type);
}

bool Creature::isSuppress(ConditionType_t type) const
{
	return ((getConditionSuppressions() & (uint32_t)type) == (uint32_t)type);
}

std::string Creature::getDescription(int32_t lookDistance) const
{
	return "a creature";
}

int32_t Creature::getStepDuration(Direction dir) const
{
	if(dir == NORTHWEST || dir == NORTHEAST || dir == SOUTHWEST || dir == SOUTHEAST)
		return getStepDuration() * 2;

	return getStepDuration();
}

int32_t Creature::getStepDuration() const
{
	if(removed)
		return 0;

	uint32_t stepSpeed = getStepSpeed();
	if(!stepSpeed)
		return 0;

	const Tile* tile = getTile();
	if(!tile || !tile->ground)
		return 0;

	return ((1000 * tile->ground->getKind()->speed) / stepSpeed) * lastStepCost;
}

int64_t Creature::getEventStepTicks() const
{
	int64_t ret = getWalkDelay();
	if(ret > 0)
		return ret;

	return getStepDuration();
}

void Creature::getCreatureLight(LightInfo& light) const
{
	light = internalLight;
}

void Creature::setNormalCreatureLight()
{
	internalLight.level = internalLight.color = 0;
}

bool Creature::registerCreatureEvent(const std::string& name)
{
	CreatureEventP event = server.creatureEvents().getEventByName(name);
	if(!event) //check for existance
		return false;

	for(CreatureEventList::iterator it = eventsList.begin(); it != eventsList.end(); ++it)
	{
		if((*it) == event) //do not allow registration of same event more than once
			return false;
	}

	if(!hasEventRegistered(event->getEventType())) //there's no such type registered yet, so set the bit in the bitfield
		scriptEventsBitField |= ((uint32_t)1 << event->getEventType());

	eventsList.push_back(event);
	return true;
}

CreatureEventList Creature::getCreatureEvents(CreatureEventType_t type)
{
	CreatureEventList retList;
	if(!hasEventRegistered(type))
		return retList;

	for(CreatureEventList::iterator it = eventsList.begin(); it != eventsList.end(); ++it)
	{
		if((*it)->getEventType() == type)
			retList.push_back(*it);
	}

	return retList;
}


ZoneType_t Creature::getZone() const {
	return _tile->getZone();
}


FrozenPathingConditionCall::FrozenPathingConditionCall(const Position& _targetPos)
{
	targetPos = _targetPos;
}

bool FrozenPathingConditionCall::isInRange(const Position& startPos, const Position& testPos,
	const FindPathParams& fpp) const
{
	int32_t dxMin = ((fpp.fullPathSearch || (startPos.x - targetPos.x) <= 0) ? fpp.maxTargetDist : 0),
	dxMax = ((fpp.fullPathSearch || (startPos.x - targetPos.x) >= 0) ? fpp.maxTargetDist : 0),
	dyMin = ((fpp.fullPathSearch || (startPos.y - targetPos.y) <= 0) ? fpp.maxTargetDist : 0),
	dyMax = ((fpp.fullPathSearch || (startPos.y - targetPos.y) >= 0) ? fpp.maxTargetDist : 0);
	if(testPos.x > targetPos.x + dxMax || testPos.x < targetPos.x - dxMin)
		return false;

	if(testPos.y > targetPos.y + dyMax || testPos.y < targetPos.y - dyMin)
		return false;

	return true;
}

bool FrozenPathingConditionCall::operator()(const Position& startPos, const Position& testPos,
	const FindPathParams& fpp, int32_t& bestMatchDist) const
{
	if(!isInRange(startPos, testPos, fpp))
		return false;

	if(fpp.clearSight && !server.game().isSightClear(testPos, targetPos, true))
		return false;

	int32_t testDist = std::max(std::abs(targetPos.x - testPos.x), std::abs(targetPos.y - testPos.y));
	if(fpp.maxTargetDist == 1)
		return (testDist >= fpp.minTargetDist && testDist <= fpp.maxTargetDist);

	if(testDist <= fpp.maxTargetDist)
	{
		if(testDist < fpp.minTargetDist)
			return false;

		if(testDist == fpp.maxTargetDist)
		{
			bestMatchDist = 0;
			return true;
		}
		else if(testDist > bestMatchDist)
		{
			//not quite what we want, but the best so far
			bestMatchDist = testDist;
			return true;
		}
	}

	return false;
}