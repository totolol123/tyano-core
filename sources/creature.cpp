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
#include "world.h"


const Duration Creature::THINK_DURATION = Seconds(1);
const Duration Creature::THINK_INTERVAL = Milliseconds(100);

LOGGER_DEFINITION(Creature);


bool Creature::canFollow(const CreatureP& target) const {
	if (target == nullptr) {
		assert(target != nullptr);
		return false;
	}

	if (target == this) {
		return false;
	}

	if (!isThinking()) {
		return false;
	}

	if (!canStep()) {
		return false;
	}

	const Position& desination = target->getPosition();
	if (desination.z != getPosition().z || !canSee(desination)) {
		return false;
	}

	return true;
}


bool Creature::canMoveTo(Direction direction) const {
	if (direction == Direction::NONE) {
		return false;
	}

	if (!isAlive()) {
		return false;
	}

	auto tile = server.game().getNextTile(*_tile, direction);
	if (tile == nullptr) {
		return false;
	}

	return canMoveTo(*tile);
}


bool Creature::canMoveTo(const Position& position) const {
	auto tile = server.game().getTile(position);
	if (tile == nullptr) {
		return false;
	}

	return canMoveTo(*tile);
}


bool Creature::canMoveTo(const Tile& tile) const {
	if (!isAlive()) {
		return false;
	}

	if (cannotMove) {
		return false;
	}

	if (&tile == _tile) {
		return false;
	}

	if (tile.testAddCreature(*this, getMoveFlags()) != RET_NOERROR) {
		return false;
	}

	return true;
}


bool Creature::canStep() const {
	if (!isAlive()) {
		return false;
	}

	if (cannotMove) {
		return false;
	}

	if (getStepDuration() <= Duration::zero()) {
		return false;
	}

	return true;
}


void Creature::didEnterWorld(World& world) {
	// override in subclasses
}


void Creature::didExitWorld(World& world) {
	// override in subclasses
}


ReturnValue Creature::enterWorld(const Position& position) {
	return server.world().addCreature(this, position);
}


void Creature::exitWorld() {
	server.world().removeCreature(this);
}


PlayerP Creature::getController() {
	return getFinalOwner()->getPlayer();
}


PlayerPC Creature::getController() const {
	return getFinalOwner()->getPlayer();
}


CreatureP Creature::getDirectOwner() {
	return nullptr;
}


CreaturePC Creature::getDirectOwner() const {
	return nullptr;
}


CreatureP Creature::getFinalOwner() {
	CreatureP finalOwner;
	for (CreatureP nextOwner = this; nextOwner != nullptr; nextOwner = nextOwner->getDirectOwner()) {
		finalOwner = nextOwner;
	}

	return finalOwner;
}


CreaturePC Creature::getFinalOwner() const {
	CreaturePC finalOwner;
	for (CreaturePC nextOwner = this; nextOwner != nullptr; nextOwner = nextOwner->getDirectOwner()) {
		finalOwner = nextOwner;
	}

	return finalOwner;
}


CreatureP Creature::getFollowedCreature() const {
	return _followedCreature;
}


auto Creature::getId() const -> Id {
	return _id;
}


uint32_t Creature::getMoveFlags() const {
	return 0;
}


bool Creature::hasController() const {
	return (getController() != nullptr);
}


bool Creature::hasDirectOwner() const {
	return (getDirectOwner() != nullptr);
}


bool Creature::hasSomethingToThinkAbout() const {
	if (attackedCreature != nullptr) {
		return true;
	}

	if (!conditions.empty()) {
		return true;
	}

	for (const CreatureP& creature : server.game().getSpectators(getPosition())) {
		if (hasToThinkAboutCreature(creature)) {
			return true;
		}
	}

	return false;
}


bool Creature::hasToThinkAboutCreature(const CreaturePC& creature) const {
	assert(creature != nullptr);

	if (creature == attackedCreature) {
		return true;
	}

	return false;
}


bool Creature::isAlive() const {
	return (isInWorld() && health > 0 && _tile != nullptr);
}


bool Creature::isDead() const {
	return (health <= 0);
}


