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



Monster::~Monster() {
	if (_raid != nullptr) {
		_raid->unRef();
	}
}


void Monster::babble() {
	if (_type->babbleEntries.empty()) {
		return;
	}

	const voiceBlock_t& entry = _type->babbleEntries[random_range<uint32_t>(0, _type->babbleEntries.size() - 1)];
	server.game().internalCreatureSay(this, entry.yellText ? SPEAK_MONSTER_YELL : SPEAK_MONSTER_SAY, entry.text, false);
}


bool Monster::canBeChallengedBy(const CreatureP& challenger) const {
	if (challenger == nullptr) {
		assert(challenger != nullptr);
		return false;
	}

	if (challenger == this) {
		return false;
	}

	if (hasController()) {
		return false;
	}

	if (!isEnemy(challenger)) {
		return false;
	}

	return true;
}


bool Monster::canBeConvincedBy(const CreatureP& convincer, bool forced) const {
	if (convincer == this) {
		return false;
	}

	if (!forced) {
		Player* convincingPlayer = convincer->getPlayer();
		if (convincingPlayer != nullptr && !_type->isConvinceable && !convincingPlayer->hasFlag(PlayerFlag_CanConvinceAll)) {
			return false;
		}
	}

	if (hasMaster()) {
		if (!forced && _master->hasController()) {
			return false;
		}

		if (convincer == _master) {
			return false;
		}
	}

	return true;
}


bool Monster::canSeeCreature(const CreatureP& creature) const {
	if (creature == nullptr) {
		assert(creature != nullptr);
		return false;
	}

	if (creature == _master) {
		return true;
	}

	return Creature::canSeeCreature(creature);
}


bool Monster::challenge(const CreatureP& challenger) {
	if (challenger == nullptr) {
		assert(challenger != nullptr);
		return false;
	}

	if (!canBeChallengedBy(challenger)) {
		return false;
	}

	if (!target(challenger)) {
		return false;
	}

	setRetargetDelay(std::chrono::seconds(8));

	return true;
}


bool Monster::convince(const CreatureP& convincer, bool forced) {
	assert(convincer != nullptr);

	if (!canBeConvincedBy(convincer, forced)) {
		return false;
	}

	setMaster(convincer);

	return true;
}


MonsterP Monster::create(MonsterType* type) {
	return new Monster(type, nullptr, nullptr);
}


MonsterP Monster::create(MonsterType* type, Raid* raid) {
	return new Monster(type, raid, nullptr);
}


MonsterP Monster::create(MonsterType* type, Spawn* spawn) {
	return new Monster(type, nullptr, spawn);
}


// TODO doesn't belong here.
MonsterP Monster::create(const std::string& name) {
	MonsterType* type = server.monsters().getMonsterType(name);
	if (type == nullptr) {
		return nullptr;
	}

	return create(type);
}


void Monster::follow(const CreatureP& creature) {
	setFollowCreature(creature.get());
}


CreatureP Monster::getDirectOwner() {
	return _master;
}


CreaturePC Monster::getDirectOwner() const {
	return _master;
}


CreatureP Monster::getMaster() const {
	return _master;
}


bool Monster::hasMaster() const {
	return (_master != nullptr);
}


bool Monster::hasRaid() const {
	return (_raid != nullptr);
}


bool Monster::hasSomethingToThinkAbout() const {
	if (Creature::hasSomethingToThinkAbout()) {
		return true;
	}

	if (isMasterInRange() && _master->isThinking()) {
		return true;
	}

	return false;
}


bool Monster::hasSpawn() const {
	return (_spawn != nullptr);
}


bool Monster::hasToThinkAboutCreature(const CreaturePC& creature) const {
	assert(creature != nullptr);

	if (Creature::hasToThinkAboutCreature(creature)) {
		return true;
	}

	if (creature->hasController()) {
		return true;
	}

	return false;
}


bool Monster::isEnemy(const CreaturePC& creature) const {
	if (creature == nullptr) {
		assert(creature != nullptr);
		return false;
	}

	if (creature->isRemoved()) {
		return false;
	}

	if (hasMaster()) {
		return _master->isEnemy(creature);
	}

	// TODO name "isAttackable" is too broad
	if (!creature->isAttackable()) {
		return false;
	}

	// TODO merge with isAttackable?
	if (creature->getZone() == ZONE_PROTECTION) {
		return false;
	}

	PlayerPC controller = creature->getController();
	if (controller == nullptr) {
		return false;
	}

	if (controller->hasFlag(PlayerFlag_IgnoredByMonsters)) {
		return false;
	}

	if (controller->isGhost()) {
		return false;
	}

	return true;
}


