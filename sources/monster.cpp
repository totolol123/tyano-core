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

#include "monster.h"
#include "spawn.h"
#include "monsters.h"

#include "spells.h"
#include "combat.h"

#include "configmanager.h"
#include "creature.h"
#include "dispatcher.h"
#include "game.h"
#include "player.h"
#include "raids.h"
#include "task.h"
#include "server.h"


LOGGER_DEFINITION(Monster);


AutoList<Monster>Monster::autoList;
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t Monster::monsterCount = 0;
#endif

boost::intrusive_ptr<Monster> Monster::createMonster(MonsterType* mType)
{
	return new Monster(mType);
}

boost::intrusive_ptr<Monster> Monster::createMonster(const std::string& name)
{
	MonsterType* mType = server.monsters().getMonsterType(name);
	if(!mType)
		return nullptr;

	return createMonster(mType);
}

Monster::Monster(MonsterType* _mType):
	Creature()
{
	isIdle = true;
	isMasterInRange = false;
	teleportToMaster = false;
	mType = _mType;
	spawn = nullptr;
	raid = nullptr;
	defaultOutfit = mType->outfit;
	currentOutfit = mType->outfit;

	double multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_HEALTH);
	health = (int32_t)(mType->health * multiplier);
	healthMax = (int32_t)(mType->healthMax * multiplier);

	baseSpeed = mType->baseSpeed;
	internalLight.level = mType->lightLevel;
	internalLight.color = mType->lightColor;
	setSkull(mType->skull);
	setShield(mType->partyShield);

	hideName = mType->hideName, hideHealth = mType->hideHealth;

	minCombatValue = 0;
	maxCombatValue = 0;

	targetTicks = 0;
	targetChangeTicks = 0;
	targetChangeCooldown = 0;
	attackTicks = 0;
	defenseTicks = 0;
	yellTicks = 0;
	extraMeleeAttack = false;
	resetTicks = false;

	// register creature events
	for(StringVector::iterator it = mType->scriptList.begin(); it != mType->scriptList.end(); ++it)
	{
		if(!registerCreatureEvent(*it))
			LOGe("[Monster::Monster] Unknown event name - " << *it);
	}

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	monsterCount++;
#endif
}

Monster::~Monster()
{
	clearTargetList();
	clearFriendList();
#ifdef __ENABLE_SERVER_DIAGNOSTIC__

	monsterCount--;
#endif
	if(raid)
	{
		raid->unRef();
		raid = nullptr;
	}
}

void Monster::onAttackedCreature(Creature* target)
{
	Creature::onAttackedCreature(target);
	if(isSummon())
		getMaster()->onSummonAttackedCreature(this, target);
}

void Monster::onAttackedCreatureDisappear(bool isLogout)
{
#ifdef __DEBUG__
	LOGt("Attacked creature disappeared.");
#endif
	attackTicks = 0;
	extraMeleeAttack = true;
}

void Monster::onAttackedCreatureDrain(Creature* target, int32_t points)
{
	Creature::onAttackedCreatureDrain(target, points);
	if(isSummon())
		getMaster()->onSummonAttackedCreatureDrain(this, target, points);
}

void Monster::onCreatureAppear(const Creature* creature)
{
	Creature::onCreatureAppear(creature);
	if(creature == this)
	{
		//We just spawned lets look around to see who is there.
		if(isSummon())
			isMasterInRange = canSee(getMaster()->getPosition());

		updateTargetList();
		updateIdleStatus();
	}
	else
		onCreatureEnter(const_cast<Creature*>(creature));
}

void Monster::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	Creature::onCreatureDisappear(creature, isLogout);
	if(creature == this)
	{
		if(spawn)
			spawn->startEvent();

		setIdle(true);
	}
	else
		onCreatureLeave(const_cast<Creature*>(creature));
}

void Monster::onCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, bool teleport)
{
	Creature::onCreatureMove(creature, newTile, newPos, oldTile, oldPos, teleport);
	if(creature == this)
	{
		if(isSummon())
			isMasterInRange = canSee(getMaster()->getPosition());

		updateTargetList();
		updateIdleStatus();
	}
	else
	{
		bool canSeeNewPos = canSee(newPos), canSeeOldPos = canSee(oldPos);
		if(canSeeNewPos && !canSeeOldPos)
			onCreatureEnter(const_cast<Creature*>(creature));
		else if(!canSeeNewPos && canSeeOldPos)
			onCreatureLeave(const_cast<Creature*>(creature));

		if(isSummon() && getMaster() == creature && canSeeNewPos) //Turn the summon on again
			isMasterInRange = true;

		updateIdleStatus();
		if(!followCreature && !isSummon() && isOpponent(creature)) //we have no target lets try pick this one
			selectTarget(const_cast<Creature*>(creature));
	}
}

