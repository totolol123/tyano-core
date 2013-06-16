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
#include "raids.h"

#include "player.h"

#include "game.h"
#include "configmanager.h"
#include "items.h"
#include "monster.h"
#include "monsters.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"


LOGGER_DEFINITION(Raids);


Raids::Raids()
{
	running = nullptr;
	loaded = started = false;
	lastRaidEnd = checkRaidsEvent = 0;
}


Raids::~Raids() {
	if (server.isReady()) {
		server.scheduler().cancelTask(checkRaidsEvent);
	}

	for(RaidList::iterator it = raidList.begin(); it != raidList.end(); ++it)
		delete (*it);
}


bool Raids::parseRaidNode(xmlNodePtr raidNode, bool checkDuplicate, FileType pathing)
{
	if(xmlStrcmp(raidNode->name, (const xmlChar*)"raid"))
		return false;

	int32_t intValue;
	std::string strValue;
	if(!readXMLString(raidNode, "name", strValue))
	{
		LOGe("[Raids::parseRaidNode] name tag missing for raid.");
		return false;
	}

	std::string name = strValue;
	if(!readXMLInteger(raidNode, "interval2", intValue) || intValue <= 0)
	{
		LOGe("[Raids::parseRaidNode] interval2 tag missing or divided by 0 for raid " << name);
		return false;
	}

	uint32_t interval = intValue * 60;
	std::string file;
	if(!readXMLString(raidNode, "file", strValue))
	{
		file = name + ".xml";
		LOGw("[Raids::parseRaidNode] file tag missing for raid " << name << ", using default: " << file);
	}
	else
		file = strValue;

	file = getFilePath(pathing, "raids/" + file);
	uint64_t margin = 0;
	if(!readXMLInteger(raidNode, "margin", intValue))
		LOGw("[Raids::parseRaidNode] margin tag missing for raid " << name << ", using default: " << margin);
	else
		margin = intValue * 60 * 1000;

	RefType_t refType = REF_NONE;
	if(readXMLString(raidNode, "reftype", strValue) || readXMLString(raidNode, "refType", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "single")
			refType = REF_SINGLE;
		else if(tmpStrValue == "block")
			refType = REF_BLOCK;
		else if(tmpStrValue != "none")
			LOGw("[Raids::parseRaidNode] Unknown reftype \"" << strValue << "\" for raid " << name);
	}

	bool ref = false;
	if(readXMLString(raidNode, "ref", strValue))
		ref = booleanString(strValue);

	bool enabled = true;
	if(readXMLString(raidNode, "enabled", strValue))
		enabled = booleanString(strValue);

	Raid* raid = new Raid(name, interval, margin, refType, ref, enabled);
	if(!raid || !raid->loadFromXml(file))
	{
		LOGe("[Raids::parseRaidNode] failed to load raid " << name);

		delete raid;
		return false;
	}

	if(checkDuplicate)
	{
		for(RaidList::iterator it = raidList.begin(); it != raidList.end(); ++it)
		{
			if((*it)->getName() == name)
				delete *it;
		}
	}

	raidList.push_back(raid);
	return true;
}

