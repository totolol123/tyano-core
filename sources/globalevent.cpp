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

#include "globalevent.h"
#include "tools.h"
#include "player.h"
#include "server.h"
#include "scheduler.h"
#include "schedulertask.h"


LOGGER_DEFINITION(GlobalEvents);


GlobalEvents::GlobalEvents()
	: m_interface("GlobalEvent Interface")
{}


void GlobalEvents::clear()
{
	thinkMap.clear();
	serverMap.clear();
	timerMap.clear();

	m_interface.reInitState();
}

GlobalEventP GlobalEvents::getEvent(const std::string& nodeName)
{
	if(asLowerCaseString(nodeName) == "globalevent")
		return std::make_shared<GlobalEvent>(&m_interface);

	return nullptr;
}

bool GlobalEvents::registerEvent(const GlobalEventP& globalEvent, xmlNodePtr p, bool override)
{
	GlobalEventMap* map = &thinkMap;
	if(globalEvent->getEventType() == GLOBAL_EVENT_TIMER)
		map = &timerMap;
	else if(globalEvent->getEventType() != GLOBAL_EVENT_NONE)
		map = &serverMap;

	GlobalEventMap::iterator it = map->find(globalEvent->getName());
	if(it == map->end())
	{
		map->insert(std::make_pair(globalEvent->getName(), globalEvent));
		return true;
	}

	if(override)
	{
		it->second = globalEvent;
		return true;
	}

	LOGw("[GlobalEvents::configureEvent] Duplicate registered globalevent with name: " << globalEvent->getName());
	return false;
}

void GlobalEvents::startup()
{
	time_t now = time(nullptr);
	tm* ts = localtime(&now);
	now = std::max(1, (int32_t)(60 - ts->tm_sec)) * 1000;

	execute(GLOBAL_EVENT_STARTUP);
	server.scheduler().addTask(SchedulerTask::create(Milliseconds(now),
		std::bind(&GlobalEvents::timer, this)));
	server.scheduler().addTask(SchedulerTask::create(Milliseconds(GLOBAL_THINK_INTERVAL),
		std::bind(&GlobalEvents::think, this, GLOBAL_THINK_INTERVAL)));
}

void GlobalEvents::timer()
{
	time_t now = time(nullptr);
	tm* ts = localtime(&now);

	uint32_t hour = (uint32_t)ts->tm_hour, minute = (uint32_t)ts->tm_min;
	for(GlobalEventMap::iterator it = timerMap.begin(); it != timerMap.end(); ++it)
	{
		if(hour != it->second->getHour() || minute != it->second->getMinute())
			continue;

		if(!it->second->executeEvent())
			LOGe("[GlobalEvents::timer] Couldn't execute event: "
				<< it->second->getName());
	}

	now = std::max(1, (int32_t)(60 - ts->tm_sec)) * 1000;
	server.scheduler().addTask(SchedulerTask::create(Milliseconds(now),
		std::bind(&GlobalEvents::timer, this)));
}

void GlobalEvents::think(uint32_t interval)
{
	time_t now = time(nullptr);
	for(GlobalEventMap::iterator it = thinkMap.begin(); it != thinkMap.end(); ++it)
	{
		if(now <= (it->second->getLastExecution() + static_cast<time_t>(it->second->getInterval())))
			continue;

		it->second->setLastExecution(now);
		if(!it->second->executeThink(it->second->getInterval(), now, interval))
			LOGe("[GlobalEvents::think] Couldn't execute event: "
				<< it->second->getName());
	}

	server.scheduler().addTask(SchedulerTask::create(Milliseconds(interval),
		std::bind(&GlobalEvents::think, this, interval)));
}

void GlobalEvents::execute(GlobalEvent_t type)
{
	for(GlobalEventMap::iterator it = serverMap.begin(); it != serverMap.end(); ++it)
	{
		if(it->second->getEventType() == type)
			it->second->executeEvent();
	}
}

GlobalEventMap GlobalEvents::getEventMap(GlobalEvent_t type)
{
	switch(type)
	{
		case GLOBAL_EVENT_NONE:
			return thinkMap;

		case GLOBAL_EVENT_TIMER:
			return timerMap;

		case GLOBAL_EVENT_STARTUP:
		case GLOBAL_EVENT_SHUTDOWN:
		case GLOBAL_EVENT_RECORD:
		{
			GlobalEventMap retMap;
			for(GlobalEventMap::iterator it = serverMap.begin(); it != serverMap.end(); ++it)
			{
				if(it->second->getEventType() == type)
					retMap[it->first] = it->second;
			}

			return retMap;
		}

		default:
			break;
	}

	return GlobalEventMap();
}