void Monster::updateTargetList()
{
	CreatureList::iterator it;
	for(it = friendList.begin(); it != friendList.end();)
	{
		if((*it)->getHealth() <= 0 || !canSee((*it)->getPosition()))
		{
			it = friendList.erase(it);
		}
		else
			++it;
	}

	for(it = targetList.begin(); it != targetList.end();)
	{
		const boost::intrusive_ptr<Creature>& target = *it;
		if(target->isRemoved() || target->getHealth() <= 0 || !canSee(target->getPosition()))
		{
			it = targetList.erase(it);
		}
		else
			++it;
	}

	const SpectatorList& list = server.game().getSpectators(getPosition());
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((*it) != this && canSee((*it)->getPosition()))
			onCreatureFound(*it);
	}
}

void Monster::clearTargetList()
{
	targetList.clear();
}

void Monster::clearFriendList()
{
	friendList.clear();
}

void Monster::onCreatureFound(Creature* creature, bool pushFront /*= false*/)
{
	if(isFriend(creature))
	{
		assert(creature != this);
		if(std::find(friendList.begin(), friendList.end(), creature) == friendList.end())
		{
			friendList.push_back(creature);
		}
	}

	if(isOpponent(creature))
	{
		assert(creature != this);
		if(std::find(targetList.begin(), targetList.end(), creature) == targetList.end())
		{
			if(pushFront)
				targetList.push_front(creature);
			else
				targetList.push_back(creature);
		}
	}

	updateIdleStatus();
}

void Monster::onCreatureEnter(Creature* creature)
{
	if(getMaster() == creature) //Turn the summon on again
	{
		isMasterInRange = true;
		updateIdleStatus();
	}

	onCreatureFound(creature, true);
}

bool Monster::isFriend(const Creature* creature)
{
	if(!isSummon() || !getMaster()->getPlayer())
		return creature->getMonster() && !creature->isSummon();

	const Player* tmpPlayer = nullptr;
	if(creature->getPlayer())
		tmpPlayer = creature->getPlayer();
	else if(creature->getMaster() && creature->getMaster()->getPlayer())
		tmpPlayer = creature->getMaster()->getPlayer();

	const Player* masterPlayer = getMaster()->getPlayer();
	return tmpPlayer && (tmpPlayer == getMaster() || masterPlayer->isPartner(tmpPlayer));
}

bool Monster::isOpponent(const Creature* creature)
{
	return (isSummon() && getMaster()->getPlayer() && creature != getMaster()) || ((creature->getPlayer()
		&& !creature->getPlayer()->hasFlag(PlayerFlag_IgnoredByMonsters)) ||
		(creature->getMaster() && creature->getMaster()->getPlayer()));
}

bool Monster::doTeleportToMaster()
{
	const Position& tmp = getPosition();
	if(server.game().internalTeleport(this, server.game().getClosestFreeTile(this,
		getMaster()->getPosition(), true), false) != RET_NOERROR)
		return false;

	server.game().addMagicEffect(tmp, MAGIC_EFFECT_POFF);
	server.game().addMagicEffect(getPosition(), MAGIC_EFFECT_TELEPORT);
	return true;
}

void Monster::onCreatureLeave(Creature* creature)
{
#ifdef __DEBUG__
	LOGt("onCreatureLeave - " << creature->getName());
#endif
	if(isSummon() && getMaster() == creature)
	{
		if(!server.configManager().getBool(ConfigManager::TELEPORT_SUMMONS) && (!getMaster()->getPlayer()
			|| !server.configManager().getBool(ConfigManager::TELEPORT_PLAYER_SUMMONS)))
		{
			//Turn the monster off until its master comes back
			isMasterInRange = false;
			updateIdleStatus();
		}
		else if(!doTeleportToMaster())
			teleportToMaster = true;
	}

	//update friendList
	if(isFriend(creature))
	{
		CreatureList::iterator it = std::find(friendList.begin(), friendList.end(), creature);
		if(it != friendList.end())
		{
			friendList.erase(it);
		}
#ifdef __DEBUG__
		else
			LOGt("Monster: " << creature->getName() << " not found in the friendList.");
#endif
	}

	//update targetList
	if(isOpponent(creature))
	{
		CreatureList::iterator it = std::find(targetList.begin(), targetList.end(), creature);
		if(it != targetList.end())
		{
			targetList.erase(it);
			if(targetList.empty())
				updateIdleStatus();
		}
#ifdef __DEBUG__
		else
			LOGt("Player: " << creature->getName() << " not found in the targetList.");
#endif
	}
}