bool Raids::loadFromXml()
{
	if(isLoaded())
		return true;

	xmlDocPtr doc = xmlParseFile(getFilePath(FileType::OTHER, "raids/raids.xml").c_str());
	if(!doc)
	{
		LOGe("[Raids::loadFromXml] Could not load raids file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr raidNode, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"raids"))
	{
		LOGe("[Raids::loadFromXml] Malformed raids file.");
		xmlFreeDoc(doc);
		return false;
	}

	raidNode = root->children;
	while(raidNode)
	{
		parseRaidNode(raidNode, false, FileType::OTHER);
		raidNode = raidNode->next;
	}

	xmlFreeDoc(doc);
	loaded = true;
	return true;
}

bool Raids::startup()
{
	if(!isLoaded() || isStarted())
		return false;

	setLastRaidEnd(OTSYS_TIME());
	checkRaidsEvent = server.scheduler().addTask(SchedulerTask::create(
		std::chrono::milliseconds(CHECK_RAIDS_INTERVAL * 1000), std::bind(&Raids::checkRaids, this)));

	started = true;
	return true;
}

void Raids::checkRaids()
{
	checkRaidsEvent = server.scheduler().addTask(SchedulerTask::create(
			std::chrono::milliseconds(CHECK_RAIDS_INTERVAL * 1000), std::bind(&Raids::checkRaids, this)));
	if(getRunning())
		return;

	uint64_t now = OTSYS_TIME();
	for(RaidList::iterator it = raidList.begin(); it != raidList.end(); ++it)
	{
		if((*it)->isEnabled() && !(*it)->hasRef() && now > (lastRaidEnd + (*it)->getMargin()) &&
			(MAX_RAND_RANGE * CHECK_RAIDS_INTERVAL / (*it)->getInterval()) >= (
			uint32_t)random_range(0, MAX_RAND_RANGE) && (*it)->startRaid())
			break;
	}
}

void Raids::clear()
{
	server.scheduler().cancelTask(checkRaidsEvent);
	checkRaidsEvent = lastRaidEnd = 0;
	loaded = started = false;

	running = nullptr;
	for(RaidList::iterator it = raidList.begin(); it != raidList.end(); ++it)
		delete (*it);

	raidList.clear();
	ScriptEvent::m_interface.reInitState();
}

bool Raids::reload()
{
	clear();
	return loadFromXml();
}

Raid* Raids::getRaidByName(const std::string& name)
{
	RaidList::iterator it;
	for(it = raidList.begin(); it != raidList.end(); it++)
	{
		if(boost::iequals((*it)->getName(), name))
			return (*it);
	}

	return nullptr;
}



LOGGER_DEFINITION(Raid);


Raid::Raid(const std::string& _name, uint32_t _interval, uint64_t _margin,
	RefType_t _refType, bool _ref, bool _enabled)
{
	name = _name;
	interval = _interval;
	margin = _margin;
	refType = _refType;
	ref = _ref;
	enabled = _enabled;

	loaded = false;
	refCount = eventCount = nextEvent = 0;
}

Raid::~Raid()
{
	stopEvents();
	for(RaidEventVector::iterator it = raidEvents.begin(); it != raidEvents.end(); it++)
		delete (*it);

	raidEvents.clear();
}

bool Raid::loadFromXml(const std::string& _filename)
{
	if(isLoaded())
		return true;

	xmlDocPtr doc = xmlParseFile(_filename.c_str());
	if(!doc)
	{
		LOGe("[Raid::loadFromXml] Could not load raid file (" << _filename << "): " << getLastXMLError());
		return false;
	}

	xmlNodePtr root, eventNode;
	root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"raid"))
	{
		LOGe("[Raid::loadFromXml] Malformed raid file (" << _filename << ").");
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	eventNode = root->children;
	while(eventNode)
	{
		RaidEvent* event;
		if(!xmlStrcmp(eventNode->name, (const xmlChar*)"announce"))
			event = new AnnounceEvent(this, ref);
		else if(!xmlStrcmp(eventNode->name, (const xmlChar*)"effect"))
			event = new EffectEvent(this, ref);
		else if(!xmlStrcmp(eventNode->name, (const xmlChar*)"itemspawn"))
			event = new ItemSpawnEvent(this, ref);
		else if(!xmlStrcmp(eventNode->name, (const xmlChar*)"singlespawn"))
			event = new SingleSpawnEvent(this, ref);
		else if(!xmlStrcmp(eventNode->name, (const xmlChar*)"areaspawn"))
			event = new AreaSpawnEvent(this, ref);
		else if(!xmlStrcmp(eventNode->name, (const xmlChar*)"script"))
			event = new ScriptEvent(this, ref);
		else
		{
			eventNode = eventNode->next;
			continue;
		}

		if(!event->configureRaidEvent(eventNode))
		{
			LOGe("[Raid::loadFromXml] Could not configure raid in file: " << _filename << ", eventNode: " << eventNode->name);
			delete event;
		}
		else
			raidEvents.push_back(event);

		eventNode = eventNode->next;
	}

	//sort by delay time
	std::sort(raidEvents.begin(), raidEvents.end(), RaidEvent::compareEvents);
	xmlFreeDoc(doc);

	loaded = true;
	return true;
}

bool Raid::startRaid()
{
	if(refCount)
		return true;

	RaidEvent* raidEvent = getNextRaidEvent();
	if(!raidEvent)
		return false;

	nextEvent = server.scheduler().addTask(SchedulerTask::create(
			std::chrono::milliseconds(raidEvent->getDelay()), std::bind(&Raid::executeRaidEvent, this, raidEvent)));
	Raids::getInstance()->setRunning(this);
	return true;
}

bool Raid::executeRaidEvent(RaidEvent* raidEvent)
{
	if(!raidEvent->executeEvent())
		return !resetRaid(false);

	RaidEvent* newRaidEvent = getNextRaidEvent();
	if(!newRaidEvent)
		return !resetRaid(false);

	nextEvent = server.scheduler().addTask(SchedulerTask::create(
			std::chrono::milliseconds(std::max(RAID_MINTICKS, (int32_t)(newRaidEvent->getDelay() - raidEvent->getDelay()))),
		std::bind(&Raid::executeRaidEvent, this, newRaidEvent)));
	return true;
}

bool Raid::resetRaid(bool checkExecution)
{
	if(checkExecution && nextEvent)
		return true;

	stopEvents();
	if(refType == REF_BLOCK && refCount > 0)
		return false;

	if(refType != REF_SINGLE || refCount <= 0)
		eventCount = 0;

	if(Raids::getInstance()->getRunning() == this)
	{
		Raids::getInstance()->setRunning(nullptr);
		Raids::getInstance()->setLastRaidEnd(OTSYS_TIME());
	}

	return true;
}

void Raid::stopEvents()
{
	if(!nextEvent)
		return;

	server.scheduler().cancelTask(nextEvent);
	nextEvent = 0;
}

RaidEvent* Raid::getNextRaidEvent()
{
	if(eventCount < raidEvents.size())
		return raidEvents[eventCount++];

	return nullptr;
}

bool RaidEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	std::string strValue;
	if(readXMLString(eventNode, "ref", strValue))
		m_ref = booleanString(strValue);

	int32_t intValue;
	if(readXMLInteger(eventNode, "delay", intValue))
		m_delay = std::max((int32_t)m_delay, intValue);

	return true;
}



