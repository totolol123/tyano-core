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

#include "creatureevent.h"
#include "tools.h"

#include "monster.h"
#include "player.h"
#include "tile.h"


LOGGER_DEFINITION(CreatureEvents);


CreatureEvents::CreatureEvents()
	: m_interface("CreatureScript Interface")
{}


void CreatureEvents::clear()
{
	//clear creature events
	for(CreatureEventList::iterator it = m_creatureEvents.begin(); it != m_creatureEvents.end(); ++it)
		it->second->clearEvent();

	//clear lua state
	m_interface.reInitState();
}


CreatureEventP CreatureEvents::getEvent(const std::string& nodeName)
{
	std::string tmpNodeName = asLowerCaseString(nodeName);
	if(tmpNodeName == "event" || tmpNodeName == "creaturevent" || tmpNodeName == "creatureevent" || tmpNodeName == "creaturescript")
		return std::make_shared<CreatureEvent>(&m_interface);

	return nullptr;
}

bool CreatureEvents::registerEvent(const CreatureEventP& creatureEvent, xmlNodePtr, bool override)
{
	if(creatureEvent->getEventType() == CREATURE_EVENT_NONE)
	{
		LOGe("[CreatureEvents::registerEvent] Trying to register event without type!");
		return false;
	}

	if(CreatureEventP oldEvent = getEventByName(creatureEvent->getName(), false))
	{
		//if there was an event with the same type that is not loaded (happens when reloading), it is reused
		if(oldEvent->getEventType() == creatureEvent->getEventType() && (!oldEvent->isLoaded() || override))
			oldEvent->copyEvent(creatureEvent);

		/*delete creatureEvent;
		return override;*/
		return false;
	}

	//if not, register it normally
	m_creatureEvents[creatureEvent->getName()] = creatureEvent;
	return true;
}

CreatureEventP CreatureEvents::getEventByName(const std::string& name, bool forceLoaded /*= true*/)
{
	CreatureEventList::iterator it = m_creatureEvents.find(name);
	if(it != m_creatureEvents.end())
	{
		if(!forceLoaded || it->second->isLoaded())
			return it->second;
	}

	return nullptr;
}

bool CreatureEvents::playerLogin(Player* player)
{
	//fire global event if is registered
	bool result = true;
	for(CreatureEventList::iterator it = m_creatureEvents.begin(); it != m_creatureEvents.end(); ++it)
	{
		if(it->second->getEventType() == CREATURE_EVENT_LOGIN && it->second->isLoaded() &&
			!it->second->executeLogin(player) && result)
			result = false;
	}

	return result;
}

bool CreatureEvents::playerLogout(Player* player, bool forceLogout)
{
	//fire global event if is registered
	bool result = true;
	for(CreatureEventList::iterator it = m_creatureEvents.begin(); it != m_creatureEvents.end(); ++it)
	{
		if(it->second->getEventType() == CREATURE_EVENT_LOGOUT && it->second->isLoaded() &&
			!it->second->executeLogout(player, forceLogout) && result)
			result = false;
	}

	return forceLogout || result;
}

/////////////////////////////////////


LOGGER_DEFINITION(CreatureEvent);


CreatureEvent::CreatureEvent(LuaScriptInterface* _interface):
Event(_interface)
{
	m_type = CREATURE_EVENT_NONE;
	m_isLoaded = false;
}

