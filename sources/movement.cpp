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

#include "movement.h"
#include "tools.h"

#include "creature.h"
#include "items.h"
#include "player.h"

#include "tile.h"
#include "vocation.h"

#include "combat.h"
#include "condition.h"
#include "game.h"
#include "server.h"


LOGGER_DEFINITION(MoveEvents);



MoveEvents::MoveEvents()
	: m_lastCacheTile(nullptr), m_interface("MoveEvents Interface")
{}


void MoveEvents::clear()
{
	m_itemIdMap.clear();
	m_actionIdMap.clear();
	m_uniqueIdMap.clear();
	m_positionMap.clear();
	m_interface.reInitState();

	m_lastCacheTile = nullptr;
	m_lastCacheItemVector.clear();
}

MoveEventP MoveEvents::getEvent(const std::string& nodeName)
{
	std::string tmpNodeName = asLowerCaseString(nodeName);
	if(tmpNodeName == "movevent" || tmpNodeName == "moveevent" || tmpNodeName == "movement")
		return std::make_shared<MoveEvent>(&m_interface);

	return nullptr;
}

bool MoveEvents::registerEvent(const MoveEventP& moveEvent, xmlNodePtr p, bool override)
{
	std::string strValue, endStrValue;
	MoveEvent_t eventType = moveEvent->getEventType();
	if((eventType == MOVE_EVENT_ADD_ITEM || eventType == MOVE_EVENT_REMOVE_ITEM) &&
		readXMLString(p, "tileitem", strValue) && booleanString(strValue))
	{
		switch(eventType)
		{
			case MOVE_EVENT_ADD_ITEM:
				moveEvent->setEventType(MOVE_EVENT_ADD_ITEM_ITEMTILE);
				break;
			case MOVE_EVENT_REMOVE_ITEM:
				moveEvent->setEventType(MOVE_EVENT_REMOVE_ITEM_ITEMTILE);
				break;
			default:
				break;
		}
	}

	StringVector strVector;
	IntegerVec intVector, endIntVector;

	bool success = true;
	if(readXMLString(p, "itemid", strValue))
	{
		strVector = explodeString(strValue, ";");
		for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
		{
			intVector = vectorAtoi(explodeString((*it), "-"));
			if(!intVector[0])
				continue;

			bool equip = moveEvent->getEventType() == MOVE_EVENT_EQUIP;
			addEvent(moveEvent, intVector[0], m_itemIdMap, override);
			if(equip)
			{
				ItemKindP kind = server.items().getMutableKind(intVector[0]);
				if (kind) {
					kind->wieldInfo = moveEvent->getWieldInfo();
					kind->minReqLevel = moveEvent->getReqLevel();
					kind->minReqMagicLevel = moveEvent->getReqMagLv();
					kind->vocationString = moveEvent->getVocationString();
				}
			}

			if(intVector.size() > 1)
			{
				while(intVector[0] < intVector[1])
				{
					addEvent(moveEvent, ++intVector[0], m_itemIdMap, override);
					if(equip)
					{
						ItemKindP kind = server.items().getMutableKind(intVector[0]);
						if (kind) {
							kind->wieldInfo = moveEvent->getWieldInfo();
							kind->minReqLevel = moveEvent->getReqLevel();
							kind->minReqMagicLevel = moveEvent->getReqMagLv();
							kind->vocationString = moveEvent->getVocationString();
						}
					}
				}
			}
		}
	}

	if(readXMLString(p, "fromid", strValue) && readXMLString(p, "toid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				bool equip = moveEvent->getEventType() == MOVE_EVENT_EQUIP;
				addEvent(moveEvent, intVector[i], m_itemIdMap, override);
				if(equip)
				{
					ItemKindP kind = server.items().getMutableKind(intVector[0]);
					if (kind) {
						kind->wieldInfo = moveEvent->getWieldInfo();
						kind->minReqLevel = moveEvent->getReqLevel();
						kind->minReqMagicLevel = moveEvent->getReqMagLv();
						kind->vocationString = moveEvent->getVocationString();
					}
				}

				while(intVector[i] < endIntVector[i])
				{
					addEvent(moveEvent, ++intVector[i], m_itemIdMap, override);
					if(equip)
					{
						ItemKindP kind = server.items().getMutableKind(intVector[0]);
						if (kind) {
							kind->wieldInfo = moveEvent->getWieldInfo();
							kind->minReqLevel = moveEvent->getReqLevel();
							kind->minReqMagicLevel = moveEvent->getReqMagLv();
							kind->vocationString = moveEvent->getVocationString();
						}
					}
				}
			}
		}
		else
			LOGw("[MoveEvents::registerEvent] Malformed entry (from item: \"" << strValue << "\", to item: \"" << endStrValue << "\")");
	}

	if(readXMLString(p, "uniqueid", strValue))
	{
		strVector = explodeString(strValue, ";");
		for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
		{
			intVector = vectorAtoi(explodeString((*it), "-"));
			if(!intVector[0])
				continue;

			addEvent(moveEvent, intVector[0], m_uniqueIdMap, override);
			if(intVector.size() > 1)
			{
				while(intVector[0] < intVector[1])
					addEvent(moveEvent, ++intVector[0], m_uniqueIdMap, override);
			}
		}
	}

	if(readXMLString(p, "fromuid", strValue) && readXMLString(p, "touid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				addEvent(moveEvent, intVector[i], m_uniqueIdMap, override);
				while(intVector[i] < endIntVector[i])
					addEvent(moveEvent, ++intVector[i], m_uniqueIdMap, override);
			}
		}
		else
			LOGw("[MoveEvents::registerEvent] Malformed entry (from unique: \"" << strValue << "\", to unique: \"" << endStrValue << "\")");
	}

	if(readXMLString(p, "actionid", strValue))
	{
		strVector = explodeString(strValue, ";");
		for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
		{
			intVector = vectorAtoi(explodeString((*it), "-"));
			if(!intVector[0])
				continue;

			addEvent(moveEvent, intVector[0], m_actionIdMap, override);
			if(intVector.size() > 1)
			{
				while(intVector[0] < intVector[1])
					addEvent(moveEvent, ++intVector[0], m_actionIdMap, override);
			}
		}
	}

	if(readXMLString(p, "fromaid", strValue) && readXMLString(p, "toaid", endStrValue))
	{
		intVector = vectorAtoi(explodeString(strValue, ";"));
		endIntVector = vectorAtoi(explodeString(endStrValue, ";"));
		if(intVector[0] && endIntVector[0] && intVector.size() == endIntVector.size())
		{
			for(size_t i = 0, size = intVector.size(); i < size; ++i)
			{
				addEvent(moveEvent, intVector[i], m_actionIdMap, override);
				while(intVector[i] < endIntVector[i])
					addEvent(moveEvent, ++intVector[i], m_actionIdMap, override);
			}
		}
		else
			LOGw("[MoveEvents::registerEvent] Malformed entry (from action: \"" << strValue << "\", to action: \"" << endStrValue << "\")");
	}

	if(readXMLString(p, "pos", strValue) || readXMLString(p, "position", strValue))
	{
		strVector = explodeString(strValue, ";");
		for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
		{
			intVector = vectorAtoi(explodeString((*it), ","));
			if(intVector.size() > 2)
				addEvent(moveEvent, Position(intVector[0], intVector[1], intVector[2]), m_positionMap, override);
			else
				success = false;
		}
	}

	return success;
}

