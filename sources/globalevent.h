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

#ifndef _GLOBALEVENT_H
#define _GLOBALEVENT_H

#include "baseevents.h"

#define GLOBAL_THINK_INTERVAL 1000

class GlobalEvent;

typedef std::shared_ptr<GlobalEvent>       GlobalEventP;
typedef std::map<std::string,GlobalEventP> GlobalEventMap;


enum GlobalEvent_t
{
	GLOBAL_EVENT_NONE,
	GLOBAL_EVENT_TIMER,

	GLOBAL_EVENT_STARTUP,
	GLOBAL_EVENT_SHUTDOWN,
	GLOBAL_EVENT_RECORD
};



class GlobalEvents : public BaseEvents<GlobalEvent> {
	public:
		GlobalEvents();

		void startup();
		void timer();
		void think(uint32_t interval);
		void execute(GlobalEvent_t type);

		GlobalEventMap getEventMap(GlobalEvent_t type);

	private:
		virtual std::string getScriptBaseName() const {return "globalevents";}
		virtual void clear();

		virtual GlobalEventP getEvent(const std::string& nodeName);
		virtual bool registerEvent(const GlobalEventP& event, xmlNodePtr p, bool override);

		virtual LuaScriptInterface& getInterface() {return m_interface;}

		LOGGER_DECLARATION;

		LuaScriptInterface m_interface;

		GlobalEventMap thinkMap, serverMap, timerMap;
};

class GlobalEvent : public Event
{
	public:
		GlobalEvent(LuaScriptInterface* _interface);
		virtual ~GlobalEvent() {}

		virtual bool configureEvent(xmlNodePtr p);
		int32_t executeThink(uint32_t interval, uint32_t lastExecution, uint32_t thinkInterval);
		int32_t executeRecord(uint32_t current, uint32_t old, Player* player);
		int32_t executeEvent();

		GlobalEvent_t getEventType() const {return m_eventType;}
		std::string getName() const {return m_name;}
		uint32_t getInterval() const {return m_interval;}

		uint32_t getHour() const {return m_hour;}
		uint32_t getMinute() const {return m_minute;}

		time_t getLastExecution() const {return m_lastExecution;}
		void setLastExecution(time_t time) {m_lastExecution = time;}

	private:
		virtual std::string getScriptEventName() const;
		virtual std::string getScriptEventParams() const;

		LOGGER_DECLARATION;

		std::string m_name;
		time_t m_lastExecution;
		uint32_t m_interval, m_hour, m_minute;
		GlobalEvent_t m_eventType;
};

#endif // _GLOBALEVENT_H
