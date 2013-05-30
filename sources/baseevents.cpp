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

#include "baseevents.h"
#include "luascript.h"
#include "tools.h"


LOGGER_DEFINITION(Event);



Event::Event(LuaScriptInterface* interface)
	: m_interface(interface),
	  m_scripted(EVENT_SCRIPT_FALSE),
	  m_scriptId(0)
{
	assert(interface != nullptr);
}


bool Event::loadBuffer(const std::string& buffer)
{
	if(!m_interface || m_scriptData != "")
	{
		LOGe("[Event::loadScriptFile] m_interface == nullptr, scriptData != \"\"");
		return false;
	}

	m_scripted = EVENT_SCRIPT_BUFFER;
	m_scriptData = buffer;
	return true;
}

bool Event::checkBuffer(const std::string& buffer)
{
	return true; //TODO
}

bool Event::loadScript(const std::string& script, bool file)
{
	if(!m_interface || m_scriptId != 0)
	{
		LOGe("[Event::loadScript] m_interface == nullptr, scriptId = " << m_scriptId);
		return false;
	}

	bool result = false;
	if(!file)
	{
		std::string buffer = script, function = "function " + getScriptEventName();
		trimString(buffer);
		if(buffer.find(function) == std::string::npos)
		{
			std::stringstream scriptstream;
			scriptstream << function << "(" << getScriptEventParams() << ")" << std::endl << buffer << std::endl << "end";
			buffer = scriptstream.str();
		}

		result = m_interface->loadBuffer(buffer);
	}
	else
		result = m_interface->loadFile(script);

	if(!result)
	{
		LOGe("[Event::loadScript] Cannot load script (" << script << "): " << m_interface->getLastError());
		return false;
	}

	int32_t id = m_interface->getEvent(getScriptEventName());
	if(id == -1)
	{
		LOGe("[Event::loadScript] Event " << getScriptEventName() << " not found (" << script << ")");
		return false;
	}

	m_scripted = EVENT_SCRIPT_TRUE;
	m_scriptId = id;
	return true;
}

bool Event::checkScript(const std::string& script, bool file)
{
	return true; //TODO
}



LOGGER_DEFINITION(CallBack);


CallBack::CallBack()
{
	m_scriptId = 0;
	m_interface = nullptr;
	m_loaded = false;
}

bool CallBack::loadCallBack(LuaScriptInterface* _interface, std::string name)
{
	if(!_interface)
	{
		LOGe("[CallBack::loadCallBack] m_interface == nullptr");
		return false;
	}

	m_interface = _interface;
	int32_t id = m_interface->getEvent(name);
	if(id == -1)
	{
		LOGe("[CallBack::loadCallBack] Event " << name << " not found.");
		return false;
	}

	m_callbackName = name;
	m_scriptId = id;
	m_loaded = true;
	return true;
}