void MoveEvents::addEvent(const MoveEventP& moveEvent, int32_t id, MoveListMap& map, bool override)
{
	MoveListMap::iterator it = map.find(id);
	if(it != map.end())
	{
		EventList& moveEventList = it->second.moveEvent[moveEvent->getEventType()];
		for(EventList::iterator it = moveEventList.begin(); it != moveEventList.end(); ++it)
		{
			if((*it)->getSlot() != moveEvent->getSlot())
				continue;

			if(override)
			{
				*it = moveEvent;
			}
			else
				LOGw("[MoveEvents::addEvent] Duplicate move event found: " << id);

			return;
		}

		moveEventList.push_back(moveEvent);
	}
	else
	{
		MoveEventList moveEventList;
		moveEventList.moveEvent[moveEvent->getEventType()].push_back(moveEvent);
		map[id] = moveEventList;
	}
}

MoveEventP MoveEvents::getEvent(Item* item, MoveEvent_t eventType)
{
	MoveListMap::iterator it;
	if(item->getUniqueId())
	{
		it = m_uniqueIdMap.find(item->getUniqueId());
		if(it != m_uniqueIdMap.end())
		{
			EventList& moveEventList = it->second.moveEvent[eventType];
			if(!moveEventList.empty())
				return *moveEventList.begin();
		}
	}

	if(item->getActionId())
	{
		it = m_actionIdMap.find(item->getActionId());
		if(it != m_actionIdMap.end())
		{
			EventList& moveEventList = it->second.moveEvent[eventType];
			if(!moveEventList.empty())
				return *moveEventList.begin();
		}
	}

	it = m_itemIdMap.find(item->getId());
	if(it != m_itemIdMap.end())
	{
		EventList& moveEventList = it->second.moveEvent[eventType];
		if(!moveEventList.empty())
			return *moveEventList.begin();
	}

	return nullptr;
}

