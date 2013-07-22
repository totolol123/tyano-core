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
#include "trashholder.h"

#include "game.h"
#include "player.h"
#include "server.h"
#include "spells.h"



TrashHolder::ClassAttributesP TrashHolder::getClassAttributes() {
	return Item::getClassAttributes();
}


const std::string& TrashHolder::getClassName() {
	static const std::string name("Trash Holder");
	return name;
}


void TrashHolder::__addThing(Creature* actor, int32_t index, Item* item)
{
	if(item == this || !item->isMoveable())
		return;

	if(getTile()->isSwimmingPool())
	{
		if(item->getId() == ITEM_WATERBALL_SPLASH)
			return;

		if(item->getId() == ITEM_WATERBALL)
		{
			server.game().transformItem(item, ITEM_WATERBALL_SPLASH);
			return;
		}
	}

	server.game().internalRemoveItem(actor, item);
	if(effect != MAGIC_EFFECT_NONE)
		server.game().addMagicEffect(getPosition(), effect);
}

void TrashHolder::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, cylinderlink_t link /*= LINK_OWNER*/)
{
	if(getParent())
		getParent()->postAddNotification(actor, thing, oldParent, index, LINK_PARENT);
}

void TrashHolder::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, cylinderlink_t link /*= LINK_OWNER*/)
{
	if(getParent())
		getParent()->postRemoveNotification(actor, thing, newParent,
			index, isCompleteRemoval, LINK_PARENT);
}


Cylinder* TrashHolder::getParent() {return Item::getParent();}
const Cylinder* TrashHolder::getParent() const {return Item::getParent();}
bool TrashHolder::isRemoved() const {return Item::isRemoved();}
Position TrashHolder::getPosition() const {return Item::getPosition();}
Tile* TrashHolder::getTile() {return Item::getTile();}
const Tile* TrashHolder::getTile() const {return Item::getTile();}
Item* TrashHolder::getItem() {return this;}
const Item* TrashHolder::getItem() const {return this;}
Creature* TrashHolder::getCreature() {return nullptr;}
const Creature* TrashHolder::getCreature() const {return nullptr;}