bool Monster::isMasterInRange() const {
	return (hasMaster() && canSee(_master->getPosition()));
}


void Monster::notifyMasterChanged(const CreatureP& previousMaster) {
	if (isRemoved()) {
		return;
	}

	Game& game = server.game();

	SpectatorList spectators;
	game.getSpectators(spectators, getPosition(), false, true);
	if (hasMaster()) {
		game.getSpectators(spectators, _master->getPosition(), true, true);
	}

	for (const auto& spectator : spectators) {
		spectator->onMonsterMasterChanged(this, previousMaster);
	}
}


void Monster::onAttackedCreature(Creature* target) {
	Creature::onAttackedCreature(target);

	if (hasMaster()) {
		_master->onSummonAttackedCreature(this, target);
	}
}


void Monster::onAttackedCreatureDisappear(bool loggedOut) {
	Creature::onAttackedCreatureDisappear(loggedOut);

	attackTicks = 0;
	extraMeleeAttack = true;
}



void Monster::onAttackedCreatureDrain(Creature* target, int32_t points) {
	Creature::onAttackedCreatureDrain(target, points);

	if (hasMaster()) {
		_master->onSummonAttackedCreatureDrain(this, target, points);
	}
}


bool Monster::onDeath() {
	if (!Creature::onDeath()) {
		return false;
	}

	remove();

	return true;
}


void Monster::onThink(Duration elapsedTime) {
	Creature::onThink(elapsedTime);

	if (!isAlive()) {
		return;
	}

	if (shouldBeRemoved() && remove()) {
		return;
	}

	updateRelationship();
	updateMovement();
	updateTarget();
	updateRetargeting(elapsedTime);
	updateFollowing();
	updateBabbling(elapsedTime);

	// TODO refactor >>
	onThinkDefense(std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count());
	// <<
}


void Monster::release() {
	setMaster(nullptr);
}


void Monster::removeFromRaid() {
	if (!hasRaid()) {
		return;
	}

	_raid->unRef();
	_raid = nullptr;
}


void Monster::removeFromSpawn() {
	if (!hasSpawn()) {
		return;
	}

	_spawn->removeMonster(this);
	_spawn->startEvent();
	_spawn = nullptr;

	// TODO refactor
	masterRadius = -1;
}


void Monster::retarget() {
	if (_type->targetDistance <= 1) {
		targetRandomEnemy();
	}
	else {
		targetClosestEnemy();
	}
}


void Monster::setMaster(const CreatureP& master) {
	assert(master != this);

	if (master == _master) {
		return;
	}

	const CreatureP& previousMaster = _master;
	_master = master;

	if (master != nullptr) {
		setDropLoot(LOOT_DROP_NONE);
		setLossSkill(false);

		removeFromRaid();
		removeFromSpawn();
	}
	else {
		setDropLoot(LOOT_DROP_NONE);
		setLossSkill(false);
	}

	follow(nullptr);
	target(nullptr);

	notifyMasterChanged(previousMaster);
}


void Monster::setRetargetDelay(Duration retargetDelay) {
	_retargetDelay = retargetDelay;
}


bool Monster::shouldBeRemoved() const {
	if (despawn()) {
		return true;
	}

	return false;
}


bool Monster::shouldTeleportToMaster() const {
	if (!hasMaster()) {
		return false;
	}

	ConfigManager& config = server.configManager();
	if (!config.getBool(ConfigManager::TELEPORT_SUMMONS) && (_master->getPlayer() == nullptr || !config.getBool(ConfigManager::TELEPORT_PLAYER_SUMMONS))) {
		return false;
	}

	if (isMasterInRange()) {
		return false;
	}

	return true;
}


// TODO move to creature
bool Monster::target(const CreatureP& creature) {
	if (creature == this || creature == _master) {
		if (attackedCreature == nullptr) {
			return true;
		}

		return setAttackedCreature(nullptr);
	}

	if (creature == attackedCreature) {
		return true;
	}

	if (creature == nullptr) {
		return setAttackedCreature(nullptr);
	}
	else {
		if (!setAttackedCreature(creature.get())) {
			return false;
		}

		setRetargetDelay(_type->retargetInterval);
		server.dispatcher().addTask(Task::create(std::bind(&Game::checkCreatureAttack, &server.game(), getID())));
	}

	return true;
}