MoveEventP MoveEvents::getEvent(Item* item, MoveEvent_t eventType, slots_t slot)
{
	uint32_t slotp = 0;
	switch(slot)
	{
		case slots_t::HEAD:
			slotp = SLOTP_HEAD;
			break;
		case slots_t::NECKLACE:
			slotp = SLOTP_NECKLACE;
			break;
		case slots_t::BACKPACK:
			slotp = SLOTP_BACKPACK;
			break;
		case slots_t::ARMOR:
			slotp = SLOTP_ARMOR;
			break;
		case slots_t::RIGHT:
			slotp = SLOTP_RIGHT;
			break;
		case slots_t::LEFT:
			slotp = SLOTP_LEFT;
			break;
		case slots_t::LEGS:
			slotp = SLOTP_LEGS;
			break;
		case slots_t::FEET:
			slotp = SLOTP_FEET;
			break;
		case slots_t::AMMO:
			slotp = SLOTP_AMMO;
			break;
		case slots_t::RING:
			slotp = SLOTP_RING;
			break;
		default:
			break;
	}

	MoveListMap::iterator it = m_itemIdMap.find(item->getId());
	if(it == m_itemIdMap.end())
		return nullptr;

	EventList& moveEventList = it->second.moveEvent[eventType];
	for(EventList::iterator it = moveEventList.begin(); it != moveEventList.end(); ++it)
	{
		if(((*it)->getSlot() & slotp))
			return *it;
	}

	return nullptr;
}

void MoveEvents::addEvent(const MoveEventP& moveEvent, Position pos, MovePosListMap& map, bool override)
{
	MovePosListMap::iterator it = map.find(pos);
	if(it != map.end())
	{
		bool add = true;
		if(!it->second.moveEvent[moveEvent->getEventType()].empty())
		{
			if(!override)
			{
				LOGw("[MoveEvents::addEvent] Duplicate move event found: " << pos);
				add = false;
			}
			else
				it->second.moveEvent[moveEvent->getEventType()].clear();
		}

		if(add)
			it->second.moveEvent[moveEvent->getEventType()].push_back(moveEvent);
	}
	else
	{
		MoveEventList moveEventList;
		moveEventList.moveEvent[moveEvent->getEventType()].push_back(moveEvent);
		map[pos] = moveEventList;
	}
}

MoveEventP MoveEvents::getEvent(const Tile* tile, MoveEvent_t eventType)
{
	MovePosListMap::iterator it = m_positionMap.find(tile->getPosition());
	if(it == m_positionMap.end())
		return nullptr;

	EventList& moveEventList = it->second.moveEvent[eventType];
	if(!moveEventList.empty())
		return *moveEventList.begin();

	return nullptr;
}

bool MoveEvents::hasEquipEvent(Item* item)
{
	return getEvent(item, MOVE_EVENT_EQUIP) && getEvent(item, MOVE_EVENT_DEEQUIP);
}

bool MoveEvents::hasTileEvent(Item* item)
{
	return (getEvent(item, MOVE_EVENT_STEP_IN) || getEvent(item, MOVE_EVENT_STEP_OUT) || getEvent(item,
		MOVE_EVENT_ADD_ITEM_ITEMTILE) || getEvent(item, MOVE_EVENT_REMOVE_ITEM_ITEMTILE));
}