bool Monster::searchTarget(TargetSearchType_t searchType /*= TARGETSEARCH_DEFAULT*/)
{
#ifdef __DEBUG__
	LOGt("Searching target... ");
#endif

	CreatureList resultList;
	const Position& myPos = getPosition();
	for(CreatureList::iterator it = targetList.begin(); it != targetList.end(); ++it)
	{
		if(followCreature != (*it) && isTarget((*it).get()) && (searchType == TARGETSEARCH_RANDOM
			|| canUseAttack(myPos, (*it).get())))
			resultList.push_back(*it);
	}

	switch(searchType)
	{
		case TARGETSEARCH_NEAREST:
		{
			Creature* target = nullptr;
			int32_t range = -1;
			for(CreatureList::iterator it = resultList.begin(); it != resultList.end(); ++it)
			{
				int32_t tmp = std::max(std::abs(myPos.x - (*it)->getPosition().x),
					std::abs(myPos.y - (*it)->getPosition().y));
				if(range >= 0 && tmp >= range)
					continue;

				target = (*it).get();
				range = tmp;
			}

			if(target && selectTarget(target))
				return target;

			break;
		}
		default:
		{
			if(!resultList.empty())
			{
				CreatureList::iterator it = resultList.begin();
				std::advance(it, random_range(0, resultList.size() - 1));
#ifdef __DEBUG__

				LOGt("Selecting target " << (*it)->getName());
#endif
				return selectTarget((*it).get());
			}

			if(searchType == TARGETSEARCH_ATTACKRANGE)
				return false;

			break;
		}
	}


	//lets just pick the first target in the list
	for(CreatureList::iterator it = targetList.begin(); it != targetList.end(); ++it)
	{
		if(followCreature == (*it) || !selectTarget((*it).get()))
			continue;

#ifdef __DEBUG__
		LOGt("Selecting target " << (*it)->getName());
#endif
		return true;
	}

	return false;
}

void Monster::onFollowCreatureComplete(const Creature* creature)
{
	if(!creature)
		return;

	CreatureList::iterator it = std::find(targetList.begin(), targetList.end(), creature);
	if(it != targetList.end())
	{
		boost::intrusive_ptr<Creature> target = (*it);
		targetList.erase(it);

		if(hasFollowPath) //push target we have found a path to the front
			targetList.push_front(target);
		else if(!isSummon()) //push target we have not found a path to the back
			targetList.push_back(target);
	}
}

