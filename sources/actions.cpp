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

#include "const.h"

#include "actions.h"
#include "tools.h"

#include "player.h"
#include "monster.h"
#include "npc.h"

#include "depot.h"
#include "item.h"
#include "container.h"

#include "game.h"
#include "configmanager.h"

#include "combat.h"
#include "spells.h"

#include "house.h"
#include "housetile.h"
#include "beds.h"

#include "server.h"
#include "scheduler.h"
#include "schedulertask.h"


LOGGER_DEFINITION(Actions);



Actions::Actions()
	: m_interface("Action Interface")
{}


void Actions::clear()
{
	useItemMap.clear();
	uniqueItemMap.clear();
	actionItemMap.clear();

	m_interface.reInitState();
}

ActionP Actions::getEvent(const std::string& nodeName)
{
	if(asLowerCaseString(nodeName) == "action")
		return std::make_shared<Action>(&m_interface);

	return nullptr;
}

bool Actions::registerEvent(const ActionP& action, xmlNodePtr p, bool override)
{
	StringVector strVector;
	IntegerVec intVector, endIntVector;

	std::string strValue, endStrValue;
	int32_t tmp = 0;

	bool success = true;
	if(readXMLString(p, "itemid", strValue))
	{
		if(!parseIntegerVec(strValue, intVector))
		{
			LOGe("[Actions::registerEvent] Invalid itemid - '" << strValue << "'");
			return false;
		}

		if(useItemMap.find(intVector[0]) != useItemMap.end())
		{
			if(!override)
			{
				LOGe("[Actions::registerEvent] Duplicate registered item id: " << intVector[0]);
				success = false;
			}
		}

		if(success)
			useItemMap[intVector[0]] = action;

		for(size_t i = 1, size = intVector.size(); i < size; ++i)
		{
			if(useItemMap.find(intVector[i]) != useItemMap.end())
			{
				if(!override)
				{
					LOGe("[Actions::registerEvent] Duplicate registered item id: " << intVector[i]);
					continue;
				}
			}

			useItemMap[intVector[i]] = action;
		}
	}
	else if(readXMLString(p, "fromid", strValue) && readXMLString(p, "toid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				tmp = intVector[i];
				while(intVector[i] <= endIntVector[i])
				{
					if(useItemMap.find(intVector[i]) != useItemMap.end())
					{
						if(!override)
						{
							LOGe("[Actions::registerEvent] Duplicate registered item with id: " << intVector[i] <<
								", in fromid: " << tmp << " and toid: " << endIntVector[i]);
							intVector[i]++;
							continue;
						}
					}

					useItemMap[intVector[i]++] = action;
				}
			}
		}
		else
			LOGe("[Actions::registerEvent] Malformed entry (from item: \"" << strValue <<
				"\", to item: \"" << endStrValue << "\")");
	}

	if(readXMLString(p, "uniqueid", strValue))
	{
		if(!parseIntegerVec(strValue, intVector))
		{
			LOGe("[Actions::registerEvent] Invalid uniqueid - '" << strValue << "'");
			return false;
		}

		if(uniqueItemMap.find(intVector[0]) != uniqueItemMap.end())
		{
			if(!override)
			{
				LOGe("[Actions::registerEvent] Duplicate registered item uid: " << intVector[0]);
				success = false;
			}
		}

		if(success)
			uniqueItemMap[intVector[0]] = action;

		for(size_t i = 1, size = intVector.size(); i < size; ++i)
		{
			if(uniqueItemMap.find(intVector[i]) != uniqueItemMap.end())
			{
				if(!override)
				{
					LOGe("[Actions::registerEvent] Duplicate registered item uid: " << intVector[i]);
					continue;
				}
			}

			uniqueItemMap[intVector[i]] = action;
		}
	}
	else if(readXMLString(p, "fromuid", strValue) && readXMLString(p, "touid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				tmp = intVector[i];
				while(intVector[i] <= endIntVector[i])
				{
					if(uniqueItemMap.find(intVector[i]) != uniqueItemMap.end())
					{
						if(!override)
						{
							LOGe("[Actions::registerEvent] Duplicate registered item with uid: " << intVector[i] <<
								", in fromuid: " << tmp << " and touid: " << endIntVector[i]);
							intVector[i]++;
							continue;
						}
					}

					uniqueItemMap[intVector[i]++] = action;
				}
			}
		}
		else
			LOGe("[Actions::registerEvent] Malformed entry (from unique: \"" << strValue <<
				"\", to unique: \"" << endStrValue << "\")");
	}

	if(readXMLString(p, "actionid", strValue) || readXMLString(p, "aid", strValue))
	{
		if(!parseIntegerVec(strValue, intVector))
		{
			LOGe("[Actions::registerEvent] Invalid actionid - '" << strValue << "'");
			return false;
		}

		if(actionItemMap.find(intVector[0]) != actionItemMap.end())
		{
			if(!override)
			{
				LOGe("[Actions::registerEvent] Duplicate registered item aid: " << intVector[0]);
				success = false;
			}
		}

		if(success)
			actionItemMap[intVector[0]] = action;

		for(size_t i = 1, size = intVector.size(); i < size; ++i)
		{
			if(actionItemMap.find(intVector[i]) != actionItemMap.end())
			{
				if(!override)
				{
					LOGe("[Actions::registerEvent] Duplicate registered item aid: " << intVector[i]);
					continue;
				}
			}

			actionItemMap[intVector[i]] = action;
		}
	}
	else if(readXMLString(p, "fromaid", strValue) && readXMLString(p, "toaid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				tmp = intVector[i];
				while(intVector[i] <= endIntVector[i])
				{
					if(actionItemMap.find(intVector[i]) != actionItemMap.end())
					{
						if(!override)
						{
							LOGe("[Actions::registerEvent] Duplicate registered item with aid: " << intVector[i] <<
								", in fromaid: " << tmp << " and toaid: " << endIntVector[i]);
							intVector[i]++;
							continue;
						}
					}

					actionItemMap[intVector[i]++] = action;
				}
			}
		}
		else
			LOGe("[Actions::registerEvent] Malformed entry (from action: \"" << strValue <<
				"\", to action: \"" << endStrValue << "\")");
	}

	return success;
}

