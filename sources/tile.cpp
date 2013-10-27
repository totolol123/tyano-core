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

#include "tile.h"
#include "housetile.h"

#include "player.h"
#include "creature.h"

#include "teleporter.h"
#include "trashholder.h"
#include "mailbox.h"

#include "combat.h"
#include "items.h"
#include "monster.h"
#include "movement.h"

#include "game.h"
#include "configmanager.h"
#include "server.h"
#include "world.h"


LOGGER_DEFINITION(Tile);


ReturnValue Tile::addCreature(const CreatureP& creature, uint32_t flags, const CreatureP& actor) {
	assert(creature != nullptr);
	assert(creature->isInWorld());

	if (_lockCount > 0) {
		return RET_LOCKED;
	}

	auto previousTile = creature->getTile();
	if (previousTile == this) {
		return RET_NOERROR;
	}

	if (previousTile != nullptr) {
		auto result = previousTile->testRemoveCreature(*creature);
		if (result != RET_NOERROR) {
			return result;
		}
	}

	auto result = testAddCreature(*creature, flags);
	if (result != RET_NOERROR) {
		return result;
	}

	uint32_t numberOfScriptsCalled = 0;
	{
		Lock lock(this);
		Lock lockPrevious(previousTile);

		result = server.moveEvents().willAddCreature(*this, creature, actor, &numberOfScriptsCalled);
		if (result != RET_NOERROR) {
			return result;
		}
	}

	if (numberOfScriptsCalled > 0) {
		// environment may have changed
		result = testAddCreature(*creature, flags);
		if (result != RET_NOERROR) {
			return result;
		}
	}

	auto& game = server.game();

	if (isForwarder()) {
		auto destinationTile = getCreatureForwardingTile(*creature);
		if (destinationTile == nullptr) {
			return RET_DESTINATIONOUTOFREACH;
		}

		result = destinationTile->addCreature(creature, FLAG_NOLIMIT|FLAG_IGNOREBLOCKCREATURE, actor);
		if (result != RET_NOERROR) {
			return result;
		}

		if (isTeleporter()) {
			game.addMagicEffect(pos, MAGIC_EFFECT_TELEPORT, creature->isGhost());
			game.addMagicEffect(destinationTile->pos, MAGIC_EFFECT_TELEPORT, creature->isGhost());
		}

		return RET_NOERROR;
	}

	bool teleported = false;

	StackPosition previousPosition;
	SpectatorList spectators;

	if (previousTile != nullptr) {
		auto previousIndex = previousTile->__getIndexOfThing(creature.get()); // FIXME remove .get()
		assert(previousIndex >= 0);

		previousPosition = StackPosition(previousTile->pos, previousIndex);

		game.getSpectators(spectators, previousTile->pos, false, true);

		if (previousPosition.z != pos.z || previousPosition.distanceTo(pos) > 1) {
			teleported = true;
		}
		else {
			Direction newDirection = Direction::EAST;
			if (previousPosition.x < pos.x) {
				// stay east
			}
			else if (previousPosition.x > pos.x) {
				newDirection = Direction::WEST;
			}
			else if (previousPosition.y > pos.y) {
				newDirection = Direction::NORTH;
			}
			else if (previousPosition.y < pos.y) {
				newDirection = Direction::SOUTH;
			}

			if (newDirection != creature->getDirection()) {
				creature->setDirection(newDirection);

				for (auto spectator : spectators) {
					if (auto player = spectator->getPlayer()) {
						if (player->canSeeCreature(*creature)) {
							player->sendCreatureTurn(creature.get());  // FIXME remove .get()
						}
					}
				}
			}
		}
	}
	else {
		spectators.push_back(creature.get()); // FIXME remove .get()
	}
	game.getSpectators(spectators, pos, true, true);

	auto creatures = makeCreatures();
	creatures->push_back(creature);

	++thingCount;

	const StackPosition newPosition(pos, __getIndexOfThing(creature.get())); // FIXME remove .get()

	std::vector<int32_t> previousIndexes;
	if (previousTile != nullptr) {
		for (auto spectator : spectators) {
			if (auto player = spectator->getPlayer()) {
				previousIndexes.push_back(previousTile->getClientIndexOfThing(player, creature.get())); // FIXME remove .get()
			}
		}

		auto previousCreatures = previousTile->getCreatures();
		assert(previousCreatures != nullptr);

		auto iterator = std::find(previousCreatures->begin(), previousCreatures->end(), creature);
		assert(iterator != previousCreatures->end());
		previousCreatures->erase(iterator);

		creature->setLastPosition(previousPosition.withoutIndex());

		--previousTile->thingCount;
	}
	else {
		creature->setLastPosition(pos);
	}

	game.clearSpectatorCache();
	creature->setParent(this);

	if (previousTile != nullptr) {
		LOGt(creature << " moved from " << previousPosition << " to " << newPosition << ".");
	}
	else {
		LOGt(creature << " added to " << newPosition << ".");
	}

	game.getMap()->onCreatureMoved(creature.get(), previousTile, this); // FIXME remove .get()

	if (previousTile != nullptr) {
		size_t i = 0;
		for (auto spectator : spectators) {
			if (auto player = spectator->getPlayer()) {
				if (player->canSeeCreature(*creature)) {
					player->sendCreatureMove(creature, this, newPosition, previousTile, previousPosition, previousIndexes[i], teleported, "Tile::addCreature(move)");
				}

				++i;
			}
		}

		previousTile->postRemoveNotification(actor.get(), creature.get(), this, previousPosition.index, true); // FIXME remove .get()

		for (auto spectator : spectators) {
			spectator->onCreatureMove(creature, previousPosition, previousTile, newPosition, this, teleported);
		}
	}
	else {
		for (auto spectator : spectators) {
			if (auto player = spectator->getPlayer()) {
				if (player->canSeeCreature(*creature)) {
					player->sendCreatureAppear(creature, "Tile::addCreature(add)"); // FIXME remove .get()
				}
			}
		}

		for (auto spectator : spectators) {
			spectator->onCreatureAppear(creature);
		}
	}

	postAddNotification(actor.get(), creature.get(), this, newPosition.index); // FIXME remove .get()

	return RET_NOERROR;
}


