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

#ifndef _BASEEVENTS_H
#define _BASEEVENTS_H

#include "tools.h"

class Event;
class LuaScriptInterface;

typedef std::shared_ptr<Event> EventP;


template<typename T>
class BaseEvents
{
	public:
		BaseEvents(): m_loaded(false) {}
		virtual ~BaseEvents() {}


		bool loadFromXml() {
			getInterface().initState();

			std::string scriptsName = getScriptBaseName();
			if(m_loaded)
			{
				LOGe("[BaseEvents::loadFromXml] " << scriptsName << " interface already loaded!");
				return false;
			}

			if(!getInterface().loadDirectory(getFilePath(FileType::OTHER, std::string(scriptsName + "/lib/"))))
				LOGe("[BaseEvents::loadFromXml] Cannot load " << scriptsName << "/lib/");

			xmlDocPtr doc = xmlParseFile(getFilePath(FileType::OTHER, std::string(scriptsName + "/" + scriptsName + ".xml")).c_str());
			if(!doc)
			{
				LOGe("[BaseEvents::loadFromXml] Cannot open " << scriptsName << ".xml file: " << getLastXMLError());
				return false;
			}

			xmlNodePtr p, root = xmlDocGetRootElement(doc);
			if(xmlStrcmp(root->name,(const xmlChar*)scriptsName.c_str()))
			{
				LOGe("[BaseEvents::loadFromXml] Malformed " << scriptsName << ".xml file.");
				xmlFreeDoc(doc);
				return false;
			}

			std::string scriptsPath = getFilePath(FileType::OTHER, std::string(scriptsName + "/scripts/"));
			p = root->children;
			while(p)
			{
				parseEventNode(p, scriptsPath, false);
				p = p->next;
			}

			xmlFreeDoc(doc);
			m_loaded = true;
			return m_loaded;
		}


		bool parseEventNode(xmlNodePtr p, std::string scriptsPath, bool override) {
			std::shared_ptr<T> event = getEvent((const char*)p->name);
			if(!event)
				return false;

			if(!event->configureEvent(p))
			{
				LOGe("[BaseEvents::loadFromXml] Cannot configure an event");
				return false;
			}

			bool success = false;
			std::string strValue, tmpStrValue;
			if(readXMLString(p, "event", strValue))
			{
				tmpStrValue = asLowerCaseString(strValue);
				if(tmpStrValue == "script")
				{
					bool file = readXMLString(p, "value", strValue);
					if(!file)
						parseXMLContentString(p->children, strValue);
					else
						strValue = scriptsPath + strValue;

					success = event->checkScript(strValue, file) && event->loadScript(strValue, file);
				}
				else if(tmpStrValue == "buffer")
				{
					if(!readXMLString(p, "value", strValue))
						parseXMLContentString(p->children, strValue);

					success = event->checkBuffer(strValue) && event->loadBuffer(strValue);
				}
				else if(tmpStrValue == "function")
				{
					if(readXMLString(p, "value", strValue))
						success = event->loadFunction(strValue);
				}
			}
			else if(readXMLString(p, "script", strValue))
			{
				bool file = asLowerCaseString(strValue) != "cdata";
				if(!file)
					parseXMLContentString(p->children, strValue);
				else
					strValue = scriptsPath + strValue;

				success = event->checkScript(strValue, file) && event->loadScript(strValue, file);
			}
			else if(readXMLString(p, "buffer", strValue))
			{
				if(asLowerCaseString(strValue) == "cdata")
					parseXMLContentString(p->children, strValue);

				success = event->checkBuffer(strValue) && event->loadBuffer(strValue);
			}
			else if(readXMLString(p, "function", strValue))
				success = event->loadFunction(strValue);
			else if(parseXMLContentString(p->children, strValue) && event->checkBuffer(strValue))
				success = event->loadBuffer(strValue);

			if(!override && readXMLString(p, "override", strValue) && booleanString(strValue))
				override = true;

			if(success) {
				registerEvent(event, p, override);
			}

			return success;
		}


		bool reload() {
			m_loaded = false;
			clear();
			return loadFromXml();
		}


		bool isLoaded() const {return m_loaded;}

	protected:
		virtual std::string getScriptBaseName() const = 0;
		virtual void clear() = 0;

		virtual bool registerEvent(const std::shared_ptr<T>& event, xmlNodePtr p, bool override) = 0;
		virtual std::shared_ptr<T> getEvent(const std::string& nodeName) = 0;

		virtual LuaScriptInterface& getInterface() = 0;

		bool m_loaded;


	private:

		LOGGER_DECLARATION;

};


template <typename T>
const LoggerPtr BaseEvents<T>::logger = Logger::getLogger("tfts.BaseEvents");


enum EventScript_t
{
	EVENT_SCRIPT_FALSE,
	EVENT_SCRIPT_BUFFER,
	EVENT_SCRIPT_TRUE
};


class EventBase {

public:

	virtual bool configureEvent(xmlNodePtr p) = 0;
	virtual bool isScripted() const = 0;

	virtual bool loadBuffer(const std::string& buffer) = 0;
	virtual bool checkBuffer(const std::string& buffer) = 0;

	virtual bool loadScript(const std::string& script, bool file) = 0;
	virtual bool checkScript(const std::string& script, bool file) = 0;

	virtual bool loadFunction(const std::string& functionName) {return false;}

protected:

	EventBase() {}
	virtual ~EventBase() {}

	virtual std::string getScriptEventName() const = 0;
	virtual std::string getScriptEventParams() const = 0;

};


class Event : virtual public EventBase, public std::enable_shared_from_this<Event> {
	protected:
		Event(LuaScriptInterface* _interface): m_interface(_interface),
			m_scripted(EVENT_SCRIPT_FALSE), m_scriptId(0) {}
		Event(const Event& copy);

	public:

		virtual bool isScripted() const {return m_scripted != EVENT_SCRIPT_FALSE;}

		virtual bool loadBuffer(const std::string& buffer);
		virtual bool checkBuffer(const std::string& buffer);

		virtual bool loadScript(const std::string& script, bool file);
		virtual bool checkScript(const std::string& script, bool file);

		virtual bool loadFunction(const std::string& functionName) {return false;}

	protected:

		LuaScriptInterface* m_interface;
		EventScript_t m_scripted;

		int32_t m_scriptId;
		std::string m_scriptData;


	private:

		LOGGER_DECLARATION;

};

class CallBack
{
	public:
		CallBack();
		virtual ~CallBack() {}

		bool loadCallBack(LuaScriptInterface* _interface, std::string name);

	protected:
		int32_t m_scriptId;
		LuaScriptInterface* m_interface;

		bool m_loaded;
		std::string m_callbackName;


	private:

		LOGGER_DECLARATION;
};

#endif // _BASEEVENTS_H