bool CreatureEvent::configureEvent(xmlNodePtr p)
{
	std::string str;
	if(!readXMLString(p, "name", str))
	{
		LOGe("[CreatureEvent::configureEvent] No name for creature event.");
		return false;
	}

	m_eventName = str;
	if(!readXMLString(p, "type", str))
	{
		LOGe("[CreatureEvent::configureEvent] No type for creature event.");
		return false;
	}

	std::string tmpStr = asLowerCaseString(str);
	if(tmpStr == "login")
		m_type = CREATURE_EVENT_LOGIN;
	else if(tmpStr == "logout")
		m_type = CREATURE_EVENT_LOGOUT;
	else if(tmpStr == "joinchannel")
		m_type = CREATURE_EVENT_CHANNEL_JOIN;
	else if(tmpStr == "leavechannel")
		m_type = CREATURE_EVENT_CHANNEL_LEAVE;
	else if(tmpStr == "advance")
		m_type = CREATURE_EVENT_ADVANCE;
	else if(tmpStr == "sendmail")
		m_type = CREATURE_EVENT_MAIL_SEND;
	else if(tmpStr == "receivemail")
		m_type = CREATURE_EVENT_MAIL_RECEIVE;
	else if(tmpStr == "traderequest")
		m_type = CREATURE_EVENT_TRADE_REQUEST;
	else if(tmpStr == "tradeaccept")
		m_type = CREATURE_EVENT_TRADE_ACCEPT;
	else if(tmpStr == "textedit")
		m_type = CREATURE_EVENT_TEXTEDIT;
	else if(tmpStr == "reportbug")
		m_type = CREATURE_EVENT_REPORTBUG;
	else if(tmpStr == "look")
		m_type = CREATURE_EVENT_LOOK;
	else if(tmpStr == "think")
		m_type = CREATURE_EVENT_THINK;
	else if(tmpStr == "direction")
		m_type = CREATURE_EVENT_DIRECTION;
	else if(tmpStr == "outfit")
		m_type = CREATURE_EVENT_OUTFIT;
	else if(tmpStr == "statschange")
		m_type = CREATURE_EVENT_STATSCHANGE;
	else if(tmpStr == "areacombat")
		m_type = CREATURE_EVENT_COMBAT_AREA;
	else if(tmpStr == "push")
		m_type = CREATURE_EVENT_PUSH;
	else if(tmpStr == "target")
		m_type = CREATURE_EVENT_TARGET;
	else if(tmpStr == "follow")
		m_type = CREATURE_EVENT_FOLLOW;
	else if(tmpStr == "combat")
		m_type = CREATURE_EVENT_COMBAT;
	else if(tmpStr == "attack")
		m_type = CREATURE_EVENT_ATTACK;
	else if(tmpStr == "cast")
		m_type = CREATURE_EVENT_CAST;
	else if(tmpStr == "kill")
		m_type = CREATURE_EVENT_KILL;
	else if(tmpStr == "death")
		m_type = CREATURE_EVENT_DEATH;
	else if(tmpStr == "preparedeath")
		m_type = CREATURE_EVENT_PREPAREDEATH;
	else
	{
		LOGe("[CreatureEvent::configureEvent] No valid type for creature event." << str);
		return false;
	}

	m_isLoaded = true;
	return true;
}

std::string CreatureEvent::getScriptEventName() const
{
	switch(m_type)
	{
		case CREATURE_EVENT_LOGIN:
			return "onLogin";
		case CREATURE_EVENT_LOGOUT:
			return "onLogout";
		case CREATURE_EVENT_CHANNEL_JOIN:
			return "onJoinChannel";
		case CREATURE_EVENT_CHANNEL_LEAVE:
			return "onLeaveChannel";
		case CREATURE_EVENT_THINK:
			return "onThink";
		case CREATURE_EVENT_ADVANCE:
			return "onAdvance";
		case CREATURE_EVENT_LOOK:
			return "onLook";
		case CREATURE_EVENT_DIRECTION:
			return "onDirection";
		case CREATURE_EVENT_OUTFIT:
			return "onOutfit";
		case CREATURE_EVENT_MAIL_SEND:
			return "onSendMail";
		case CREATURE_EVENT_MAIL_RECEIVE:
			return "onReceiveMail";
		case CREATURE_EVENT_TRADE_REQUEST:
			return "onTradeRequest";
		case CREATURE_EVENT_TRADE_ACCEPT:
			return "onTradeAccept";
		case CREATURE_EVENT_TEXTEDIT:
			return "onTextEdit";
		case CREATURE_EVENT_REPORTBUG:
			return "onReportBug";
		case CREATURE_EVENT_STATSCHANGE:
			return "onStatsChange";
		case CREATURE_EVENT_COMBAT_AREA:
			return "onAreaCombat";
		case CREATURE_EVENT_PUSH:
			return "onPush";
		case CREATURE_EVENT_TARGET:
			return "onTarget";
		case CREATURE_EVENT_FOLLOW:
			return "onFollow";
		case CREATURE_EVENT_COMBAT:
			return "onCombat";
		case CREATURE_EVENT_ATTACK:
			return "onAttack";
		case CREATURE_EVENT_CAST:
			return "onCast";
		case CREATURE_EVENT_KILL:
			return "onKill";
		case CREATURE_EVENT_DEATH:
			return "onDeath";
		case CREATURE_EVENT_PREPAREDEATH:
			return "onPrepareDeath";
		case CREATURE_EVENT_NONE:
		default:
			break;
	}

	return "";
}