uint32_t MoveEvents::onCreatureMove(Creature* actor, Creature* creature, const Tile* fromTile, const Tile* toTile, bool isStepping)
{
	MoveEvent_t eventType = MOVE_EVENT_STEP_OUT;
	const Tile* tile = fromTile;
	if(isStepping)
	{
		eventType = MOVE_EVENT_STEP_IN;
		tile = toTile;
	}

	Position fromPos;
	if(fromTile)
		fromPos = fromTile->getPosition();

	Position toPos;
	if(toTile)
		toPos = toTile->getPosition();

	uint32_t ret = 1;
	MoveEventP moveEvent;
	if((moveEvent = getEvent(tile, eventType)))
		ret &= moveEvent->fireStepEvent(actor, creature, nullptr, Position(), fromPos, toPos);

	Item* tileItem = nullptr;
	if(m_lastCacheTile == tile)
	{
		if(m_lastCacheItemVector.empty())
			return ret;

		//We cannot use iterators here since the scripts can invalidate the iterator
		for(int32_t i = 0, j = m_lastCacheItemVector.size(); i < j; ++i)
		{
			if((tileItem = m_lastCacheItemVector[i]) && (moveEvent = getEvent(tileItem, eventType)))
				ret &= moveEvent->fireStepEvent(actor, creature, tileItem, tile->getPosition(), fromPos, toPos);
		}

		return ret;
	}

	m_lastCacheTile = tile;
	m_lastCacheItemVector.clear();

	//We cannot use iterators here since the scripts can invalidate the iterator
	Thing* thing = nullptr;
	for(int32_t i = tile->__getFirstIndex(), j = tile->__getLastIndex(); i < j; ++i) //already checked the ground
	{
		if(!(thing = tile->__getThing(i)) || !(tileItem = thing->getItem()))
			continue;

		if((moveEvent = getEvent(tileItem, eventType)))
		{
			m_lastCacheItemVector.push_back(tileItem);
			ret &= moveEvent->fireStepEvent(actor, creature, tileItem, tile->getPosition(), fromPos, toPos);
		}
		else if(hasTileEvent(tileItem))
			m_lastCacheItemVector.push_back(tileItem);
	}

	return ret;
}

uint32_t MoveEvents::onPlayerEquip(Player* player, Item* item, slots_t slot, bool isCheck)
{
	if(MoveEventP moveEvent = getEvent(item, MOVE_EVENT_EQUIP, slot))
		return moveEvent->fireEquip(player, item, slot, isCheck);

	return 1;
}

uint32_t MoveEvents::onPlayerDeEquip(Player* player, Item* item, slots_t slot, bool isRemoval)
{
	if(MoveEventP moveEvent = getEvent(item, MOVE_EVENT_DEEQUIP, slot))
		return moveEvent->fireEquip(player, item, slot, isRemoval);

	return 1;
}

uint32_t MoveEvents::onItemMove(Creature* actor, Item* item, Tile* tile, bool isAdd)
{
	MoveEvent_t eventType1 = MOVE_EVENT_REMOVE_ITEM, eventType2 = MOVE_EVENT_REMOVE_ITEM_ITEMTILE;
	if(isAdd)
	{
		eventType1 = MOVE_EVENT_ADD_ITEM;
		eventType2 = MOVE_EVENT_ADD_ITEM_ITEMTILE;
	}

	uint32_t ret = 1;
	MoveEventP moveEvent = getEvent(tile, eventType1);
	if(moveEvent)
		ret &= moveEvent->fireAddRemItem(actor, item, nullptr, tile->getPosition());

	moveEvent = getEvent(item, eventType1);
	if(moveEvent)
		ret &= moveEvent->fireAddRemItem(actor, item, nullptr, tile->getPosition());

	Item* tileItem = nullptr;
	if(m_lastCacheTile == tile)
	{
		if(m_lastCacheItemVector.empty())
			return ret;

		//We cannot use iterators here since the scripts can invalidate the iterator
		for(int32_t i = 0, j = m_lastCacheItemVector.size(); i < j; ++i)
		{
			if((tileItem = m_lastCacheItemVector[i]) && tileItem != item
				&& (moveEvent = getEvent(tileItem, eventType2)))
				ret &= moveEvent->fireAddRemItem(actor, item,
					tileItem, tile->getPosition());
		}

		return ret;
	}

	m_lastCacheTile = tile;
	m_lastCacheItemVector.clear();

	//we cannot use iterators here since the scripts can invalidate the iterator
	Thing* thing = nullptr;
	for(int32_t i = tile->__getFirstIndex(), j = tile->__getLastIndex(); i < j; ++i) //already checked the ground
	{
		if(!(thing = tile->__getThing(i)) || !(tileItem = thing->getItem()) || tileItem == item)
			continue;

		if((moveEvent = getEvent(tileItem, eventType2)))
		{
			m_lastCacheItemVector.push_back(tileItem);
			ret &= moveEvent->fireAddRemItem(actor, item, tileItem, tile->getPosition());
		}
		else if(hasTileEvent(tileItem))
			m_lastCacheItemVector.push_back(tileItem);
	}

	return ret;
}