ReturnValue Actions::canUse(const Player* player, const Position& pos)
{
	const Position& playerPos = player->getPosition();
	if(playerPos.z > pos.z)
		return RET_FIRSTGOUPSTAIRS;

	if(playerPos.z < pos.z)
		return RET_FIRSTGODOWNSTAIRS;

	if(!Position::areInRange<1,1,0>(playerPos, pos))
		return RET_TOOFARAWAY;

	Tile* tile = server.game().getTile(pos);
	if(tile)
	{
		HouseTile* houseTile = tile->getHouseTile();
		if(houseTile && houseTile->getHouse() && !houseTile->getHouse()->isInvited(player))
			return RET_PLAYERISNOTINVITED;
	}
	return RET_NOERROR;
}

ReturnValue Actions::canUseEx(const Player* player, const ExtendedPosition& position, const Item* item)
{
	ActionP action;
	if((action = getAction(item, ACTION_UNIQUEID)))
		return action->canExecuteAction(player, position);

	if((action = getAction(item, ACTION_ACTIONID)))
		return action->canExecuteAction(player, position);

	if((action = getAction(item, ACTION_ITEMID)))
		return action->canExecuteAction(player, position);

	if((action = getAction(item, ACTION_RUNEID)))
		return action->canExecuteAction(player, position);

	return RET_NOERROR;
}