LOGGER_DEFINITION(AnnounceEvent);


bool AnnounceEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	std::string strValue;
	if(!readXMLString(eventNode, "message", strValue))
	{
		LOGe("[AnnounceEvent::configureRaidEvent] Message tag missing for announce event.");
		return false;
	}

	m_message = strValue;
	if(readXMLString(eventNode, "type", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "warning")
			m_messageType = MSG_STATUS_WARNING;
		else if(tmpStrValue == "event")
			m_messageType = MSG_EVENT_ADVANCE;
		else if(tmpStrValue == "default")
			m_messageType = MSG_EVENT_DEFAULT;
		else if(tmpStrValue == "description")
			m_messageType = MSG_INFO_DESCR;
		else if(tmpStrValue == "status")
			m_messageType = MSG_STATUS_SMALL;
		else if(tmpStrValue == "blue")
			m_messageType = MSG_STATUS_CONSOLE_BLUE;
		else if(tmpStrValue == "red")
			m_messageType = MSG_STATUS_CONSOLE_RED;
		else
			LOGw("[AnnounceEvent::configureRaidEvent] Unknown type tag for announce event, using default: "
				<< (int32_t)m_messageType);
	}
	else
		LOGw("[AnnounceEvent::configureRaidEvent] Missing type tag for announce event. Using default: "
			<< (int32_t)m_messageType);

	return true;
}