void MoveEvents::onAddTileItem(const Tile* tile, Item* item)
{
	if(m_lastCacheTile != tile)
		return;

	std::vector<Item*>::iterator it = std::find(m_lastCacheItemVector.begin(), m_lastCacheItemVector.end(), item);
	if(it == m_lastCacheItemVector.end() && hasTileEvent(item))
		m_lastCacheItemVector.push_back(item);
}

void MoveEvents::onRemoveTileItem(const Tile* tile, Item* item)
{
	if(m_lastCacheTile != tile)
		return;

	for(uint32_t i = 0; i < m_lastCacheItemVector.size(); ++i)
	{
		if(m_lastCacheItemVector[i] != item)
			continue;

		m_lastCacheItemVector[i] = nullptr;
		break;
	}
}



LOGGER_DEFINITION(MoveEvent);


MoveEvent::MoveEvent(LuaScriptInterface* _interface):
Event(_interface)
{
	m_eventType = MOVE_EVENT_NONE;
	stepFunction = nullptr;
	moveFunction = nullptr;
	equipFunction = nullptr;
	slot = SLOTP_WHEREEVER;
	wieldInfo = 0;
	reqLevel = 0;
	reqMagLevel = 0;
	premium = false;
}

MoveEvent::~MoveEvent()
{
	//
}

std::string MoveEvent::getScriptEventName() const
{
	switch(m_eventType)
	{
		case MOVE_EVENT_STEP_IN:
			return "onStepIn";

		case MOVE_EVENT_STEP_OUT:
			return "onStepOut";

		case MOVE_EVENT_EQUIP:
			return "onEquip";

		case MOVE_EVENT_DEEQUIP:
			return "onDeEquip";

		case MOVE_EVENT_ADD_ITEM:
			return "onAddItem";

		case MOVE_EVENT_REMOVE_ITEM:
			return "onRemoveItem";

		default:
			break;
	}

	LOGe("[MoveEvent::getScriptEventName] No valid event type.");
	return "";
}

std::string MoveEvent::getScriptEventParams() const
{
	switch(m_eventType)
	{
		case MOVE_EVENT_STEP_IN:
		case MOVE_EVENT_STEP_OUT:
			return "cid, item, position, lastPosition, fromPosition, toPosition, actor";

		case MOVE_EVENT_EQUIP:
		case MOVE_EVENT_DEEQUIP:
			return "cid, item, slot";

		case MOVE_EVENT_ADD_ITEM:
		case MOVE_EVENT_REMOVE_ITEM:
			return "moveItem, tileItem, position, cid";

		default:
			break;
	}

	LOGe("[MoveEvent::getScriptEventParams] No valid event type.");
	return "";
}