Tile* Tile::getAvailableItemForwardingTile(const Item& item) const {
	static const uint32_t MAXIMUM_DISTANCE = 10;

	if (!isForwarder()) {
		return nullptr;
	}

	auto destination = getForwardingDestination();
	if (!destination.isValid()) {
		LOGe("Forwarder " << this->getTeleporter()->getId() << " at position " << getPosition() << " leads to invalid position.");
		return nullptr;
	}

	if (destination == getPosition()) {
		LOGe("Forwarder " << this->getTeleporter()->getId() << " at position " << getPosition() << " leads to itself.");
		return nullptr;
	}

	auto& game = server.game();

	auto tile = game.getTile(destination);
	if (tile == nullptr) {
		LOGe("Forwarder " << this->getTeleporter()->getId() << " at position " << getPosition() << " leads to unreachable destination " << destination << ".");
		return nullptr;
	}

	// try destination tile first
	if (!tile->isForwarder() && tile->__queryAdd(INDEX_WHEREEVER, &item, 1, 0) == RET_NOERROR) {
		return tile;
	}

	// try neighbor tiles in random order
	for (uint32_t distance = 1; distance <= MAXIMUM_DISTANCE; ++distance) {
		auto alternativeTiles = tile->neighbors(distance);
		if (!alternativeTiles.empty()) {
			std::random_shuffle(alternativeTiles.begin(), alternativeTiles.end());

			for (auto alternativeTile : alternativeTiles) {
				if (alternativeTile->isForwarder()) {
					continue;
				}
				if (alternativeTile->__queryAdd(INDEX_WHEREEVER, &item, 1, 0) != RET_NOERROR) {
					continue;
				}

				return alternativeTile;
			}
		}
	}

	// give up!
	return nullptr;
}


Position Tile::getForwardingDestination() const {
	if (isLocalForwarder()) {
		int8_t offsetX = 0;
		int8_t offsetY = 0;
		int8_t offsetZ = 0;

		if (hasFlag(TILESTATE_FLOORCHANGE_DOWN)) {
			offsetZ = 1;
		}
		else {
			offsetZ = -1;
		}

		bool resolved = true;
		if (hasFlag(TILESTATE_FLOORCHANGE_NORTH)) {
			offsetY = -1;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_SOUTH)) {
			offsetY = 1;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_EAST)) {
			offsetX = 1;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_WEST)) {
			offsetX = -1;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_NORTH_EX)) {
			offsetY = -2;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_SOUTH_EX)) {
			offsetY = 2;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_EAST_EX)) {
			offsetX = 2;
		}
		else if (hasFlag(TILESTATE_FLOORCHANGE_WEST_EX)) {
			offsetX = -2;
		}
		else if (!hasFlag(TILESTATE_FLOORCHANGE_RESOLVED)) {
			resolved = false;
		}

		auto position = pos;
		auto destinationX = position.x + offsetX;
		auto destinationY = position.y + offsetY;
		auto destinationZ = position.z + offsetZ;

		if (!Position::isValid(destinationX, destinationY, destinationZ)) {
			return Position::INVALID;
		}

		Position destination(static_cast<uint16_t>(destinationX), static_cast<uint16_t>(destinationY), static_cast<uint8_t>(destinationZ));
		if (!resolved) {
			// FIXME this floorchange tile state is nonsense and the way of resolving directions too!
			auto newThis = const_cast<Tile*>(this);

			newThis->setFlag(TILESTATE_FLOORCHANGE_RESOLVED);

			auto destinationTile = server.game().getTile(destination);
			if (destinationTile != nullptr && destinationTile->isLocalForwarder()) {
				if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_NORTH)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_SOUTH);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_SOUTH)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_NORTH);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_EAST)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_WEST);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_WEST)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_EAST);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_NORTH_EX)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_SOUTH_EX);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_SOUTH_EX)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_NORTH_EX);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_EAST_EX)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_WEST_EX);
				}
				else if (destinationTile->hasFlag(TILESTATE_FLOORCHANGE_WEST_EX)) {
					newThis->setFlag(TILESTATE_FLOORCHANGE_EAST_EX);
				}

				return getForwardingDestination();
			}
		}

		return destination;
	}

	auto teleporter = getTeleporter();
	if (teleporter != nullptr) {
		return teleporter->getDestination();
	}

	return Position::INVALID;
}


Tile* Tile::getCreatureForwardingTile(const Creature& creature) const {
	static const uint32_t MAXIMUM_DISTANCE = 10;

	auto tile = getForwardingDestinationTile();
	if (tile == nullptr) {
		return nullptr;
	}

	auto moveFlags = creature.getMoveFlags();
	if (isLocalForwarder()) {
		moveFlags |= FLAG_IGNOREBLOCKCREATURE;
	}

	auto result = server.world().findTileForThingNearPosition(creature, tile->getPosition(), MAXIMUM_DISTANCE, moveFlags|FLAG_PATHFINDING, moveFlags|FLAG_IGNOREFIELDDAMAGE|FLAG_PATHFINDING);
	return result.getTile();
}


Tile* Tile::getForwardingDestinationTile() const {
	if (!isForwarder()) {
		return nullptr;
	}

	auto destination = getForwardingDestination();
	if (!destination.isValid()) {
		LOGe("Forwarder at position " << getPosition() << " leads to invalid position.");
		return nullptr;
	}

	if (destination == getPosition()) {
		LOGe("Forwarder at position " << getPosition() << " leads to itself.");
		return nullptr;
	}

	auto& game = server.game();

	auto tile = game.getTile(destination);
	if (tile == nullptr) {
		LOGe("Forwarder at position " << getPosition() << " leads to unreachable destination " << destination << ".");
		return nullptr;
	}

	return tile;
}


ItemP Tile::getGround() const {
	return ground;
}


bool Tile::isForwarder() const {
	return (isLocalForwarder() || isTeleporter());
}


bool Tile::isLocalForwarder() const {
	return hasFlag(TILESTATE_FLOORCHANGE);
}


bool Tile::isTeleporter() const {
	return (getTeleporter() != nullptr);
}


std::vector<Tile*> Tile::neighbors(uint16_t distance) const {
	std::vector<Tile*> neighbors;

	if (distance == 0) {
		return neighbors;
	}

	auto& game = server.game();

	auto position = getPosition();

	auto startOffset = -static_cast<int32_t>(distance);
	auto endOffset   = static_cast<int32_t>(distance);

	auto neighborPosition = getPosition();
	for (auto xOffset = startOffset; xOffset <= endOffset; ++xOffset) {
		auto x = position.x + xOffset;
		if (x < static_cast<int32_t>(Position::MIN_X) || x > static_cast<int32_t>(Position::MAX_X)) {
			continue;
		}

		neighborPosition.x = static_cast<int16_t>(x);

		int32_t yOffsetIncrease;
		if (xOffset == startOffset || xOffset == endOffset) {
			// we're on the left or the right border - go through all y tiles
			yOffsetIncrease = 1;
		}
		else {
			// we're somewhere in between - go through top and bottom tile only
			yOffsetIncrease = endOffset - startOffset;
		}

		for (auto yOffset = startOffset; yOffset <= endOffset; yOffset += yOffsetIncrease) {
			auto y = position.y + yOffset;
			if (y < static_cast<int32_t>(Position::MIN_Y) || y > static_cast<int32_t>(Position::MAX_Y)) {
				continue;
			}

			neighborPosition.y = static_cast<int16_t>(y);

			Tile* neighbor = game.getTile(neighborPosition);
			if (neighbor != nullptr) {
				neighbors.push_back(neighbor);
			}
		}
	}

	return neighbors;
}