bool AnnounceEvent::executeEvent() const
{
	server.game().broadcastMessage(m_message, m_messageType);
	return true;
}



LOGGER_DEFINITION(EffectEvent);


bool EffectEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	int32_t intValue;
	std::string strValue;
	if(!readXMLInteger(eventNode, "id", intValue))
	{
		if(!readXMLString(eventNode, "name", strValue))
		{
			LOGe("[EffectEvent::configureRaidEvent] id (or name) tag missing for effect event.");
			return false;
		}
		else
			m_effect = getMagicEffect(strValue);
	}
	else
		m_effect = (MagicEffect_t)intValue;

	if(!readXMLString(eventNode, "pos", strValue))
	{
		if(!readXMLInteger(eventNode, "x", intValue))
		{
			LOGe("[EffectEvent::configureRaidEvent] x tag missing for effect event.");
			return false;
		}

		m_position.x = intValue;
		if(!readXMLInteger(eventNode, "y", intValue))
		{
			LOGe("[EffectEvent::configureRaidEvent] y tag missing for effect event.");
			return false;
		}

		m_position.y = intValue;
		if(!readXMLInteger(eventNode, "z", intValue))
		{
			LOGe("[EffectEvent::configureRaidEvent] z tag missing for effect event.");
			return false;
		}

		m_position.z = intValue;
	}
	else
	{
		IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
		if(posList.size() < 3)
		{
			LOGe("[EffectEvent::configureRaidEvent] Malformed pos tag for effect event.");
			return false;
		}

		m_position = Position(posList[0], posList[1], posList[2]);
	}

	return true;
}

bool EffectEvent::executeEvent() const
{
	server.game().addMagicEffect(m_position, m_effect);
	return true;
}



LOGGER_DEFINITION(ItemSpawnEvent);


bool ItemSpawnEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	int32_t intValue;
	std::string strValue;
	if(!readXMLInteger(eventNode, "id", intValue))
	{
		if(!readXMLString(eventNode, "name", strValue))
		{
			LOGe("[ItemSpawnEvent::configureRaidEvent] id (or name) tag missing for itemspawn event.");
			return false;
		}
		else
			m_itemId = server.items().getKindIdByName(strValue);
	}
	else
		m_itemId = intValue;

	if(readXMLInteger(eventNode, "chance", intValue))
		m_chance = intValue;

	if(readXMLInteger(eventNode, "subType", intValue))
		m_subType = intValue;

	if(!readXMLString(eventNode, "pos", strValue))
	{
		if(!readXMLInteger(eventNode, "x", intValue))
		{
			LOGe("[ItemSpawnEvent::configureRaidEvent] x tag missing for itemspawn event.");
			return false;
		}

		m_position.x = intValue;
		if(!readXMLInteger(eventNode, "y", intValue))
		{
			LOGe("[ItemSpawnEvent::configureRaidEvent] y tag missing for itemspawn event.");
			return false;
		}

		m_position.y = intValue;
		if(!readXMLInteger(eventNode, "z", intValue))
		{
			LOGe("[ItemSpawnEvent::configureRaidEvent] z tag missing for itemspawn event.");
			return false;
		}

		m_position.z = intValue;
	}
	else
	{
		IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
		if(posList.size() < 3)
		{
			LOGe("[ItemSpawnEvent::configureRaidEvent] Malformed pos tag for itemspawn event.");
			return false;
		}

		m_position = Position(posList[0], posList[1], posList[2]);
	}

	return true;
}

