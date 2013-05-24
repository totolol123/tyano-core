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
#include "scriptmanager.h"

#include "actions.h"
#include "movement.h"
#include "spells.h"
#include "talkaction.h"
#include "creatureevent.h"
#include "globalevent.h"
#include "weapons.h"

#include "monsters.h"
#include "spawn.h"
#include "raids.h"
#include "group.h"
#include "vocation.h"
#include "outfit.h"
#include "quests.h"
#include "items.h"
#include "chat.h"

#include "configmanager.h"
#include "luascript.h"
#include "server.h"


LOGGER_DEFINITION(ScriptingManager);


bool ScriptingManager::load()
{
	if(!server.weapons().loadFromXml())
	{
		LOGe("Unable to load weapons!");
		return false;
	}
	server.weapons().loadDefaults();

	if(!server.spells().loadFromXml())
	{
		LOGe("Unable to load spells!");
		return false;
	}

	if(!server.actions().loadFromXml())
	{
		LOGe("Unable to load actions!");
		return false;
	}

	if(!server.talkActions().loadFromXml())
	{
		LOGe("Unable to load talk actions!");
		return false;
	}

	if(!server.moveEvents().loadFromXml())
	{
		LOGe("Unable to load move events!");
		return false;
	}

	if(!server.creatureEvents().loadFromXml())
	{
		LOGe("Unable to load creature events!");
		return false;
	}

	if(!server.globalEvents().loadFromXml())
	{
		LOGe("Unable to load global events!");
		return false;
	}

	return true;
}

bool ScriptingManager::loadMods()
{
	boost::filesystem::path modsPath(getFilePath(FileType::MOD, ""));
	if(!boost::filesystem::exists(modsPath))
		return true; //silently ignore

	int32_t i = 0, j = 0;
	bool enabled = false;
	for(boost::filesystem::directory_iterator it(modsPath), end; it != end; ++it)
	{
		std::string s = it->path().filename().string();
		if(boost::filesystem::is_directory(it->status()) && (s.size() > 4 ? s.substr(s.size() - 4) : "") != ".xml")
			continue;

		LOGd("Loading mod '" << s << "'...");
		if(loadFromXml(s, enabled))
		{
			if(!enabled)
			{
				++j;
			}

		}

		++i;
	}

	modsLoaded = true;
	return true;
}

void ScriptingManager::clearMods()
{
	modMap.clear();
	libMap.clear();
}

bool ScriptingManager::reloadMods()
{
	clearMods();
	return loadMods();
}

