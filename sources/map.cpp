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
#include "map.h"

#include "combat.h"
#include "creature.h"
#include "game.h"
#include "iomap.h"
#include "iomapserialize.h"
#include "item.h"
#include "position.h"
#include "server.h"
#include "tile.h"


LOGGER_DEFINITION(AStarNodes);


AStarNodes::AStarNodes()
{
	curNode = 0;
	openNodes.reset();
}

AStarNode* AStarNodes::createOpenNode()
{
	if(curNode >= MAX_NODES)
		return nullptr;

	uint32_t retNode = curNode;
	curNode++;

	openNodes[retNode] = 1;
	return &nodes[retNode];
}

AStarNode* AStarNodes::getBestNode()
{
	if(!curNode)
		return nullptr;

	int32_t bestNodeF = 100000;
	uint32_t bestNode = 0;

	bool found = false;
	for(uint32_t i = 0; i < curNode; i++)
	{
		if(nodes[i].f < bestNodeF && openNodes[i] == 1)
		{
			found = true;
			bestNodeF = nodes[i].f;
			bestNode = i;
		}
	}

	if(found)
		return &nodes[bestNode];

	return nullptr;
}

void AStarNodes::closeNode(AStarNode* node)
{
	uint32_t pos = GET_NODE_INDEX(node);
	if(pos < MAX_NODES)
	{
		openNodes[pos] = 0;
		return;
	}

	assert(pos >= MAX_NODES);
	LOGe("AStarNodes. trying to close node out of range");
	return;
}

void AStarNodes::openNode(AStarNode* node)
{
	uint32_t pos = GET_NODE_INDEX(node);
	if(pos < MAX_NODES)
	{
		openNodes[pos] = 1;
		return;
	}

	assert(pos >= MAX_NODES);
	LOGe("AStarNodes. trying to open node out of range");
	return;
}

uint32_t AStarNodes::countClosedNodes()
{
	uint32_t counter = 0;
	for(uint32_t i = 0; i < curNode; i++)
	{
		if(!openNodes[i])
			counter++;
	}

	return counter;
}

uint32_t AStarNodes::countOpenNodes()
{
	uint32_t counter = 0;
	for(uint32_t i = 0; i < curNode; i++)
	{
		if(openNodes[i] == 1)
			counter++;
	}

	return counter;
}

bool AStarNodes::isInList(uint16_t x, uint16_t y)
{
	for(uint32_t i = 0; i < curNode; i++)
	{
		if(nodes[i].x == x && nodes[i].y == y)
			return true;
	}

	return false;
}

AStarNode* AStarNodes::getNodeInList(uint16_t x, uint16_t y)
{
	for(uint32_t i = 0; i < curNode; i++)
	{
		if(nodes[i].x == x && nodes[i].y == y)
			return &nodes[i];
	}

	return nullptr;
}

int32_t AStarNodes::getMapWalkCost(const Creature* creature, AStarNode* node,
	const Tile* neighbourTile, const Position& neighbourPos)
{
	if(std::abs(node->x - neighbourPos.x) == std::abs(node->y - neighbourPos.y)) //diagonal movement extra cost
		return MAP_DIAGONALWALKCOST;

	return MAP_NORMALWALKCOST;
}

int32_t AStarNodes::getTileWalkCost(const Creature* creature, const Tile* tile)
{
	int32_t cost = 0;
	if(tile->getTopVisibleCreature(creature)) //destroy creature cost
		cost += MAP_NORMALWALKCOST * 3;

	if(const MagicField* field = tile->getFieldItem())
	{
		if(!creature->isImmune(field->getCombatType()))
			cost += MAP_NORMALWALKCOST * 3;
	}

	return cost;
}

int32_t AStarNodes::getEstimatedDistance(uint16_t x, uint16_t y, uint16_t xGoal, uint16_t yGoal)
{
	int32_t diagonal = std::min(std::abs(x - xGoal), std::abs(y - yGoal));
	return (MAP_DIAGONALWALKCOST * diagonal) + (MAP_NORMALWALKCOST * ((std::abs(
		x - xGoal) + std::abs(y - yGoal)) - (2 * diagonal)));
}



LOGGER_DEFINITION(Map);


Map::Map()
	: _creatureBlockCountX(0),
	  _creatureBlockCountY(0),
	  _creatureBlockCountZ(0),
	  _height(0),
	  _width(0),
	  _creatures(nullptr),
	  _layers { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }
{}


Map::~Map() {
	delete[] _creatures;
}


void Map::addDescription(const std::string& description) {
	_descriptions.push_back(description);
}