bool Creature::isDrunk() const {
	return hasCondition(CONDITION_DRUNK);
}


bool Creature::isFollowing() const {
	return (_followedCreature != nullptr);
}


bool Creature::isInWorld() const {
	return _inWorld;
}


bool Creature::isMonster() const {
	return (getMonster() != nullptr);
}


bool Creature::isNpc() const {
	return (getNpc() != nullptr);
}


bool Creature::isPlayer() const {
	return (getPlayer() != nullptr);
}


bool Creature::isRemoved() const {
	// legacy for ScriptEnviroment::getThingByUID and Player::isRemoved
	return !isInWorld();
}


bool Creature::isRouting() const {
	return !_route.empty();
}


bool Creature::isThinking() const {
	return (_thinkDuration > Duration::zero());
}


bool Creature::isWandering() const {
	return _wandering;
}


void Creature::killSummons() {
	auto summons = std::move(this->summons);
	for (const auto& summon : summons) {
		summon->changeHealth(-summon->getHealth(), nullptr); // FIXME: make a kill method
		summon->release();
	}
}


Time Creature::getNextMoveTime() const {
	return _nextMoveTime;
}


uint32_t Creature::getPreferredFollowDistance() const {
	return 1;
}


Direction Creature::getRandomStepDirection(bool includesNone) const {
	if (!isAlive()) {
		return Direction::NONE;
	}

	Game& game = server.game();

	std::vector<Direction> directions(9);
	directions.push_back(Direction::EAST);
	directions.push_back(Direction::NORTH);
	directions.push_back(Direction::NORTH_EAST);
	directions.push_back(Direction::NORTH_WEST);
	directions.push_back(Direction::SOUTH);
	directions.push_back(Direction::SOUTH_EAST);
	directions.push_back(Direction::SOUTH_WEST);
	directions.push_back(Direction::WEST);
	if (includesNone) {
		directions.push_back(Direction::NONE);
	}

	std::random_shuffle(directions.begin(), directions.end());

	for (Direction direction : directions) {
		if (direction == Direction::NONE) {
			return direction;
		}

		Tile* tile = game.getNextTile(*_tile, direction);
		if (tile == nullptr) {
			continue;
		}
		if (!canMoveTo(*tile)) {
			continue;
		}

		return direction;
	}

	return Direction::NONE;
}


auto Creature::getRoute() const -> const Route& {
	return _route;
}


Direction Creature::getWanderingDirection() const {
	return getRandomStepDirection();
}


Duration Creature::getWanderingInterval() const {
	return Duration::zero();
}


bool Creature::moveTo(Tile& tile) {
	if (!isAlive()) {
		return false;
	}

	if (!canMoveTo(tile)) {
		return false;
	}

	Tile& previousTile = *_tile;

	if (tile.addCreature(this, getMoveFlags()) != RET_NOERROR) {
		return false;
	}

	Direction direction = getDirectionTo(previousTile.getPosition(), tile.getPosition());

	_nextMoveTime = Clock::now() + getStepDuration(direction);
	onMove(previousTile, tile);

	return true;
}


void Creature::onCreatureAppear(const CreatureP& creature) {
	LOGt(this << "::onCreatureAppear(" << creature.get() << ")");

	assert(creature != nullptr);

	if (creature == this) {
		startThinking();
	}
	else if (hasToThinkAboutCreature(creature)) {
		startThinking(true);
	}
}


void Creature::onFollowingStarted() {
	LOGt(this << "::onFollowingStarted(" << _followedCreature.get() << ")");

	CreatureP followedCreature = _followedCreature;

	CreatureEventList events = getCreatureEvents(CREATURE_EVENT_FOLLOW);
	for (const auto& event : events) {
		event->executeFollow(this, followedCreature.get());
	}
}


void Creature::onFollowingStopped(bool preliminary) {
	LOGt(this << "::onFollowingStopped(preliminary = " << (preliminary ? "true" : "false") << ")");
}