bool MoveEvent::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	int32_t intValue;
	if(readXMLString(p, "type", strValue) || readXMLString(p, "event", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "stepin")
			m_eventType = MOVE_EVENT_STEP_IN;
		else if(tmpStrValue == "stepout")
			m_eventType = MOVE_EVENT_STEP_OUT;
		else if(tmpStrValue == "equip")
			m_eventType = MOVE_EVENT_EQUIP;
		else if(tmpStrValue == "deequip")
			m_eventType = MOVE_EVENT_DEEQUIP;
		else if(tmpStrValue == "additem")
			m_eventType = MOVE_EVENT_ADD_ITEM;
		else if(tmpStrValue == "removeitem")
			m_eventType = MOVE_EVENT_REMOVE_ITEM;
		else
		{
			LOGe("[MoveEvent::configureMoveEvent] Unknown event type \"" << strValue << "\"");
			return false;
		}

		if(m_eventType == MOVE_EVENT_EQUIP || m_eventType == MOVE_EVENT_DEEQUIP)
		{
			if(readXMLString(p, "slot", strValue))
			{
				std::string tmpStrValue = asLowerCaseString(strValue);
				if(tmpStrValue == "head")
					slot = SLOTP_HEAD;
				else if(tmpStrValue == "necklace")
					slot = SLOTP_NECKLACE;
				else if(tmpStrValue == "backpack")
					slot = SLOTP_BACKPACK;
				else if(tmpStrValue == "armor")
					slot = SLOTP_ARMOR;
				else if(tmpStrValue == "left-hand")
					slot = SLOTP_LEFT;
				else if(tmpStrValue == "right-hand")
					slot = SLOTP_RIGHT;
				else if(tmpStrValue == "hands" || tmpStrValue == "two-handed")
					slot = SLOTP_TWO_HAND;
				else if(tmpStrValue == "hand" || tmpStrValue == "shield")
					slot = SLOTP_RIGHT | SLOTP_LEFT;
				else if(tmpStrValue == "legs")
					slot = SLOTP_LEGS;
				else if(tmpStrValue == "feet")
					slot = SLOTP_FEET;
				else if(tmpStrValue == "ring")
					slot = SLOTP_RING;
				else if(tmpStrValue == "ammo" || tmpStrValue == "ammunition")
					slot = SLOTP_AMMO;
				else if(tmpStrValue == "pickupable")
					slot = SLOTP_RIGHT | SLOTP_LEFT | SLOTP_AMMO;
				else
					LOGw("[MoveEvent::configureMoveEvent] Unknown slot type \"" << strValue << "\"");
			}

			wieldInfo = 0;
			if(readXMLInteger(p, "lvl", intValue) || readXMLInteger(p, "level", intValue))
			{
	 			reqLevel = intValue;
				if(reqLevel > 0)
					wieldInfo |= WIELDINFO_LEVEL;
			}

			if(readXMLInteger(p, "maglv", intValue) || readXMLInteger(p, "maglevel", intValue))
			{
	 			reqMagLevel = intValue;
				if(reqMagLevel > 0)
					wieldInfo |= WIELDINFO_MAGLV;
			}

			if(readXMLString(p, "prem", strValue) || readXMLString(p, "premium", strValue))
			{
				premium = booleanString(strValue);
				if(premium)
					wieldInfo |= WIELDINFO_PREMIUM;
			}

			StringVector vocStringVec;
			std::string error = "";
			xmlNodePtr vocationNode = p->children;
			while(vocationNode)
			{
				if(!parseVocationNode(vocationNode, vocEquipMap, vocStringVec, error))
					LOGw("[MoveEvent::configureEvent] " << error);

				vocationNode = vocationNode->next;
			}

			if(!vocEquipMap.empty())
				wieldInfo |= WIELDINFO_VOCREQ;

			vocationString = parseVocationString(vocStringVec);
		}
	}
	else
	{
		LOGe("[MoveEvent::configureMoveEvent] No event found.");
		return false;
	}

	return true;
}