bool Map::canThrowObjectTo(const Position& origin, const Position& destination, bool checkLineOfSight /*= true*/, int32_t rangeX /*= Map::maxClientViewportX*/, int32_t rangeY /*= Map::maxClientViewportY*/) const {
	if (isUnderground(origin.z) != isUnderground(destination.z)) {
		return false;
	}
	if (origin.z - destination.z > 2) {
		return false;
	}

	int32_t deltaX = std::abs(origin.x - destination.x);
	int32_t deltaY = std::abs(origin.y - destination.y);
	int32_t deltaZ = std::abs(origin.z - destination.z);
	if (deltaX - deltaZ > rangeX || deltaY - deltaZ > rangeY) {
		return false;
	}

	if (checkLineOfSight && !isSightClear(origin, destination, false)) {
		return false;
	}

	return true;
}


bool Map::canWalkTo(const Creature* creature, const Position& destination) const {
	Tile* tile = getTile(destination);
	if (creature->getTile() != tile && (tile == nullptr || tile->testAddCreature(*creature, FLAG_PATHFINDING|FLAG_IGNOREFIELDDAMAGE) != RET_NOERROR)) {
		return false;
	}

	return true;
}


bool Map::checkSightLine(const Position& origin, const Position& destination) const {
	Position start = origin;
	Position end = destination;

	int32_t x, y, z;
	int32_t dx = std::abs(start.x - end.x);
	int32_t dy = std::abs(start.y - end.y);
	int32_t dz = std::abs(start.z - end.z);
	int32_t sx, sy, sz, ey, ez;
	int32_t max = dx;
	int32_t dir = 0;

	if (dz > max) {
		max = dz;
		dir = 2;
	}
	else if (dy > max) {
		max = dy;
		dir = 1;
	}

	switch (dir) {
		case 1:
			//x -> y
			//y -> x
			//z -> z
			std::swap(start.x, start.y);
			std::swap(end.x, end.y);
			std::swap(dx, dy);
			break;

		case 2: {
			//x -> z
			//y -> y
			//z -> x
			uint16_t temp;

			temp = start.x;
			start.x = start.z;
			start.z = temp;

			temp = end.x;
			end.x = end.z;
			end.z = temp;

			std::swap(dx, dz);
			break;
		}

		default:
			//x -> x
			//y -> y
			//z -> z
			break;
	}

	sx = ((start.x < end.x) ? 1 : -1);
	sy = ((start.y < end.y) ? 1 : -1);
	sz = ((start.z < end.z) ? 1 : -1);

	ey = ez = 0;
	x = start.x;
	y = start.y;
	z = start.z;

	int32_t lastrx = 0, lastry = 0, lastrz = 0;
	for (; x != end.x + sx; x += sx) {
		int32_t rx, ry, rz;
		switch (dir) {
			case 1:
				rx = y; ry = x; rz = z;
				break;
			case 2:
				rx = z; ry = y; rz = x;
				break;
			default:
				rx = x; ry = y; rz = z;
				break;
		}

		if (!lastrx && !lastry && !lastrz) {
			lastrx = rx;
			lastry = ry;
			lastrz = rz;
		}

		if (lastrz != rz || ((destination.x != rx || destination.y != ry || destination.z != rz) && (origin.x != rx || origin.y != ry || origin.z != rz))) {
			if (lastrz != rz && this->getTile(lastrx, lastry, std::min(lastrz, rz)) != nullptr) {
				return false;
			}

			lastrx = rx;
			lastry = ry;
			lastrz = rz;

			const Tile* tile = this->getTile(rx, ry, rz);
			if (tile != nullptr && tile->hasProperty(BLOCKPROJECTILE)) {
				return false;
			}
		}

		ey += dy;
		ez += dz;

		if (2 * ey >= dx) {
			y += sy;
			ey -= dx;
		}

		if (2 * ez >= dx) {
			z += sz;
			ez -= dx;
		}
	}

	return true;
}


void Map::clearSpectatorCache() {
	_spectatorCache.clear();
}


uint32_t Map::creaturesIndexForPosition(const Position& position) const {
	uint32_t yBlocks = _creatureBlockCountY;
	uint32_t zBlocks = _creatureBlockCountZ;

	uint32_t xIndex = zBlocks * yBlocks * (position.x / Map::creaturesBlockSize);
	uint32_t yIndex = zBlocks * (position.y / Map::creaturesBlockSize);
	uint32_t zIndex = position.z;

	return (xIndex + yIndex + zIndex);
}


uint16_t Map::getHeight() const {
	return _height;
}