void Creature::onCreatureMove(const CreatureP& creature, const Position& origin, Tile* originTile, const Position& destination, Tile* destinationTile, bool teleport) {
	assert(creature != nullptr);
	assert(originTile != nullptr);
	assert(destinationTile != nullptr);

	// TODO refactor
	if (creature == this) {
		setLastPosition(origin);

		if(!summons.empty())
		{
			MonsterList::iterator cit;
			std::list<Creature*> despawnList;
			for(cit = summons.begin(); cit != summons.end(); ++cit)
			{
				const Position pos = (*cit)->getPosition();
				if((std::abs(pos.z - destination.z) > 2) || (std::max(std::abs((
					destination.x) - pos.x), std::abs((destination.y - 1) - pos.y)) > 30))
					despawnList.push_back((*cit).get());
			}

			for (auto creature : despawnList) {
				creature->exitWorld();
			}
		}

		if(destinationTile->getZone() != originTile->getZone())
			onChangeZone(getZone());

		if (isRouting()) {
			auto iterator = std::find(_route.rbegin(), _route.rend(), destination);
			if (iterator == _route.rend()) {
				stopRouting();
			}
			else {
				_route.erase(_route.begin(), iterator.base());
				if (_route.empty()) {
					onRoutingStopped(false);
				}
			}
		}
	}
	else if (creature == _followedCreature) {
		if (canFollow(creature)) {
			if (isRouting()) {
				auto iterator = std::find(_route.begin(), _route.end(), destination);
				if (iterator == _route.end()) {
					stopRouting();
				}
				else {
					_route.erase(iterator, _route.end());
					if (_route.empty()) {
						onRoutingStopped(false);
					}
				}
			}
			else if (getPosition().distanceTo(destination) > 1) {
				_needsNewRouteToFollowedCreature = true;
			}
		}
		else {
			stopFollowing(true);
		}
	}
	else {
		if (isFollowing() && !isRouting()) {
			// someone may have blocked us earlier and the way may now be free
			_needsNewRouteToFollowedCreature = true;
		}
	}

	if(creature == attackedCreature || (creature == this && attackedCreature))
	{
		if(destination.z == origin.z && canSee(attackedCreature->getPosition()))
		{
			if(hasExtraSwing()) //our target is moving lets see if we can get in hit
				server.dispatcher().addTask(Task::create(
					std::bind(&Game::checkCreatureAttack, &server.game(), getId())));

			if(destinationTile->getZone() != originTile->getZone())
				onAttackedCreatureChangeZone(attackedCreature->getZone());
		}
		else
			internalCreatureDisappear(attackedCreature, false);
	}

	if (creature == this) {
		startThinking();
	}
	else if (canSee(creature->getPosition()) && hasToThinkAboutCreature(creature)) {
		startThinking(true);
	}
}


void Creature::onMonsterMasterChanged(const MonsterP& monster, const CreatureP& previousMaster) {
	assert(monster != nullptr);

	if (monster->getMaster() == this) {
		addSummon(monster);
	}
	else if (previousMaster == this) {
		removeSummon(monster);
	}

	if (monster == this || previousMaster == this) {
		startThinking();
	}
	else if (canSee(monster->getPosition()) && hasToThinkAboutCreature(monster)) {
		startThinking(true);
	}
	else if (previousMaster != nullptr && (canSee(previousMaster->getPosition()) && hasToThinkAboutCreature(previousMaster))) {
		startThinking(true);
	}
}


void Creature::onMove(Tile& originTile, Tile& destinationTile) {
	// override in subclasses
}


void Creature::onRoutingStarted() {
	if (logger->isTraceEnabled()) {
		std::ostringstream positions;
		for (Position position : _route) {
			if (positions.tellp() > 0) {
				positions << ", ";
			}

			positions << position;
		}

		LOGt(this << "::onRoutingStarted(" << positions.str() << ")");
	}
}


void Creature::onRoutingStopped(bool preliminary) {
	LOGt(this << "::onRoutingStopped(preliminary = " << (preliminary ? "true" : "false") << ")");

	if (isFollowing()) {
		_needsNewRouteToFollowedCreature = true;
	}
}


void Creature::onThinkingStarted() {
	startWandering();
}


void Creature::onThinkingStopped() {
	stopFollowing(false);
	stopRouting();
	stopWandering();

	healMap.clear();
	damageMap.clear();
}


