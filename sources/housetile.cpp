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


ReturnValue HouseTile::testAddCreature(const Creature& creature, uint32_t flags) const {
	auto result = DynamicTile::testAddCreature(creature, flags);
	if (result != RET_NOERROR) {
		return result;
	}

	if (auto player = creature.getPlayer()) {
		if (!house->isInvited(player)) {
			return RET_PLAYERISNOTINVITED;
		}

		return RET_NOERROR;
	}

	return RET_NOTPOSSIBLE;
}










HouseTile::HouseTile(int32_t x, int32_t y, int32_t z, House* _house):
	DynamicTile(x, y, z)
{
	house = _house;
	setFlag(TILESTATE_HOUSE);
}

void HouseTile::__addThing(Creature* actor, int32_t index, Item* item)
{
	Tile::__addThing(actor, index, item);
	if(!item->getParent())
		return;

	updateHouse(item);
}

void HouseTile::__internalAddThing(uint32_t index, Item* item)
{
	Tile::__internalAddThing(index, item);
	if(!item->getParent())
		return;

	updateHouse(item);
}

void HouseTile::updateHouse(Item* item)
{
	if(item->getTile() != this)
		return;

	Door* door = item->getDoor();
	if(door) {
		if (door->getDoorId()) {
			house->addDoor(door);
		}
		else {
			LOGe("House entrance at " << door->getPosition() << " has no door ID and won't work.");
		}
	}
	else if(BedItem* bed = item->getBed())
		house->addBed(bed);
}

ReturnValue HouseTile::__queryAdd(int32_t index, const Item* item, uint32_t count, uint32_t flags) const
{
	const uint32_t itemLimit = server.configManager().getNumber(ConfigManager::ITEMLIMIT_HOUSETILE);
	if(itemLimit && getThingCount() > itemLimit)
		return RET_TILEISFULL;

	return Tile::__queryAdd(index, item, count, flags);
}