bool Map::getPathMatching(const Creature* creature, Route& route, const FrozenPathingConditionCall& pathCondition, const FindPathParams& findParameters) const {
	route.clear();

	Position startPos = creature->getPosition();
	Position endPos;

	AStarNodes nodes;
	AStarNode* startNode = nodes.createOpenNode();

	startNode->x = startPos.x;
	startNode->y = startPos.y;

	startNode->f = 0;
	startNode->parent = nullptr;

	int32_t bestMatch = 0;

	Position pos;
	pos.z = startPos.z;

	static const int16_t neighbourOrderList[8][2] = {
		{-1, 0},
		{0, 1},
		{1, 0},
		{0, -1},

		//diagonal
		{-1, -1},
		{1, -1},
		{1, 1},
		{-1, 1},
	};

	AStarNode* found = nullptr;
	AStarNode* n = nullptr;

	while (findParameters.maxSearchDist != -1 || nodes.countClosedNodes() < 100) {
		n = nodes.getBestNode();
		if (n == nullptr) {
			if (found) {
				// not quite what we want, but we found something
				break;
			}

			// no path found
			route.clear();
			return false;
		}

		if (pathCondition(startPos, Position(n->x, n->y, startPos.z), findParameters, bestMatch)) {
			found = n;
			endPos = Position(n->x, n->y, startPos.z);

			if (!bestMatch) {
				break;
			}
		}

		int32_t dirCount = (findParameters.allowDiagonal ? 8 : 4);
		for (int32_t i = 0; i < dirCount; ++i) {
			int32_t newX = n->x + neighbourOrderList[i][0];
			if (newX < 0 || newX > Map::maxX) {
				continue;
			}

			int32_t newY = n->y + neighbourOrderList[i][1];
			if (newY < 0 || newY > Map::maxY) {
				continue;
			}

			pos.x = static_cast<uint16_t>(newX);
			pos.y = static_cast<uint16_t>(newY);

			if (findParameters.maxSearchDist != -1 && (std::abs(startPos.x - pos.x) > findParameters.maxSearchDist || std::abs(startPos.y - pos.y) > findParameters.maxSearchDist)) {
				continue;
			}

			if (findParameters.keepDistance && !pathCondition.isInRange(startPos, pos, findParameters)) {
				continue;
			}

			if (canWalkTo(creature, pos)) {
				const Tile* tile = getTile(pos);

				//The cost (g) for this neighbour
				int32_t cost = nodes.getMapWalkCost(creature, n, tile, pos);
				int32_t extraCost = nodes.getTileWalkCost(creature, tile);
				int32_t newf = n->f + cost + extraCost;

				//Check if the node is already in the closed/open list
				//If it exists and the nodes already on them has a lower cost (g) then we can ignore this neighbour node
				AStarNode* neighbourNode = nodes.getNodeInList(pos.x, pos.y);
				if (neighbourNode != nullptr) {
					if (neighbourNode->f <= newf) { //The node on the closed/open list is cheaper than this one
						continue;
					}

					nodes.openNode(neighbourNode);
				}
				else {
					// Does not exist in the open/closed list, create a new node
					neighbourNode = nodes.createOpenNode();
					if (neighbourNode == nullptr) {
						if (found) {
							// not quite what we want, but we found something
							break;
						}

						// seems we ran out of nodes
						route.clear();
						return false;
					}
				}

				//This node is the best node so far with this state
				neighbourNode->x = pos.x;
				neighbourNode->y = pos.y;

				neighbourNode->parent = n;
				neighbourNode->f = newf;
			}
		}

		nodes.closeNode(n);
	}

	if (!found) {
		return false;
	}

	int32_t prevx = endPos.x;
	int32_t prevy = endPos.y;
	int32_t dx, dy;

	found = found->parent;
	while (found) {
		pos.x = found->x;
		pos.y = found->y;

		dx = pos.x - prevx;
		dy = pos.y - prevy;

		prevx = pos.x;
		prevy = pos.y;

		found = found->parent;
		if (dx == 1 && dy == 1) {
			route.push_front(Direction::NORTH_WEST);
		}
		else if (dx == -1 && dy == 1) {
			route.push_front(Direction::NORTH_EAST);
		}
		else if (dx == 1 && dy == -1) {
			route.push_front(Direction::SOUTH_WEST);
		}
		else if (dx == -1 && dy == -1) {
			route.push_front(Direction::SOUTH_EAST);
		}
		else if (dx == 1) {
			route.push_front(Direction::WEST);
		}
		else if (dx == -1) {
			route.push_front(Direction::EAST);
		}
		else if (dy == 1) {
			route.push_front(Direction::NORTH);
		}
		else if (dy == -1) {
			route.push_front(Direction::SOUTH);
		}
	}

	return true;
}