ReturnValue Tile::removeCreature(const CreatureP& creature, const CreatureP& actor) {
	assert(creature != nullptr);

	auto result = testRemoveCreature(*creature);
	if (result != RET_NOERROR) {
		return result;
	}

	auto& game = server.game();

	auto index = __getIndexOfThing(creature.get()); // FIXME remove .get()
	assert(index >= 0);

	StackPosition position(pos, index);

	SpectatorList spectators;
	game.getSpectators(spectators, pos, false, true);

	std::vector<int32_t> previousIndexes;
	for (auto spectator : spectators) {
		if (auto player = spectator->getPlayer()) {
			previousIndexes.push_back(getClientIndexOfThing(player, creature.get())); // FIXME remove .get()
		}
	}

	auto creatures = getCreatures();
	assert(creatures != nullptr);

	auto iterator = std::find(creatures->begin(), creatures->end(), creature);
	assert(iterator != creatures->end());
	creatures->erase(iterator);

	creature->setLastPosition(position.withoutIndex());

	--thingCount;

	game.clearSpectatorCache();
	creature->setParent(nullptr);

	LOGt(creature << " removed from " << position << ".");

	game.getMap()->onCreatureMoved(creature.get(), this, nullptr); // FIXME remove .get()

	size_t i = 0;
	for (auto spectator : spectators) {
		if (auto player = spectator->getPlayer()) {
			if (player->canSeeCreature(*creature)) {
				player->sendCreatureDisappear(creature.get(), previousIndexes[i], "Tile::removeCreature()"); // FIXME remove .get()
			}

			++i;
		}
	}

	for (auto spectator : spectators) {
		spectator->onCreatureDisappear(creature.get()); // FIXME remove .get()
	}

	postRemoveNotification(actor.get(), creature.get(), nullptr, position.index, true); // FIXME remove .get()

	return RET_NOERROR;
}


ReturnValue Tile::testAddCreature(const Creature& creature, uint32_t flags) const {
	// First check whether the creature is allowed to be added in general. Then check whether the tile is full or occupied.
	// Teleporters take the information to check whether a creature is allowed to teleport and will choose an alternative destination
	// if the creature is allowed to be added in general but can't because the tile is full or occupied.

	if (_lockCount > 0) {
		return RET_LOCKED;
	}

	auto previousTile = creature.getTile();
	if (previousTile == this) {
		return RET_NOERROR;
	}

	if (ground == nullptr) {
		return RET_NOTPOSSIBLE;
	}

	auto monster = creature.getMonster();
	if (!hasBitSet(FLAG_NOLIMIT, flags)) {
		auto magicField = getFieldItem();
		if (magicField != nullptr && magicField->isBlocking(&creature)) {
			return RET_NOTPOSSIBLE;
		}

		if (monster != nullptr) {
			if (monster->isHostile()) {
				if (!hasBitSet(FLAG_IGNORE_PROTECTION_ZONE, flags) && hasFlag(TILESTATE_PROTECTIONZONE)) {
					if (previousTile == nullptr || !previousTile->hasFlag(TILESTATE_PROTECTIONZONE)) {
						return RET_NOTPOSSIBLE;
					}
				}
			}

			if (hasFlag(TILESTATE_IMMOVABLEBLOCKSOLID)) {
				return RET_NOTPOSSIBLE;
			}

			if (hasBitSet(FLAG_PATHFINDING, flags) && hasFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH)) {
				return RET_NOTPOSSIBLE;
			}

			bool blockedByItems = (hasFlag(TILESTATE_BLOCKSOLID) || (hasBitSet(FLAG_PATHFINDING, flags) && hasFlag(TILESTATE_NOFIELDBLOCKPATH)));
			if (blockedByItems) {
				if (!monster->canPushItems() || hasBitSet(FLAG_IGNOREBLOCKITEM, flags)) {
					return RET_NOTPOSSIBLE;
				}
			}

			if (magicField != nullptr && !hasBitSet(FLAG_IGNOREFIELDDAMAGE, flags)) {
				auto combatType = magicField->getCombatType();
				if (!monster->isImmune(combatType)) {
					if (!monster->hasCondition(Combat::DamageToConditionType(combatType), 0, false)) {
						bool alreadyAffected = false;
						if (previousTile != nullptr) {
							auto previousMagicField = previousTile->getFieldItem();
							if (previousMagicField != nullptr) {
								auto previousCombatType = magicField->getCombatType();
								if (!monster->isImmune(previousCombatType)) {
									alreadyAffected = true;
								}
							}
						}

						if (!alreadyAffected) {
							return RET_NOTPOSSIBLE;
						}
					}
				}
			}
		}
		else if (auto player = creature.getPlayer()) {
			if (player->isPzLocked()) {
				if (previousTile != nullptr) {
					if (!previousTile->hasFlag(TILESTATE_PVPZONE) && hasFlag(TILESTATE_PVPZONE)) {
						return RET_PLAYERISPZLOCKEDENTERPVPZONE;
					}

					if (previousTile->hasFlag(TILESTATE_PVPZONE) && !hasFlag(TILESTATE_PVPZONE))  {
						return RET_PLAYERISPZLOCKEDLEAVEPVPZONE;
					}

					if (!previousTile->hasFlag(TILESTATE_PROTECTIONZONE) && hasFlag(TILESTATE_PROTECTIONZONE)) {
						return RET_PLAYERISPZLOCKED;
					}

					if (!previousTile->hasFlag(TILESTATE_NOPVPZONE) && hasFlag(TILESTATE_NOPVPZONE)) {
						return RET_PLAYERISPZLOCKED;
					}
				}
				else {
					if (hasFlag(TILESTATE_PROTECTIONZONE)) {
						return RET_PLAYERISPZLOCKED;
					}

					if (hasFlag(TILESTATE_NOPVPZONE)) {
						return RET_PLAYERISPZLOCKED;
					}
				}
			}
		}
	}

	if (isForwarder()) {
		if (hasBitSet(FLAG_PATHFINDING, flags) && !hasBitSet(FLAG_NOLIMIT, flags)) {
			return RET_NOTPOSSIBLE;
		}

		auto destinationTile = getForwardingDestinationTile();
		if (destinationTile == nullptr) {
			return RET_NOTPOSSIBLE;
		}

		auto result = destinationTile->testAddCreature(creature, flags);
		if (result != RET_NOERROR && result != RET_NOTENOUGHROOM) {
			// Creature is not allowed to use the teleporter since it's not allowed to move on the destination tile in general.
			return result;
		}

		if (getCreatureForwardingTile(creature) == nullptr) {
			// Also cannot find an alternative.
			return RET_DESTINATIONOUTOFREACH;
		}
	}

	if (hasBitSet(FLAG_IGNOREBLOCKITEM, flags)) {
		if (ground->isBlocking(&creature)) {
			return RET_NOTPOSSIBLE;
		}

		auto existingItems = getItemList();
		if (existingItems != nullptr && !existingItems->empty()) {
			for (const auto& existingItem : *existingItems) {
				if (!existingItem->isBlocking(&creature)) {
					continue;
				}

				if (existingItem->isMoveable()) {
					continue;
				}

				return RET_NOTPOSSIBLE;
			}
		}
	}
	else if (hasFlag(TILESTATE_IMMOVABLEBLOCKSOLID)) {
		return RET_NOTPOSSIBLE;
	}
	else if (hasFlag(TILESTATE_BLOCKSOLID)) {
		return RET_NOTENOUGHROOM;
	}

	if (!hasBitSet(FLAG_IGNOREBLOCKCREATURE, flags)) {
		auto existingCreatures = getCreatures();
		if (existingCreatures != nullptr && !existingCreatures->empty()) {
			if (monster != nullptr && monster->canPushCreatures() && !monster->hasController()) {
				for (const auto& existingCreature : *existingCreatures) {
					if (creature.canWalkthrough(*existingCreature)) {
						continue;
					}

					auto existingMonster = existingCreature->getMonster();
					if (existingMonster == nullptr) {
						return RET_NOTENOUGHROOM;
					}

					if (!existingMonster->isPushable()) {
						return RET_NOTENOUGHROOM;
					}

					if (existingMonster->hasController()) {
						return RET_NOTENOUGHROOM;
					}
				}
			}
			else {
				for (const auto& existingCreature : *existingCreatures) {
					if (!creature.canWalkthrough(*existingCreature)) {
						return RET_NOTENOUGHROOM;
					}
				}
			}
		}
	}

	if (thingCount >= StackPosition::MAX_INDEX) {
		return RET_NOTENOUGHROOM;
	}

	return RET_NOERROR;
}


