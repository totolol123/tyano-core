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

// So far World is a wrapper around Game to have a better separation of logic.
// Over time more logic will be moved here.

#include "otpch.h"
#include "world.h"

#include "game.h"
#include "monster.h"
#include "npc.h"
#include "player.h"
#include "server.h"
#include "tile.h"


LOGGER_DEFINITION(World);

const World::CreatureId World::MAXIMUM_MONSTER_ID = 0x4FFFFFFF;
const World::CreatureId World::MINIMUM_MONSTER_ID = 0x40000000;
const World::CreatureId World::MAXIMUM_NPC_ID     = 0x8FFFFFFF;
const World::CreatureId World::MINIMUM_NPC_ID     = 0x80000000;
const World::CreatureId World::MAXIMUM_PLAYER_ID  = 0x1FFFFFFF;
const World::CreatureId World::MINIMUM_PLAYER_ID  = 0x10000000;


World::World()
	: _monstersById(MINIMUM_MONSTER_ID, MAXIMUM_MONSTER_ID),
	  _npcsById(MINIMUM_NPC_ID, MAXIMUM_NPC_ID),
	  _playersById(MINIMUM_PLAYER_ID, MAXIMUM_PLAYER_ID)
{}


World::~World()
{}


ReturnValue World::addCreature(const CreatureP& creature, const Position& position, uint16_t radius) {
	assert(creature != nullptr);

	if (creature->isInWorld()) {
		LOGw("Cannot add " << creature << " to world at position " << position << " because it already entered the world.");
		return RET_NOTPOSSIBLE;
	}

	auto findTileResult = findTileForThingNearPosition(*creature.get(), position, radius);
	if (!findTileResult.isSuccess()) {
		return findTileResult.getError();
	}

	creature->willEnterWorld(*this);

	Creature::Id id;
	if (auto monster = creature->getMonster()) { // most likely
		id = _monstersById.insert(monster);
		_monsters.push_back(monster);
	}
	else if (auto player = creature->getPlayer()) { // less likely
		id = _playersById.insert(player);
		_players.push_back(player);
	}
	else if (auto npc = creature->getNpc()) { // rarely
		id = _npcsById.insert(npc);
		_npcs.push_back(npc);
	}
	else {
		LOGe("Cannot add " << creature << " of unsupported type to world.");
		return RET_NOTPOSSIBLE;
	}

	_creaturesById[id] = creature;
	_creatures.push_back(creature);

	creature->setId(id);
	creature->setInWorld(true);

	auto result = findTileResult.getTile()->addCreature(creature, findTileResult.getFlags());
	assert(result == RET_NOERROR);

	creature->didEnterWorld(*this);

	return RET_NOERROR;
}


auto World::findTileForThingNearPosition(const Thing& thing, const Position& position, uint16_t radius, uint32_t directFlags, uint32_t indirectFlags) const -> FindTileResult {
	static const uint16_t optimisticRadius = 10;  // we prepare our pathfinding unordered_set's memory allocation using this value [max expected elements = ((optimisticRadius * 2) + 1) ^ 2]

	using Test = std::function<ReturnValue(const Tile& tile, uint32_t flags)>;

	Test test;
	if (auto creature = thing.getCreature()) {
		test = [creature] (const Tile& tile, uint32_t flags) {
			return tile.testAddCreature(*creature, flags);
		};
	}
	else if (auto item = thing.getItem()) {
		test = [item] (const Tile& tile, uint32_t flags) {
			return tile.__queryAdd(INDEX_WHEREEVER, item, 1, flags);
		};
	}
	else {
		assert(false && "invalid thing");
		return FindTileResult(RET_NOTPOSSIBLE);
	}

	auto& game = server.game();
	auto pathfindingFlags = directFlags|indirectFlags|FLAG_IGNOREBLOCKCREATURE|FLAG_PATHFINDING;
	auto finalResult = RET_NOERROR;

	// try the exact destination
	auto tile = game.getTile(position);
	if (tile != nullptr) {
		finalResult = test(*tile, directFlags);
		if (finalResult == RET_NOERROR) {
			return FindTileResult(tile, directFlags);
		}
	}

	if (radius >= 1) {
		std::vector<Position> moreNeighbors; // reusable vector for neighbors

		auto directNeighborPositions = position.neighbors(1);
		if (!directNeighborPositions.empty()) {
			bool hasDirectNeighborTiles = false;

			// make the game less predictable :)
			std::random_shuffle(directNeighborPositions.begin(), directNeighborPositions.end());

			// next try direct neighbors
			for (auto directNeighborPosition : directNeighborPositions) {
				tile = game.getTile(directNeighborPosition);
				if (tile == nullptr) {
					continue;
				}

				auto result = test(*tile, indirectFlags);
				if (result == RET_NOERROR) {
					return FindTileResult(tile, indirectFlags);
				}
				if (finalResult == RET_NOERROR) {
					finalResult = RET_NOTPOSSIBLE;
				}

				hasDirectNeighborTiles = true;
			}

			if (radius >= 2 && hasDirectNeighborTiles) {
				// next we walk paths until we find something or hit the radius limit

				std::queue<Position> testablePositions;
				std::unordered_set<Position> testedPositions;

				const auto optimisticAxisLength = ((optimisticRadius * 2) + 1);
				testedPositions.reserve(optimisticAxisLength * optimisticAxisLength);
				testedPositions.insert(position);
				testedPositions.insert(directNeighborPositions.begin(), directNeighborPositions.end());

				std::vector<Position> nextNeighborPositions; // reusable vector to reduce memory allocations

				auto queueNeighbors = [&nextNeighborPositions,pathfindingFlags,position,radius,&test,&testablePositions,&testedPositions] (Tile& tile, const Position& tilePosition) {
					auto result = test(tile, pathfindingFlags);
					switch (result) {
					case RET_NOERROR:
					case RET_NOTENOUGHROOM:
					case RET_TILEISFULL: {
						nextNeighborPositions.clear();
						tilePosition.neighbors(1, nextNeighborPositions);

						// make the game less predictable :)
						std::random_shuffle(nextNeighborPositions.begin(), nextNeighborPositions.end());

						// usually we can use this tile - it's just temporarily blocked - so we can also check its neighbors
						for (auto neighborPosition : nextNeighborPositions) {
							if (neighborPosition.distanceTo(position) <= radius && testedPositions.count(neighborPosition) == 0) {
								testedPositions.insert(neighborPosition);
								testablePositions.push(neighborPosition);
							}
						}

						break;
					}

					default:
						break;
					}
				};

				// let's start with direct neighbors
				for (auto directNeighborPosition : directNeighborPositions) {
					tile = game.getTile(directNeighborPosition);
					if (tile == nullptr) {
						continue;
					}

					queueNeighbors(*tile, directNeighborPosition);
				}

				// now let's continue our search until we hit a wall
				while (!testablePositions.empty()) {
					auto tilePosition = testablePositions.front();

					tile = game.getTile(tilePosition);
					if (tile != nullptr) {
						auto result = test(*tile, indirectFlags);
						if (result == RET_NOERROR) {
							return FindTileResult(tile, indirectFlags);
						}

						queueNeighbors(*tile, tilePosition);
					}

					testablePositions.pop();
				}
			}
		}
	}

	if (finalResult == RET_NOERROR) {
		// no tiles anywhere at target position & direct neighbors
		finalResult = RET_NOTPOSSIBLE;
	}

	return FindTileResult(finalResult);
}