bool Map::getPathTo(const Creature* creature, const Position& destination, Route& route, int32_t maxDistance /*= -1*/) const {
	route.clear();

	if (!canWalkTo(creature, destination)) {
		return false;
	}

	Position startPos = destination;
	Position endPos = creature->getPosition();

	if (startPos.z != endPos.z) {
		return false;
	}

	AStarNodes nodes;
	AStarNode* startNode = nodes.createOpenNode();

	startNode->x = startPos.x;
	startNode->y = startPos.y;

	startNode->g = 0;
	startNode->h = nodes.getEstimatedDistance(startPos.x, startPos.y, endPos.x, endPos.y);

	startNode->f = startNode->g + startNode->h;
	startNode->parent = nullptr;

	Position pos;
	pos.z = startPos.z;

	static const int16_t neighbourOrderList[8][2] = {
		{-1, 0},
		{0, 1},
		{1, 0},
		{0, -1},

		//diagonal
		{-1, -1},
		{1, -1},
		{1, 1},
		{-1, 1},
	};

	AStarNode* found = nullptr;
	AStarNode* n = nullptr;

	while (maxDistance != -1 || nodes.countClosedNodes() < 100) {
		n = nodes.getBestNode();
		if (n == nullptr) {
			// no path found
			route.clear();
			return false;
		}

		if (n->x == endPos.x && n->y == endPos.y) {
			found = n;
			break;
		}

		for (uint8_t i = 0; i < 8; ++i) {
			int32_t newX = n->x + neighbourOrderList[i][0];
			if (newX < 0 || newX > Map::maxX) {
				continue;
			}

			int32_t newY = n->y + neighbourOrderList[i][1];
			if (newY < 0 || newY > Map::maxY) {
				continue;
			}

			pos.x = static_cast<uint16_t>(newX);
			pos.y = static_cast<uint16_t>(newY);

			if (maxDistance != -1 && (std::abs(endPos.x - pos.x) > maxDistance || std::abs(endPos.y - pos.y) > maxDistance)) {
				continue;
			}

			if (canWalkTo(creature, pos)) {
				const Tile* tile = getTile(pos);

				//The cost (g) for this neighbour
				int32_t cost = nodes.getMapWalkCost(creature, n, tile, pos);
				int32_t extraCost = nodes.getTileWalkCost(creature, tile);
				int32_t newg = n->g + cost + extraCost;

				//Check if the node is already in the closed/open list
				//If it exists and the nodes already on them has a lower cost (g) then we can ignore this neighbour node
				AStarNode* neighbourNode = nodes.getNodeInList(pos.x, pos.y);
				if (neighbourNode != nullptr) {
					if (neighbourNode->g <= newg) { //The node on the closed/open list is cheaper than this one
						continue;
					}

					nodes.openNode(neighbourNode);
				}
				else {
					// Does not exist in the open/closed list, create a new node
					neighbourNode = nodes.createOpenNode();
					if (neighbourNode == nullptr) {
						// seems we ran out of nodes
						route.clear();
						return false;
					}
				}

				//This node is the best node so far with this state
				neighbourNode->x = pos.x;
				neighbourNode->y = pos.y;

				neighbourNode->g = newg;
				neighbourNode->h = nodes.getEstimatedDistance(neighbourNode->x, neighbourNode->y, endPos.x, endPos.y);

				neighbourNode->f = neighbourNode->g + neighbourNode->h;
				neighbourNode->parent = n;
			}
		}

		nodes.closeNode(n);
	}

	int32_t prevx = endPos.x;
	int32_t prevy = endPos.y;
	int32_t dx, dy;

	while (found) {
		pos.x = found->x;
		pos.y = found->y;

		dx = pos.x - prevx;
		dy = pos.y - prevy;

		prevx = pos.x;
		prevy = pos.y;

		found = found->parent;
		if (dx == -1 && dy == -1) {
			route.push_back(Direction::NORTH_WEST);
		}
		else if (dx == 1 && dy == -1) {
			route.push_back(Direction::NORTH_EAST);
		}
		else if (dx == -1 && dy == 1) {
			route.push_back(Direction::SOUTH_WEST);
		}
		else if (dx == 1 && dy == 1) {
			route.push_back(Direction::SOUTH_EAST);
		}
		else if (dx == -1) {
			route.push_back(Direction::WEST);
		}
		else if (dx == 1) {
			route.push_back(Direction::EAST);
		}
		else if (dy == -1) {
			route.push_back(Direction::NORTH);
		}
		else if (dy == 1) {
			route.push_back(Direction::SOUTH);
		}
	}

	return !route.empty();
}


