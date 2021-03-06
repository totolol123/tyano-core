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

#ifndef _CREATUREEVENT_H
#define _CREATUREEVENT_H

#include "baseevents.h"

class Tile;


class CreatureEvent;

using CreatureP      = boost::intrusive_ptr<Creature>;
using CreatureEventP = std::shared_ptr<CreatureEvent>;



class CreatureEvents : public BaseEvents<CreatureEvent> {
	public:
		CreatureEvents();

		// global events
		bool playerLogin(Player* player);
		bool playerLogout(Player* player, bool forceLogout);

		CreatureEventP getEventByName(const std::string& name, bool forceLoaded = true);

	protected:
		virtual std::string getScriptBaseName() const {return "creaturescripts";}
		virtual void clear();

		virtual CreatureEventP getEvent(const std::string& nodeName);
		virtual bool registerEvent(const CreatureEventP& event, xmlNodePtr p, bool override);

		virtual LuaScriptInterface& getInterface() {return m_interface;}
		LuaScriptInterface m_interface;

		//creature events
		typedef std::map<std::string, CreatureEventP> CreatureEventList;
		CreatureEventList m_creatureEvents;


	private:

		LOGGER_DECLARATION;
};

struct DeathEntry;
typedef std::vector<DeathEntry> DeathList;

typedef std::map<uint32_t, Player*> UsersMap;
class CreatureEvent : public Event
{
	public:
		CreatureEvent(LuaScriptInterface* _interface);
		virtual ~CreatureEvent() {}

		virtual bool configureEvent(xmlNodePtr p);

		bool isLoaded() const {return m_isLoaded;}
		const std::string& getName() const {return m_eventName;}
		CreatureEventType_t getEventType() const {return m_type;}

		void copyEvent(const CreatureEventP& creatureEvent);
		void clearEvent();

		//scripting
		uint32_t executeLogin(Player* player);
		uint32_t executeLogout(Player* player, bool forceLogout);
		uint32_t executeChannelJoin(Player* player, uint16_t channelId, UsersMap usersMap);
		uint32_t executeChannelLeave(Player* player, uint16_t channelId, UsersMap usersMap);
		uint32_t executeAdvance(Player* player, skills_t skill, uint32_t oldLevel, uint32_t newLevel);
		uint32_t executeLook(Player* player, Thing* thing, const Position& position, int16_t stackpos, int32_t lookDistance);
		uint32_t executeMailSend(Player* player, Player* receiver, Item* item, bool openBox);
		uint32_t executeMailReceive(Player* player, Player* sender, Item* item, bool openBox);
		uint32_t executeTradeRequest(Player* player, Player* target, Item* item);
		uint32_t executeTradeAccept(Player* player, Player* target, Item* item, Item* targetItem);
		uint32_t executeTextEdit(Player* player, Item* item, std::string newText);
		uint32_t executeReportBug(Player* player, std::string comment);
		uint32_t executeThink(Creature* creature, uint32_t interval);
		uint32_t executeDirection(Creature* creature, Direction old, Direction current);
		uint32_t executeOutfit(Creature* creature, const Outfit_t& old, const Outfit_t& current);
		uint32_t executeStatsChange(const CreatureP& creature, const CreatureP& attacker, StatsChange_t type, CombatType_t combat, int32_t value);
		uint32_t executeCombatArea(const CreatureP& creature, Tile* tile, bool isAggressive);
		uint32_t executePush(Player* player, Creature* target);
		uint32_t executeTarget(const CreatureP& creature, const CreatureP& target);
		uint32_t executeFollow(Creature* creature, Creature* target);
		uint32_t executeCombat(const CreatureP& creature, const CreatureP& target);
		uint32_t executeAttack(Creature* creature, Creature* target);
		uint32_t executeCast(Creature* creature, Creature* target = nullptr);
		uint32_t executeKill(Creature* creature, Creature* target, bool lastHit, double experience);
		uint32_t executeDeath(Creature* creature, Item* corpse, DeathList deathList);
		uint32_t executePrepareDeath(Creature* creature, DeathList deathList);
		//

	private:
		virtual std::string getScriptEventName() const;
		virtual std::string getScriptEventParams() const;


		LOGGER_DECLARATION;

		bool m_isLoaded;
		std::string m_eventName;
		CreatureEventType_t m_type;
};

#endif // _CREATUREEVENT_H
