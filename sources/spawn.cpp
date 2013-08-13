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

#include "spawn.h"
#include "tools.h"

#include "player.h"
#include "npc.h"

#include "configmanager.h"
#include "game.h"
#include "monster.h"
#include "monsters.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"
#include "world.h"

#define MINSPAWN_INTERVAL 1000
#define DEFAULTSPAWN_INTERVAL 60000


LOGGER_DEFINITION(Spawns);


Direction Spawns::getDirectionForPosition(const Position& position) const {
	auto i = _directions.find(position);
	if (i == _directions.end()) {
		return Direction::NONE;
	}

	return i->second;
}


void Spawns::loadDirections() {
	_directions.clear();

	auto state = std::unique_ptr<lua_State>(luaL_newstate());

	auto L = state.get();
	assert(L != nullptr);

	luaL_openlibs(L);

	auto path = getFilePath(FileType::OTHER, "world/spawnDirections.lua");
	if (!boost::filesystem::exists(path)) {
		LOGd("Won't load non-existent spawn direction file '" << path << "'.");
		return;
	}

	LOGi("Loading spawn directions...");

	auto result = luaL_loadfile(L, path.c_str());
	if (result != 0) {
		LOGe("Cannot parse spawn direction file: " << lua_tostring(L, -1));
		return;
	}

	lua_register(L, "spawn", luaAddSpawnDirection);

	lua_pushinteger(L, +Direction::EAST);
	lua_setglobal(L, "EAST");
	lua_pushinteger(L, +Direction::NORTH);
	lua_setglobal(L, "NORTH");
	lua_pushinteger(L, +Direction::SOUTH);
	lua_setglobal(L, "SOUTH");
	lua_pushinteger(L, +Direction::WEST);
	lua_setglobal(L, "WEST");

	result = lua_pcall(L, 0, 0, 0);
	if (result != 0) {
		_directions.clear();
		LOGe("Cannot run spawn direction file " << lua_tostring(L, -1));
	}
}


int Spawns::luaAddSpawnDirection(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_pushstring(L, "x");
	lua_gettable(L, -2);
	if (!lua_isnumber(L, -1)) {
		luaL_error(L, "'x' must be set to a number.");
	}
	lua_pop(L, 1);
	auto x = static_cast<int_fast32_t>(lua_tonumber(L, 0));

	lua_pushstring(L, "y");
	lua_gettable(L, -2);
	if (!lua_isnumber(L, -1)) {
		luaL_error(L, "'y' must be set to a number.");
	}
	lua_pop(L, 1);
	auto y = static_cast<int_fast32_t>(lua_tonumber(L, 0));

	lua_pushstring(L, "z");
	lua_gettable(L, -2);
	if (!lua_isnumber(L, -1)) {
		luaL_error(L, "'z' must be set to a number.");
	}
	lua_pop(L, 1);
	auto z = static_cast<int_fast32_t>(lua_tonumber(L, 0));

	lua_pushstring(L, "direction");
	lua_gettable(L, -2);

	auto direction = Direction::NONE;
	if (lua_isnumber(L, -1)) {
		lua_pop(L, 1);
		direction = static_cast<Direction>(lua_tonumber(L, 0));
	}

	switch (direction) {
	case Direction::EAST:
	case Direction::NORTH:
	case Direction::SOUTH:
	case Direction::WEST:
		break;

	default:
		luaL_error(L, "'direction' must be set to either EAST, NORTH, SOUTH or WEST.");
	}

	if (!Position::isValid(x, y, z)) {
		luaL_error(L, "Position %d/%d/%d is not valid.", x, y, z);
	}

	Position position(x, y, z);

	auto& instance = *getInstance();
	if (instance._directions.find(position) != instance._directions.end()) {
		LOGw("Multiple spawn directions defined for position " << position << ".");
	}

	instance._directions[position] = direction;

	return 0;
}










Spawns::Spawns()
{
	filename = "";
	loaded = started = false;
}

Spawns::~Spawns()
{
	if(started)
		clear();
}