LOGGER_DEFINITION(GlobalEvent);


GlobalEvent::GlobalEvent(LuaScriptInterface* _interface):
	Event(_interface)
{
	m_eventType = GLOBAL_EVENT_NONE;
	m_lastExecution = time(nullptr);
	m_interval = m_hour = m_minute = 0;
}

bool GlobalEvent::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if(!readXMLString(p, "name", strValue))
	{
		LOGe("[GlobalEvent::configureEvent] No name for a globalevent.");
		return false;
	}

	m_name = strValue;
	m_eventType = GLOBAL_EVENT_NONE;
	if(readXMLString(p, "type", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "startup" || tmpStrValue == "start" || tmpStrValue == "load")
			m_eventType = GLOBAL_EVENT_STARTUP;
		else if(tmpStrValue == "shutdown" || tmpStrValue == "quit" || tmpStrValue == "exit")
			m_eventType = GLOBAL_EVENT_SHUTDOWN;
		else if(tmpStrValue == "record" || tmpStrValue == "playersrecord")
			m_eventType = GLOBAL_EVENT_RECORD;
		else
		{
			LOGe("[GlobalEvent::configureEvent] No valid type \"" << strValue << "\" for globalevent with name " << m_name);
			return false;
		}

		return true;
	}
	else if(readXMLString(p, "time", strValue) || readXMLString(p, "at", strValue))
	{
		IntegerVec params = vectorAtoi(explodeString(strValue, ":"));
		if(params.size() < 2 || params[0] > 23 || params[0] < 0 || params[1] > 59 || params[1] < 0)
		{
			LOGe("[GlobalEvent::configureEvent] No valid time \"" << strValue << "\" for globalevent with name " << m_name);
			return false;
		}

		m_hour = params[0], m_minute = params[1];
		m_eventType = GLOBAL_EVENT_TIMER;
		return true;
	}
	else
	{
		int32_t intValue;
		if(readXMLInteger(p, "interval", intValue))
		{
			m_interval = intValue;
			return true;
		}
	}

	LOGe("[GlobalEvent::configureEvent] No interval for globalevent with name " << m_name);
	return false;
}

std::string GlobalEvent::getScriptEventName() const
{
	switch(m_eventType)
	{
		case GLOBAL_EVENT_STARTUP:
			return "onStartup";
		case GLOBAL_EVENT_SHUTDOWN:
			return "onShutdown";
		case GLOBAL_EVENT_RECORD:
			return "onRecord";
		case GLOBAL_EVENT_TIMER:
			return "onTimer";
		default:
			return "onThink";
	}
}

std::string GlobalEvent::getScriptEventParams() const
{
	switch(m_eventType)
	{
		case GLOBAL_EVENT_RECORD:
			return "current, old, cid";

		case GLOBAL_EVENT_NONE:
			return "interval, lastExecution, thinkInterval";

		default:
			return "";
	}
}

int32_t GlobalEvent::executeThink(uint32_t interval, uint32_t lastExecution, uint32_t thinkInterval)
{
	//onThink(interval, lastExecution, thinkInterval)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			std::stringstream scriptstream;
			scriptstream << "local interval = " << interval << std::endl;

			scriptstream << "local lastExecution = " << lastExecution << std::endl;
			scriptstream << "local thinkInterval = " << thinkInterval << std::endl;

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			lua_State* L = m_interface->getState();

			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, interval);

			lua_pushnumber(L, lastExecution);
			lua_pushnumber(L, thinkInterval);

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[GlobalEvent::executeThink] Call stack overflow.");
		return 0;
	}
}

int32_t GlobalEvent::executeRecord(uint32_t current, uint32_t old, Player* player)
{
	//onRecord(current, old, cid)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			std::stringstream scriptstream;
			scriptstream << "local current = " << current << std::endl;
			scriptstream << "local old = " << old << std::endl;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			lua_State* L = m_interface->getState();

			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, current);
			lua_pushnumber(L, old);
			lua_pushnumber(L, env->addThing(player));

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[GlobalEvent::executeRecord] Call stack overflow.");
		return 0;
	}
}

int32_t GlobalEvent::executeEvent()
{
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			bool result = true;
			if(m_interface->loadBuffer(m_scriptData))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			m_interface->pushFunction(m_scriptId);

			bool result = m_interface->callFunction(0);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[GlobalEvent::executeEvent] Call stack overflow.");
		return 0;
	}
}
