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

#ifndef _MAP_H
#define _MAP_H

#include "waypoints.h"

#define MAX_NODES  512
#define GET_NODE_INDEX(a) (a - &nodes[0])

#define MAP_NORMALWALKCOST 10
#define MAP_DIAGONALWALKCOST 25

#define FLOOR_BITS 3
#define FLOOR_SIZE (1 << FLOOR_BITS)
#define FLOOR_MASK (FLOOR_SIZE - 1)

#define MAP_MAX_LAYERS 16

class Creature;
class FindPathParams;
class FrozenPathingConditionCall;
class Position;
class Tile;

using CreatureP = boost::intrusive_ptr<Creature>;

typedef std::list<boost::intrusive_ptr<Creature>>  CreatureList;
typedef std::list<CreatureP>                       SpectatorList;
typedef std::unordered_map<Position,SpectatorList> SpectatorCache;


struct AStarNode {
	uint16_t x, y;
	AStarNode* parent;
	int32_t f, g, h;
};



class AStarNodes {

public:

	AStarNodes();

	void openNode(AStarNode* node);
	void closeNode(AStarNode* node);

	uint32_t countOpenNodes();
	uint32_t countClosedNodes();

	AStarNode* getBestNode();
	AStarNode* createOpenNode();
	AStarNode* getNodeInList(uint16_t x, uint16_t y);

	bool isInList(uint16_t x, uint16_t y);
	int32_t getEstimatedDistance(uint16_t x, uint16_t y, uint16_t xGoal, uint16_t yGoal);

	int32_t getMapWalkCost(const Creature* creature, AStarNode* node,
		const Tile* neighbourTile, const Position& neighbourPos);
	static int32_t getTileWalkCost(const Creature* creature, const Tile* tile);

private:


	LOGGER_DECLARATION;

	AStarNode nodes[MAX_NODES];

	std::bitset<MAX_NODES> openNodes;
	uint32_t curNode;

};



class MapLayer {

private:

	MapLayer(uint8_t z);
	~MapLayer();

	void    clear            ();
	void    complete         ();
	Tile*   getTile          (uint16_t x, uint16_t y) const;
	int32_t indexForPosition (uint16_t x, uint16_t y) const;
	void    prepareWithSize  (uint16_t width, uint16_t height);
	bool    setTile          (uint16_t x, uint16_t y, Tile* tile);


	LOGGER_DECLARATION;

	bool _completed;

	uint32_t _endX;
	uint32_t _endY;
	uint32_t _maxLoadedX;
	uint32_t _maxLoadedY;
	uint32_t _minLoadedX;
	uint32_t _minLoadedY;
	uint32_t _startX;
	uint32_t _startY;
	uint32_t _z;

	Tile** _tiles;

	friend class Map;

};



class Map {

public:

	typedef std::deque<Direction>  Route;


	static const uint16_t creaturesBlockSize = 100;
	static const uint16_t maxX = 60000;
	static const uint16_t maxY = 60000;
	static const uint16_t maxZ = 15;
	static const uint16_t maxViewportX = 11;
	static const uint16_t maxViewportY = 11;
	static const uint16_t maxClientViewportX = 8;
	static const uint16_t maxClientViewportY = 6;
	static const uint32_t numZ = maxZ + 1;


	Map();
	~Map();

	bool                 canThrowObjectTo    (const Position& origin, const Position& destination, bool checkLineOfSight = true, int32_t rangeX = Map::maxClientViewportX, int32_t rangeY = Map::maxClientViewportY) const;
	bool                 checkSightLine      (const Position& origin, const Position& destination) const;
	void                 clearSpectatorCache ();
	uint16_t             getHeight           () const;
	bool                 getPathMatching     (const Creature* creature, Route& route, const FrozenPathingConditionCall& pathCondition, const FindPathParams& findParameters) const;
	bool                 getPathTo           (const Creature* creature, const Position& destination, Route& route, int32_t maxDistance = -1) const;
	const SpectatorList& getSpectators       (const Position& center);
	void                 getSpectators       (SpectatorList& spectators, const Position& center, bool checkForDuplicates = false, bool multiFloor = false, int32_t westRange = 0, int32_t eastRange = 0, int32_t northRange = 0, int32_t southRange = 0);
	Tile*                getTile             (int32_t x, int32_t y, int32_t z) const;
	Tile*                getTile             (const Position& position) const;
	Waypoints&           getWaypoints        ();
	const Waypoints&     getWaypoints        () const;
	uint16_t             getWidth            () const;
	bool                 isSightClear        (const Position& origin, const Position& to, bool requireSameFloor) const;
	bool                 load                (const std::string& identifier);
	void                 onCreatureMoved     (Creature* creature, const Tile* fromTile, const Tile* toTile);
	bool                 placeCreature       (const Position& center, Creature* creature, bool extendedRangeInSight = false, bool ignoreObstacles = false);
	bool                 save                () const;
	bool                 setTile             (uint16_t x, uint16_t y, uint16_t z, Tile* tile);
	bool                 setTile             (const Position& position, Tile* tile);

	static bool isUnderground (uint32_t z);


private:

	void               addDescription            (const std::string& description);
	bool               canWalkTo                 (const Creature* creature, const Position& destination) const;
	uint32_t           creaturesIndexForPosition (const Position& position) const;
	const std::string& getHousesFileName         () const;
	const std::string& getSpawnsFileName         () const;
	void               setHousesFileName         (const std::string& housesFileName);
	void               getSpectators             (SpectatorList& spectators, const Position& center, bool checkForDuplicates, int32_t minOffsetX, int32_t maxOffsetX, int32_t minOffsetY, int32_t maxOffsetY, uint16_t minZ, uint16_t maxZ) const;
	void               setSize                   (uint16_t width, uint16_t height);
	void               setSpawnsFileName         (const std::string& spawnsFileName);


	LOGGER_DECLARATION;

	uint16_t _creatureBlockCountX;
	uint16_t _creatureBlockCountY;
	uint16_t _creatureBlockCountZ;
	uint16_t _height;
	uint16_t _width;

	CreatureList* _creatures;

	std::vector<std::string> _descriptions;
	std::string              _housesFileName;
	MapLayer                 _layers[numZ];
	std::string              _spawnsFileName;
	SpectatorCache           _spectatorCache;
	Waypoints                _waypoints;


	friend class IOMap;
};

#endif // _MAP_H