bool Spawns::loadFromXml(const std::string& _filename)
{
	if(isLoaded())
		return true;

	loadDirections();

	filename = _filename;
	xmlDocPtr doc = xmlParseFile(filename.c_str());
	if(!doc)
	{
		LOGe("[Spawns::loadFromXml] Cannot open spawns file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr spawnNode, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"spawns"))
	{
		LOGe("[Spawns::loadFromXml] Malformed spawns file.");
		xmlFreeDoc(doc);
		return false;
	}

	spawnNode = root->children;
	while(spawnNode)
	{
		parseSpawnNode(spawnNode, false);
		spawnNode = spawnNode->next;
	}

	xmlFreeDoc(doc);
	loaded = true;

	return true;
}

bool Spawns::parseSpawnNode(xmlNodePtr p, bool checkDuplicate)
{
	if(xmlStrcmp(p->name, (const xmlChar*)"spawn"))
		return false;

	int32_t intValue;
	std::string strValue;

	Position centerPos;
	if(!readXMLString(p, "centerpos", strValue))
	{
		if(!readXMLInteger(p, "centerx", intValue))
			return false;

		centerPos.x = intValue;
		if(!readXMLInteger(p, "centery", intValue))
			return false;

		centerPos.y = intValue;
		if(!readXMLInteger(p, "centerz", intValue))
			return false;

		centerPos.z = intValue;
	}
	else
	{
		IntegerVec posVec = vectorAtoi(explodeString(",", strValue));
		if(posVec.size() < 3)
			return false;

		centerPos = Position(posVec[0], posVec[1], posVec[2]);
	}

	if(!readXMLInteger(p, "radius", intValue))
		return false;

	int32_t radius = intValue;
	Spawn* spawn = new Spawn(centerPos, radius);
	if(checkDuplicate)
	{
		for(SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it)
		{
			if((*it)->getPosition() == centerPos)
				delete *it;
		}
	}

	spawnList.push_back(spawn);
	xmlNodePtr tmpNode = p->children;
	while(tmpNode)
	{
		if(!xmlStrcmp(tmpNode->name, (const xmlChar*)"monster"))
		{
			std::string name;
			if(!readXMLString(tmpNode, "name", strValue))
			{
				tmpNode = tmpNode->next;
				continue;
			}

			name = strValue;
			int32_t interval = MINSPAWN_INTERVAL / 1000;
			if(readXMLInteger(tmpNode, "spawntime", intValue) || readXMLInteger(tmpNode, "interval", intValue))
			{
				if(intValue <= interval)
				{
					LOGe("[Spawns::loadFromXml] " << name << " " << centerPos << " spawntime cannot"
					" be less than " << interval << " seconds.");

					tmpNode = tmpNode->next;
					continue;
				}

				interval = intValue;
			}

			interval *= 1000;
			Position placePos = centerPos;
			if(readXMLInteger(tmpNode, "x", intValue))
				placePos.x += intValue;

			if(readXMLInteger(tmpNode, "y", intValue))
				placePos.y += intValue;

			if(readXMLInteger(tmpNode, "z", intValue))
				placePos.z /*+*/= intValue;

			Direction direction = Direction::NORTH;
			if(readXMLInteger(tmpNode, "direction", intValue) && intValue >= static_cast<uint8_t>(Direction::EAST) && intValue <= static_cast<uint8_t>(Direction::WEST))
				direction = (Direction)intValue;

			spawn->addMonster(name, placePos, direction, interval);
		}
		else if(!xmlStrcmp(tmpNode->name, (const xmlChar*)"npc"))
		{
			std::string name;
			if(!readXMLString(tmpNode, "name", strValue))
			{
				tmpNode = tmpNode->next;
				continue;
			}

			name = strValue;
			Position placePos = centerPos;
			if(readXMLInteger(tmpNode, "x", intValue))
				placePos.x += intValue;

			if(readXMLInteger(tmpNode, "y", intValue))
				placePos.y += intValue;

			if(readXMLInteger(tmpNode, "z", intValue))
				placePos.z /*+*/= intValue;

			Direction direction = Direction::NORTH;
			if(readXMLInteger(tmpNode, "direction", intValue) && intValue >= static_cast<uint8_t>(Direction::EAST) && intValue <= static_cast<uint8_t>(Direction::WEST))
				direction = (Direction)intValue;

			Npc* npc = Npc::createNpc(name);
			if(!npc)
			{
				tmpNode = tmpNode->next;
				continue;
			}

			npc->setMasterPosition(placePos, radius);
			npc->setDirection(direction);

			auto overriddenDirection = getDirectionForPosition(placePos);
			if (overriddenDirection != Direction::NONE) {
				npc->setDirection(overriddenDirection);
			}
			else {
				npc->setDirection(direction);
			}

			npcList.push_back(npc);
		}

		tmpNode = tmpNode->next;
	}

	return true;
}

void Spawns::startup()
{
	if(!isLoaded() || isStarted())
		return;

	for (auto& npc : npcList) {
		if (npc->enterWorld(npc->getMasterPosition()) != RET_NOERROR) {
			LOGe("Cannot spawn npc '" << npc->getName() << "' at " << npc->getMasterPosition() << ".");
		}
	}
	npcList.clear();

	for(SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it)
		(*it)->startup();

	started = true;
}

void Spawns::clear()
{
	started = false;

	npcList.clear();

	for(SpawnList::iterator it = spawnList.begin(); it != spawnList.end(); ++it)
		delete (*it);
	spawnList.clear();

	loaded = false;
	filename = "";
}

bool Spawns::isInZone(const Position& centerPos, int32_t radius, const Position& pos)
{
	if(radius == -1)
		return true;

	return ((pos.x >= centerPos.x - radius) && (pos.x <= centerPos.x + radius) &&
		(pos.y >= centerPos.y - radius) && (pos.y <= centerPos.y + radius));
}




LOGGER_DEFINITION(Spawn);


void Spawn::startEvent()
{
	if(checkSpawnEvent == 0)
		checkSpawnEvent = server.scheduler().addTask(SchedulerTask::create(Milliseconds(getInterval()), std::bind(&Spawn::checkSpawn, this)));
}

Spawn::Spawn(const Position& _pos, int32_t _radius)
{
	centerPos = _pos;
	despawnRadius = despawnRange = 0;
	radius = _radius;
	interval = DEFAULTSPAWN_INTERVAL;
	checkSpawnEvent = 0;
}

Spawn::~Spawn()
{
	stopEvent();
	Monster* monster = nullptr;
	for(SpawnedMap::iterator it = spawnedMap.begin(); it != spawnedMap.end(); ++it)
	{
		if(!(monster = it->second.get()))
			continue;

		monster->removeFromSpawn();
	}

	spawnedMap.clear();
	spawnMap.clear();
}

bool Spawn::findPlayer(const Position& pos)
{
	SpectatorList list;
	server.game().getSpectators(list, pos);

	Player* tmpPlayer = nullptr;
	for(SpectatorList::iterator it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->hasFlag(PlayerFlag_IgnoredByMonsters) && !tmpPlayer->isGhost())
			return true;
	}

	return false;
}

bool Spawn::spawnMonster(uint32_t spawnId, MonsterType* mType, const Position& pos, Direction dir, bool startup /*= false*/)
{
	spawnMap[spawnId].lastSpawn = OTSYS_TIME();

	auto monster = Monster::create(mType, this);
	if (monster->enterWorld(pos) != RET_NOERROR) {
		monster->removeFromSpawn();

		LOGe("Cannot spawn monster '" << mType->name << "' at " << pos << ".");
		return false;
	}

	monster->setMasterPosition(pos, radius);

	auto direction = Spawns::getInstance()->getDirectionForPosition(pos);
	if (direction != Direction::NONE) {
		monster->setDirection(direction);
	}
	else {
		monster->setDirection(dir);
	}

	spawnedMap.insert(SpawnedPair(spawnId, monster));
	return true;
}

void Spawn::startup()
{
	for(SpawnMap::iterator it = spawnMap.begin(); it != spawnMap.end(); ++it)
	{
		spawnBlock_t& sb = it->second;
		spawnMonster(it->first, sb.mType, sb.pos, sb.direction, true);
	}
}

void Spawn::checkSpawn()
{
	LOGt("Spawn::checkSpawn() - this = " << this);

	checkSpawnEvent = 0;

	Monster* monster;
	uint32_t spawnId;

	for(SpawnedMap::iterator it = spawnedMap.begin(); it != spawnedMap.end();)
	{
		spawnId = it->first;
		monster = it->second.get();

		if(!monster->isAlive())
		{
			if(spawnId != 0)
				spawnMap[spawnId].lastSpawn = OTSYS_TIME();

			it = spawnedMap.erase(it);
		}
		else if(!isInSpawnZone(monster->getPosition()) && spawnId != 0)
		{
			spawnedMap.insert(SpawnedPair(0, monster));
			it = spawnedMap.erase(it);
		}
		else
			++it;
	}

	uint32_t spawnCount = 0;
	for(SpawnMap::iterator it = spawnMap.begin(); it != spawnMap.end(); ++it)
	{
		spawnId = it->first;
		spawnBlock_t& sb = it->second;

		if(spawnedMap.count(spawnId) == 0)
		{
			if(OTSYS_TIME() >= sb.lastSpawn + sb.interval)
			{
				if(findPlayer(sb.pos))
				{
					sb.lastSpawn = OTSYS_TIME();
					continue;
				}

				spawnMonster(spawnId, sb.mType, sb.pos, sb.direction);
				++spawnCount;
				if(spawnCount >= (uint32_t)server.configManager().getNumber(ConfigManager::RATE_SPAWN))
					break;
			}
		}
	}

	if(spawnedMap.size() < spawnMap.size())
		checkSpawnEvent = server.scheduler().addTask(SchedulerTask::create(Milliseconds(getInterval()), std::bind(&Spawn::checkSpawn, this)));
}

bool Spawn::addMonster(const std::string& _name, const Position& _pos, Direction _dir, uint32_t _interval)
{
	if(!server.game().getTile(_pos))
	{
		LOGe("[Spawn::addMonster] nullptr tile at spawn position (" << _pos << ")");
		return false;
	}

	MonsterType* mType = server.monsters().getMonsterType(_name);
	if(!mType)
	{
		LOGe("[Spawn::addMonster] Cannot find \"" << _name << "\"");
		return false;
	}

	if(_interval < interval)
		interval = _interval;

	spawnBlock_t sb;
	sb.mType = mType;
	sb.pos = _pos;
	sb.direction = _dir;
	sb.interval = _interval;
	sb.lastSpawn = 0;

	uint32_t spawnId = (int32_t)spawnMap.size() + 1;
	spawnMap[spawnId] = sb;
	return true;
}

void Spawn::removeMonster(Monster* monster)
{
	for(SpawnedMap::iterator it = spawnedMap.begin(); it != spawnedMap.end(); ++it)
	{
		if(it->second == monster)
		{
			spawnedMap.erase(it);
			break;
		}
	}
}

void Spawn::stopEvent()
{
	if(checkSpawnEvent != 0)
	{
		server.scheduler().cancelTask(checkSpawnEvent);
		checkSpawnEvent = 0;
	}
}