bool ItemSpawnEvent::executeEvent() const
{
	if(m_chance < (uint32_t)random_range(0, (int32_t)MAX_ITEM_CHANCE))
		return true;

	Tile* tile = server.game().getTile(m_position);
	if(!tile)
	{
		LOGe("[ItemSpawnEvent::executeEvent] Missing tile at position " << m_position);
		return false;
	}

	ItemKindPC kind = server.items()[m_itemId];
	if (!kind) {
		LOGe("[ItemSpawnEvent::executeEvent] Cannot create item with id " << m_itemId);
		return false;
	}

	if(kind->stackable && m_subType > 100)
	{
		int32_t subCount = m_subType;
		while(subCount > 0)
		{
			int32_t stackCount = std::min(100, (int32_t)subCount);
			subCount -= stackCount;

			boost::intrusive_ptr<Item> newItem = Item::CreateItem(m_itemId, stackCount);
			if(!newItem)
			{
				LOGe("[ItemSpawnEvent::executeEvent] Cannot create item with id " << m_itemId);
				return false;
			}

			ReturnValue ret = server.game().internalAddItem(nullptr, tile, newItem.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
			if(ret != RET_NOERROR)
			{
				LOGe("[ItemSpawnEvent::executeEvent] Cannot spawn item with id " << m_itemId);
				return false;
			}

			if(m_raid->usesRef() && m_ref)
			{
				newItem->setRaid(m_raid);
				m_raid->addRef();
			}
		}
	}
	else
	{
		boost::intrusive_ptr<Item> newItem = Item::CreateItem(m_itemId, m_subType);
		if(!newItem)
		{
			LOGe("[ItemSpawnEvent::executeEvent] Cannot create item with id " << m_itemId);
			return false;
		}

		ReturnValue ret = server.game().internalAddItem(nullptr, tile, newItem.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
		if(ret != RET_NOERROR)
		{
			LOGe("[ItemSpawnEvent::executeEvent] Cannot spawn item with id " << m_itemId);
			return false;
		}

		if(m_raid->usesRef() && m_ref)
		{
			newItem->setRaid(m_raid);
			m_raid->addRef();
		}
	}

	return true;
}



LOGGER_DEFINITION(SingleSpawnEvent);


bool SingleSpawnEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	std::string strValue;
	if(!readXMLString(eventNode, "name", strValue))
	{
		LOGe("[SingleSpawnEvent::configureRaidEvent] name tag missing for singlespawn event.");
		return false;
	}

	m_monsterName = strValue;
	if(!readXMLString(eventNode, "pos", strValue))
	{
		int32_t intValue;
		if(!readXMLInteger(eventNode, "x", intValue))
		{
			LOGe("[SingleSpawnEvent::configureRaidEvent] x tag missing for singlespawn event.");
			return false;
		}

		m_position.x = intValue;
		if(!readXMLInteger(eventNode, "y", intValue))
		{
			LOGe("[SingleSpawnEvent::configureRaidEvent] y tag missing for singlespawn event.");
			return false;
		}

		m_position.y = intValue;
		if(!readXMLInteger(eventNode, "z", intValue))
		{
			LOGe("[SingleSpawnEvent::configureRaidEvent] z tag missing for singlespawn event.");
			return false;
		}

		m_position.z = intValue;
	}
	else
	{
		IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
		if(posList.size() < 3)
		{
			LOGe("[SingleSpawnEvent::configureRaidEvent] Malformed pos tag for singlespawn event.");
			return false;
		}

		m_position = Position(posList[0], posList[1], posList[2]);
	}

	return true;
}

bool SingleSpawnEvent::executeEvent() const {
	MonsterType* type = server.monsters().getMonsterType(m_monsterName);
	if (type == nullptr) {
		LOGe("[SingleSpawnEvent::executeEvent] Cannot find monster with name '" << m_monsterName << "'.");
		return false;
	}

	Raid* raid = nullptr;
	if (m_raid->usesRef() && m_ref) {
		raid = m_raid;
		raid->addRef();
	}

	boost::intrusive_ptr<Monster> monster = Monster::create(type, raid);
	if(!monster)
	{
		LOGe("[SingleSpawnEvent::executeEvent] Cannot create monster " << m_monsterName);
		return false;
	}

	if(!server.game().placeCreature(monster.get(), m_position))
	{
		LOGe("[SingleSpawnEvent::executeEvent] Cannot spawn monster " << m_monsterName);
		return false;
	}

	return true;
}



LOGGER_DEFINITION(AreaSpawnEvent);


bool AreaSpawnEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	int32_t intValue;
	std::string strValue;
	if(readXMLInteger(eventNode, "radius", intValue))
	{
		int32_t radius = intValue;
		Position centerPos;
		if(readXMLString(eventNode, "centerPosition", strValue) || readXMLString(eventNode, "centerpos", strValue))
		{
			IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
			if(posList.size() < 3)
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] Malformed centerPosition tag for areaspawn event.");
				return false;
			}

			centerPos = Position(posList[0], posList[1], posList[2]);
		}
		else
		{
			if(!readXMLInteger(eventNode, "centerx", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] centerx tag missing for areaspawn event.");
				return false;
			}

			centerPos.x = intValue;
			if(!readXMLInteger(eventNode, "centery", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] centery tag missing for areaspawn event.");
				return false;
			}

			centerPos.y = intValue;
			if(!readXMLInteger(eventNode, "centerz", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] centerz tag missing for areaspawn event.");
				return false;
			}

			centerPos.z = intValue;
		}

		m_fromPos.x = centerPos.x - radius;
		m_fromPos.y = centerPos.y - radius;
		m_fromPos.z = centerPos.z;

		m_toPos.x = centerPos.x + radius;
		m_toPos.y = centerPos.y + radius;
		m_toPos.z = centerPos.z;
	}
	else
	{
		if(readXMLString(eventNode, "fromPosition", strValue) || readXMLString(eventNode, "frompos", strValue))
		{
			IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
			if(posList.size() < 3)
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] Malformed fromPosition tag for areaspawn event.");
				return false;
			}

			m_fromPos = Position(posList[0], posList[1], posList[2]);
		}
		else
		{
			if(!readXMLInteger(eventNode, "fromx", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] fromx tag missing for areaspawn event.");
				return false;
			}

			m_fromPos.x = intValue;
			if(!readXMLInteger(eventNode, "fromy", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] fromy tag missing for areaspawn event.");
				return false;
			}

			m_fromPos.y = intValue;
			if(!readXMLInteger(eventNode, "fromz", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] fromz tag missing for areaspawn event.");
				return false;
			}

			m_fromPos.z = intValue;
		}

		if(readXMLString(eventNode, "toPosition", strValue) || readXMLString(eventNode, "topos", strValue))
		{
			IntegerVec posList = vectorAtoi(explodeString(strValue, ";"));
			if(posList.size() < 3)
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] Malformed toPosition tag for areaspawn event.");
				return false;
			}

			m_toPos = Position(posList[0], posList[1], posList[2]);
		}
		else
		{
			if(!readXMLInteger(eventNode, "tox", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] tox tag missing for areaspawn event.");
				return false;
			}

			m_toPos.x = intValue;
			if(!readXMLInteger(eventNode, "toy", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] toy tag missing for areaspawn event.");
				return false;
			}

			m_toPos.y = intValue;
			if(!readXMLInteger(eventNode, "toz", intValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] toz tag missing for areaspawn event.");
				return false;
			}

			m_toPos.z = intValue;
		}
	}

	xmlNodePtr monsterNode = eventNode->children;
	while(monsterNode)
	{
		if(!xmlStrcmp(monsterNode->name, (const xmlChar*)"monster"))
		{
			if(!readXMLString(monsterNode, "name", strValue))
			{
				LOGe("[AreaSpawnEvent::configureRaidEvent] name tag missing for monster node.");
				return false;
			}

			std::string name = strValue;
			int32_t min = 0, max = 0;
			if(readXMLInteger(monsterNode, "min", intValue) || readXMLInteger(monsterNode, "maxamount", intValue))
				min = intValue;

			if(readXMLInteger(monsterNode, "max", intValue) || readXMLInteger(monsterNode, "maxamount", intValue))
				max = intValue;

			if(!min && !max)
			{
				if(!readXMLInteger(monsterNode, "amount", intValue))
				{
					LOGe("[AreaSpawnEvent::configureRaidEvent] amount tag missing for monster node.");
					return false;
				}

				min = max = intValue;
			}

			addMonster(name, min, max);
		}

		monsterNode = monsterNode->next;
	}

	return true;
}