BlockType_t Monster::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense/* = false*/, bool checkArmor/* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor);
	if(!damage)
		return blockType;

	int32_t elementMod = 0;
	ElementMap::iterator it = mType->elementMap.find(combatType);
	if(it != mType->elementMap.end())
		elementMod = it->second;

	if(!elementMod)
		return blockType;

	damage = (int32_t)std::ceil(damage * ((float)(100 - elementMod) / 100));
	if(damage > 0)
		return blockType;

	damage = 0;
	blockType = BLOCK_DEFENSE;
	return blockType;
}

bool Monster::isTarget(Creature* creature)
{
	return (!creature->isRemoved() && creature->isAttackable() && creature->getZone() != ZONE_PROTECTION
		&& canSeeCreature(creature) && creature->getPosition().z == getPosition().z);
}

bool Monster::selectTarget(Creature* creature)
{
#ifdef __DEBUG__
	LOGt("Selecting target... ");
#endif
	if(!isTarget(creature))
		return false;

	CreatureList::iterator it = std::find(targetList.begin(), targetList.end(), creature);
	if(it == targetList.end())
	{
		//Target not found in our target list.
#ifdef __DEBUG__
		LOGt("Target not found in targetList.");
#endif
		return false;
	}

	if((isHostile() || isSummon()) && setAttackedCreature(creature) && !isSummon())
		server.dispatcher().addTask(Task::create(
			std::bind(&Game::checkCreatureAttack, &server.game(), getID())));

	return setFollowCreature(creature, true);
}

void Monster::setIdle(bool _idle)
{
	if(isRemoved() || getHealth() <= 0)
		return;

	isIdle = _idle;
	if(isIdle)
	{
		onIdleStatus();
		clearTargetList();
		clearFriendList();
		server.game().removeCreatureCheck(this);
	}
	else
		server.game().addCreatureCheck(this);
}

void Monster::updateIdleStatus()
{
	bool idle = false;
	if(conditions.empty())
	{
		if(isSummon())
		{
			if((!isMasterInRange && !teleportToMaster) || (getMaster()->getMonster() && getMaster()->getMonster()->getIdleStatus()))
				idle = true;
		}
		else if(targetList.empty())
			idle = true;
	}

	setIdle(idle);
}

void Monster::onAddCondition(ConditionType_t type, bool hadCondition)
{
	Creature::onAddCondition(type, hadCondition);

	updateIdleStatus();
}

void Monster::onEndCondition(ConditionType_t type)
{
	Creature::onEndCondition(type);

	updateIdleStatus();
}

void Monster::onThink(uint32_t interval)
{
	Creature::onThink(interval);
	if(despawn())
	{
		server.game().removeCreature(this, true);
		setIdle(true);
		return;
	}

	updateIdleStatus();
	if(isIdle)
		return;

	if(teleportToMaster && doTeleportToMaster())
		teleportToMaster = false;

	addEventWalk();
	if(isSummon())
	{
		if(!attackedCreature)
		{
			if(getMaster() && getMaster()->getAttackedCreature()) //This happens if the monster is summoned during combat
				selectTarget(getMaster()->getAttackedCreature());
			else if(getMaster() != followCreature) //Our master has not ordered us to attack anything, lets follow him around instead.
				setFollowCreature(getMaster());
		}
		else if(attackedCreature == this)
			setFollowCreature(nullptr);
		else if(followCreature != attackedCreature) //This happens just after a master orders an attack, so lets follow it aswell.
			setFollowCreature(attackedCreature);
	}
	else if(!targetList.empty())
	{
		if(!followCreature || !hasFollowPath)
			searchTarget();
		else if(isFleeing() && attackedCreature && !canUseAttack(getPosition(), attackedCreature))
			searchTarget(TARGETSEARCH_ATTACKRANGE);
	}

	onThinkTarget(interval);
	onThinkYell(interval);
	onThinkDefense(interval);
}

void Monster::doAttacking(uint32_t interval)
{
	if(!attackedCreature || (isSummon() && attackedCreature == this))
		return;

	bool updateLook = true;
	resetTicks = (interval != 0);
	attackTicks += interval;

	const Position& myPos = getPosition();
	for(SpellList::iterator it = mType->spellAttackList.begin(); it != mType->spellAttackList.end(); ++it)
	{
		if(!attackedCreature || attackedCreature->isRemoved())
			break;

		const Position& targetPos = attackedCreature->getPosition();
		if(it->isMelee && isFleeing())
			continue;

		bool inRange = false;
		if(canUseSpell(myPos, targetPos, *it, interval, inRange))
		{
			if(it->chance >= (uint32_t)random_range(1, 100))
			{
				if(updateLook)
				{
					updateLookDirection();
					updateLook = false;
				}

				double multiplier;
				if(maxCombatValue > 0) //defense
					multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_DEFENSE);
				else //attack
					multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_ATTACK);

				minCombatValue = (int32_t)(it->minCombatValue * multiplier);
				maxCombatValue = (int32_t)(it->maxCombatValue * multiplier);

				it->spell->castSpell(this, attackedCreature);
				if(it->isMelee)
					extraMeleeAttack = false;
#ifdef __DEBUG__
				static uint64_t prevTicks = OTSYS_TIME();
				LOGt("doAttacking ticks: " << OTSYS_TIME() - prevTicks);
				prevTicks = OTSYS_TIME();
#endif
			}
		}

		if(!inRange && it->isMelee) //melee swing out of reach
			extraMeleeAttack = true;
	}

	if(updateLook)
		updateLookDirection();

	if(resetTicks)
		attackTicks = 0;
}

bool Monster::canUseAttack(const Position& pos, const Creature* target) const
{
	if(!isHostile())
		return true;

	const Position& targetPos = target->getPosition();
	for(SpellList::iterator it = mType->spellAttackList.begin(); it != mType->spellAttackList.end(); ++it)
	{
		if((*it).range != 0 && std::max(std::abs(pos.x - targetPos.x), std::abs(pos.y - targetPos.y)) <= (int32_t)(*it).range)
			return server.game().isSightClear(pos, targetPos, true);
	}

	return false;
}

bool Monster::canUseSpell(const Position& pos, const Position& targetPos,
	const spellBlock_t& sb, uint32_t interval, bool& inRange)
{
	inRange = true;
	if(!sb.isMelee || !extraMeleeAttack)
	{
		if(sb.speed > attackTicks)
		{
			resetTicks = false;
			return false;
		}

		if(attackTicks % sb.speed >= interval) //already used this spell for this round
			return false;
	}

	if(!sb.range || std::max(std::abs(pos.x - targetPos.x), std::abs(pos.y - targetPos.y)) <= (int32_t)sb.range)
		return true;

	inRange = false;
	return false;
}

void Monster::onThinkTarget(uint32_t interval)
{
	if(isSummon() || mType->changeTargetSpeed <= 0)
		return;

	bool canChangeTarget = true;
	if(targetChangeCooldown > 0)
	{
		targetChangeCooldown -= interval;
		if(targetChangeCooldown <= 0)
		{
			targetChangeCooldown = 0;
			targetChangeTicks = (uint32_t)mType->changeTargetSpeed;
		}
		else
			canChangeTarget = false;
	}

	if(!canChangeTarget)
		return;

	targetChangeTicks += interval;
	if(targetChangeTicks < (uint32_t)mType->changeTargetSpeed)
		return;

	targetChangeTicks = 0;
	targetChangeCooldown = (uint32_t)mType->changeTargetSpeed;
	if(mType->changeTargetChance < random_range(1, 100))
		return;

	if(mType->targetDistance <= 1)
		searchTarget(TARGETSEARCH_RANDOM);
	else
		searchTarget(TARGETSEARCH_NEAREST);
}

void Monster::onThinkDefense(uint32_t interval)
{
	resetTicks = true;
	defenseTicks += interval;
	for(SpellList::iterator it = mType->spellDefenseList.begin(); it != mType->spellDefenseList.end(); ++it)
	{
		if(it->speed > defenseTicks)
		{
			if(resetTicks)
				resetTicks = false;

			continue;
		}

		if(defenseTicks % it->speed >= interval) //already used this spell for this round
			continue;

		if((it->chance >= (uint32_t)random_range(1, 100)))
		{
			minCombatValue = it->minCombatValue;
			maxCombatValue = it->maxCombatValue;
			it->spell->castSpell(this, this);
		}
	}

	if(!isSummon())
	{
		if(mType->maxSummons < 0 || (int32_t)summons.size() < mType->maxSummons)
		{
			for(SummonList::iterator it = mType->summonList.begin(); it != mType->summonList.end(); ++it)
			{
				if((int32_t)summons.size() >= mType->maxSummons)
					break;

				if(it->interval > defenseTicks)
				{
					if(resetTicks)
						resetTicks = false;

					continue;
				}

				if(defenseTicks % it->interval >= interval)
					continue;

				uint32_t typeCount = 0;
				for(CreatureList::iterator cit = summons.begin(); cit != summons.end(); ++cit)
				{
					if(!(*cit)->isRemoved() && (*cit)->getMonster() &&
						(*cit)->getMonster()->getName() == it->name)
						typeCount++;
				}

				if(typeCount >= it->amount)
					continue;

				if((it->chance >= (uint32_t)random_range(1, 100)))
				{
					if(boost::intrusive_ptr<Monster> summon = Monster::createMonster(it->name))
					{
						addSummon(summon.get());
						if(server.game().placeCreature(summon.get(), getPosition()))
							server.game().addMagicEffect(getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
						else
							removeSummon(summon.get());
					}
				}
			}
		}
	}

	if(resetTicks)
		defenseTicks = 0;
}

void Monster::onThinkYell(uint32_t interval)
{
	if(mType->yellSpeedTicks <= 0)
		return;

	yellTicks += interval;
	if(yellTicks < mType->yellSpeedTicks)
		return;

	yellTicks = 0;
	if(mType->voiceVector.empty() || (mType->yellChance < (uint32_t)random_range(1, 100)))
		return;

	const voiceBlock_t& vb = mType->voiceVector[random_range(0, mType->voiceVector.size() - 1)];
	if(vb.yellText)
		server.game().internalCreatureSay(this, SPEAK_MONSTER_YELL, vb.text, false);
	else
		server.game().internalCreatureSay(this, SPEAK_MONSTER_SAY, vb.text, false);
}

bool Monster::pushItem(Item* item, int32_t radius)
{
	const Position& centerPos = item->getPosition();
	PairVector pairVector;
	pairVector.push_back(PositionPair(-1, -1));
	pairVector.push_back(PositionPair(-1, 0));
	pairVector.push_back(PositionPair(-1, 1));
	pairVector.push_back(PositionPair(0, -1));
	pairVector.push_back(PositionPair(0, 1));
	pairVector.push_back(PositionPair(1, -1));
	pairVector.push_back(PositionPair(1, 0));
	pairVector.push_back(PositionPair(1, 1));

	std::random_shuffle(pairVector.begin(), pairVector.end());
	Position tryPos;
	for(int32_t n = 1; n <= radius; ++n)
	{
		for(PairVector::iterator it = pairVector.begin(); it != pairVector.end(); ++it)
		{
			int32_t dx = it->first * n, dy = it->second * n;
			tryPos = centerPos;

			tryPos.x = tryPos.x + dx;
			tryPos.y = tryPos.y + dy;

			Tile* tile = server.game().getTile(tryPos);
			if(tile && server.game().canThrowObjectTo(centerPos, tryPos) && server.game().internalMoveItem(this, item->getParent(),
				tile, INDEX_WHEREEVER, item, item->getItemCount(), nullptr) == RET_NOERROR)
				return true;
		}
	}

	return false;
}

void Monster::pushItems(Tile* tile)
{
	TileItemVector* items = tile->getItemList();
	if(!items)
		return;

	//We cannot use iterators here since we can push the item to another tile
	//which will invalidate the iterator.
	//start from the end to minimize the amount of traffic
	int32_t moveCount = 0, removeCount = 0, downItemsSize = tile->getDownItemCount();
	Item* item = nullptr;
	for(int32_t i = downItemsSize - 1; i >= 0; --i)
	{
		assert(i >= 0 && i < downItemsSize);
		if((item = items->at(i)) && item->hasProperty(MOVEABLE) &&
			(item->hasProperty(BLOCKPATH) || item->hasProperty(BLOCKSOLID)))
		{
			if(moveCount < 20 && pushItem(item, 1))
				moveCount++;
			else if(server.game().internalRemoveItem(this, item) == RET_NOERROR)
				++removeCount;
		}
	}

	if(removeCount > 0)
		server.game().addMagicEffect(tile->getPosition(), MAGIC_EFFECT_POFF);
}

bool Monster::pushCreature(Creature* creature)
{
	DirVector dirVector;
	dirVector.push_back(NORTH);
	dirVector.push_back(SOUTH);
	dirVector.push_back(WEST);
	dirVector.push_back(EAST);

	std::random_shuffle(dirVector.begin(), dirVector.end());
	Position monsterPos = creature->getPosition();

	Tile* tile = nullptr;
	for(DirVector::iterator it = dirVector.begin(); it != dirVector.end(); ++it)
	{
		if((tile = server.game().getTile(Spells::getCasterPosition(creature, (*it)))) && !tile->hasProperty(
			BLOCKPATH) && server.game().internalMoveCreature(creature, (*it)) == RET_NOERROR)
			return true;
	}

	return false;
}

void Monster::pushCreatures(Tile* tile)
{
	CreatureVector* creatures = tile->getCreatures();
	if(!creatures)
		return;

	bool effect = false;
	Monster* monster = nullptr;
	for(uint32_t i = 0; i < creatures->size();)
	{
		if((monster = creatures->at(i)->getMonster()) && monster->isPushable())
		{
			if(pushCreature(monster))
				continue;

			monster->setDropLoot(LOOT_DROP_NONE);
			monster->changeHealth(-monster->getHealth());
			if(!effect)
				effect = true;
		}

		++i;
	}

	if(effect)
		server.game().addMagicEffect(tile->getPosition(), MAGIC_EFFECT_BLOCKHIT);
}

bool Monster::getNextStep(Direction& dir, uint32_t& flags)
{
	if(isIdle || getHealth() <= 0)
	{
		//we dont have anyone watching might aswell stop walking
		eventWalk = 0;
		return false;
	}

	bool result = false;
	if((!followCreature || !hasFollowPath) && !isSummon())
	{
		if(followCreature || getTimeSinceLastMove() > 1000) //choose a random direction
			result = getRandomStep(getPosition(), dir);
	}
	else if(isSummon() || followCreature)
	{
		result = Creature::getNextStep(dir, flags);
		if(!result)
		{
			//target dancing
			if(attackedCreature && attackedCreature == followCreature)
			{
				if(isFleeing())
					result = getDanceStep(getPosition(), dir, false, false);
				else if(mType->staticAttackChance < (uint32_t)random_range(1, 100))
					result = getDanceStep(getPosition(), dir);
			}
		}
		else
			flags |= FLAG_PATHFINDING;
	}

	if(result && (canPushItems() || canPushCreatures()))
	{
		if(Tile* tile = server.game().getTile(Spells::getCasterPosition(this, dir)))
		{
			if(canPushItems())
				pushItems(tile);

			if(canPushCreatures())
				pushCreatures(tile);
		}
#ifdef __DEBUG__
		else
			LOGt("[Monster::getNextStep] no tile found.");
#endif
	}

	return result;
}

bool Monster::getRandomStep(const Position& creaturePos, Direction& dir)
{
	DirVector dirVector;
	dirVector.push_back(NORTH);
	dirVector.push_back(SOUTH);
	dirVector.push_back(WEST);
	dirVector.push_back(EAST);

	std::random_shuffle(dirVector.begin(), dirVector.end());
	for(DirVector::iterator it = dirVector.begin(); it != dirVector.end(); ++it)
	{
		if(!canWalkTo(creaturePos, *it))
			continue;

		dir = *it;
		return true;
	}

	return false;
}

bool Monster::getDanceStep(const Position& creaturePos, Direction& dir,	bool keepAttack /*= true*/, bool keepDistance /*= true*/)
{
	assert(attackedCreature);
	bool canDoAttackNow = canUseAttack(creaturePos, attackedCreature);
	const Position& centerPos = attackedCreature->getPosition();

	uint32_t tmpDist, centerToDist = std::max(std::abs(creaturePos.x - centerPos.x), std::abs(creaturePos.y - centerPos.y));
	DirVector dirVector;
	if(!keepDistance || creaturePos.y - centerPos.y >= 0)
	{
		tmpDist = std::max(std::abs((creaturePos.x) - centerPos.x), std::abs((creaturePos.y - 1) - centerPos.y));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, NORTH))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x, creaturePos.y - 1, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(NORTH);
		}
	}

	if(!keepDistance || creaturePos.y - centerPos.y <= 0)
	{
		tmpDist = std::max(std::abs((creaturePos.x) - centerPos.x), std::abs((creaturePos.y + 1) - centerPos.y));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, SOUTH))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x, creaturePos.y + 1, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(SOUTH);
		}
	}

	if(!keepDistance || creaturePos.x - centerPos.x >= 0)
	{
		tmpDist = std::max(std::abs((creaturePos.x + 1) - centerPos.x), std::abs((creaturePos.y) - centerPos.y));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, EAST))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x + 1, creaturePos.y, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(EAST);
		}
	}

	if(!keepDistance || creaturePos.x - centerPos.x <= 0)
	{
		tmpDist = std::max(std::abs((creaturePos.x - 1) - centerPos.x), std::abs((creaturePos.y) - centerPos.y));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, WEST))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x - 1, creaturePos.y, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(WEST);
		}
	}

	if(dirVector.empty())
		return false;

	std::random_shuffle(dirVector.begin(), dirVector.end());
	dir = dirVector[random_range(0, dirVector.size() - 1)];
	return true;
}

