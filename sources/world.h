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

#ifndef _WORLD_H
#define _WORLD_H

#include "const.h"
#include "idmap.h"

class Creature;
class Monster;
class Npc;
class Player;
class Position;
class Thing;
class Tile;


class World {

public:

	using CreatureP     = boost::intrusive_ptr<Creature>;
	using CreatureId    = IdMap<CreatureP>::Id;
	using Creatures     = std::vector<CreatureP>;
	using CreaturesById = std::unordered_map<CreatureId,CreatureP>;
	using MonsterP      = boost::intrusive_ptr<Monster>;
	using Monsters      = std::vector<MonsterP>;
	using MonstersById  = IdMap<MonsterP>;
	using NpcP          = boost::intrusive_ptr<Npc>;
	using Npcs          = std::vector<NpcP>;
	using NpcsById      = IdMap<NpcP>;
	using PlayerP       = boost::intrusive_ptr<Player>;
	using Players       = std::vector<PlayerP>;
	using PlayersById   = IdMap<PlayerP>;

	class FindTileResult;


	World();
	~World();

	ReturnValue      addCreature                  (const CreatureP& creature, const Position& position, uint16_t radius = 10);
	FindTileResult   findTileForThingNearPosition (const Thing& thing, const Position& position, uint16_t radius, uint32_t directFlags = FLAG_PATHFINDING, uint32_t indirectFlags = FLAG_IGNOREFIELDDAMAGE|FLAG_PATHFINDING) const;
	CreatureP        getCreatureById              (CreatureId id) const;
	const Creatures& getCreatures                 () const;
	MonsterP         getMonsterById               (CreatureId id) const;
	const Monsters&  getMonsters                  () const;
	NpcP             getNpcById                   (CreatureId id) const;
	const Npcs&      getNpcs                      () const;
	PlayerP          getPlayerById                (CreatureId id) const;
	const Players&   getPlayers                   () const;
	ReturnValue      removeCreature               (const CreatureP& creature);


private:

	LOGGER_DECLARATION;

	static const CreatureId MAXIMUM_MONSTER_ID;
	static const CreatureId MINIMUM_MONSTER_ID;
	static const CreatureId MAXIMUM_NPC_ID;
	static const CreatureId MINIMUM_NPC_ID;
	static const CreatureId MAXIMUM_PLAYER_ID;
	static const CreatureId MINIMUM_PLAYER_ID;

	World(const World&) = delete;
	World(World&&) = delete;

	Creatures     _creatures;
	CreaturesById _creaturesById;
	Monsters      _monsters;
	MonstersById  _monstersById;
	Npcs          _npcs;
	NpcsById      _npcsById;
	Players       _players;
	PlayersById   _playersById;

};



class World::FindTileResult {

public:

	FindTileResult(Tile* tile, uint32_t flags);
	FindTileResult(ReturnValue error);
	FindTileResult(const FindTileResult&) = default;
	FindTileResult(FindTileResult&&) = default;

	ReturnValue getError  () const;
	uint32_t    getFlags  () const;
	Tile*       getTile   () const;
	bool        isSuccess () const;


private:

	ReturnValue _error;
	uint32_t    _flags;
	Tile*       _tile;

};

#endif // _WORLD_H