const std::string& Map::getHousesFileName() const {
	return _housesFileName;
}


const std::string& Map::getSpawnsFileName() const {
	return _spawnsFileName;
}


void Map::getSpectators(SpectatorList& spectators, const Position& center, bool checkForDuplicates, int32_t minOffsetX, int32_t maxOffsetX, int32_t minOffsetY, int32_t maxOffsetY, uint16_t minZ, uint16_t maxZ) const {
	if (_creatures == nullptr) {
		return;
	}

	uint32_t yBlocks = _creatureBlockCountY;
	uint32_t zBlocks = _creatureBlockCountZ;

	auto indexStepX = zBlocks * yBlocks;
	auto indexStepY = zBlocks;

	auto basicMinX = static_cast<int32_t>(center.x) + minOffsetX;
	auto basicMaxX = static_cast<int32_t>(center.x) + maxOffsetX;
	auto basicMinY = static_cast<int32_t>(center.y) + minOffsetY;
	auto basicMaxY = static_cast<int32_t>(center.y) + maxOffsetY;

	for (auto z = minZ; z <= maxZ; ++z) {
		auto deltaZ = center.z - z;

		auto minX = static_cast<uint16_t>(std::max(0, std::min(basicMinX + deltaZ, static_cast<int32_t>(Map::maxX))));
		auto maxX = static_cast<uint16_t>(std::max(0, std::min(basicMaxX + deltaZ, static_cast<int32_t>(Map::maxX))));
		auto minY = static_cast<uint16_t>(std::max(0, std::min(basicMinY + deltaZ, static_cast<int32_t>(Map::maxY))));
		auto maxY = static_cast<uint16_t>(std::max(0, std::min(basicMaxY + deltaZ, static_cast<int32_t>(Map::maxY))));

		if (maxX < minX || maxY < minY) {
			continue;
		}

		auto minIndexX = indexStepX * (minX / Map::creaturesBlockSize);
		auto maxIndexX = indexStepX * (maxX / Map::creaturesBlockSize);
		auto minIndexY = indexStepY * (minY / Map::creaturesBlockSize);
		auto maxIndexY = indexStepY * (maxY / Map::creaturesBlockSize);

		for (auto indexX = minIndexX; indexX <= maxIndexX; indexX += indexStepX) {
			for (auto indexY = minIndexY; indexY <= maxIndexY; indexY += indexStepY) {
				auto index = indexX + indexY + z;

				const auto& creatures = _creatures[index];
				for (const auto& creature : creatures) {
					const auto& position = creature->getPosition();
					if (position.x < minX || position.x > maxX || position.y < minY || position.y > maxY) {
						continue;
					}

					if (!checkForDuplicates || std::find(spectators.begin(), spectators.end(), creature) == spectators.end()) {
						spectators.push_back(creature);
					}
				}
			}
		}
	}
}


void Map::getSpectators(SpectatorList& spectators, const Position& center, bool checkForDuplicates /*= false*/, bool multiFloor /*= false*/, int32_t westRange /*= 0*/, int32_t eastRange /*= 0*/, int32_t northRange /*= 0*/, int32_t southRange /*= 0*/) {
	if (center.z > Map::maxZ) {
		return;
	}

	int32_t minOffsetX = (westRange == 0 ? -Map::maxViewportX : -westRange);
	int32_t maxOffsetX = (eastRange == 0 ? Map::maxViewportX : eastRange);
	int32_t minOffsetY = (northRange == 0 ? -Map::maxViewportY : -northRange);
	int32_t maxOffsetY = (southRange == 0 ? Map::maxViewportY : southRange);

	bool cacheResult = false;
	if (minOffsetX == -Map::maxViewportX && maxOffsetX == Map::maxViewportX && minOffsetY == -Map::maxViewportY && maxOffsetY == Map::maxViewportY && multiFloor && !checkForDuplicates) {
		auto entry = _spectatorCache.find(center);
		if (entry != _spectatorCache.end() && &entry->second != &spectators) {
			// TODO allow caching with checkForDuplicates = true
			spectators = entry->second;
			return;
		}

		cacheResult = true;
	}

	uint16_t minZ, maxZ;
	if (multiFloor) {
		if (isUnderground(center.z)) {
			minZ = static_cast<uint16_t>(std::max(static_cast<int32_t>(center.z) - 2, 0));
			maxZ = static_cast<uint16_t>(std::min(static_cast<uint32_t>(center.z) + 2, static_cast<uint32_t>(Map::maxZ)));
		}
		else {
			minZ = 0;
			maxZ = 7;
		}
	}
	else {
		minZ = center.z;
		maxZ = center.z;
	}

	getSpectators(spectators, center, true, minOffsetX, maxOffsetX, minOffsetY, maxOffsetY, minZ, maxZ);

	if (cacheResult) {
		SpectatorList& cachedSpectators = _spectatorCache[center];
		if (&cachedSpectators != &spectators) {
			cachedSpectators = spectators;
		}
	}
}