ReturnValue Tile::testRemoveCreature(const Creature& creature) const {
	if (_lockCount > 0) {
		return RET_LOCKED;
	}

	if (creature.getTile() != this) {
		return RET_NOTPOSSIBLE;
	}

	return RET_NOERROR;
}




Tile::Lock::Lock(Tile* tile)
	: _tile(tile)
{
	if (tile != nullptr) {
		++tile->_lockCount;
	}
}


Tile::Lock::~Lock() {
	if (_tile != nullptr) {
		assert(_tile->_lockCount > 0);
		--_tile->_lockCount;
	}
}












Tile::~Tile() {
	if (ground != nullptr) {
		ground->setParent(nullptr);
	}
}


bool Tile::hasProperty(enum ITEMPROPERTY prop) const
{
	if(ground && ground->hasProperty(prop))
		return true;

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((*it)->hasProperty(prop))
				return true;
		}
	}

	return false;
}

bool Tile::hasProperty(Item* exclude, enum ITEMPROPERTY prop) const
{
	assert(exclude);
	if(ground && exclude != ground && ground->hasProperty(prop))
		return true;

	if(const TileItemVector* items = getItemList())
	{
		Item* item = nullptr;
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((item = (*it).get()) && item != exclude && item->hasProperty(prop))
				return true;
		}
	}

	return false;
}

HouseTile* Tile::getHouseTile()
{
	if(isHouseTile())
		return static_cast<HouseTile*>(this);

	return nullptr;
}

const HouseTile* Tile::getHouseTile() const
{
	if(isHouseTile())
		return static_cast<const HouseTile*>(this);

	return nullptr;
}

bool Tile::hasHeight(uint32_t n) const
{
	uint32_t height = 0;
	if(ground)
	{
		if(ground->hasProperty(HASHEIGHT))
			++height;

		if(n == height)
			return true;
	}

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((*it)->hasProperty(HASHEIGHT))
				++height;

			if(n == height)
				return true;
		}
	}

	return false;
}

bool Tile::isSwimmingPool(bool checkPz /*= true*/) const
{
	if(TrashHolder* trashHolder = getTrashHolder())
		return trashHolder->getEffect() == MAGIC_EFFECT_LOSE_ENERGY && (!checkPz ||
			getZone() == ZONE_PROTECTION || getZone() == ZONE_NOPVP);

	return false;
}

uint32_t Tile::getCreatureCount() const
{
	if(const CreatureVector* creatures = getCreatures())
		return creatures->size();

	return 0;
}

uint32_t Tile::getItemCount() const
{
	if(const TileItemVector* items = getItemList())
		return (uint32_t)items->size();

	return 0;
}

uint32_t Tile::getTopItemCount() const
{
	if(const TileItemVector* items = getItemList())
		return items->getTopItemCount();

	return 0;
}

uint32_t Tile::getDownItemCount() const
{
	if(const TileItemVector* items =getItemList())
		return items->getDownItemCount();

	return 0;
}

Teleporter* Tile::getTeleporter() const
{
	if(!hasFlag(TILESTATE_TELEPORTER))
		return nullptr;

	if(ground && ground->asTeleporter())
		return ground->asTeleporter();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->asTeleporter())
				return (*it)->asTeleporter();
		}
	}

	return nullptr;
}

MagicField* Tile::getFieldItem() const
{
	if(!hasFlag(TILESTATE_MAGICFIELD))
		return nullptr;

	if(ground && ground->getMagicField())
		return ground->getMagicField();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getMagicField())
				return (*it)->getMagicField();
		}
	}

	return nullptr;
}

TrashHolder* Tile::getTrashHolder() const
{
	if(!hasFlag(TILESTATE_TRASHHOLDER))
		return nullptr;

	if(ground && ground->getTrashHolder())
		return ground->getTrashHolder();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getTrashHolder())
				return (*it)->getTrashHolder();
		}
	}

	return nullptr;
}

Mailbox* Tile::getMailbox() const
{
	if(!hasFlag(TILESTATE_MAILBOX))
		return nullptr;

	if(ground && ground->getMailbox())
		return ground->getMailbox();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getMailbox())
				return (*it)->getMailbox();
		}
	}

	return nullptr;
}

BedItem* Tile::getBedItem() const
{
	if(!hasFlag(TILESTATE_BED))
		return nullptr;

	if(ground && ground->getBed())
		return ground->getBed();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getBed())
				return (*it)->getBed();
		}
	}

	return nullptr;
}

Creature* Tile::getTopCreature()
{
	if(CreatureVector* creatures = getCreatures())
	{
		if(!creatures->empty())
			return (*creatures->begin()).get();
	}

	return nullptr;
}

Item* Tile::getTopDownItem()
{
	if(TileItemVector* items = getItemList())
	{
		if(items->getDownItemCount() > 0)
			return (*items->getBeginDownItem()).get();
	}

	return nullptr;
}

Item* Tile::getTopTopItem()
{
	if(TileItemVector* items = getItemList())
	{
		if(items->getTopItemCount() > 0)
			return (*(items->getEndTopItem() - 1)).get();
	}

	return nullptr;
}

Item* Tile::getItemByTopOrder(uint32_t topOrder)
{
	if(TileItemVector* items = getItemList())
	{
		ItemVector::reverse_iterator eit = ItemVector::reverse_iterator(items->getBeginTopItem());
		for(ItemVector::reverse_iterator it = ItemVector::reverse_iterator(items->getEndTopItem()); it != eit; ++it)
		{
			if((*it)->getKind()->alwaysOnTopOrder == (int32_t)topOrder)
				return (*it).get();
		}
	}

	return nullptr;
}