void Creature::onWanderingStarted() {
	// override in subclasses
}


void Creature::onWanderingStopped() {
	// override in subclasses
}


void Creature::releaseSummons() {
	auto summons = std::move(this->summons);
	for (const auto& summon : summons) {
		summon->release();
	}
}


Direction Creature::route() {
	if (!isRouting()) {
		return Direction::NONE;
	}

	if (!canStep()) {
		return Direction::NONE;
	}

	Position nextPosition = _route.front();
	if (getPosition().distanceTo(nextPosition) != 1) {
		LOGw(this << " routing broken. Check this!");

		stopRouting();
		return Direction::NONE;
	}

	Tile* nextTile = server.game().getTile(nextPosition);
	if (nextTile == nullptr) {
		stopRouting();
		return Direction::NONE;
	}

	if (!moveTo(*nextTile)) {
		stopRouting();
		return Direction::NONE;
	}

	return direction;
}


void Creature::setDefaultOutfit(Outfit_t defaultOutfit) {
	this->defaultOutfit = defaultOutfit;
}


void Creature::setId(Id id) {
	assert((id != 0) != (_id != 0));

	_id = id;
}


void Creature::setInWorld(bool inWorld) {
	_inWorld = inWorld;
}


bool Creature::shouldStagger() const {
	// 25% chance when drunk
	return (isDrunk() && random_range(1, 100) <= 25);
}


Direction Creature::stagger() {
	if (!canStep()) {
		return Direction::NONE;
	}

	server.game().internalCreatureSay(this, SPEAK_MONSTER_SAY, "Hicks!", isGhost());

	Direction direction = getRandomStepDirection();
	if (!stepInDirection(direction)) {
		return Direction::NONE;
	}

	return direction;
}


bool Creature::startFollowing(const CreatureP& target) {
	if (!canFollow(target)) {
		stopFollowing(true);
		return false;
	}

	if (target == _followedCreature) {
		return true;
	}

	stopRouting();

	_followedCreature = target;
	_needsNewRouteToFollowedCreature = (target != nullptr);

	onFollowingStarted();
	return true;
}


bool Creature::startRouting(const DirectionRoute& directionRoute) {
	if (!isThinking()) {
		stopRouting();
		return false;
	}

	stopRouting();

	if (directionRoute.empty()) {
		_route.clear();
	}
	else {
		Route route;

		Position position = getPosition();
		for (Direction direction : directionRoute) {
			position = getNextPosition(direction, position);
			route.push_back(position);
		}

		_route = route;

		onRoutingStarted();
	}

	return true;
}


bool Creature::startThinking(bool forced) {
	if (isThinking()) {
		return true;
	}

	if (!isAlive()) {
		return false;
	}

	if (!forced && !hasSomethingToThinkAbout()) {
		return false;
	}

	_previousThinkTime = Clock::now();
	_thinkDuration = THINK_DURATION;

	server.dispatcher().addTask(Task::create(std::bind(&Creature::think, CreatureP(this))));

	onThinkingStarted();
	return true;
}


bool Creature::startWandering() {
	if (!isThinking()) {
		stopWandering();
		return false;
	}

	if (_wandering) {
		return true;
	}

	Duration interval = getWanderingInterval();
	if (interval <= Duration::zero()) {
		return false;
	}

	_wandering = true;
	_nextWanderingTime = Clock::now() + interval;

	onWanderingStarted();
	return true;
}


void Creature::stopFollowing() {
	stopFollowing(false);
}


void Creature::stopFollowing(bool preliminary) {
	if (_followedCreature == nullptr) {
		return;
	}

	_followedCreature = nullptr;
	_needsNewRouteToFollowedCreature = false;

	stopRouting();

	onFollowingStopped(preliminary);
}


void Creature::stopRouting() {
	if (_route.empty()) {
		return;
	}

	_route.clear();
	onRoutingStopped(true);
}


void Creature::stopThinking() {
	if (!isThinking()) {
		return;
	}

	if (_thinkTaskId != 0) {
		server.scheduler().cancelTask(_thinkTaskId);
		_thinkTaskId = 0;
	}

	_thinkDuration = Duration::zero();
	onThinkingStopped();
}