ReturnValue Actions::canUseFar(const Creature* creature, const Position& toPos, bool checkLineOfSight)
{
	const Position& creaturePos = creature->getPosition();
	if(creaturePos.z > toPos.z)
		return RET_FIRSTGOUPSTAIRS;

	if(creaturePos.z < toPos.z)
		return RET_FIRSTGODOWNSTAIRS;

	if(!Position::areInRange<7,5,0>(toPos, creaturePos))
		return RET_TOOFARAWAY;

	if(checkLineOfSight && !server.game().canThrowObjectTo(creaturePos, toPos))
		return RET_CANNOTTHROW;

	return RET_NOERROR;
}

ActionP Actions::getAction(const Item* item, ActionType_t type) const
{
	if(item->getUniqueId() && (type == ACTION_ANY || type == ACTION_UNIQUEID))
	{
		ActionUseMap::const_iterator it = uniqueItemMap.find(item->getUniqueId());
		if(it != uniqueItemMap.end())
			return it->second;
	}

	if(item->getActionId() && (type == ACTION_ANY || type == ACTION_ACTIONID))
	{
		ActionUseMap::const_iterator it = actionItemMap.find(item->getActionId());
		if(it != actionItemMap.end())
			return it->second;
	}

	if(type == ACTION_ANY || type == ACTION_ITEMID)
	{
		ActionUseMap::const_iterator it = useItemMap.find(item->getId());
		if(it != useItemMap.end())
			return it->second;
	}

	if(type == ACTION_ANY || type == ACTION_RUNEID)
	{
		if(ActionP runeSpell = server.spells().getRuneSpell(item->getId()))
			return runeSpell;
	}

	return nullptr;
}

bool Actions::executeUse(const ActionP& action, Player* player, Item* item, const ExtendedPosition& origin, uint32_t creatureId)
{
	return action->executeUse(player, item, origin, origin, false, creatureId);
}

ReturnValue Actions::internalUseItem(Player* player, Item* item, const ExtendedPosition& origin, uint8_t openContainerId, uint32_t creatureId)
{
	if(Door* door = item->getDoor())
	{
		if(!door->canUse(player))
			return RET_CANNOTUSETHISOBJECT;
	}

	ExtendedPosition newOrigin = ExtendedPosition::nowhere();
	if (origin.hasPosition() && item->getParent()) {
		newOrigin = ExtendedPosition::forStackPosition(StackPosition(origin.getPosition(), item->getParent()->__getIndexOfThing(item)));
	}
	else {
		newOrigin = origin;
	}

	bool executed = false;

	ActionP action;
	if((action = getAction(item, ACTION_UNIQUEID)))
	{
		if(action->isScripted())
		{
			if(executeUse(action, player, item, newOrigin, creatureId))
				return RET_NOERROR;
		}
		else if(action->function)
		{
			if(action->function(player, item, newOrigin, newOrigin, false, creatureId))
				return RET_NOERROR;
		}

		executed = true;
	}

	if((action = getAction(item, ACTION_ACTIONID)))
	{
		if(action->isScripted())
		{
			if(executeUse(action, player, item, newOrigin, creatureId))
				return RET_NOERROR;
		}
		else if(action->function)
		{
			if(action->function(player, item, newOrigin, newOrigin, false, creatureId))
				return RET_NOERROR;
		}

		executed = true;
	}

	if((action = getAction(item, ACTION_ITEMID)))
	{
		if(action->isScripted())
		{
			if(executeUse(action, player, item, newOrigin, creatureId))
				return RET_NOERROR;
		}
		else if(action->function)
		{
			if(action->function(player, item, newOrigin, newOrigin, false, creatureId))
				return RET_NOERROR;
		}

		executed = true;
	}

	if((action = getAction(item, ACTION_RUNEID)))
	{
		if(action->isScripted())
		{
			if(executeUse(action, player, item, newOrigin, creatureId))
				return RET_NOERROR;
		}
		else if(action->function)
		{
			if(action->function(player, item, newOrigin, newOrigin, false, creatureId))
				return RET_NOERROR;
		}

		executed = true;
	}

	if(BedItem* bed = item->getBed())
	{
		if(!bed->canUse(player))
			return RET_CANNOTUSETHISOBJECT;

		bed->sleep(player);
		return RET_NOERROR;
	}

	Container* container = item->getContainer();
	if(container)
	{
		if(container->getCorpseOwner() && !player->canOpenCorpse(container->getCorpseOwner())
			&& server.configManager().getBool(ConfigManager::CHECK_CORPSE_OWNER))
			return RET_YOUARENOTTHEOWNER;

		Container* tmpContainer = nullptr;
		if(Depot* depot = container->getDepot())
		{
			if(player->hasFlag(PlayerFlag_CannotPickupItem))
				return RET_CANNOTUSETHISOBJECT;

			if(Depot* playerDepot = player->getDepot(depot->getDepotId(), true))
			{
				player->useDepot(depot->getDepotId(), true);
				playerDepot->setParent(depot->getParent());
				tmpContainer = playerDepot;
			}
		}

		if(!tmpContainer)
			tmpContainer = container;

		int32_t oldId = player->getContainerID(tmpContainer);
		if(oldId != -1)
		{
			player->onCloseContainer(tmpContainer);
			player->closeContainer(oldId);
		}
		else
		{
			player->addContainer(openContainerId, tmpContainer);
			player->onSendContainer(tmpContainer);
		}

		return RET_NOERROR;
	}

	if(item->isReadable())
	{
		if(item->canWriteText())
		{
			player->setWriteItem(item, item->getMaxWriteLength());
			player->sendTextWindow(item, item->getMaxWriteLength(), true);
		}
		else
		{
			player->setWriteItem(nullptr);
			player->sendTextWindow(item, 0, false);
		}

		return RET_NOERROR;
	}

	if(!executed)
		return RET_CANNOTUSETHISOBJECT;

	return RET_NOERROR;
}