std::string CreatureEvent::getScriptEventParams() const
{
	switch(m_type)
	{
		case CREATURE_EVENT_LOGIN:
			return "cid";
		case CREATURE_EVENT_LOGOUT:
			return "cid, forceLogout";
		case CREATURE_EVENT_CHANNEL_JOIN:
		case CREATURE_EVENT_CHANNEL_LEAVE:
			return "cid, channel, users";
		case CREATURE_EVENT_ADVANCE:
			return "cid, skill, oldLevel, newLevel";
		case CREATURE_EVENT_LOOK:
			return "cid, thing, position, lookDistance";
		case CREATURE_EVENT_MAIL_SEND:
			return "cid, receiver, item, openBox";
		case CREATURE_EVENT_MAIL_RECEIVE:
			return "cid, sender, item, openBox";
		case CREATURE_EVENT_TRADE_REQUEST:
		case CREATURE_EVENT_TRADE_ACCEPT:
			return "cid, target, item";
		case CREATURE_EVENT_TEXTEDIT:
			return "cid, item, newText";
		case CREATURE_EVENT_REPORTBUG:
			return "cid, comment";
		case CREATURE_EVENT_THINK:
			return "cid, interval";
		case CREATURE_EVENT_DIRECTION:
		case CREATURE_EVENT_OUTFIT:
			return "cid, old, current";
		case CREATURE_EVENT_STATSCHANGE:
			return "cid, attacker, type, combat, value";
		case CREATURE_EVENT_COMBAT_AREA:
			return "cid, ground, position, aggressive";
		case CREATURE_EVENT_PUSH:
		case CREATURE_EVENT_TARGET:
		case CREATURE_EVENT_FOLLOW:
		case CREATURE_EVENT_COMBAT:
		case CREATURE_EVENT_ATTACK:
		case CREATURE_EVENT_CAST:
			return "cid, target";
		case CREATURE_EVENT_KILL:
			return "cid, target, lastHit";
		case CREATURE_EVENT_DEATH:
			return "cid, corpse, deathList";
		case CREATURE_EVENT_PREPAREDEATH:
			return "cid, deathList";
		case CREATURE_EVENT_NONE:
		default:
			break;
	}

	return "";
}

void CreatureEvent::copyEvent(const CreatureEventP& creatureEvent)
{
	m_scriptId = creatureEvent->m_scriptId;
	m_interface = creatureEvent->m_interface;
	m_scripted = creatureEvent->m_scripted;
	m_isLoaded = creatureEvent->m_isLoaded;
}

void CreatureEvent::clearEvent()
{
	m_scriptId = 0;
	m_scripted = EVENT_SCRIPT_FALSE;
	m_isLoaded = false;
}