bool ScriptingManager::loadFromXml(const std::string& file, bool& enabled)
{
	Actions& actions = server.actions();
	CreatureEvents& creatureEvents = server.creatureEvents();
	GlobalEvents& globalEvents = server.globalEvents();
	MoveEvents& moveEvents = server.moveEvents();
	Spells& spells = server.spells();
	TalkActions& talkActions = server.talkActions();
	Weapons& weapons = server.weapons();

	enabled = false;
	std::string modPath = getFilePath(FileType::MOD, file);

	xmlDocPtr doc = xmlParseFile(modPath.c_str());
	if(!doc)
	{
		LOGe("[ScriptingManager::loadFromXml] Cannot load mod " << modPath << ": " << getLastXMLError());
		return false;
	}

	int32_t intValue;
	std::string strValue;

	xmlNodePtr p, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"mod"))
	{
		LOGe("[ScriptingManager::loadFromXml] Malformed mod " << modPath << ": " << getLastXMLError());

		xmlFreeDoc(doc);
		return false;
	}

	if(!readXMLString(root, "name", strValue))
	{
		LOGe("[ScriptingManager::loadFromXml] Empty name in mod " << modPath);
		xmlFreeDoc(doc);
		return false;
	}

	ModBlock mod;
	mod.enabled = false;
	if(readXMLString(root, "enabled", strValue) && booleanString(strValue))
		mod.enabled = true;

	mod.file = file;
	mod.name = strValue;
	if(readXMLString(root, "author", strValue))
		mod.author = strValue;
	if(readXMLString(root, "version", strValue))
		mod.version = strValue;
	if(readXMLString(root, "contact", strValue))
		mod.contact = strValue;

	if(mod.enabled)
	{
		std::string scriptsPath = getFilePath(FileType::MOD, "scripts/");
		p = root->children;
		while(p)
		{
			if(!xmlStrcmp(p->name, (const xmlChar*)"quest"))
				Quests::getInstance()->parseQuestNode(p, modsLoaded);
			else if(!xmlStrcmp(p->name, (const xmlChar*)"outfit"))
				Outfits::getInstance()->parseOutfitNode(p);
			else if(!xmlStrcmp(p->name, (const xmlChar*)"vocation"))
				Vocations::getInstance()->parseVocationNode(p); //duplicates checking is dangerous, shouldn't be performed
			else if(!xmlStrcmp(p->name, (const xmlChar*)"group"))
				Groups::getInstance()->parseGroupNode(p); //duplicates checking is dangerous, shouldn't be performed
			else if(!xmlStrcmp(p->name, (const xmlChar*)"raid"))
				Raids::getInstance()->parseRaidNode(p, modsLoaded, FileType::MOD);
			else if(!xmlStrcmp(p->name, (const xmlChar*)"spawn"))
				Spawns::getInstance()->parseSpawnNode(p, modsLoaded);
			else if(!xmlStrcmp(p->name, (const xmlChar*)"channel"))
				server.chat().parseChannelNode(p); //TODO: duplicates (channel destructor needs sending self close to users)
			else if(!xmlStrcmp(p->name, (const xmlChar*)"monster"))
			{
				std::string file, name;
				if(readXMLString(p, "file", file) && readXMLString(p, "name", name))
				{
					file = getFilePath(FileType::MOD, "monster/" + file);
					server.monsters().loadMonster(file, name, true);
				}
			}
			else if(!xmlStrcmp(p->name, (const xmlChar*)"item"))
			{
				// TODO looks scary... refactor!
				if(readXMLInteger(p, "id", intValue))
					server.items().loadKindFromXmlNode(p, intValue, file); //duplicates checking isn't necessary here
			}
			if(!xmlStrcmp(p->name, (const xmlChar*)"description") || !xmlStrcmp(p->name, (const xmlChar*)"info"))
			{
				if(parseXMLContentString(p->children, strValue))
				{
					replaceString(strValue, "\t", "");
					mod.description = strValue;
				}
			}
			else if(!xmlStrcmp(p->name, (const xmlChar*)"lib") || !xmlStrcmp(p->name, (const xmlChar*)"config"))
			{
				if(!readXMLString(p, "name", strValue))
				{
					LOGe("[ScriptingManager::loadFromXml] Lib without name in mod " << strValue);
					p = p->next;
					continue;
				}

				toLowerCaseString(strValue);
				std::string strLib;
				if(parseXMLContentString(p->children, strLib))
				{
					LibMap::iterator it = libMap.find(strValue);
					if(it == libMap.end())
					{
						LibBlock lb;
						lb.first = file;
						lb.second = strLib;

						libMap[strValue] = lb;
					}
					else
						LOGw("[ScriptingManager::loadFromXml] Duplicated lib in mod "
							<< strValue << ", previously declared in " << it->second.first);
				}
			}
			else if(!actions.parseEventNode(p, scriptsPath, modsLoaded))
			{
				if(!talkActions.parseEventNode(p, scriptsPath, modsLoaded))
				{
					if(!moveEvents.parseEventNode(p, scriptsPath, modsLoaded))
					{
						if(!creatureEvents.parseEventNode(p, scriptsPath, modsLoaded))
						{
							if(!globalEvents.parseEventNode(p, scriptsPath, modsLoaded))
							{
								if(!spells.parseEventNode(p, scriptsPath, modsLoaded))
									weapons.parseEventNode(p, scriptsPath, modsLoaded);
							}
						}
					}
				}
			}

			p = p->next;
		}
	}

	enabled = mod.enabled;
	modMap[mod.name] = mod;
	xmlFreeDoc(doc);
	return true;
}