bool Actions::useItem(Player* player, Item* item, const ExtendedPosition& origin, uint8_t openContainerId)
{
	if(!player->canDoAction())
		return false;

	player->setNextActionTask(nullptr);
	player->stopRouting();
	player->setNextAction(Clock::now() + Milliseconds(server.configManager().getNumber(ConfigManager::ACTIONS_DELAY_INTERVAL)));

	ReturnValue ret = internalUseItem(player, item, origin, openContainerId, 0);
	if(ret == RET_NOERROR)
		return true;

	player->sendCancelMessage(ret);
	return false;
}

bool Actions::executeUseEx(const ActionP& action, Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, uint32_t creatureId)
{
	return (action->executeUse(player, item, origin, destination, true, creatureId) || action->hasOwnErrorHandler());
}

ReturnValue Actions::internalUseItemEx(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, uint32_t creatureId)
{
	ActionP action;
	if((action = getAction(item, ACTION_UNIQUEID)))
	{
		ReturnValue ret = action->canExecuteAction(player, destination);
		if(ret != RET_NOERROR)
			return ret;

		//only continue with next action in the list if the previous returns false
		if(executeUseEx(action, player, item, origin, destination, creatureId))
			return RET_NOERROR;
	}

	if((action = getAction(item, ACTION_ACTIONID)))
	{
		ReturnValue ret = action->canExecuteAction(player, destination);
		if(ret != RET_NOERROR)
			return ret;

		//only continue with next action in the list if the previous returns false
		if(executeUseEx(action, player, item, origin, destination, creatureId))
			return RET_NOERROR;

	}

	if((action = getAction(item, ACTION_ITEMID)))
	{
		ReturnValue ret = action->canExecuteAction(player, destination);
		if(ret != RET_NOERROR)
			return ret;

		//only continue with next action in the list if the previous returns false
		if(executeUseEx(action, player, item, origin, destination, creatureId))
			return RET_NOERROR;
	}

	if((action = getAction(item, ACTION_RUNEID)))
	{
		ReturnValue ret = action->canExecuteAction(player, destination);
		if(ret != RET_NOERROR)
			return ret;

		//only continue with next action in the list if the previous returns false
		if(executeUseEx(action, player, item, origin, destination, creatureId))
			return RET_NOERROR;
	}

	return RET_CANNOTUSETHISOBJECT;
}