AreaSpawnEvent::~AreaSpawnEvent()
{
	for(MonsterSpawnList::iterator it = m_spawnList.begin(); it != m_spawnList.end(); it++)
		delete (*it);

	m_spawnList.clear();
}

void AreaSpawnEvent::addMonster(MonsterSpawn* _spawn)
{
	m_spawnList.push_back(_spawn);
}

void AreaSpawnEvent::addMonster(const std::string& name, uint32_t min, uint32_t max)
{
	MonsterSpawn* monsterSpawn = new MonsterSpawn();
	monsterSpawn->min = min;
	monsterSpawn->max = max;

	monsterSpawn->name = name;
	addMonster(monsterSpawn);
}

bool AreaSpawnEvent::executeEvent() const
{
	for(MonsterSpawnList::const_iterator it = m_spawnList.begin(); it != m_spawnList.end(); it++)
	{
		MonsterSpawn* spawn = *it;
		MonsterType* type = server.monsters().getMonsterType(spawn->name);
		if (type == nullptr) {
			LOGe("[AreaSpawnEvent::executeEvent] Cannot find monster with name '" << spawn->name << "'.");
			return false;
		}

		uint32_t amount = (uint32_t)random_range(spawn->min, spawn->max);
		for(uint32_t i = 0; i < amount; ++i)
		{
			Raid* raid = nullptr;
			if (m_raid->usesRef() && m_ref) {
				raid = m_raid;
				raid->addRef();
			}

			boost::intrusive_ptr<Monster> monster = Monster::create(type, raid);
			if(!monster)
			{
				LOGe("[AreaSpawnEvent::executeEvent] Cannot create monster " << spawn->name);
				return false;
			}

			for(int32_t t = 0; t < MAXIMUM_TRIES_PER_MONSTER; ++t)
			{
				if(!server.game().placeCreature(monster.get(), Position(random_range(m_fromPos.x, m_toPos.x),
					random_range(m_fromPos.y, m_toPos.y), random_range(m_fromPos.z, m_toPos.z))))
					continue;

				break;
			}
		}
	}

	return true;
}