bool Monster::targetClosestEnemy() {
	Position position = getPosition();

	SpectatorList spectators;
	server.game().getSpectators(spectators, position);

	uint32_t closestDistance = std::numeric_limits<uint32_t>::max();
	Creature* closestSpectator = nullptr;

	if (spectators.empty()) {
		target(nullptr);
		return false;
	}

	for (auto spectator : spectators) {
		if (!isEnemy(spectator)) {
			continue;
		}

		auto distance = position.distanceTo(spectator->getPosition());
		if (distance <= _type->targetDistance && distance < closestDistance) {
			closestSpectator = spectator;
			if (distance == 1) {
				break;
			}

			closestDistance = distance;
		}
	}

	if (closestSpectator == nullptr) {
		target(nullptr);
		return false;
	}

	return target(closestSpectator);
}


bool Monster::targetRandomEnemy() {
	Position position = getPosition();

	SpectatorList spectators;
	server.game().getSpectators(spectators, position);

	if (spectators.empty()) {
		target(nullptr);
		return false;
	}

	for (auto i = spectators.begin(); i != spectators.end(); ) {
		if (!isEnemy(*i)) {
			i = spectators.erase(i);
			continue;
		}

		++i;
	}

	if (spectators.empty()) {
		target(nullptr);
		return false;
	}

	auto randomEnemy = *std::next(spectators.begin(), random_range<uint32_t>(0, spectators.size() - 1));
	return target(randomEnemy);
}


bool Monster::teleportToMaster() {
	if (!hasMaster()) {
		return false;
	}

	Game& game = server.game();

	Position initialPosition = getPosition();
	if (game.internalTeleport(this, game.getClosestFreeTile(this, _master->getPosition(), true), false) != RET_NOERROR) {
		return false;
	}

	game.addMagicEffect(initialPosition, MAGIC_EFFECT_POFF);
	game.addMagicEffect(getPosition(), MAGIC_EFFECT_TELEPORT);

	return true;
}


void Monster::updateBabbling(Duration elapsedTime) {
	assert(elapsedTime >= Duration::zero());

	if (_babbleDelay <= Duration::zero()) {
		return;
	}

	if (elapsedTime >= _babbleDelay) {
		_babbleDelay = _type->babbleInterval;

		if (_type->babbleChance < random_range(1, 100)) {
			return;
		}

		babble();
	}
	else {
		_babbleDelay -= elapsedTime;
	}
}


void Monster::updateFollowing() {
	if (attackedCreature != nullptr) {
		follow(attackedCreature);
	}
	else if (hasMaster()) {
		follow(_master.get());
	}
	else {
		follow(nullptr);
	}
}


void Monster::updateMovement() {
	addEventWalk();
}


void Monster::updateRelationship() {
	if (shouldTeleportToMaster()) {
		teleportToMaster();
	}
}


void Monster::updateRetargeting(Duration elapsedTime) {
	assert(elapsedTime >= Duration::zero());

	if (_retargetDelay <= Duration::zero()) {
		return;
	}

	if (attackedCreature == nullptr) {
		return;
	}

	if (hasMaster()) {
		return;
	}

	if (elapsedTime >= _retargetDelay) {
		_retargetDelay = _type->retargetInterval;

		if (_type->retargetChance < random_range(1, 100)) {
			return;
		}

		retarget();
	}
	else {
		_retargetDelay -= elapsedTime;
	}
}


void Monster::updateTarget() {
	if (hasMaster()) {
		target(_master->getAttackedCreature());
	}
	else if (isFleeing()) {
		target(nullptr);
	}
	else if (attackedCreature == nullptr || (!attackedCreature->isAlive() || !isEnemy(attackedCreature))) {
		retarget();
	}
}


void Monster::willRemove() {
	killSummons();

	target(nullptr);
	follow(nullptr);

	release();
	removeFromSpawn();
	removeFromRaid();

	Creature::willRemove();
}





















AutoList<Monster>Monster::autoList;