bool Monster::isInSpawnRange(const Position& toPos)
{
	return masterRadius == -1 || !inDespawnRange(toPos);
}

bool Monster::canWalkTo(Position pos, Direction dir)
{
	if(getNoMove())
		return false;

	switch(dir)
	{
		case NORTH:
			pos.y += -1;
			break;
		case WEST:
			pos.x += -1;
			break;
		case EAST:
			pos.x += 1;
			break;
		case SOUTH:
			pos.y += 1;
			break;
		default:
			break;
	}

	if(!isInSpawnRange(pos))
		return false;

	Tile* tile = server.game().getTile(pos);
	if(!tile || getTile()->isSwimmingPool(false) != tile->isSwimmingPool(false)) // prevent monsters entering/exiting to swimming pool
		return false;

	return !tile->getTopVisibleCreature(this) && tile->__queryAdd(
		0, this, 1, FLAG_PATHFINDING) == RET_NOERROR;
}

bool Monster::onDeath()
{
	if(!Creature::onDeath())
		return false;

	destroySummons();
	clearTargetList();
	clearFriendList();

	setAttackedCreature(nullptr);
	onIdleStatus();
	if(raid)
	{
		raid->unRef();
		raid = nullptr;
	}

	server.game().removeCreature(this, false);
	return true;
}

