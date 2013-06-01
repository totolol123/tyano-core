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

#ifndef _ACTIONS_H
#define _ACTIONS_H

#include "baseevents.h"
#include "const.h"

class Action;
class Container;
class Creature;
class Item;
class Player;
class Position;

typedef std::shared_ptr<Action> ActionP;


enum ActionType_t
{
	ACTION_ANY,
	ACTION_UNIQUEID,
	ACTION_ACTIONID,
	ACTION_ITEMID,
	ACTION_RUNEID
};

class Actions : public BaseEvents<Action> {
	public:
		Actions();

		bool useItem(Player* player, const Position& pos, uint8_t index, Item* item);
		bool useItemEx(Player* player, const Position& fromPos, const Position& toPos,
			uint8_t toStackPos, Item* item, bool isHotkey, uint32_t creatureId = 0);

		ReturnValue canUse(const Player* player, const Position& pos);
		ReturnValue canUseEx(const Player* player, const Position& pos, const Item* item);
		ReturnValue canUseFar(const Creature* creature, const Position& toPos, bool checkLineOfSight);
		bool hasAction(const Item* item) const {return getAction(item, ACTION_ANY) != nullptr;}

	protected:
		virtual std::string getScriptBaseName() const {return "actions";}
		virtual void clear();

		virtual ActionP getEvent(const std::string& nodeName);
		virtual bool registerEvent(const ActionP& event, xmlNodePtr p, bool override);

		virtual LuaScriptInterface& getInterface() {return m_interface;}
		LuaScriptInterface m_interface;

		void registerItemID(int32_t itemId, const ActionP& event);
		void registerActionID(int32_t actionId, const ActionP& event);
		void registerUniqueID(int32_t uniqueId, const ActionP& event);

		typedef std::map<uint16_t, ActionP> ActionUseMap;

		ActionUseMap useItemMap;
		ActionUseMap uniqueItemMap;
		ActionUseMap actionItemMap;

		bool executeUse(const ActionP& action, Player* player, Item* item, const PositionEx& posEx, uint32_t creatureId);
		ReturnValue internalUseItem(Player* player, const Position& pos,
			uint8_t index, Item* item, uint32_t creatureId);
		bool executeUseEx(const ActionP& action, Player* player, Item* item, const PositionEx& fromPosEx,
			const PositionEx& toPosEx, bool isHotkey, uint32_t creatureId);
		ReturnValue internalUseItemEx(Player* player, const PositionEx& fromPosEx, const PositionEx& toPosEx,
			Item* item, bool isHotkey, uint32_t creatureId);

		ActionP getAction(const Item* item, ActionType_t type) const;

	private:

		LOGGER_DECLARATION;

};

typedef bool (ActionFunction)(Player* player, Item* item, const PositionEx& posFrom, const PositionEx& posTo, bool extendedUse, uint32_t creatureId);
class Action : public Event
{
	public:
		Action(LuaScriptInterface* interface);
		virtual ~Action() {}

		virtual bool configureEvent(xmlNodePtr p);
		virtual bool loadFunction(const std::string& functionName);

		//scripting
		virtual bool executeUse(Player* player, Item* item, const PositionEx& posFrom,
			const PositionEx& posTo, bool extendedUse, uint32_t creatureId);

		bool getAllowFarUse() const {return allowFarUse;}
		void setAllowFarUse(bool v) {allowFarUse = v;}

		bool getCheckLineOfSight() const {return checkLineOfSight;}
		void setCheckLineOfSight(bool v) {checkLineOfSight = v;}

		virtual ReturnValue canExecuteAction(const Player* player, const Position& pos);
		virtual bool hasOwnErrorHandler() {return false;}

		ActionFunction* function;

	protected:
		virtual std::string getScriptEventName() const {return "onUse";}
		virtual std::string getScriptEventParams() const {return "cid, item, fromPosition, itemEx, toPosition";}

		static ActionFunction increaseItemId;
		static ActionFunction decreaseItemId;

		bool allowFarUse;
		bool checkLineOfSight;

	private:

		LOGGER_DECLARATION;
};

#endif // _ACTIONS_H