void Creature::stopWandering() {
	if (!_wandering) {
		return;
	}

	_wandering = false;
	onWanderingStopped();
}


bool Creature::stepInDirection(Direction direction) {
	if (direction == Direction::NONE) {
		return false;
	}

	Tile* nextTile = server.game().getNextTile(*_tile, direction);
	if (nextTile == nullptr) {
		return false;
	}

	return moveTo(*nextTile);
}


void Creature::think() {
	if (!isThinking()) {
		return;
	}

	Time thinkTime = Clock::now();

	Duration elapsedTime = thinkTime - _previousThinkTime;
	_previousThinkTime = thinkTime;
	_thinkTaskId = 0;

	if (!isAlive()) {
		stopThinking();
		return;
	}

	onThink(elapsedTime);

	if (!isAlive()) {
		stopThinking();
		return;
	}

	if (_thinkDuration > elapsedTime) {
		_thinkDuration -= elapsedTime;
	}
	else if (hasSomethingToThinkAbout()) {
		_thinkDuration = THINK_DURATION;
	}
	else {
		stopThinking();
		return;
	}

	if (_thinkTaskId == 0) {
		_thinkTaskId = server.scheduler().addTask(SchedulerTask::create(THINK_INTERVAL, std::bind(&Creature::think, CreatureP(this))));
	}
}


void Creature::updateFollowing() {
	if (!_needsNewRouteToFollowedCreature) {
		return;
	}

	_needsNewRouteToFollowedCreature = false;

	if (!isFollowing()) {
		return;
	}

	if (!_followedCreature->isAlive()) {
		stopFollowing(false);
		return;
	}

	if (getPosition().distanceTo(_followedCreature->getPosition()) == getPreferredFollowDistance()) {
		return;
	}

	FindPathParams parameters;
	getPathSearchParams(_followedCreature.get(), parameters);

	DirectionRoute route;
	if (server.game().getPathToEx(this, _followedCreature->getPosition(), route, parameters)) {
		startRouting(route);
	}
	else {
		stopRouting();
	}

	// set to true by onRoutingStopped()
	_needsNewRouteToFollowedCreature = false;
}


void Creature::updateMovement(Time now) {
	if (_nextMoveTime > now) {
		return;
	}

	if (isRouting()) {
		if (shouldStagger()) {
			stagger();
			return;
		}

		route();
		return;
	}

	if (isWandering()) {
		if (_nextWanderingTime > now) {
			return;
		}

		if (shouldStagger()) {
			stagger();
			return;
		}

		wander();
		return;
	}
}


Direction Creature::wander() {
	if (!isWandering()) {
		return Direction::NONE;
	}

	if (!canStep()) {
		return Direction::NONE;
	}

	Direction direction = getWanderingDirection();
	if (!stepInDirection(direction)) {
		_nextWanderingTime = Clock::now() + Seconds(1);

		return Direction::NONE;
	}

	Duration interval = getWanderingInterval();
	if (interval > Duration::zero()) {
		_nextWanderingTime = Clock::now() + getWanderingInterval();
	}
	else {
		stopWandering();
	}

	return direction;
}


void Creature::willEnterWorld(World& world) {
	// override in subclasses
}


void Creature::willExitWorld(World& world) {
	stopThinking();
	releaseSummons();
}














Creature::Creature()
	: _id(0),
	  _inWorld(false),
	  _needsNewRouteToFollowedCreature(false),
	  _thinkTaskId(0),
	  _tile(nullptr),
	  _wandering(false)
{
	direction = Direction::SOUTH;
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

	baseSpeed = 220;
	varSpeed = 0;

	masterRadius = -1;
	masterPosition = Position();

	attackedCreature = nullptr;
	lastHitCreature = 0;
	lastDamageSource = COMBAT_NONE;
	blockCount = 0;
	blockTicks = 0;

	scriptEventsBitField = 0;
}


Creature::~Creature() {
	if (_thinkTaskId != 0) {
		server.scheduler().cancelTask(_thinkTaskId);
		_thinkTaskId = 0;
	}

	attackedCreature = nullptr;

	for (const auto& summon : summons) {
		summon->release();
	}

	for(ConditionList::iterator it = conditions.begin(); it != conditions.end(); ++it) {
		delete *it;
	}
}