LuaScriptInterface ScriptEvent::m_interface("Raid Interface");


LOGGER_DEFINITION(ScriptEvent);


bool ScriptEvent::configureRaidEvent(xmlNodePtr eventNode)
{
	if(!RaidEvent::configureRaidEvent(eventNode))
		return false;

	std::string scriptsName = Raids::getInstance()->getScriptBaseName();
	if(!m_interface.loadDirectory(getFilePath(FileType::OTHER, std::string(scriptsName + "/lib/"))))
		LOGe("[ScriptEvent::configureRaidEvent] Cannot load " << scriptsName << "/lib/");

	std::string strValue;
	if(readXMLString(eventNode, "file", strValue))
	{
		if(!loadScript(getFilePath(FileType::OTHER, std::string(scriptsName + "/scripts/" + strValue)), true))
		{
			LOGe("[ScriptEvent::configureRaidEvent] Cannot load raid script file (" << strValue << ").");
			return false;
		}
	}
	else if(!parseXMLContentString(eventNode->children, strValue) && !loadBuffer(strValue))
	{
		LOGe("[ScriptEvent::configureRaidEvent] Cannot load raid script buffer.");
		return false;
	}

	return true;
}

bool ScriptEvent::executeEvent() const
{
	//onRaid()
	if(m_interface.reserveEnv())
	{
		ScriptEnviroment* env = m_interface.getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			bool result = true;
			if(m_interface.loadBuffer(m_scriptData))
			{
				lua_State* L = m_interface.getState();
				result = m_interface.getGlobalBool(L, "_result", true);
			}

			m_interface.releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, &m_interface);
			m_interface.pushFunction(m_scriptId);

			bool result = m_interface.callFunction(0);
			m_interface.releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[ScriptEvent::executeEvent] Call stack overflow.");
		return false;
	}
}