bool MoveEvent::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "onstepinfield")
		stepFunction = StepInField;
	else if(tmpFunctionName == "onaddfield")
		moveFunction = AddItemField;
	else if(tmpFunctionName == "onequipitem")
		equipFunction = EquipItem;
	else if(tmpFunctionName == "ondeequipitem")
		equipFunction = DeEquipItem;
	else
	{
		LOGw("[MoveEvent::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

MoveEvent_t MoveEvent::getEventType() const
{
	if(m_eventType == MOVE_EVENT_NONE)
	{
		LOGe("[MoveEvent::getEventType] MOVE_EVENT_NONE");
		return (MoveEvent_t)0;
	}

	return m_eventType;
}

void MoveEvent::setEventType(MoveEvent_t type)
{
	m_eventType = type;
}

uint32_t MoveEvent::StepInField(const CreatureP& creature, Item* item)
{
	if(MagicField* field = item->getMagicField())
	{
		field->onStepInField(creature, creature->getPlayer());
		return 1;
	}

	return LUA_ERROR_ITEM_NOT_FOUND;
}

uint32_t MoveEvent::AddItemField(Item* item)
{
	if(MagicField* field = item->getMagicField())
	{
		if(Tile* tile = item->getTile())
		{
			if(CreatureVector* creatures = tile->getCreatures())
			{
				for(CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
					field->onStepInField((*cit).get());
			}
		}

		return 1;
	}

	return LUA_ERROR_ITEM_NOT_FOUND;
}

uint32_t MoveEvent::EquipItem(const MoveEventP& moveEvent, Player* player, Item* item, slots_t slot, bool isCheck)
{
	if(player->isItemAbilityEnabled(slot))
		return 1;

	if(!player->hasFlag(PlayerFlag_IgnoreEquipCheck) && moveEvent->getWieldInfo() != 0)
	{
		if(player->getLevel() < (uint32_t)moveEvent->getReqLevel() || player->getMagicLevel() < (uint32_t)moveEvent->getReqMagLv())
			return 0;

		if(moveEvent->isPremium() && !player->isPremium())
			return 0;

		if(!moveEvent->getVocEquipMap().empty() && moveEvent->getVocEquipMap().find(player->getVocationId()) == moveEvent->getVocEquipMap().end())
			return 0;
	}

	if(isCheck)
		return 1;

	ItemKindPC kind = item->getKind();
	if(kind->transformEquipTo)
	{
		ItemP newItem = server.game().transformItem(item, kind->transformEquipTo);
		server.game().startDecay(newItem.get());
	}
	else
		player->setItemAbility(slot, true);

	if(kind->abilities.invisible)
	{
		Condition* condition = Condition::createCondition((ConditionId_t)slot, CONDITION_INVISIBLE, -1, 0);
		player->addCondition(condition);
	}

	if(kind->abilities.manaShield)
	{
		Condition* condition = Condition::createCondition((ConditionId_t)slot, CONDITION_MANASHIELD, -1, 0);
		player->addCondition(condition);
	}

	if(kind->abilities.speed)
		server.game().changeSpeed(player, kind->abilities.speed);

	if(kind->abilities.conditionSuppressions)
	{
		player->setConditionSuppressions(kind->abilities.conditionSuppressions, false);
		player->sendIcons();
	}

	if(kind->abilities.regeneration)
	{
		Condition* condition = Condition::createCondition((ConditionId_t)slot, CONDITION_REGENERATION, -1, 0);
		if(kind->abilities.healthGain)
			condition->setParam(CONDITIONPARAM_HEALTHGAIN, kind->abilities.healthGain);

		if(kind->abilities.healthTicks)
			condition->setParam(CONDITIONPARAM_HEALTHTICKS, kind->abilities.healthTicks);

		if(kind->abilities.manaGain)
			condition->setParam(CONDITIONPARAM_MANAGAIN, kind->abilities.manaGain);

		if(kind->abilities.manaTicks)
			condition->setParam(CONDITIONPARAM_MANATICKS, kind->abilities.manaTicks);

		player->addCondition(condition);
	}

	bool needUpdateSkills = false;
	for(uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i)
	{
		if(kind->abilities.skills[i])
		{
			player->setVarSkill((skills_t)i, kind->abilities.skills[i]);
			if(!needUpdateSkills)
				needUpdateSkills = true;
		}

		if(kind->abilities.skillsPercent[i])
		{
			player->setVarSkill((skills_t)i, (int32_t)(player->getSkill((skills_t)i, SKILL_LEVEL) * ((kind->abilities.skillsPercent[i] - 100) / 100.f)));
			if(!needUpdateSkills)
				needUpdateSkills = true;
		}
	}

	if(needUpdateSkills)
		player->sendSkills();

	bool needUpdateStats = false;
	for(uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s)
	{
		if(kind->abilities.stats[s])
		{
			player->setVarStats((stats_t)s, kind->abilities.stats[s]);
			if(!needUpdateStats)
				needUpdateStats = true;
		}

		if(kind->abilities.statsPercent[s])
		{
			player->setVarStats((stats_t)s, (int32_t)(player->getDefaultStats((stats_t)s) * ((kind->abilities.statsPercent[s] - 100) / 100.f)));
			if(!needUpdateStats)
				needUpdateStats = true;
		}
	}

	if(needUpdateStats)
		player->sendStats();

	return 1;
}

uint32_t MoveEvent::DeEquipItem(const MoveEventP& moveEvent, Player* player, Item* item, slots_t slot, bool isRemoval)
{
	if(!player->isItemAbilityEnabled(slot))
		return 1;

	player->setItemAbility(slot, false);

	ItemKindPC kind = item->getKind();
	if(isRemoval && kind->transformDeEquipTo)
	{
		server.game().transformItem(item, kind->transformDeEquipTo);
		server.game().startDecay(item);
	}

	if(kind->abilities.invisible)
		player->removeCondition(CONDITION_INVISIBLE, (ConditionId_t)slot);

	if(kind->abilities.manaShield)
		player->removeCondition(CONDITION_MANASHIELD, (ConditionId_t)slot);

	if(kind->abilities.speed)
		server.game().changeSpeed(player, -kind->abilities.speed);

	if(kind->abilities.conditionSuppressions)
	{
		player->setConditionSuppressions(kind->abilities.conditionSuppressions, true);
		player->sendIcons();
	}

	if(kind->abilities.regeneration)
		player->removeCondition(CONDITION_REGENERATION, (ConditionId_t)slot);

	bool needUpdateSkills = false;
	for(uint32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i)
	{
		if(kind->abilities.skills[i])
		{
			needUpdateSkills = true;
			player->setVarSkill((skills_t)i, -kind->abilities.skills[i]);
		}

		if(kind->abilities.skillsPercent[i])
		{
			needUpdateSkills = true;
			player->setVarSkill((skills_t)i, -(int32_t)(player->getSkill((skills_t)i, SKILL_LEVEL) * ((kind->abilities.skillsPercent[i] - 100) / 100.f)));
		}
	}

	if(needUpdateSkills)
		player->sendSkills();

	bool needUpdateStats = false;
	for(uint32_t s = STAT_FIRST; s <= STAT_LAST; ++s)
	{
		if(kind->abilities.stats[s])
		{
			needUpdateStats = true;
			player->setVarStats((stats_t)s, -kind->abilities.stats[s]);
		}

		if(kind->abilities.statsPercent[s])
		{
			needUpdateStats = true;
			player->setVarStats((stats_t)s, -(int32_t)(player->getDefaultStats((stats_t)s) * ((kind->abilities.statsPercent[s] - 100) / 100.f)));
		}
	}

	if(needUpdateStats)
		player->sendStats();

	return 1;
}

uint32_t MoveEvent::fireStepEvent(Creature* actor, const CreatureP& creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos)
{
	if(isScripted())
		return executeStep(actor, creature, item, pos, fromPos, toPos);

	return stepFunction(creature, item);
}

uint32_t MoveEvent::executeStep(Creature* actor, const CreatureP& creature, Item* item, const Position& pos, const Position& fromPos, const Position& toPos)
{
	//onStepIn(cid, item, position, lastPosition, fromPosition, toPosition, actor)
	//onStepOut(cid, item, position, lastPosition, fromPosition, toPosition, actor)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			env->streamThing(scriptstream, "item", item, env->addThing(item));
			env->streamPosition(scriptstream, "position", pos, 0);
			env->streamPosition(scriptstream, "lastPosition", creature->getLastPosition(), 0);
			env->streamPosition(scriptstream, "fromPosition", fromPos, 0);
			env->streamPosition(scriptstream, "toPosition", toPos, 0);
			scriptstream << "local actor = " << env->addThing(actor) << std::endl;

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
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));

			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			LuaScriptInterface::pushPosition(L, pos);
			LuaScriptInterface::pushPosition(L, creature->getLastPosition());
			LuaScriptInterface::pushPosition(L, fromPos);
			LuaScriptInterface::pushPosition(L, toPos);

			lua_pushnumber(L, env->addThing(actor));
			bool result = m_interface->callFunction(7);

			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[MoveEvent::executeStep] Call stack overflow.");
		return 0;
	}
}

