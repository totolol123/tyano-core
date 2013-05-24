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

#include "gameservers.h"
#include "tools.h"


LOGGER_DEFINITION(GameServers);


void GameServers::clear()
{
	for(GameServersMap::iterator it = serverList.begin(); it != serverList.end(); ++it)
		delete it->second;

	serverList.clear();
}

bool GameServers::reload(bool showResult/* = true*/)
{
	clear();
	return loadFromXml(showResult);
}

bool GameServers::loadFromXml(bool showResult/* = true*/)
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FileType::XML, "servers.xml").c_str());
	if(!doc)
	{
		LOGe("[GameServers::loadFromXml] Cannot load servers file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr p, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"servers"))
	{
		LOGe("[GameServers::loadFromXml] Malformed servers file.");
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	int32_t intValue;
	p = root->children;
	while(p)
	{
		if(xmlStrcmp(p->name, (const xmlChar*)"server"))
		{
			p = p->next;
			continue;
		}

		std::string name, address;
		uint32_t id, versionMin, versionMax, port;
		if(readXMLInteger(p, "id", intValue))
			id = intValue;
		else
		{
			LOGw("[GameServers::loadFromXml] Missing id, skipping");
			p = p->next;
			continue;
		}

		if(getServerById(id))
		{
			LOGw("[GameServers::loadFromXml] Duplicate server id " << id << ", skipping");
			p = p->next;
			continue;
		}

		if(readXMLString(p, "name", strValue))
			name = strValue;
		else
		{
			name = "Server #" + id;
			LOGw("[GameServers::loadFromXml] Missing name for server " << id << ", using default");
		}

		if(readXMLInteger(p, "versionMin", intValue))
			versionMin = intValue;
		else
		{
			versionMin = CLIENT_VERSION_MIN;
			LOGw("[GameServers::loadFromXml] Missing versionMin for server " << id << ", using default");
		}

		if(readXMLInteger(p, "versionMax", intValue))
			versionMax = intValue;
		else
		{
			versionMax = CLIENT_VERSION_MAX;
			LOGw("[GameServers::loadFromXml] Missing versionMax for server " << id << ", using default");
		}

		if(readXMLString(p, "address", strValue) || readXMLString(p, "ip", strValue))
			address = strValue;
		else
		{
			address = "localhost";
			LOGw("[GameServers::loadFromXml] Missing address for server " << id << ", using default");
		}

		if(readXMLInteger(p, "port", intValue))
			port = intValue;
		else
		{
			port = 7171;
			LOGw("[GameServers::loadFromXml] Missing port for server " << id << ", using default");
		}

		if(GameServer* server = new GameServer(name, versionMin, versionMax, inet_addr(address.c_str()), port))
			serverList[id] = server;
		else
			LOGe("[GameServers::loadFromXml] Couldn't add server " << name);

		p = p->next;
	}

	xmlFreeDoc(doc);
	if(showResult)
	{
		LOGi("Servers loaded:");
		for(GameServersMap::iterator it = serverList.begin(); it != serverList.end(); it++)
			LOGi(it->second->getName() << " (" << it->second->getAddress() << ":" << it->second->getPort() << ")");
	}

	return true;
}

GameServer* GameServers::getServerById(uint32_t id) const
{
	GameServersMap::const_iterator it = serverList.find(id);
	if(it != serverList.end())
		return it->second;

	return nullptr;
}

GameServer* GameServers::getServerByName(std::string name) const
{
	for(GameServersMap::const_iterator it = serverList.begin(); it != serverList.end(); ++it)
	{
		if(it->second->getName() == name)
			return it->second;
	}

	return nullptr;
}

GameServer* GameServers::getServerByAddress(uint32_t address) const
{
	for(GameServersMap::const_iterator it = serverList.begin(); it != serverList.end(); ++it)
	{
		if(it->second->getAddress() == address)
			return it->second;
	}

	return nullptr;
}

GameServer* GameServers::getServerByPort(uint32_t port) const
{
	for(GameServersMap::const_iterator it = serverList.begin(); it != serverList.end(); ++it)
	{
		if(it->second->getPort() == port)
			return it->second;
	}

	return nullptr;
}