boost::intrusive_ptr<Item> Monster::createCorpse(DeathList deathList)
{
	boost::intrusive_ptr<Item> corpse = Creature::createCorpse(deathList);
	if(!corpse)
		return nullptr;

	if(mType->corpseUnique)
		corpse->setUniqueId(mType->corpseUnique);

	if(mType->corpseAction)
		corpse->setActionId(mType->corpseAction);

	DeathEntry ownerEntry = deathList[0];
	if(ownerEntry.isNameKill())
		return corpse;

	Creature* owner = ownerEntry.getKillerCreature();
	if(!owner)
		return corpse;

	uint32_t ownerId = 0;
	if(owner->getPlayer())
		ownerId = owner->getID();
	else if(owner->getMaster() && owner->getPlayerMaster())
		ownerId = owner->getMaster()->getID();

	if(ownerId)
		corpse->setCorpseOwner(ownerId);

	return corpse;
}

bool Monster::inDespawnRange(const Position& pos)
{
	if(!spawn || mType->isLureable)
		return false;

	int32_t radius = server.configManager().getNumber(ConfigManager::DEFAULT_DESPAWNRADIUS);
	if(!radius)
		return false;

	if(!Spawns::getInstance()->isInZone(masterPosition, radius, pos))
		return true;

	int32_t range = server.configManager().getNumber(ConfigManager::DEFAULT_DESPAWNRANGE);
	if(!range)
		return false;

	return std::abs(pos.z - masterPosition.z) > range;
}

