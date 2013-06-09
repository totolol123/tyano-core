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
#include "housetile.h"

#include "creature.h"
#include "house.h"
#include "game.h"
#include "configmanager.h"
#include "player.h"
#include "server.h"


LOGGER_DEFINITION(HouseTile);


HouseTile::HouseTile(int32_t x, int32_t y, int32_t z, House* _house):
	DynamicTile(x, y, z)
{
	house = _house;
	setFlag(TILESTATE_HOUSE);
}

void HouseTile::__addThing(Creature* actor, int32_t index, Thing* thing)
{
	Tile::__addThing(actor, index, thing);
	if(!thing->getParent())
		return;

	if(Item* item = thing->getItem())
		updateHouse(item);
}

void HouseTile::__internalAddThing(uint32_t index, Thing* thing)
{
	Tile::__internalAddThing(index, thing);
	if(!thing->getParent())
		return;

	if(Item* item = thing->getItem())
		updateHouse(item);
}

void HouseTile::updateHouse(Item* item)
{
	if(item->getTile() != this)
		return;

	Door* door = item->getDoor();
	if(door && door->getDoorId())
		house->addDoor(door);
	else if(BedItem* bed = item->getBed())
		house->addBed(bed);
}

ReturnValue HouseTile::__queryAdd(int32_t index, const Thing* thing, uint32_t count, uint32_t flags) const
{
	if(const Creature* creature = thing->getCreature())
	{
		if(const Player* player = creature->getPlayer())
		{
			if(!house->isInvited(player))
				return RET_PLAYERISNOTINVITED;
		}
		else
			return RET_NOTPOSSIBLE;
	}
	else if(thing->getItem())
	{
		const uint32_t itemLimit = server.configManager().getNumber(ConfigManager::ITEMLIMIT_HOUSETILE);
		if(itemLimit && getThingCount() > itemLimit)
			return RET_TILEISFULL;
	}

	return Tile::__queryAdd(index, thing, count, flags);
}

Cylinder* HouseTile::__queryDestination(int32_t& index, const Thing* thing, Item** destItem, uint32_t& flags)
{
	if(const Creature* creature = thing->getCreature())
	{
		if(const Player* player = creature->getPlayer())
		{
			if(!house->isInvited(player) && !player->hasFlag(PlayerFlag_CanEditHouses))
			{
				Tile* destTile = server.game().getTile(house->getEntry());
				if(!destTile)
				{
					LOGe("[HouseTile::__queryDestination] Tile at house entry position for house: " << house->getName() << " (" << house->getId() << ") does not exist.");
					destTile = server.game().getTile(player->getMasterPosition());
				}

				index = -1;
				*destItem = nullptr;
				return destTile;
			}
		}
	}

	return Tile::__queryDestination(index, thing, destItem, flags);
}
