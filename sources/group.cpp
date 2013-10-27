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

#include "group.h"
#include "tools.h"


LOGGER_DEFINITION(Groups);

Group Groups::defGroup = Group();


void Groups::clear()
{
	for(GroupsMap::iterator it = groupsMap.begin(); it != groupsMap.end(); ++it)
		delete it->second;

	groupsMap.clear();
}

bool Groups::reload()
{
	clear();
	return loadFromXml();
}

bool Groups::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FileType::XML, "groups.xml").c_str());
	if(!doc)
	{
		LOGe("[Groups::loadFromXml] Cannot load groups file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr p, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"groups"))
	{
		LOGe("[Groups::loadFromXml] Malformed groups file.");
		xmlFreeDoc(doc);
		return false;
	}

	p = root->children;
	while(p)
	{
		parseGroupNode(p);
		p = p->next;
	}

	xmlFreeDoc(doc);
	return true;
}

bool Groups::parseGroupNode(xmlNodePtr p)
{
	if(xmlStrcmp(p->name, (const xmlChar*)"group"))
		return false;

	int32_t intValue;
	if(!readXMLInteger(p, "id", intValue))
	{
		LOGe("[Groups::parseGroupNode] Missing group id.");
		return false;
	}

	std::string strValue;
	int64_t int64Value;

	Group* group = new Group(intValue);
	if(readXMLString(p, "name", strValue))
	{
		group->setFullName(strValue);
		group->setName(asLowerCaseString(strValue));
	}

	if(readXMLInteger64(p, "flags", int64Value))
		group->setFlags(int64Value);

	if(readXMLInteger64(p, "customFlags", int64Value))
		group->setCustomFlags(int64Value);

	if(readXMLInteger(p, "access", intValue))
		group->setAccess(intValue);

	if(readXMLInteger(p, "ghostAccess", intValue))
		group->setGhostAccess(intValue);
	else
		group->setGhostAccess(group->getAccess());

	if(readXMLInteger(p, "violationReasons", intValue))
		group->setViolationReasons(intValue);

	if(readXMLInteger(p, "nameViolationFlags", intValue))
		group->setNameViolationFlags(intValue);

	if(readXMLInteger(p, "statementViolationFlags", intValue))
		group->setStatementViolationFlags(intValue);

	if(readXMLInteger(p, "depotLimit", intValue))
		group->setDepotLimit(intValue);

	if(readXMLInteger(p, "maxVips", intValue))
		group->setMaxVips(intValue);

	if(readXMLInteger(p, "outfit", intValue))
		group->setOutfit(intValue);

	groupsMap[group->getId()] = group;
	return true;
}

Group* Groups::getGroup(uint32_t groupId)
{
	GroupsMap::iterator it = groupsMap.find(groupId);
	if(it != groupsMap.end())
		return it->second;

	LOGe("[Groups::getGroup] Group " << groupId << " not found.");
	return &defGroup;
}

int32_t Groups::getGroupId(const std::string& name)
{
	for(GroupsMap::iterator it = groupsMap.begin(); it != groupsMap.end(); ++it)
	{
		if(!strcasecmp(it->second->getName().c_str(), name.c_str()))
			return it->first;
	}

	return -1;
}


SpeakClasses Group::getSpeakClass() const {
	if (hasFlag(PlayerFlag_CanTalkRedChannel)) {
		return SPEAK_CHANNEL_RN;
	}
	if (hasFlag(PlayerFlag_TalkOrangeHelpChannel)) {
		return SPEAK_CHANNEL_O;
	}

	return SPEAK_CHANNEL_Y;
}


uint32_t Group::getDepotLimit() const
{
	if(m_depotLimit > 0)
		return m_depotLimit;

	return 2000;
}

uint32_t Group::getMaxVips() const
{
	if(m_maxVips > 0)
		return m_maxVips;

	return 100;
}