void Creature::setDirection(Direction direction) {
	switch(direction) {
	case Direction::EAST:
	case Direction::NORTH:
	case Direction::SOUTH:
	case Direction::WEST:
		break;

	default:
		LOGe("Server tried to set the direction of " << this << " to '" << direction << "' which is not supported and will crash the client. I prevented that.");
		return;
	}

	this->direction = direction;
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

bool Creature::canSeeCreature(const CreatureP& creature) const
{
	return creature == this || (!creature->isGhost() && (!creature->isInvisible() || canSeeInvisibility()));
}


void Creature::onThink(Duration elapsedTime) {
	updateFollowing();
	updateMovement(Clock::now());

	if(attackedCreature && !canSeeCreature(attackedCreature))
		internalCreatureDisappear(attackedCreature, false);

	blockTicks += std::chrono::duration_cast<Milliseconds>(elapsedTime).count();
	if(blockTicks >= 1000)
	{
		blockCount = std::min((uint32_t)blockCount + 1, (uint32_t)2);
		blockTicks = 0;
	}

	if (!isAlive()) {
		return;
	}

	onAttacking(std::chrono::duration_cast<Milliseconds>(elapsedTime).count());
	executeConditions(std::chrono::duration_cast<Milliseconds>(elapsedTime).count());

	CreatureEventList thinkEvents = getCreatureEvents(CREATURE_EVENT_THINK);
	for(CreatureEventList::iterator it = thinkEvents.begin(); it != thinkEvents.end(); ++it)
		(*it)->executeThink(this, std::chrono::duration_cast<Milliseconds>(elapsedTime).count());
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

	if(server.game().isSightClear(getPosition(), attackedCreature->getPosition(), true))
		doAttacking(interval);
}


bool Creature::startAutoWalk(const DirectionRoute& route)
{
	if(getPlayer() && getPlayer()->getNoMove())
	{
		getPlayer()->sendCancelWalk();
		return false;
	}

	startRouting(route);
	return true;
}

void Creature::internalCreatureDisappear(const Creature* creature, bool isLogout)
{
	if(attackedCreature == creature)
	{
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(isLogout);
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



void Creature::setParent(Cylinder* cylinder) {
	assert(isInWorld());

	Thing::setParent(cylinder);

	_tile = dynamic_cast<Tile*>(getParent());
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

void Creature::onCreatureDisappear(const Creature* creature)
{
	LOGt(this << "::onCreatureDisappear(" << creature << ")");

	if (creature == _followedCreature) {
		stopFollowing(true);
	}
	else {
		if (isFollowing() && !isRouting()) {
			// someone may have blocked us earlier and the way may now be free
			_needsNewRouteToFollowedCreature = true;
		}
	}

	internalCreatureDisappear(creature, true);
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
			else if(it->getKillerCreature()->getController())
				tmp = it->getKillerCreature()->getController().get();
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

	auto& world = server.world();
	for (auto& damageEntry : damageMap) {
		if (auto creature = world.getCreatureById(damageEntry.first)) {
			creature->onAttackedCreatureKilled(this);
		}
	}

	dropCorpse(deathList);

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

	if (server.game().internalAddItem(nullptr, tile, corpse.get(), INDEX_WHEREEVER, FLAG_NOLIMIT) == RET_TILEISFULL) {
		auto items = tile->getItemList();
		if (items != nullptr) {
			ItemP itemWithShortestDuration;
			int32_t minimumDuration = std::numeric_limits<int32_t>::max();

			for (auto item : *items) {
				if (item->getDecaying() != DECAYING_TRUE) {
					continue;
				}

				auto duration = item->getDuration();
				if (duration > 0 && duration < minimumDuration) {
					itemWithShortestDuration = item;
					minimumDuration = duration;
				}
			}

			if (itemWithShortestDuration != nullptr) {
				if (server.game().internalRemoveItem(nullptr, itemWithShortestDuration.get(), 1) == RET_NOERROR) {
					server.game().internalAddItem(nullptr, tile, corpse.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
				}
			}
		}
	}

	dropLoot(corpse->getContainer());
	server.game().startDecay(corpse.get());
}

DeathList Creature::getKillers() {
	auto& world = server.world();

	DeathList list;
	CreatureP lhc = nullptr;
	if(!(lhc = world.getCreatureById(lastHitCreature)))
		list.push_back(DeathEntry(getCombatName(lastDamageSource), 0));
	else
		list.push_back(DeathEntry(lhc.get(), 0));

	int32_t requiredTime = server.configManager().getNumber(ConfigManager::DEATHLIST_REQUIRED_TIME);
	int64_t now = OTSYS_TIME();

	CountBlock_t cb;
	for(CountMap::const_iterator it = damageMap.begin(); it != damageMap.end(); ++it)
	{
		cb = it->second;
		if((now - cb.ticks) > requiredTime)
			continue;

		CreatureP mdc = world.getCreatureById(it->first);
		if(!mdc || mdc == lhc || (lhc && (mdc->getDirectOwner() == lhc || lhc->getDirectOwner() == mdc)))
			continue;

		bool deny = false;
		for(DeathList::iterator fit = list.begin(); fit != list.end(); ++fit)
		{
			if(fit->isNameKill())
				continue;

			Creature* tmp = fit->getKillerCreature();
			if(!(mdc->getName() == tmp->getName() && mdc->getDirectOwner() == tmp->getDirectOwner()) &&
				(!mdc->getDirectOwner() || (mdc->getDirectOwner() != tmp && mdc->getDirectOwner() != tmp->getDirectOwner()))
				&& (mdc->getSummonCount() <= 0 || tmp->getDirectOwner() != mdc))
				continue;

			deny = true;
			break;
		}

		if(!deny)
			list.push_back(DeathEntry(mdc.get(), cb.total));
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

void Creature::changeHealth (int32_t healthChange, const CreatureP& actor) {
	if (!isAlive()) {
		return;
	}

	auto previousHealth = health;
	int32_t newHealth;

	if(healthChange > 0)
		newHealth = previousHealth + std::min(healthChange, getMaxHealth() - previousHealth);
	else
		newHealth = std::max((int32_t)0, previousHealth + healthChange);

	if (newHealth == previousHealth) {
		return;
	}

	health = newHealth;

	server.game().addCreatureHealth(this);

	if (actor != nullptr) {
		auto healthChange = newHealth - previousHealth;
		if (healthChange > 0) {
			actor->onTargetCreatureGainHealth(this, healthChange);
		}
		else {
			actor->onAttackedCreatureDrainHealth(this, -healthChange);
		}
	}

	if (health == 0) {
		onDeath();
	}
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

void Creature::gainHealth(const CreatureP& caster, int32_t healthGain) {
	if (healthGain == 0) {
		return;
	}
	if (!isAlive()) {
		return;
	}

	if (healthGain < 0) {
		drainHealth(caster, COMBAT_LIFEDRAIN, -healthGain);
		return;
	}

	changeHealth(healthGain, caster);
}


void Creature::drainHealth(const CreatureP& attacker, CombatType_t combatType, int32_t damage) {
	if (damage == 0) {
		return;
	}
	if (!isAlive()) {
		return;
	}

	if (damage < 0) {
		gainHealth(attacker, -damage);
		return;
	}

	lastDamageSource = combatType;
	onAttacked();

	changeHealth(-damage, attacker);
}


void Creature::drainMana(const CreatureP& attacker, CombatType_t combatType, int32_t damage)
{
	lastDamageSource = combatType;
	onAttacked();

	changeMana(-damage);
	if(attacker)
		attacker->onAttackedCreatureDrainMana(this, damage);
}

BlockType_t Creature::blockHit(const CreatureP& attacker, CombatType_t combatType, int32_t& damage,
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

bool Creature::setAttackedCreature(Creature* creature) {
	LOGt(*this << "::setAttackedCreature(" << creature << ")");

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

		startThinking(true);

		for (const auto& summon : summons) {
			summon->startThinking();
		}
	}

	return true;
}

void Creature::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	fpp.fullPathSearch = _followedCreature != nullptr;
	fpp.clearSight = true;
	fpp.maxSearchDist = 12;
	fpp.minTargetDist = fpp.maxTargetDist = 1;
}


Position Creature::getPosition() const {
	if (_tile == nullptr) {
		return lastPosition;
	}

	return _tile->getPosition();
}

double Creature::getDamageRatio(Creature* attacker) const
{
	double totalDamage = 0, attackerDamage = 0;
	for(CountMap::const_iterator it = damageMap.begin(); it != damageMap.end(); ++it)
	{
		totalDamage += it->second.total;
		if(it->first == attacker->getId())
			attackerDamage += it->second.total;
	}

	return attackerDamage / totalDamage;
}

void Creature::addDamagePoints(Creature* attacker, int32_t damagePoints)
{
	uint32_t attackerId = 0;
	if(attacker)
		attackerId = attacker->getId();

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
		casterId = caster->getId();

	CountMap::iterator it = healMap.find(casterId);
	if(it != healMap.end())
	{
		it->second.ticks = OTSYS_TIME();
		it->second.total += healthPoints;
	}
	else
		healMap[casterId] = CountBlock_t(healthPoints);
}

void Creature::onAddCondition(ConditionType_t type, bool hadCondition) {
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
	if (!isAlive()) {
		_remove = true;
		return;
	}

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

void Creature::onCombatRemoveCondition(const CreatureP& attacker, Condition* condition)
{
	removeCondition(condition);
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

	CreatureP directOwner = getDirectOwner();
	if (directOwner != nullptr) {
		ret = directOwner->onKilledCreature(target, flags);
	}

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

	CreatureP directOwner = getDirectOwner();
	if (directOwner != nullptr) {
		gainExp = gainExp / 2;
		directOwner->onGainExperience(gainExp, fromMonster, multiplied);
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

	CreatureP directOwner = getDirectOwner();
	if (directOwner != nullptr) {
		gainExp = gainExp / 2;
		directOwner->onGainSharedExperience(gainExp, fromMonster, multiplied);
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


void Creature::addSummon(const MonsterP& creature) {
	summons.push_back(creature);
}


void Creature::removeSummon(const MonsterP& creature) {
	auto i = std::find(summons.begin(), summons.end(), creature);
	if (i != summons.end()) {
		summons.erase(i);
	}
}


bool Creature::addCondition(Condition* condition)
{
	if(!condition)
		return false;

	if(isSuppress(condition->getType()))
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

		startThinking(true);
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

void Creature::removeCondition(const CreatureP& attacker, ConditionType_t type)
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

Duration Creature::getStepDuration(Direction dir) const
{
	if(dir == Direction::NORTH_WEST || dir == Direction::NORTH_EAST || dir == Direction::SOUTH_WEST || dir == Direction::SOUTH_EAST)
		return getStepDuration() * 2;

	return getStepDuration();
}

Duration Creature::getStepDuration() const
{
	uint32_t stepSpeed = getStepSpeed();
	if(!stepSpeed)
		return Duration::zero();

	const Tile* tile = getTile();
	if(!tile || !tile->ground)
		return Duration::zero();

	return Milliseconds((1000 * tile->ground->getKind()->speed) / stepSpeed);
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

CreatureEventList Creature::getCreatureEvents(CreatureEventType_t type) const
{
	CreatureEventList retList;
	if(!hasEventRegistered(type))
		return retList;

	for(auto it = eventsList.cbegin(); it != eventsList.cend(); ++it)
	{
		if((*it)->getEventType() == type)
			retList.push_back(*it);
	}

	return retList;
}


ZoneType_t Creature::getZone() const {
	if (_tile == nullptr) {
		return ZONE_NORMAL;
	}

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



std::ostream& operator << (std::ostream& stream, const Creature* creature) {
	if (creature != nullptr) {
		return stream << *creature;
	}
	else {
		return stream << "Creature(null)";
	}
}


std::ostream& operator << (std::ostream& stream, const Creature& creature) {
	return stream << "Creature('" << creature.getName() << "', " << creature.getId() << ")";
}