bool Actions::useItemEx(Player* player, const ExtendedPosition& origin, const ExtendedPosition& destination, Item* item, uint32_t creatureId/* = 0*/)
{
	if(!player->canDoAction())
		return false;

	player->setNextActionTask(nullptr);
	player->stopRouting();
	player->setNextAction(Clock::now() + Milliseconds(server.configManager().getNumber(ConfigManager::EX_ACTIONS_DELAY_INTERVAL)));

	ActionP action = getAction(item, ACTION_ANY);
	if(!action)
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ExtendedPosition newOrigin = ExtendedPosition::nowhere();
	if (origin.hasPosition() && item->getParent()) {
		newOrigin = ExtendedPosition::forStackPosition(StackPosition(origin.getPosition(), item->getParent()->__getIndexOfThing(item)));
	}
	else {
		newOrigin = origin;
	}

	ReturnValue ret = internalUseItemEx(player, item, newOrigin, destination, creatureId);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		return false;
	}

	return true;
}



LOGGER_DEFINITION(Action);



Action::Action(LuaScriptInterface* _interface) :
		Event(_interface),
		function(nullptr),
		allowFarUse(false),
		checkLineOfSight(true)
{}


bool Action::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if(readXMLString(p, "allowfaruse", strValue) || readXMLString(p, "allowFarUse", strValue))
		setAllowFarUse(booleanString(strValue));

	if(readXMLString(p, "blockwalls", strValue) || readXMLString(p, "blockWalls", strValue))
		setCheckLineOfSight(booleanString(strValue));

	return true;
}

bool Action::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "increaseitemid")
		function = increaseItemId;
	else if(tmpFunctionName == "decreaseitemid")
		function = decreaseItemId;
	else
	{
		LOGe("[Action::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

bool Action::increaseItemId(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, bool extendedUse, uint32_t creatureId)
{
	if(!player || !item)
		return false;

	server.game().transformItem(item, item->getId() + 1);
	return true;
}

bool Action::decreaseItemId(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, bool extendedUse, uint32_t creatureId)
{
	if(!player || !item)
		return false;

	server.game().transformItem(item, item->getId() - 1);
	return true;
}

ReturnValue Action::canExecuteAction(const Player* player, const ExtendedPosition& toPos)
{
	if (!toPos.hasPosition(true)) {
		return RET_NOERROR;
	}

	if(!getAllowFarUse())
		return server.actions().canUse(player, toPos.getPosition(true));

	return server.actions().canUseFar(player, toPos.getPosition(true), getCheckLineOfSight());
}

bool Action::executeUse(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, bool extendedUse, uint32_t)
{
	//onUse(cid, item, fromPosition, itemEx, toPosition)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));
			env->streamExtendedPosition(scriptstream, "fromPosition", origin);

			Thing* thing = server.game().internalGetThing(player, destination);
			if(thing && (thing != item || !extendedUse))
			{
				env->streamThing(scriptstream, "itemEx", thing, env->addThing(thing));
				env->streamExtendedPosition(scriptstream, "toPosition", destination);
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
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(player->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			lua_pushnumber(L, env->addThing(player));
			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			LuaScriptInterface::pushExtendedPosition(L, origin);

			Thing* thing = server.game().internalGetThing(player, destination);
			if(thing && (thing != item || !extendedUse))
			{
				LuaScriptInterface::pushThing(L, thing, env->addThing(thing));
				LuaScriptInterface::pushExtendedPosition(L, destination);
			}
			else
			{
				LuaScriptInterface::pushThing(L, nullptr, 0);
				LuaScriptInterface::pushExtendedPosition(L, origin);
			}

			bool result = m_interface->callFunction(5);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[Action::executeUse] Call stack overflow.");
		return false;
	}
}