bool Monster::despawn()
{
	return inDespawnRange(getPosition());
}

bool Monster::getCombatValues(int32_t& min, int32_t& max)
{
	if(!minCombatValue && !maxCombatValue)
		return false;

	double multiplier;
	if(maxCombatValue > 0) //defense
		multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_DEFENSE);
	else //attack
		multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_ATTACK);

	min = (int32_t)(minCombatValue * multiplier);
	max = (int32_t)(maxCombatValue * multiplier);
	return true;
}

void Monster::updateLookDirection()
{
	Direction newDir = getDirection();
	if(attackedCreature)
	{
		const Position& pos = getPosition();
		const Position& attackedCreaturePos = attackedCreature->getPosition();

		int32_t dx = attackedCreaturePos.x - pos.x, dy = attackedCreaturePos.y - pos.y;
		if(std::abs(dx) > std::abs(dy))
		{
			//look EAST/WEST
			if(dx < 0)
				newDir = WEST;
			else
				newDir = EAST;
		}
		else if(std::abs(dx) < std::abs(dy))
		{
			//look NORTH/SOUTH
			if(dy < 0)
				newDir = NORTH;
			else
				newDir = SOUTH;
		}
		else if(dx < 0 && dy < 0)
		{
			if(getDirection() == SOUTH)
				newDir = WEST;
			else if(getDirection() == EAST)
				newDir = NORTH;
		}
		else if(dx < 0 && dy > 0)
		{
			if(getDirection() == NORTH)
				newDir = WEST;
			else if(getDirection() == EAST)
				newDir = SOUTH;
		}
		else if(dx > 0 && dy < 0)
		{
			if(getDirection() == SOUTH)
				newDir = EAST;
			else if(getDirection() == WEST)
				newDir = NORTH;
		}
		else if(getDirection() == NORTH)
			newDir = EAST;
		else if(getDirection() == WEST)
			newDir = SOUTH;
	}

	server.game().internalCreatureTurn(this, newDir);
}