uint32_t CreatureEvent::executeLogin(Player* player)
{
	//onLogin(cid)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(player));

			bool result = m_interface->callFunction(1);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeLogin] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeLogout(Player* player, bool forceLogout)
{
	//onLogout(cid, forceLogout)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			scriptstream << "local forceLogout = " << (forceLogout ? "true" : "false") << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushboolean(L, forceLogout);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeLogout] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeChannelJoin(Player* player, uint16_t channelId, UsersMap usersMap)
{
	//onJoinChannel(cid, channel, users)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local channel = " << channelId << std::endl;
			scriptstream << "local users = {}" << std::endl;
			for(UsersMap::iterator it = usersMap.begin(); it != usersMap.end(); ++it)
				scriptstream << "users:insert(" << env->addThing(it->second) << ")" << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, channelId);

			UsersMap::iterator it = usersMap.begin();
			lua_newtable(L);
			for(int32_t i = 1; it != usersMap.end(); ++it, ++i)
			{
				lua_pushnumber(L, i);
				lua_pushnumber(L, env->addThing(it->second));
				lua_settable(L, -3);
			}

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeChannelJoin] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeChannelLeave(Player* player, uint16_t channelId, UsersMap usersMap)
{
	//onLeaveChannel(cid, channel, users)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local channel = " << channelId << std::endl;
			scriptstream << "local users = {}" << std::endl;
			for(UsersMap::iterator it = usersMap.begin(); it != usersMap.end(); ++it)
				scriptstream << "users:insert(" << env->addThing(it->second) << ")" << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, channelId);

			UsersMap::iterator it = usersMap.begin();
			lua_newtable(L);
			for(int32_t i = 1; it != usersMap.end(); ++it, ++i)
			{
				lua_pushnumber(L, i);
				lua_pushnumber(L, env->addThing(it->second));
				lua_settable(L, -3);
			}

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeChannelLeave] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeAdvance(Player* player, skills_t skill, uint32_t oldLevel, uint32_t newLevel)
{
	//onAdvance(cid, skill, oldLevel, newLevel)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local skill = " << skill << std::endl;
			scriptstream << "local oldLevel = " << oldLevel << std::endl;
			scriptstream << "local newLevel = " << newLevel << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, (uint32_t)skill);

			lua_pushnumber(L, oldLevel);
			lua_pushnumber(L, newLevel);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeAdvance] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeMailSend(Player* player, Player* receiver, Item* item, bool openBox)
{
	//onSendMail(cid, receiver, item, openBox)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local receiver = " << env->addThing(receiver) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));
			scriptstream << "local openBox = " << (openBox ? "true" : "false") << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[30];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, env->addThing(receiver));

			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			lua_pushboolean(L, openBox);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeMailSend] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeMailReceive(Player* player, Player* sender, Item* item, bool openBox)
{
	//onReceiveMail(cid, sender, item, openBox)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local sender = " << env->addThing(sender) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));
			scriptstream << "local openBox = " << (openBox ? "true" : "false") << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[30];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, env->addThing(sender));

			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			lua_pushboolean(L, openBox);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeMailReceive] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeTradeRequest(Player* player, Player* target, Item* item)
{
	//onTradeRequest(cid, target, item)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local target = " << env->addThing(target) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, env->addThing(target));
			LuaScriptInterface::pushThing(L, item, env->addThing(item));

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CreatureEvent::executeTradeRequest] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeTradeAccept(Player* player, Player* target, Item* item, Item* targetItem)
{
	//onTradeAccept(cid, target, item, targetItem)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local target = " << env->addThing(target) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, env->addThing(target));
			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			LuaScriptInterface::pushThing(L, targetItem, env->addThing(targetItem));

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeTradeAccept] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeLook(Player* player, Thing* thing, const Position& position, int16_t stackpos, int32_t lookDistance)
{
	//onLook(cid, thing, position, lookDistance)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			scriptstream << "local thing = " << env->addThing(thing) << std::endl;
			env->streamPosition(scriptstream, "position", position, stackpos);
			scriptstream << "local lookDistance = " << lookDistance << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[30];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			LuaScriptInterface::pushThing(L, thing, env->addThing(thing));

			LuaScriptInterface::pushPosition(L, position, stackpos);
			lua_pushnumber(L, lookDistance);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeLook] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeDirection(Creature* creature, Direction old, Direction current)
{
	//onDirection(cid, old, current)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			scriptstream << "local old = " << old << std::endl;
			scriptstream << "local current = " << current << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[30];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, static_cast<std::underlying_type<Direction>::type>(old));
			lua_pushnumber(L, static_cast<std::underlying_type<Direction>::type>(current));

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeDirection] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeOutfit(Creature* creature, const Outfit_t& old, const Outfit_t& current)
{
	//onOutfit(cid, old, current)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			env->streamOutfit(scriptstream, "old", old);
			env->streamOutfit(scriptstream, "current", current);

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[30];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			LuaScriptInterface::pushOutfit(L, old);
			LuaScriptInterface::pushOutfit(L, current);

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeOutfit] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeThink(Creature* creature, uint32_t interval)
{
	//onThink(cid, interval)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local interval = " << interval << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, interval);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeThink] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeStatsChange(Creature* creature, Creature* attacker, StatsChange_t type, CombatType_t combat, int32_t value)
{
	//onStatsChange(cid, attacker, type, combat, value)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local attacker = " << env->addThing(attacker) << std::endl;

			scriptstream << "local type = " << (uint32_t)type << std::endl;
			scriptstream << "local combat = " << (uint32_t)combat << std::endl;
			scriptstream << "local value = " << value << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(attacker));

			lua_pushnumber(L, (uint32_t)type);
			lua_pushnumber(L, (uint32_t)combat);
			lua_pushnumber(L, value);

			bool result = m_interface->callFunction(5);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeStatsChange] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeCombatArea(Creature* creature, Tile* tile, bool aggressive)
{
	//onAreaCombat(cid, ground, position, aggressive)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			env->streamThing(scriptstream, "ground", tile->ground.get(), env->addThing(tile->ground.get()));
			env->streamPosition(scriptstream, "position", tile->getPosition(), 0);
			scriptstream << "local aggressive = " << (aggressive ? "true" : "false") << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			LuaScriptInterface::pushThing(L, tile->ground.get(), env->addThing(tile->ground.get()));

			LuaScriptInterface::pushPosition(L, tile->getPosition(), 0);
			lua_pushboolean(L, aggressive);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeAreaCombat] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeCombat(Creature* creature, Creature* target)
{
	//onCombat(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local target = " << env->addThing(target) << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeCombat] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeAttack(Creature* creature, Creature* target)
{
	//onAttack(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local target = " << env->addThing(target) << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeAttack] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeCast(Creature* creature, Creature* target/* = nullptr*/)
{
	//onCast(cid[, target])
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local target = ";
			if(target)
				scriptstream << env->addThing(target);
			else
				scriptstream << "nil";

			scriptstream << std::endl << m_scriptData;
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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeCast] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeKill(Creature* creature, Creature* target, bool lastHit)
{
	//onKill(cid, target, lastHit)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			scriptstream << "local target = " << env->addThing(target) << std::endl;
			scriptstream << "local lastHit = " << (lastHit ? "true" : "false") << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));
			lua_pushboolean(L, lastHit);

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeKill] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeDeath(Creature* creature, Item* corpse, DeathList deathList)
{
	//onDeath(cid, corpse, deathList)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			env->streamThing(scriptstream, "corpse", corpse, env->addThing(corpse));
			scriptstream << "local deathList = {}" << std::endl;
			for(DeathList::iterator it = deathList.begin(); it != deathList.end(); ++it)
			{
				scriptstream << "deathList:insert(";
				if(it->isCreatureKill())
					scriptstream << env->addThing(it->getKillerCreature());
				else
					scriptstream << it->getKillerName();

				scriptstream << ")" << std::endl;
			}

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			LuaScriptInterface::pushThing(L, corpse, env->addThing(corpse));

			lua_newtable(L);
			DeathList::iterator it = deathList.begin();
			for(int32_t i = 1; it != deathList.end(); ++it, ++i)
			{
				lua_pushnumber(L, i);
				if(it->isCreatureKill())
					lua_pushnumber(L, env->addThing(it->getKillerCreature()));
				else
					lua_pushstring(L, it->getKillerName().c_str());

				lua_settable(L, -3);
			}

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeDeath] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executePrepareDeath(Creature* creature, DeathList deathList)
{
	//onPrepareDeath(cid, deathList)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			scriptstream << "local deathList = {}" << std::endl;
			for(DeathList::iterator it = deathList.begin(); it != deathList.end(); ++it)
			{
				scriptstream << "deathList:insert(";
				if(it->isCreatureKill())
					scriptstream << env->addThing(it->getKillerCreature());
				else
					scriptstream << it->getKillerName();

				scriptstream << ")" << std::endl;
			}

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", creature->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));

			lua_newtable(L);
			DeathList::iterator it = deathList.begin();
			for(int32_t i = 1; it != deathList.end(); ++it, ++i)
			{
				lua_pushnumber(L, i);
				if(it->isCreatureKill())
					lua_pushnumber(L, env->addThing(it->getKillerCreature()));
				else
					lua_pushstring(L, it->getKillerName().c_str());

				lua_settable(L, -3);
			}

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();

			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executePrepareDeath] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeTextEdit(Player* player, Item* item, std::string newText)
{
	//onTextEdit(cid, item, newText)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(player) << std::endl;

			env->streamThing(scriptstream, "item", item, env->addThing(item));
			scriptstream << "local newText = " << newText.c_str() << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			lua_pushstring(L, newText.c_str());

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeTextEdit] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeReportBug(Player* player, std::string comment)
{
	//onReportBug(cid, comment)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			scriptstream << "local comment = " << comment.c_str() << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			char desc[35];
			sprintf(desc, "%s", player->getName().c_str());
			env->setEventDesc(desc);
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushstring(L, comment.c_str());

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeReportBug] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executePush(Player* player, Creature* target)
{
	//onPush(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			scriptstream << "local target = " << env->addThing(target) << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << player->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executePush] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeTarget(Creature* creature, Creature* target)
{
	//onTarget(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local target = " << env->addThing(target) << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeTarget] Call stack overflow.");
		return 0;
	}
}

uint32_t CreatureEvent::executeFollow(Creature* creature, Creature* target)
{
	//onFollow(cid, target)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			scriptstream << "local target = " << env->addThing(target) << std::endl;

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
			#ifdef __DEBUG_LUASCRIPTS__
			std::stringstream desc;
			desc << creature->getName();
			env->setEventDesc(desc.str());
			#endif

			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(creature));
			lua_pushnumber(L, env->addThing(target));

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("CreatureEvent::executeFollow] Call stack overflow.");
		return 0;
	}
}