const SpectatorList& Map::getSpectators(const Position& center) {
	if (center.z > Map::maxZ) {
		static SpectatorList empty;
		return empty;
	}

	bool cached = (_spectatorCache.find(center) != _spectatorCache.end());

	SpectatorList& spectators = _spectatorCache[center];
	if (!cached) {
		getSpectators(spectators, center, false, true, 0, 0, 0, 0);
	}

	return spectators;
}


Tile* Map::getTile(int32_t x, int32_t y, int32_t z) const {
	if (x < 0 || x > Map::maxX || y < 0 || y > Map::maxY || z < 0 || z > Map::maxZ) {
		return nullptr;
	}

	return _layers[z].getTile(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
}


Tile* Map::getTile(const Position& position) const {
	return getTile(position.x, position.y, position.z);
}


Waypoints& Map::getWaypoints() {
	return _waypoints;
}


const Waypoints& Map::getWaypoints() const {
	return _waypoints;
}


uint16_t Map::getWidth() const {
	return _width;
}


bool Map::isSightClear(const Position& origin, const Position& destination, bool requireSameFloor) const {
	if (requireSameFloor && origin.z != destination.z) {
		return false;
	}

	// Cast two converging rays and see if either yields a result.
	return (checkSightLine(origin, destination) || checkSightLine(destination, origin));
}


bool Map::isUnderground(uint32_t z) {
	return (z >= 8);
}


bool Map::load(const std::string& identifier) {
	delete[] _creatures;
	_creatures = nullptr;

	IOMap loader;

	if (!loader.loadMap(this, identifier)) {
		LOGe("OTBM Loader - " << loader.getLastErrorString());
		return false;
	}

	for (uint32_t z = 0; z < Map::numZ; ++z) {
		_layers[z].complete();
	}

	IOMapSerialize& serializer = *IOMapSerialize::getInstance();

	LOGi("Loading spawns...");
	if (!loader.loadSpawns(this)) {
		LOGe("WARNING: Could not load spawn data.");
		return false;
	}

	LOGi("Loading houses...");
	if (!loader.loadHouses(this)) {
		LOGe("Could not load house data.");
		return false;
	}
	serializer.updateHouses();
	serializer.updateAuctions();
	serializer.loadHouses();
	serializer.loadMap(this);

	return true;
}


void Map::onCreatureMoved(Creature* creature, const Tile* fromTile, const Tile* toTile) {
	int32_t fromIndex = (fromTile != nullptr ? creaturesIndexForPosition(fromTile->getPosition()) : -1);
	int32_t toIndex = (toTile != nullptr ? creaturesIndexForPosition(toTile->getPosition()) : -1);

	if (fromIndex == toIndex) {
		return;
	}

	if (fromIndex >= 0 && _creatures != nullptr) {
		CreatureList& creaturesBlock = _creatures[fromIndex];
		CreatureList::iterator it = std::find(creaturesBlock.begin(), creaturesBlock.end(), creature);
		assert(it != creaturesBlock.end());
		creaturesBlock.erase(it);
	}

	if (toIndex >= 0) {
		if (_creatures == nullptr) {
			_creatures = new CreatureList[_creatureBlockCountX * _creatureBlockCountY * _creatureBlockCountZ];
		}

		CreatureList& creaturesBlock = _creatures[toIndex];
		creaturesBlock.push_back(creature);
	}
}


bool Map::placeCreature(const Position& center, Creature* creature, bool extendedRangeInSight /*= false*/, bool ignoreObstacles /*= false*/) {
	// TODO refactor
	typedef std::pair<int32_t, int32_t> PositionPair;
	typedef std::vector<PositionPair> PairVector;

	bool placeInProtectionZone = false;
	bool tileIsValid = false;

	Tile* tile = getTile(center);
	if (tile != nullptr) {
		placeInProtectionZone = tile->hasFlag(TILESTATE_PROTECTIONZONE);

		uint32_t flags = FLAG_IGNOREBLOCKITEM;
		if (creature->isAccountManager()) {
			flags |= FLAG_IGNOREBLOCKCREATURE;
		}

		if (ignoreObstacles) {
			flags |= FLAG_IGNOREFIELDDAMAGE;
			tileIsValid = true;
		}
		else {
			ReturnValue result = tile->testAddCreature(*creature, flags);
			if (result == RET_NOERROR || result == RET_PLAYERISNOTINVITED) {
				tileIsValid = true;
			}
		}
	}

	if (!tileIsValid) {
		PairVector possibleOffsets;
		if (extendedRangeInSight) {
			possibleOffsets.push_back(PositionPair(-2, 0));
			possibleOffsets.push_back(PositionPair(0, -2));
			possibleOffsets.push_back(PositionPair(0, 2));
			possibleOffsets.push_back(PositionPair(2, 0));
			std::random_shuffle(possibleOffsets.begin(), possibleOffsets.end());
		}

		possibleOffsets.push_back(PositionPair(-1, -1));
		possibleOffsets.push_back(PositionPair(-1, 0));
		possibleOffsets.push_back(PositionPair(-1, 1));
		possibleOffsets.push_back(PositionPair(0, -1));
		possibleOffsets.push_back(PositionPair(0, 1));
		possibleOffsets.push_back(PositionPair(1, -1));
		possibleOffsets.push_back(PositionPair(1, 0));
		possibleOffsets.push_back(PositionPair(1, 1));
		std::random_shuffle(possibleOffsets.end() - 8, possibleOffsets.end());

		Position position;
		position.z = center.z;

		for (PairVector::iterator it = possibleOffsets.begin(); it != possibleOffsets.end() && !tileIsValid; ++it) {
			int32_t x = center.x + it->first;
			if (x < 0 || x > Map::maxX) {
				continue;
			}

			int32_t y = center.y + it->second;
			if (y < 0 || y > Map::maxY) {
				continue;
			}

			position.x = static_cast<uint16_t>(x);
			position.y = static_cast<uint16_t>(y);

			tile = getTile(position);
			if (tile == nullptr) {
				continue;
			}

			if (placeInProtectionZone && !tile->hasFlag(TILESTATE_PROTECTIONZONE)) {
				continue;
			}

			if (tile->testAddCreature(*creature) != RET_NOERROR) {
				continue;
			}

			if (extendedRangeInSight && !isSightClear(center, position, false)) {
				continue;
			}

			tileIsValid = true;
		}

		if (!tileIsValid) {
			return false;
		}
	}

	return (tile->addCreature(creature) == RET_NOERROR);
}


bool Map::save() const {
	IOMapSerialize& serializer = *IOMapSerialize::getInstance();

	bool saved = false;
	for (uint32_t tries = 0; tries < 3; ++tries) {
		if (!serializer.saveHouses()) {
			continue;
		}

		saved = true;
		break;
	}

	if (!saved) {
		return false;
	}

	saved = false;
	for (uint32_t tries = 0; tries < 3; ++tries) {
		if (!serializer.saveMap(this)) {
			continue;
		}

		saved = true;
		break;
	}

	return saved;
}


void Map::setHousesFileName(const std::string& housesFileName) {
	_housesFileName = housesFileName;
}


void Map::setSpawnsFileName(const std::string& spawnsFileName) {
	_spawnsFileName = spawnsFileName;
}


void Map::setSize(uint16_t width, uint16_t height) {
	_height = height;
	_width = width;

	if (_creatures != nullptr) {
		assert(_creatures == nullptr);
		delete[] _creatures;
		_creatures = nullptr;
	}

	_creatureBlockCountX = static_cast<uint16_t>(width / Map::creaturesBlockSize + 1u);
	_creatureBlockCountY = static_cast<uint16_t>(height / Map::creaturesBlockSize + 1u);
	_creatureBlockCountZ = Map::numZ;

	for (uint32_t z = 0; z < Map::numZ; ++z) {
		_layers[z].prepareWithSize(width, height);
	}
}


bool Map::setTile(uint16_t x, uint16_t y, uint16_t z, Tile* tile) {
	if (x >= _width || y >= _height || z > Map::maxZ) {
		LOGe("Cannot set tile at invalid map coordinate " << x << "/" << y << "/" << z);
		return false;
	}

	if (_layers[z].getTile(x, y) != nullptr) {
		LOGe("Another tile already exists at map coordinate " << x << "/" << y << "/" << z);
		return false;
	}

	if (!_layers[z].setTile(x, y, tile)) {
		return false;
	}

	if (tile->hasFlag(TILESTATE_REFRESH)) {
		RefreshBlock_t refreshBlock;
		if (TileItemVector* tileItems = tile->getItemList()) {
			for (ItemVector::iterator it = tileItems->getBeginDownItem(); it != tileItems->getEndDownItem(); ++it) {
				refreshBlock.list.push_back((*it)->clone().get());
			}
		}

		refreshBlock.lastRefresh = OTSYS_TIME();
		server.game().addRefreshTile(tile, refreshBlock);
	}

	return true;
}


bool Map::setTile(const Position& position, Tile* tile) {
	return setTile(position.x, position.y, position.z, tile);
}



LOGGER_DEFINITION(MapLayer);


MapLayer::MapLayer(uint8_t z)
	: _completed(false),
	  _endX(0),
	  _endY(0),
	  _maxLoadedX(0),
	  _maxLoadedY(0),
	  _minLoadedX(std::numeric_limits<uint16_t>::max()),
	  _minLoadedY(std::numeric_limits<uint16_t>::max()),
	  _startX(0),
	  _startY(0),
	  _z(z),
	  _tiles(nullptr)
{}


MapLayer::~MapLayer() {
	clear();
}


void MapLayer::clear() {
	if (_tiles != nullptr) {
		uint32_t count = (_endX - _startX) * (_endY - _startY);
		for (uint32_t i = 0; i < count; ++i) {
			delete _tiles[i];
		}

		delete[] _tiles;
		_tiles = nullptr;
	}

	_startX = _endX = _startY = _endY = _maxLoadedX = _maxLoadedY = 0;
	_minLoadedX = _minLoadedY = std::numeric_limits<uint16_t>::max();
	_completed = false;
}


void MapLayer::complete() {
	if (_completed) {
		assert(!_completed);
		return;
	}

	if (_minLoadedX > _maxLoadedX || _minLoadedY > _maxLoadedY || _tiles == nullptr) {
		clear();

		_completed = true;
		return;
	}

	_completed = true;

	uint32_t width = (_endX - _startX);

	if (_minLoadedX == _startX && _minLoadedY == _startY && _maxLoadedX == _endX - 1 && _maxLoadedY == _endY - 1) {
		return;
	}

	uint32_t optimizedWidth = (_maxLoadedX - _minLoadedX + 1);
	uint32_t optimizedHeight = (_maxLoadedY - _minLoadedY + 1);

	Tile** optimizedTiles = new Tile*[optimizedWidth * optimizedHeight];
	for (uint32_t y = _minLoadedY; y <= _maxLoadedY; ++y) {
		uint32_t startIndex = (width * (y - _startY)) + _minLoadedX;
		uint32_t optimizedStartIndex = (optimizedWidth * (y - _minLoadedY));

		Tile* const* startPointer = &_tiles[startIndex];
		Tile** optimizedStartPointer = &optimizedTiles[optimizedStartIndex];

		memcpy(optimizedStartPointer, startPointer, optimizedWidth * sizeof(Tile*));
	}

	_startX = _minLoadedX;
	_startY = _minLoadedY;
	_endX = _maxLoadedX + 1;
	_endY = _maxLoadedY + 1;

	delete[] _tiles;
	_tiles = optimizedTiles;
}


Tile* MapLayer::getTile(uint16_t x, uint16_t y) const {
	int32_t index = indexForPosition(x, y);
	if (index < 0) {
		return nullptr;
	}

	return _tiles[index];
}


int32_t MapLayer::indexForPosition(uint16_t x, uint16_t y) const {
	if (x < _startX || y < _startY || x >= _endX || y >= _endY) {
		return -1;
	}

	uint32_t indexX = x - _startX;
	uint32_t indexY = (_endX - _startX) * (y - _startY);

	return (indexX + indexY);
}


void MapLayer::prepareWithSize(uint16_t width, uint16_t height) {
	clear();

	_endX = width;
	_endY = height;

	_tiles = new Tile*[width * height];
	memset(_tiles, 0, width * height * sizeof(Tile*));
}


bool MapLayer::setTile(uint16_t x, uint16_t y, Tile* tile) {
	int32_t index = indexForPosition(x, y);
	if (index < 0) {
		LOGe("Cannot set tile at invalid map coordinate " << x << "/" << y << "/" << _z);
		return false;
	}

	if (!_completed) {
		if (x < _minLoadedX) {
			_minLoadedX = x;
		}
		if (x > _maxLoadedX) {
			_maxLoadedX = x;
		}
		if (y < _minLoadedY) {
			_minLoadedY = y;
		}
		if (y > _maxLoadedY) {
			_maxLoadedY = y;
		}
	}

	delete _tiles[index];
	_tiles[index] = tile;

	return true;
}