void Monster::dropLoot(Container* corpse)
{
	if(corpse && lootDrop == LOOT_DROP_FULL)
		mType->dropLoot(corpse);
}

bool Monster::isImmune(CombatType_t type) const
{
	ElementMap::const_iterator it = mType->elementMap.find(type);
	if(it == mType->elementMap.end())
		return Creature::isImmune(type);

	return it->second >= 100;
}

void Monster::setNormalCreatureLight()
{
	internalLight.level = mType->lightLevel;
	internalLight.color = mType->lightColor;
}

void Monster::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainHealth(attacker, combatType, damage);
	if(isInvisible())
		removeCondition(CONDITION_INVISIBLE);
}

void Monster::changeHealth(int32_t healthChange)
{
	//In case a player with ignore flag set attacks the monster
	setIdle(false);
	Creature::changeHealth(healthChange);
}

bool Monster::challengeCreature(Creature* creature)
{
	if(isSummon() || !selectTarget(creature))
		return false;

	targetChangeCooldown = 8000;
	targetChangeTicks = 0;
	return true;
}

bool Monster::convinceCreature(Creature* creature)
{
	Player* player = creature->getPlayer();
	if(player && !player->hasFlag(PlayerFlag_CanConvinceAll) && !mType->isConvinceable)
		return false;

	Creature* oldMaster = nullptr;
	if(isSummon())
		oldMaster = getMaster();

	if(oldMaster)
	{
		if(oldMaster->getPlayer() || oldMaster == creature)
			return false;

		oldMaster->removeSummon(this);
	}

	setFollowCreature(nullptr);
	setAttackedCreature(nullptr);
	destroySummons();

	creature->addSummon(this);
	updateTargetList();
	updateIdleStatus();

	//Notify surrounding about the change
	SpectatorList list;
	server.game().getSpectators(list, getPosition(), false, true);
	server.game().getSpectators(list, creature->getPosition(), true, true);

	isMasterInRange = true;
	for(SpectatorList::iterator it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureConvinced(creature, this);

	if(spawn)
	{
		spawn->removeMonster(this);
		spawn = nullptr;
		masterRadius = -1;
	}

	if(raid)
	{
		raid->unRef();
		raid = nullptr;
	}

	return true;
}

void Monster::onCreatureConvinced(const Creature* convincer, const Creature* creature)
{
	if(convincer == this || (!isFriend(creature) && !isOpponent(creature)))
		return;

	updateTargetList();
	updateIdleStatus();
}

void Monster::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.minTargetDist = 1;
	fpp.maxTargetDist = mType->targetDistance;
	if(isSummon())
	{
		if(getMaster() == creature)
		{
			fpp.maxTargetDist = 2;
			fpp.fullPathSearch = true;
		}
		else if(mType->targetDistance <= 1)
			fpp.fullPathSearch = true;
		else
			fpp.fullPathSearch = !canUseAttack(getPosition(), creature);
	}
	else if(isFleeing())
	{
		//Distance should be higher than the client view range (Map::maxClientViewportX/Map::maxClientViewportY)
		fpp.maxTargetDist = Map::maxViewportX;
		fpp.clearSight = fpp.fullPathSearch = false;
		fpp.keepDistance = true;
	}
	else if(mType->targetDistance <= 1)
		fpp.fullPathSearch = true;
	else
		fpp.fullPathSearch = !canUseAttack(getPosition(), creature);
}



const std::string& Monster::getName() const {return mType->name;}
const std::string& Monster::getNameDescription() const {return mType->nameDescription;}
std::string Monster::getDescription(int32_t lookDistance) const {return mType->nameDescription + ".";}

RaceType_t Monster::getRace() const {return mType->race;}
int32_t Monster::getArmor() const {return mType->armor;}
int32_t Monster::getDefense() const {return mType->defense;}
MonsterType* Monster::getMonsterType() const {return mType;}
bool Monster::isPushable() const {return mType->pushable && (baseSpeed > 0);}
bool Monster::isAttackable() const {return mType->isAttackable;}

bool Monster::canPushItems() const {return mType->canPushItems;}
bool Monster::canPushCreatures() const {return mType->canPushCreatures;}
bool Monster::isHostile() const {return mType->isHostile;}
bool Monster::isWalkable() const {return mType->isWalkable;}
bool Monster::canSeeInvisibility() const {return Creature::isImmune(CONDITION_INVISIBLE);}
uint32_t Monster::getManaCost() const {return mType->manaCost;}

bool Monster::isFleeing() const {return getHealth() <= mType->runAwayHealth;}

uint64_t Monster::getLostExperience() const {return ((skillLoss ? mType->experience : 0));}
uint32_t Monster::getDamageImmunities() const {return mType->damageImmunities;}
uint32_t Monster::getConditionImmunities() const {return mType->conditionImmunities;}
uint16_t Monster::getLookCorpse() {return mType->lookCorpse;}
uint16_t Monster::getLookCorpse() const {return mType->lookCorpse;}