uint32_t MoveEvent::fireEquip(Player* player, Item* item, slots_t slot, bool boolean)
{
	if(isScripted())
		return executeEquip(player, item, slot);

	return equipFunction(shared_from_this(), player, item, slot, boolean);
}

uint32_t MoveEvent::executeEquip(Player* player, Item* item, slots_t slot)
{
	//onEquip(cid, item, slot)
	//onDeEquip(cid, item, slot)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(player->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(player) << std::endl;
			env->streamThing(scriptstream, "item", item, env->addThing(item));
			scriptstream << "local slot = " << +slot << std::endl;

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
			lua_pushnumber(L, +slot);

			bool result = m_interface->callFunction(3);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[MoveEvent::executeEquip] Call stack overflow.");
		return 0;
	}
}

uint32_t MoveEvent::fireAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos)
{
	if(isScripted())
		return executeAddRemItem(actor, item, tileItem, pos);

	return moveFunction(item);
}

uint32_t MoveEvent::executeAddRemItem(Creature* actor, Item* item, Item* tileItem, const Position& pos)
{
	//onAddItem(moveItem, tileItem, position, cid)
	//onRemoveItem(moveItem, tileItem, position, cid)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(pos);
			std::stringstream scriptstream;

			env->streamThing(scriptstream, "moveItem", item, env->addThing(item));
			env->streamThing(scriptstream, "tileItem", tileItem, env->addThing(tileItem));

			env->streamPosition(scriptstream, "position", pos, 0);
			scriptstream << "local cid = " << env->addThing(actor) << std::endl;

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
			env->setRealPos(pos);

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);

			LuaScriptInterface::pushThing(L, item, env->addThing(item));
			LuaScriptInterface::pushThing(L, tileItem, env->addThing(tileItem));
			LuaScriptInterface::pushPosition(L, pos);

			lua_pushnumber(L, env->addThing(actor));
			bool result = m_interface->callFunction(4);

			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[MoveEvent::executeAddRemItem] Call stack overflow.");
		return 0;
	}
}
