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
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"

#define MINSPAWN_INTERVAL 1000
#define DEFAULTSPAWN_INTERVAL 60000


LOGGER_DEFINITION(Spawns);


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

			Direction direction = NORTH;
			if(readXMLInteger(tmpNode, "direction", intValue) && direction >= EAST && direction <= WEST)
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

			Direction direction = NORTH;
			if(readXMLInteger(tmpNode, "direction", intValue) && direction >= EAST && direction <= WEST)
				direction = (Direction)intValue;

			Npc* npc = Npc::createNpc(name);
			if(!npc)
			{
				tmpNode = tmpNode->next;
				continue;
			}

			npc->setMasterPosition(placePos, radius);
			npc->setDirection(direction);
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

	for(NpcList::iterator it = npcList.begin(); it != npcList.end(); ++it) {
		server.game().placeCreature((*it).get(), (*it)->getMasterPosition(), false, true);
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
		checkSpawnEvent = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(getInterval()), std::bind(&Spawn::checkSpawn, this)));
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

		monster->setSpawn(nullptr);
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
		if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->hasFlag(PlayerFlag_IgnoredByMonsters))
			return true;
	}

	return false;
}

bool Spawn::spawnMonster(uint32_t spawnId, MonsterType* mType, const Position& pos, Direction dir, bool startup /*= false*/)
{
	boost::intrusive_ptr<Monster> monster = Monster::createMonster(mType);
	if(!monster)
		return false;

	if(startup)
	{
		//No need to send out events to the surrounding since there is no one out there to listen!
		if(!server.game().internalPlaceCreature(monster.get(), pos, false, true))
		{
			return false;
		}
	}
	else
	{
		if(!server.game().placeCreature(monster.get(), pos, false, true))
		{
			return false;
		}
	}

	monster->setSpawn(this);

	monster->setMasterPosition(pos, radius);
	monster->setDirection(dir);

	spawnedMap.insert(SpawnedPair(spawnId, monster));
	spawnMap[spawnId].lastSpawn = OTSYS_TIME();
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

		if(monster->isRemoved())
		{
			if(spawnId != 0)
				spawnMap[spawnId].lastSpawn = OTSYS_TIME();

			spawnedMap.erase(it++);
		}
		else if(!isInSpawnZone(monster->getPosition()) && spawnId != 0)
		{
			spawnedMap.insert(SpawnedPair(0, monster));
			spawnedMap.erase(it++);
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
		checkSpawnEvent = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(getInterval()), std::bind(&Spawn::checkSpawn, this)));
#ifdef __DEBUG_SPAWN__
	else
		LOGt("Spawn::checkSpawn stopped " << this);
#endif
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
