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
#include "mailbox.h"

#include "player.h"
#include "iologindata.h"

#include "depot.h"
#include "town.h"

#include "configmanager.h"
#include "game.h"
#include "server.h"
#include "tools.h"


LOGGER_DEFINITION(Mailbox);



Mailbox::ClassAttributesP Mailbox::getClassAttributes() {
	return Item::getClassAttributes();
}


const std::string& Mailbox::getClassName() {
	static const std::string name("Mailbox");
	return name;
}


ReturnValue Mailbox::__queryAdd(int32_t index, const Item* item, uint32_t count,
	uint32_t flags) const
{
	if(canSend(item))
		return RET_NOERROR;

	return RET_NOTPOSSIBLE;
}

ReturnValue Mailbox::__queryMaxCount(int32_t index, const Item* item, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	maxQueryCount = std::max((uint32_t)1, count);
	return RET_NOERROR;
}

void Mailbox::__addThing(Creature* actor, int32_t index, Item* item)
{
	if(canSend(item))
		sendItem(actor, item);
}

bool Mailbox::sendItem(Creature* actor, Item* item)
{
	uint32_t depotId = 0;
	std::string name;
	if(!getRecipient(item, name, depotId) || name.empty() || !depotId)
		return false;

	return IOLoginData::getInstance()->playerMail(actor, name, depotId, item);
}

bool Mailbox::getDepotId(const std::string& townString, uint32_t& depotId)
{
	Town* town = server.towns().getTown(townString);
	if(!town)
		return false;

	IntegerVec disabledTowns = vectorAtoi(explodeString(server.configManager().getString(ConfigManager::MAILBOX_DISABLED_TOWNS), ","));
	if(disabledTowns[0] != -1)
	{	
		for(IntegerVec::iterator it = disabledTowns.begin(); it != disabledTowns.end(); ++it)
		{
			if(town->getID() == uint32_t(*it))
				return false;
		}
	}

	depotId = town->getID();
	return true;
}

bool Mailbox::getRecipient(Item* item, std::string& name, uint32_t& depotId)
{
	if(!item)
		return false;

	if(item->getId() == ITEM_PARCEL) /**We need to get the text from the label incase its a parcel**/
	{
		if(Container* parcel = item->getContainer())
		{
			for(ItemList::const_iterator cit = parcel->getItems(); cit != parcel->getEnd(); ++cit)
			{
				if((*cit)->getId() == ITEM_LABEL && !(*cit)->getText().empty())
				{
					item = (*cit).get();
					break;
				}
			}
		}
	}
	else if(item->getId() != ITEM_LETTER) /**The item is somehow not a parcel or letter**/
	{
		LOGe("[Mailbox::getReciver] Trying to get receiver from unkown item with id: " << item->getId() << "!");
		return false;
	}

	if(!item || item->getText().empty()) /**No label/letter found or its empty.**/
		return false;

	std::istringstream iss(item->getText(), std::istringstream::in);
	uint32_t curLine = 0;

	std::string tmp, townString;
	while(getline(iss, tmp, '\n') && curLine < 2)
	{
		if(curLine == 0)
			name = tmp;
		else if(curLine == 1)
			townString = tmp;

		++curLine;
	}

	trimString(name);
	if(townString.empty())
		return false;

	trimString(townString);
	return getDepotId(townString, depotId);
}


Cylinder* Mailbox::getParent() {return Item::getParent();}
const Cylinder* Mailbox::getParent() const {return Item::getParent();}
bool Mailbox::isRemoved() const {return Item::isRemoved();}
Position Mailbox::getPosition() const {return Item::getPosition();}
Tile* Mailbox::getTile() {return Item::getTile();}
const Tile* Mailbox::getTile() const {return Item::getTile();}
Item* Mailbox::getItem() {return this;}
const Item* Mailbox::getItem() const {return this;}
Creature* Mailbox::getCreature() {return nullptr;}
const Creature* Mailbox::getCreature() const {return nullptr;}