Thing* Tile::getTopVisibleThing(const Creature* creature)
{
	if(Creature* _creature = getTopVisibleCreature(creature))
		return _creature;

	if(TileItemVector* items = getItemList())
	{
		for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
		{
			if(!(*it)->getKind()->lookThrough)
				return (*it).get();
		}

		for(ItemVector::reverse_iterator it = ItemVector::reverse_iterator(items->getEndTopItem()),
			end = ItemVector::reverse_iterator(items->getBeginTopItem()); it != end; ++it)
		{
			if(!(*it)->getKind()->lookThrough)
				return (*it).get();
		}
	}

	return ground.get();
}

Creature* Tile::getTopVisibleCreature(const Creature* creature)
{
	if(CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if(creature->canSeeCreature((*cit).get()))
				return (*cit).get();
		}
	}

	return nullptr;
}

const Creature* Tile::getTopVisibleCreature(const Creature* creature) const
{
	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if(creature->canSeeCreature((*cit).get()))
				return (*cit).get();
		}
	}

	return nullptr;
}

void Tile::onAddTileItem(Item* item)
{
	updateTileFlags(item, false);
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendAddTileItem(this, cylinderMapPos, item, BOOST_CURRENT_FUNCTION);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onAddTileItem(this, cylinderMapPos, item);
}

void Tile::onUpdateTileItem(Item* oldItem, const ItemKindPC& oldType, Item* newItem, const ItemKindPC& newType)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTileItem(this, cylinderMapPos, oldItem, newItem);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTileItem(this, cylinderMapPos, oldItem, oldType, newItem, newType);
}

void Tile::onRemoveTileItem(const SpectatorList& list, std::vector<int32_t>& oldStackposVector, Item* item)
{
	updateTileFlags(item, true);
	const Position& cylinderMapPos = pos;

	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	uint32_t i = 0;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer())) {
			int32_t index = oldStackposVector[i++];
			if (index < 0) {
				continue;
			}

			tmpPlayer->sendRemoveTileItem(this, StackPosition(cylinderMapPos, index), item, BOOST_CURRENT_FUNCTION);
		}
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onRemoveTileItem(this, cylinderMapPos, item->getKind(), item);
}

void Tile::onUpdateTile()
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTile(this, cylinderMapPos);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTile(this, cylinderMapPos);
}

ReturnValue Tile::__queryAdd(int32_t index, const Item* item, uint32_t count, uint32_t flags) const {
	if (isForwarder()) {
		if (getAvailableItemForwardingTile(*item) == nullptr) {
			return RET_DESTINATIONOUTOFREACH;
		}

		return RET_NOERROR;
	}

	if (!hasRoomForThing(*item)) {
		return RET_TILEISFULL;
	}

	const CreatureVector* creatures = getCreatures();
	const TileItemVector* items = getItemList();

	if(items && items->size() >= 0xFFFF)
		return RET_NOTPOSSIBLE;

	if(hasBitSet(FLAG_NOLIMIT, flags))
		return RET_NOERROR;

	bool itemIsHangable = item->isHangable();
	if(!ground && !itemIsHangable)
		return RET_NOTPOSSIBLE;

	if(creatures && !creatures->empty() && !hasBitSet(FLAG_IGNOREBLOCKCREATURE, flags))
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if(!(*cit)->isGhost() && item->isBlocking((*cit).get()))
				return RET_NOTENOUGHROOM;
		}
	}

	if(hasFlag(TILESTATE_PROTECTIONZONE))
	{
		const uint32_t itemLimit = server.configManager().getNumber(ConfigManager::ITEMLIMIT_PROTECTIONZONE);
		if(itemLimit && getThingCount() > itemLimit)
			return RET_TILEISFULL;
	}

	bool hasHangable = false, supportHangable = false;
	if(items)
	{
		Thing* iThing = nullptr;
		for(uint32_t i = 0; i < getThingCount(); ++i)
		{
			iThing = __getThing(i);
			if(const Item* iItem = iThing->getItem())
			{
				ItemKindPC kind = iItem->getKind();
				if(kind->isHangable)
					hasHangable = true;

				if(kind->isHorizontal || kind->isVertical)
					supportHangable = true;

				if(itemIsHangable && (kind->isHorizontal || kind->isVertical))
					continue;
				else if(kind->blockSolid)
				{
					if(!item->isPickupable())
						return RET_NOTENOUGHROOM;

					if(kind->allowPickupable)
						continue;

					if(!kind->hasHeight || kind->pickupable || kind->isBed())
						return RET_NOTENOUGHROOM;
				}
			}
		}
	}

	if(itemIsHangable && hasHangable && supportHangable)
		return RET_NEEDEXCHANGE;

	return RET_NOERROR;
}

ReturnValue Tile::__queryMaxCount(int32_t index, const Item* item, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	maxQueryCount = std::max((uint32_t)1, count);
	return RET_NOERROR;
}

ReturnValue Tile::__queryRemove(const Item* item, uint32_t count, uint32_t flags) const
{
	int32_t index = __getIndexOfThing(item);
	if(index == -1)
		return RET_NOTPOSSIBLE;

	if(!count || (item->isStackable() && count > item->getItemCount())
		|| (item->isNotMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags)))
		return RET_NOTPOSSIBLE;

	return RET_NOERROR;
}

Cylinder* Tile::__queryDestination(int32_t& index, const Item* item, Item** destItem,
	uint32_t& flags)
{
	Tile* destTile = nullptr;
	*destItem = nullptr;

	Position _pos = pos;
	if(floorChange(CHANGE_DOWN))
	{
		_pos.z++;
		for(int32_t i = CHANGE_FIRST_EX; i < CHANGE_LAST; ++i)
		{
			Position __pos = _pos;
			Tile* tmpTile = nullptr;
			switch(i)
			{
				case CHANGE_NORTH_EX:
					__pos.y++;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.y++;

					break;
				case CHANGE_SOUTH_EX:
					__pos.y--;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.y--;

					break;
				case CHANGE_EAST_EX:
					__pos.x--;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.x--;

					break;
				case CHANGE_WEST_EX:
					__pos.x++;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.x++;

					break;
				default:
					break;
			}

			if(!tmpTile || !tmpTile->floorChange((FloorChange_t)i))
				continue;

			destTile = server.game().getTile(__pos);
			break;
		}

		if(!destTile)
		{
			if(Tile* downTile = server.game().getTile(_pos))
			{
				if(downTile->floorChange(CHANGE_NORTH) || downTile->floorChange(CHANGE_NORTH_EX))
					_pos.y++;

				if(downTile->floorChange(CHANGE_SOUTH) || downTile->floorChange(CHANGE_SOUTH_EX))
					_pos.y--;

				if(downTile->floorChange(CHANGE_EAST) || downTile->floorChange(CHANGE_EAST_EX))
					_pos.x--;

				if(downTile->floorChange(CHANGE_WEST) || downTile->floorChange(CHANGE_WEST_EX))
					_pos.x++;

				destTile = server.game().getTile(_pos);
			}
		}
	}
	else if(floorChange())
	{
		_pos.z--;
		if(floorChange(CHANGE_NORTH))
			_pos.y--;

		if(floorChange(CHANGE_SOUTH))
			_pos.y++;

		if(floorChange(CHANGE_EAST))
			_pos.x++;

		if(floorChange(CHANGE_WEST))
			_pos.x--;

		if(floorChange(CHANGE_NORTH_EX))
			_pos.y -= 2;

		if(floorChange(CHANGE_SOUTH_EX))
			_pos.y += 2;

		if(floorChange(CHANGE_EAST_EX))
			_pos.x += 2;

		if(floorChange(CHANGE_WEST_EX))
			_pos.x -= 2;

		destTile = server.game().getTile(_pos);
	}

	if(!destTile)
		destTile = this;
	else
		flags |= FLAG_NOLIMIT; //will ignore that there is blocking items/creatures

	if(destTile)
	{
		Thing* destThing = destTile->getTopDownItem();
		if(destThing && !destThing->isRemoved())
			*destItem = destThing->getItem();
	}

	return destTile;
}