auto World::getCreatureById(CreatureId id) const -> CreatureP {
	auto i = _creaturesById.find(id);
	if (i == _creaturesById.end()) {
		return nullptr;
	}

	return i->second;
}


auto World::getCreatures() const -> const Creatures& {
	return _creatures;
}


auto World::getMonsterById(CreatureId id) const -> MonsterP {
	return _monstersById.get(id);
}


auto World::getMonsters() const -> const Monsters& {
	return _monsters;
}


auto World::getNpcById(CreatureId id) const -> NpcP {
	return _npcsById.get(id);
}


auto World::getNpcs() const -> const Npcs& {
	return _npcs;
}


auto World::getPlayerById(CreatureId id) const -> PlayerP {
	return _playersById.get(id);
}


auto World::getPlayers() const -> const Players& {
	return _players;
}


ReturnValue World::removeCreature(const CreatureP& creature) {
	assert(creature != nullptr);

	if (!creature->isInWorld()) {
		LOGw("Cannot remove " << creature << " from the world because it did not enter the world.");
		return RET_NOTPOSSIBLE;
	}

	auto tile = creature->getTile();
	if (tile != nullptr) {
		auto result = tile->testRemoveCreature(*creature);
		if (result != RET_NOERROR) {
			return result;
		}
	}

	creature->willExitWorld(*this);

	if (tile != nullptr) {
		auto result = tile->removeCreature(creature);
		assert(result == RET_NOERROR);
	}

	if (auto monster = creature->getMonster()) { // most likely
		_monstersById.erase(creature->getId());

		auto i = std::find(_monsters.begin(), _monsters.end(), monster);
		if (i != _monsters.end()) {
			_monsters.erase(i);
		}
	}
	else if (auto player = creature->getPlayer()) { // less likely
		_playersById.erase(creature->getId());

		auto i = std::find(_players.begin(), _players.end(), player);
		if (i != _players.end()) {
			_players.erase(i);
		}
	}
	else if (auto npc = creature->getNpc()) { // rarely
		_npcsById.erase(creature->getId());

		auto i = std::find(_npcs.begin(), _npcs.end(), npc);
		if (i != _npcs.end()) {
			_npcs.erase(i);
		}
	}
	else {
		LOGe("Cannot remove " << creature << " of unsupported type from world.");
		return RET_NOTPOSSIBLE;
	}

	_creaturesById.erase(creature->getId());

	auto i = std::find(_creatures.begin(), _creatures.end(), creature);
	if (i != _creatures.end()) {
		_creatures.erase(i);
	}

	creature->setInWorld(false);
	creature->didExitWorld(*this);

	return RET_NOERROR;
}




World::FindTileResult::FindTileResult(Tile* tile, uint32_t flags)
	: _error(RET_NOERROR),
	  _flags(flags),
	  _tile(tile)
{}


World::FindTileResult::FindTileResult(ReturnValue error)
	: _error(error),
	  _flags(0),
	  _tile(nullptr)
{
	assert(error != RET_NOERROR);
}


ReturnValue World::FindTileResult::getError() const {
	return _error;
}


uint32_t World::FindTileResult::getFlags() const {
	return _flags;
}


Tile* World::FindTileResult::getTile() const {
	return _tile;
}


bool World::FindTileResult::isSuccess() const {
	return (_error == RET_NOERROR);
}