Monster::Monster(MonsterType* __type, Raid* raid, Spawn* spawn)
	: Creature(),
	  _raid(nullptr),
	  _spawn(nullptr)
{
	_type = __type;
	defaultOutfit = _type->outfit;
	currentOutfit = _type->outfit;

	double multiplier = server.configManager().getDouble(ConfigManager::RATE_MONSTER_HEALTH);
	health = (int32_t)(_type->health * multiplier);
	healthMax = (int32_t)(_type->healthMax * multiplier);

	baseSpeed = _type->baseSpeed;
	internalLight.level = _type->lightLevel;
	internalLight.color = _type->lightColor;
	setSkull(_type->skull);
	setShield(_type->partyShield);

	hideName = _type->hideName, hideHealth = _type->hideHealth;

	minCombatValue = 0;
	maxCombatValue = 0;

	targetTicks = 0;
	attackTicks = 0;
	defenseTicks = 0;
	yellTicks = 0;
	extraMeleeAttack = false;
	resetTicks = false;

	// register creature events
	for(StringVector::iterator it = _type->scriptList.begin(); it != _type->scriptList.end(); ++it)
	{
		if(!registerCreatureEvent(*it))
			LOGe("[Monster::Monster] Unknown event name - " << *it);
	}
}














BlockType_t Monster::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense/* = false*/, bool checkArmor/* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor);
	if(!damage)
		return blockType;

	int32_t elementMod = 0;
	ElementMap::iterator it = _type->elementMap.find(combatType);
	if(it != _type->elementMap.end())
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


void Monster::doAttacking(uint32_t interval)
{
	if(!attackedCreature)
		return;

	bool updateLook = true;
	resetTicks = (interval != 0);
	attackTicks += interval;

	const Position& myPos = getPosition();
	for(SpellList::iterator it = _type->spellAttackList.begin(); it != _type->spellAttackList.end(); ++it)
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
	for(SpellList::iterator it = _type->spellAttackList.begin(); it != _type->spellAttackList.end(); ++it)
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

void Monster::onThinkDefense(uint32_t interval)
{
	resetTicks = true;
	defenseTicks += interval;
	for(SpellList::iterator it = _type->spellDefenseList.begin(); it != _type->spellDefenseList.end(); ++it)
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

	if(!hasMaster())
	{
		if(_type->maxSummons < 0 || (int32_t)summons.size() < _type->maxSummons)
		{
			for(SummonList::iterator it = _type->summonList.begin(); it != _type->summonList.end(); ++it)
			{
				if((int32_t)summons.size() >= _type->maxSummons)
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
				for(MonsterList::iterator cit = summons.begin(); cit != summons.end(); ++cit)
				{
					if(!(*cit)->isRemoved() && (*cit)->getMonster() &&
						(*cit)->getMonster()->getName() == it->name)
						typeCount++;
				}

				if(typeCount >= it->amount)
					continue;

				if((it->chance >= (uint32_t)random_range(1, 100)))
				{
					if(boost::intrusive_ptr<Monster> summon = Monster::create(it->name))
					{
						summon->setMaster(this);
						if(server.game().placeCreature(summon.get(), getPosition()))
							server.game().addMagicEffect(getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
						else
							summon->setMaster(nullptr);
					}
				}
			}
		}
	}

	if(resetTicks)
		defenseTicks = 0;
}


bool Monster::pushItem(Item* item, int32_t radius)
{
	typedef std::pair<int32_t, int32_t> PositionPair;
	typedef std::vector<PositionPair> PairVector;

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
	std::vector<Direction> dirVector;
	dirVector.push_back(Direction::NORTH);
	dirVector.push_back(Direction::SOUTH);
	dirVector.push_back(Direction::WEST);
	dirVector.push_back(Direction::EAST);

	std::random_shuffle(dirVector.begin(), dirVector.end());

	Tile* tile = nullptr;
	for(auto it = dirVector.begin(); it != dirVector.end(); ++it)
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
	if(!isThinking() || !isAlive())
	{
		//we dont have anyone watching might aswell stop walking
		eventWalk = 0;
		return false;
	}

	bool result = false;
	if((!followCreature || !hasFollowPath) && !hasMaster())
	{
		if(followCreature || getTimeSinceLastMove() > 1000) //choose a random direction
			result = getRandomStep(getPosition(), dir);
	}
	else if(hasMaster() || followCreature)
	{
		result = Creature::getNextStep(dir, flags);
		if(!result)
		{
			//target dancing
			if(attackedCreature && attackedCreature == followCreature)
			{
				if(isFleeing())
					result = getDanceStep(getPosition(), dir, false, false);
				else if(_type->staticAttackChance < (uint32_t)random_range(1, 100))
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
	}

	return result;
}

bool Monster::getRandomStep(const Position& creaturePos, Direction& dir)
{
	std::vector<Direction> dirVector;
	dirVector.push_back(Direction::NORTH);
	dirVector.push_back(Direction::SOUTH);
	dirVector.push_back(Direction::WEST);
	dirVector.push_back(Direction::EAST);

	std::random_shuffle(dirVector.begin(), dirVector.end());
	for(auto it = dirVector.begin(); it != dirVector.end(); ++it)
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

	uint32_t tmpDist, centerToDist = std::max(std::abs(static_cast<signed>(creaturePos.x) - static_cast<signed>(centerPos.x)), std::abs(static_cast<signed>(creaturePos.y) - static_cast<signed>(centerPos.y)));
	std::vector<Direction> dirVector;
	if(!keepDistance || creaturePos.y >= centerPos.y)
	{
		tmpDist = std::max(std::abs(static_cast<signed>(creaturePos.x) - static_cast<signed>(centerPos.x)), std::abs(static_cast<signed>(creaturePos.y - 1) - static_cast<signed>(centerPos.y)));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, Direction::NORTH))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x, creaturePos.y - 1, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(Direction::NORTH);
		}
	}

	if(!keepDistance || creaturePos.y <= centerPos.y)
	{
		tmpDist = std::max(std::abs(static_cast<signed>(creaturePos.x) - static_cast<signed>(centerPos.x)), std::abs(static_cast<signed>(creaturePos.y + 1) - static_cast<signed>(centerPos.y)));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, Direction::SOUTH))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x, creaturePos.y + 1, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(Direction::SOUTH);
		}
	}

	if(!keepDistance || creaturePos.x <= centerPos.x)
	{
		tmpDist = std::max(std::abs(static_cast<signed>(creaturePos.x + 1) - static_cast<signed>(centerPos.x)), std::abs(static_cast<signed>(creaturePos.y) - static_cast<signed>(centerPos.y)));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, Direction::EAST))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x + 1, creaturePos.y, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(Direction::EAST);
		}
	}

	if(!keepDistance || creaturePos.x >= centerPos.x)
	{
		tmpDist = std::max(std::abs(static_cast<signed>(creaturePos.x - 1) - static_cast<signed>(centerPos.x)), std::abs(static_cast<signed>(creaturePos.y) - static_cast<signed>(centerPos.y)));
		if(tmpDist == centerToDist && canWalkTo(creaturePos, Direction::WEST))
		{
			bool result = true;
			if(keepAttack)
				result = (!canDoAttackNow || canUseAttack(Position(creaturePos.x - 1, creaturePos.y, creaturePos.z), attackedCreature));

			if(result)
				dirVector.push_back(Direction::WEST);
		}
	}

	if(dirVector.empty())
		return false;

	dir = dirVector[random_range<uint32_t>(0, dirVector.size() - 1)];
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
		case Direction::NORTH:
			pos.y += -1;
			break;
		case Direction::WEST:
			pos.x += -1;
			break;
		case Direction::EAST:
			pos.x += 1;
			break;
		case Direction::SOUTH:
			pos.y += 1;
			break;

		case Direction::NORTH_EAST:
		case Direction::NORTH_WEST:
		case Direction::SOUTH_EAST:
		case Direction::SOUTH_WEST:
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


boost::intrusive_ptr<Item> Monster::createCorpse(DeathList deathList)
{
	boost::intrusive_ptr<Item> corpse = Creature::createCorpse(deathList);
	if(!corpse)
		return nullptr;

	if(_type->corpseUnique)
		corpse->setUniqueId(_type->corpseUnique);

	if(_type->corpseAction)
		corpse->setActionId(_type->corpseAction);

	DeathEntry ownerEntry = deathList[0];
	if(ownerEntry.isNameKill())
		return corpse;

	Creature* owner = ownerEntry.getKillerCreature();
	if(!owner)
		return corpse;

	uint32_t ownerId = 0;
	if(owner->getPlayer())
		ownerId = owner->getID();
	else if(owner->hasController())
		ownerId = owner->getController()->getID();

	if(ownerId)
		corpse->setCorpseOwner(ownerId);

	return corpse;
}

bool Monster::inDespawnRange(const Position& pos) const
{
	if(_spawn == nullptr || _type->isLureable)
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

bool Monster::despawn() const
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
				newDir = Direction::WEST;
			else
				newDir = Direction::EAST;
		}
		else if(std::abs(dx) < std::abs(dy))
		{
			//look NORTH/SOUTH
			if(dy < 0)
				newDir = Direction::NORTH;
			else
				newDir = Direction::SOUTH;
		}
		else if(dx < 0 && dy < 0)
		{
			if(getDirection() == Direction::SOUTH)
				newDir = Direction::WEST;
			else if(getDirection() == Direction::EAST)
				newDir = Direction::NORTH;
		}
		else if(dx < 0 && dy > 0)
		{
			if(getDirection() == Direction::NORTH)
				newDir = Direction::WEST;
			else if(getDirection() == Direction::EAST)
				newDir = Direction::SOUTH;
		}
		else if(dx > 0 && dy < 0)
		{
			if(getDirection() == Direction::SOUTH)
				newDir = Direction::EAST;
			else if(getDirection() == Direction::WEST)
				newDir = Direction::NORTH;
		}
		else if(getDirection() == Direction::NORTH)
			newDir = Direction::EAST;
		else if(getDirection() == Direction::WEST)
			newDir = Direction::SOUTH;
	}

	server.game().internalCreatureTurn(this, newDir);
}

void Monster::dropLoot(Container* corpse)
{
	if(corpse && lootDrop == LOOT_DROP_FULL)
		_type->dropLoot(corpse);
}

bool Monster::isImmune(CombatType_t type) const
{
	ElementMap::const_iterator it = _type->elementMap.find(type);
	if(it == _type->elementMap.end())
		return Creature::isImmune(type);

	return it->second >= 100;
}

void Monster::setNormalCreatureLight()
{
	internalLight.level = _type->lightLevel;
	internalLight.color = _type->lightColor;
}

void Monster::drainHealth(Creature* attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainHealth(attacker, combatType, damage);
	if(isInvisible())
		removeCondition(CONDITION_INVISIBLE);
}

void Monster::changeHealth(int32_t healthChange)
{
	Creature::changeHealth(healthChange);
}



void Monster::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.minTargetDist = 1;
	fpp.maxTargetDist = _type->targetDistance;
	if(hasMaster())
	{
		if(getMaster() == creature)
		{
			fpp.maxTargetDist = 2;
			fpp.fullPathSearch = true;
		}
		else if(_type->targetDistance <= 1)
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
	else if(_type->targetDistance <= 1)
		fpp.fullPathSearch = true;
	else
		fpp.fullPathSearch = !canUseAttack(getPosition(), creature);
}



const std::string& Monster::getName() const {return _type->name;}
const std::string& Monster::getNameDescription() const {return _type->nameDescription;}
std::string Monster::getDescription(int32_t lookDistance) const {return _type->nameDescription + ".";}

RaceType_t Monster::getRace() const {return _type->race;}
int32_t Monster::getArmor() const {return _type->armor;}
int32_t Monster::getDefense() const {return _type->defense;}
MonsterType* Monster::getMonsterType() const {return _type;}
bool Monster::isPushable() const {return _type->pushable && (baseSpeed > 0);}
bool Monster::isAttackable() const {return _type->isAttackable;}

bool Monster::canPushItems() const {return _type->canPushItems;}
bool Monster::canPushCreatures() const {return _type->canPushCreatures;}
bool Monster::isHostile() const {return _type->isHostile;}
bool Monster::isWalkable() const {return _type->isWalkable;}
bool Monster::canSeeInvisibility() const {return Creature::isImmune(CONDITION_INVISIBLE);}
uint32_t Monster::getManaCost() const {return _type->manaCost;}

bool Monster::isFleeing() const {return getHealth() <= _type->runAwayHealth;}

uint64_t Monster::getLostExperience() const {return ((skillLoss ? _type->experience : 0));}
uint32_t Monster::getDamageImmunities() const {return _type->damageImmunities;}
uint32_t Monster::getConditionImmunities() const {return _type->conditionImmunities;}
uint16_t Monster::getLookCorpse() {return _type->lookCorpse;}
uint16_t Monster::getLookCorpse() const {return _type->lookCorpse;}