void Tile::__addThing(Creature* actor, int32_t index, Item* item)
{
	assert(item->getParent() == nullptr || item->getParent() == &VirtualCylinder::virtualCylinder);

	if (isForwarder()) {
		auto destinationTile = getAvailableItemForwardingTile(*item);
		if (destinationTile == nullptr) {
			return;
		}

		destinationTile->__addThing(actor, item);

		if (isTeleporter()) {
			bool ghost = (actor == nullptr || actor->isGhost());
			server.game().addMagicEffect(pos, MAGIC_EFFECT_TELEPORT, ghost);
			server.game().addMagicEffect(destinationTile->pos, MAGIC_EFFECT_TELEPORT, ghost);
		}

		return;
	}

	if (!hasRoomForThing(*item)) {
		return;
	}

	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	TileItemVector* items = getItemList();
	if(items && items->size() >= 0xFFFF)
		return/* RET_NOTPOSSIBLE*/;

	item->setParent(this);
	if (isHouseTile()) {
		item->retain();
	}
	else {
		item->release();
	}

	if(item->isGroundTile())
	{
		if(ground)
		{
			ItemKindPC oldKind = ground->getKind();
			int32_t oldGroundIndex = __getIndexOfThing(ground.get());
			boost::intrusive_ptr<Item> oldGround = ground;

			ground->setParent(nullptr);
			ground = item;

			updateTileFlags(oldGround.get(), true);
			updateTileFlags(item, false);

			onUpdateTileItem(oldGround.get(), oldKind, item, item->getKind());
			postRemoveNotification(actor, oldGround.get(), nullptr, oldGroundIndex, true);
		}
		else
		{
			ground = item;
			++thingCount;
			onAddTileItem(item);
		}
	}
	else if(item->isAlwaysOnTop())
	{
		if(item->isSplash())
		{
			//remove old splash if exists
			if(items)
			{
				for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
				{
					if(!(*it)->isSplash())
						continue;

					int32_t oldSplashIndex = __getIndexOfThing((*it).get());
					boost::intrusive_ptr<Item> oldSplash = *it;

					__removeThing(oldSplash.get(), 1);
					oldSplash->setParent(nullptr);

					postRemoveNotification(actor, oldSplash.get(), nullptr, oldSplashIndex, true);
					break;
				}
			}
		}

		bool isInserted = false;
		if(items)
		{
			for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				//Note: this is different from internalAddThing
				if(item->getKind()->alwaysOnTopOrder > (*it)->getKind()->alwaysOnTopOrder)
					continue;

				items->insert(it, item);
				++thingCount;

				isInserted = true;
				break;
			}
		}
		else
			items = makeItemList();

		if(!isInserted)
		{
			items->push_back(item);
			++thingCount;
		}

		onAddTileItem(item);
	}
	else
	{
		if(item->isMagicField())
		{
			//remove old field item if exists
			if(items)
			{
				boost::intrusive_ptr<MagicField> oldField;
				for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
				{
					if(!(oldField = (*it)->getMagicField()))
						continue;

					if(oldField->isReplaceable())
					{
						int32_t oldFieldIndex = __getIndexOfThing((*it).get());
						__removeThing(oldField.get(), 1);

						oldField->setParent(nullptr);

						postRemoveNotification(actor, oldField.get(), nullptr, oldFieldIndex, true);
						break;
					}

					//This magic field cannot be replaced.
					item->setParent(nullptr);
					return;
				}
			}
		}

		if(item->getId() == ITEM_WATERBALL_SPLASH && !hasFlag(TILESTATE_TRASHHOLDER))
			item->setKind(server.items().getKind(ITEM_WATERBALL));

		items = makeItemList();
		items->insert(items->getBeginDownItem(), item);

		++items->downItemCount;
		++thingCount;
		onAddTileItem(item);
	}
}

void Tile::__updateThing(Item* item, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(item);
	if(index == -1)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	ItemKindPC newKind = server.items().getKind(itemId);
	if (!newKind) {
		return;
	}

	ItemKindPC oldKind = item->getKind();

	//Need to update it here too since the old and new item is the same
	updateTileFlags(item, true);

	item->setKind(newKind);
	item->setSubType(count);

	if (isHouseTile()) {
		item->retain();
	}
	else {
		item->release();
	}

	updateTileFlags(item, false);
	onUpdateTileItem(item, oldKind, item, newKind);
}

void Tile::__replaceThing(uint32_t index, Item* item)
{

	int32_t pos = index;
	boost::intrusive_ptr<Item> oldItem;
	if(ground)
	{
		if(!pos)
		{
			oldItem = ground.get();
			ground = item;
		}

		--pos;
	}

	TileItemVector* items = getItemList();
	if(!oldItem && items)
	{
		int32_t topItemSize = getTopItemCount();
		if(pos < topItemSize)
		{
			ItemVector::iterator it = items->getBeginTopItem();
			it += pos;

			oldItem = (*it);
			it = items->erase(it);
			items->insert(it, item);
		}

		pos -= topItemSize;
	}

	if(!oldItem)
	{
		if(CreatureVector* creatures = getCreatures())
		{
			if(pos < (int32_t)creatures->size())
			{
				return/* RET_NOTPOSSIBLE*/;
			}

			pos -= (uint32_t)creatures->size();
		}
	}

	if(!oldItem && items)
	{
		int32_t downItemSize = getDownItemCount();
		if(pos < downItemSize)
		{
			ItemVector::iterator it = items->begin();
			it += pos;
			pos = 0;

			oldItem = (*it);
			it = items->erase(it);
			items->insert(it, item);
		}
	}

	if(oldItem)
	{
		item->setParent(this);
		if (isHouseTile()) {
			item->retain();
		}
		else {
			item->release();
		}

		updateTileFlags(oldItem.get(), true);
		updateTileFlags(item, false);

		onUpdateTileItem(oldItem.get(), oldItem->getKind(), item, item->getKind());
		oldItem->setParent(nullptr);
		return/* RET_NOERROR*/;
	}
}

void Tile::__removeThing(Item* item, uint32_t count)
{
	assert(item->getParent() == this);

	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(item);
	if(index == -1)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	if(item == ground)
	{
		const SpectatorList& list = server.game().getSpectators(pos);
		std::vector<int32_t> oldStackposVector;

		Player* tmpPlayer = nullptr;
		for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()))
				oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, ground.get()));
		}

		ground->setParent(nullptr);
		ground = nullptr;

		--thingCount;
		onRemoveTileItem(list, oldStackposVector, item);
		return/* RET_NOERROR*/;
	}

	TileItemVector* items = getItemList();
	if(!items)
		return;

	if(item->isAlwaysOnTop())
	{
		for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
		{
			if(*it != item)
				continue;

			const SpectatorList& list = server.game().getSpectators(pos);
			std::vector<int32_t> oldStackposVector;

			Player* tmpPlayer = nullptr;
			for(SpectatorList::const_iterator iit = list.begin(); iit != list.end(); ++iit)
			{
				if((tmpPlayer = (*iit)->getPlayer()))
					oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, (*it).get()));
			}

			server.game().autorelease(*it);

			(*it)->setParent(nullptr);
			items->erase(it);

			--thingCount;
			onRemoveTileItem(list, oldStackposVector, item);
			return/* RET_NOERROR*/;
		}
	}
	else
	{
		for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
		{
			if((*it) != item)
				continue;

			if(item->isStackable() && count != item->getItemCount())
			{
				uint8_t newCount = (uint8_t)std::max((int32_t)0, (int32_t)(item->getItemCount() - count));
				updateTileFlags(item, true);

				item->setItemCount(newCount);
				updateTileFlags(item, false);

				onUpdateTileItem(item, item->getKind(), item, item->getKind());
			}
			else
			{
				const SpectatorList& list = server.game().getSpectators(pos);
				std::vector<int32_t> oldStackposVector;

				Player* tmpPlayer = nullptr;
				for(SpectatorList::const_iterator iit = list.begin(); iit != list.end(); ++iit)
				{
					if((tmpPlayer = (*iit)->getPlayer()))
						oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, (*it).get()));
				}

				server.game().autorelease(*it);

				(*it)->setParent(nullptr);
				items->erase(it);

				--items->downItemCount;
				--thingCount;
				onRemoveTileItem(list, oldStackposVector, item);
			}

			return/* RET_NOERROR*/;
		}
	}
}

int32_t Tile::getClientIndexOfThing(const Player* player, const Thing* thing) const
{
	if(ground && ground == thing)
		return 0;

	if(const Item* item = thing->getItem())
	{
		if(item->isGroundTile())
			return 0;
	}

	int32_t n = 0;
	if(!ground)
		n--;

	const TileItemVector* items = getItemList();
	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getTopItemCount();
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if((*cit) == thing)
				return ++n;

			if(player->canSeeCreature(**cit))
				++n;
		}
	}

	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getDownItemCount();
	}

	return -1;
}

int32_t Tile::__getIndexOfThing(const Thing* thing) const
{
	if(ground && ground == thing)
		return 0;

	int32_t n = 0;
	const TileItemVector* items = getItemList();
	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getTopItemCount();
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			++n;
			if((*cit) == thing)
				return n;
		}
	}

	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getDownItemCount();
	}

	return -1;
}

uint32_t Tile::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/, bool itemCount /*= true*/, bool) const
{
	uint32_t count = 0;
	Thing* thing = nullptr;
	for(uint32_t i = 0; i < getThingCount(); ++i)
	{
		if(!(thing = __getThing(i)))
			continue;

		if(const Item* item = thing->getItem())
		{
			if(item->getId() != itemId || (subType != -1 && subType != item->getSubType()))
				continue;

			if(!itemCount)
			{
				if(item->isRune())
					count+= item->getCharges();
				else
					count+= item->getItemCount();
			}
			else
				count+= item->getItemCount();
		}
	}

	return count;
}

Thing* Tile::__getThing(uint32_t index) const
{
	if(ground)
	{
		if(!index)
			return ground.get();

		--index;
	}

	const TileItemVector* items = getItemList();
	if(items)
	{
		uint32_t topItemSize = items->getTopItemCount();
		if(index < topItemSize)
		{
			Item* item = items->at(items->downItemCount + index);
			if(item && !item->isRemoved())
				return item;
		}

		index -= topItemSize;
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		if(index < (uint32_t)creatures->size())
			return creatures->at(index).get();

		index -= (uint32_t)creatures->size();
	}

	if(items && index < items->getDownItemCount())
	{
		Item* item = items->at(index);
		if(item && !item->isRemoved())
			return item;
	}

	return nullptr;
}

void Tile::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, cylinderlink_t link/* = LINK_OWNER*/)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->postAddNotification(actor, thing, oldParent, index, LINK_NEAR);
	}

	//add a reference to this item, it may be deleted after being added (mailbox for example)
	if(link == LINK_OWNER)
	{
		//calling movement scripts
		if(Creature* creature = thing->getCreature())
		{
			const Tile* fromTile = nullptr;
			if(oldParent)
				fromTile = oldParent->getTile();

			server.moveEvents().onCreatureMove(actor, creature, fromTile, this, true);
		}
		else if(Item* item = thing->getItem())
		{
			server.moveEvents().onAddTileItem(this, item);
			server.moveEvents().onItemMove(actor, item, this, true);
		}

		if (Item* item = thing->getItem()) {
			if(hasFlag(TILESTATE_TRASHHOLDER))
			{
				if(TrashHolder* trashHolder = getTrashHolder())
					trashHolder->__addThing(actor, item);
			}
			else if(hasFlag(TILESTATE_MAILBOX))
			{
				if(Mailbox* mailbox = getMailbox())
					mailbox->__addThing(actor, item);
			}
		}
	}
}

void Tile::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, cylinderlink_t link/* = LINK_OWNER*/)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;
	if(/*isCompleteRemoval && */getThingCount() > 8)
		onUpdateTile();

	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->postRemoveNotification(actor, thing, newParent, index, isCompleteRemoval, LINK_NEAR);
	}

	if (auto creature = thing->getCreature()) {
		if (newParent != nullptr) {
			auto newTile = newParent->getTile();
			if (newTile != nullptr) {
				server.moveEvents().onCreatureMove(actor, creature, this, newTile, false);
			}
		}
	}
	else if (auto item = thing->getItem()) {
		server.moveEvents().onRemoveTileItem(this, item);

		if (newParent != nullptr) {
			server.moveEvents().onItemMove(actor, item, this, false);
		}
	}
}


bool Tile::hasRoomForThing(const Thing& thing) const {
	if (thingCount >= StackPosition::MAX_INDEX) {
		// we are full
		return false;
	}

	if (thing.getCreature() != nullptr) {
		// add creatures until we are full
		return true;
	}

	if (thing.getItem() != nullptr) {
		auto creatures = getCreatures();
		if (creatures != nullptr && !creatures->empty()) {
			// we have at least one creature - add items until we are full
			return true;
		}

		if (thingCount + 1 >= StackPosition::MAX_INDEX) {
			// we have no creatures - leave at least one spot free so that creatures can still walk over this tile
			return false;
		}
	}

	return true;
}


void Tile::__internalAddThing(uint32_t index, Item* item)
{
	assert(item->getParent() == nullptr);

	if (!hasRoomForThing(*item)) {
		return;
	}

	if(!item)
		return;

	TileItemVector* items = makeItemList();
	if(items && items->size() >= 0xFFFF)
		return/* RET_NOTPOSSIBLE*/;

	if(item->isGroundTile())
	{
		if (ground != nullptr) {
			return;
		}

		ground = item;
	}
	else if(item->isAlwaysOnTop())
	{
		bool isInserted = false;
		for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
		{
			if((*it)->getKind()->alwaysOnTopOrder <= item->getKind()->alwaysOnTopOrder)
				continue;

			items->insert(it, item);

			isInserted = true;
			break;
		}

		if(!isInserted)
		{
			items->push_back(item);
		}
	}
	else
	{
		items->insert(items->getBeginDownItem(), item);
		++items->downItemCount;
	}

	item->setParent(this);
	if (isHouseTile()) {
		item->retain();
	}
	else {
		item->release();
	}

	++thingCount;

	updateTileFlags(item, false);
}

void Tile::updateTileFlags(Item* item, bool removed)
{
	if(!removed)
	{
		if(!hasFlag(TILESTATE_FLOORCHANGE))
		{
			if(item->floorChange(CHANGE_DOWN))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_DOWN);
			}

			if(item->floorChange(CHANGE_NORTH))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_NORTH);
			}

			if(item->floorChange(CHANGE_SOUTH))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_SOUTH);
			}

			if(item->floorChange(CHANGE_EAST))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_EAST);
			}

			if(item->floorChange(CHANGE_WEST))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_WEST);
			}

			if(item->floorChange(CHANGE_NORTH_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_NORTH_EX);
			}

			if(item->floorChange(CHANGE_SOUTH_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_SOUTH_EX);
			}

			if(item->floorChange(CHANGE_EAST_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_EAST_EX);
			}

			if(item->floorChange(CHANGE_WEST_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_WEST_EX);
			}
		}

		if(item->asTeleporter())
			setFlag(TILESTATE_TELEPORTER);

		if(item->getMagicField())
			setFlag(TILESTATE_MAGICFIELD);

		if(item->getMailbox())
			setFlag(TILESTATE_MAILBOX);

		if(item->getTrashHolder())
			setFlag(TILESTATE_TRASHHOLDER);

		if(item->getBed())
			setFlag(TILESTATE_BED);

		if(item->getContainer() && item->getContainer()->getDepot())
			setFlag(TILESTATE_DEPOT);

		if(item->hasProperty(BLOCKSOLID))
			setFlag(TILESTATE_BLOCKSOLID);

		if(item->hasProperty(IMMOVABLEBLOCKSOLID))
			setFlag(TILESTATE_IMMOVABLEBLOCKSOLID);

		if(item->hasProperty(BLOCKPATH))
			setFlag(TILESTATE_BLOCKPATH);

		if(item->hasProperty(NOFIELDBLOCKPATH))
			setFlag(TILESTATE_NOFIELDBLOCKPATH);

		if(item->hasProperty(IMMOVABLENOFIELDBLOCKPATH))
			setFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH);
	}
	else
	{
		if(item->floorChange(CHANGE_DOWN))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_DOWN);
		}

		if(item->floorChange(CHANGE_NORTH))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_NORTH);
		}

		if(item->floorChange(CHANGE_SOUTH))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_SOUTH);
		}

		if(item->floorChange(CHANGE_EAST))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_EAST);
		}

		if(item->floorChange(CHANGE_WEST))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_WEST);
		}

		if(item->floorChange(CHANGE_NORTH_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_NORTH_EX);
		}

		if(item->floorChange(CHANGE_SOUTH_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_SOUTH_EX);
		}

		if(item->floorChange(CHANGE_EAST_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_EAST_EX);
		}

		if(item->floorChange(CHANGE_WEST_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_WEST_EX);
		}

		if(item->asTeleporter())
			resetFlag(TILESTATE_TELEPORTER);

		if(item->getMagicField())
			resetFlag(TILESTATE_MAGICFIELD);

		if(item->getMailbox())
			resetFlag(TILESTATE_MAILBOX);

		if(item->getTrashHolder())
			resetFlag(TILESTATE_TRASHHOLDER);

		if(item->getBed())
			resetFlag(TILESTATE_BED);

		if(item->getContainer() && item->getContainer()->getDepot())
			resetFlag(TILESTATE_DEPOT);

		if(item->hasProperty(BLOCKSOLID) && !hasProperty(item, BLOCKSOLID))
			resetFlag(TILESTATE_BLOCKSOLID);

		if(item->hasProperty(IMMOVABLEBLOCKSOLID) && !hasProperty(item, IMMOVABLEBLOCKSOLID))
			resetFlag(TILESTATE_IMMOVABLEBLOCKSOLID);

		if(item->hasProperty(BLOCKPATH) && !hasProperty(item, BLOCKPATH))
			resetFlag(TILESTATE_BLOCKPATH);

		if(item->hasProperty(NOFIELDBLOCKPATH) && !hasProperty(item, NOFIELDBLOCKPATH))
			resetFlag(TILESTATE_NOFIELDBLOCKPATH);

		if(item->hasProperty(IMMOVABLEBLOCKPATH) && !hasProperty(item, IMMOVABLEBLOCKPATH))
			resetFlag(TILESTATE_IMMOVABLEBLOCKPATH);

		if(item->hasProperty(IMMOVABLENOFIELDBLOCKPATH) && !hasProperty(item, IMMOVABLENOFIELDBLOCKPATH))
			resetFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH);
	}
}


DynamicTile::DynamicTile(uint16_t x, uint16_t y, uint16_t z):
	Tile(x, y, z)
{
	m_flags |= TILESTATE_DYNAMIC_TILE;
}


DynamicTile::~DynamicTile() {
	for (CreatureVector::iterator it = creatures.begin(); it != creatures.end(); ++it) {
		(*it)->setParent(nullptr);
	}

	for (ItemVector::iterator it = items.begin(); it != items.end(); ++it) {
		(*it)->setParent(nullptr);
	}
}


StaticTile::StaticTile(uint16_t x, uint16_t y, uint16_t z):
	Tile(x, y, z), items(nullptr), creatures(nullptr) {}


StaticTile::~StaticTile() {
	if (creatures != nullptr) {
		for (CreatureVector::iterator it = creatures->begin(); it != creatures->end(); ++it) {
			(*it)->setParent(nullptr);
		}

		delete creatures;
	}

	if (items != nullptr) {
		for (ItemVector::iterator it = items->begin(); it != items->end(); ++it) {
			(*it)->setParent(nullptr);
		}

		delete items;
	}
}
