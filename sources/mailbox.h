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

#ifndef _MAILBOX_H
#define _MAILBOX_H

#include "cylinder.h"
#include "item.h"


class Mailbox : public Item, public Cylinder
{
	public:

		static ClassAttributesP   getClassAttributes();
		static const std::string& getClassName();

		Mailbox(const ItemKindPC& kind): Item(kind) {}
		virtual ~Mailbox() {}

		virtual Mailbox* getMailbox() {return this;}
		virtual const Mailbox* getMailbox() const {return this;}

		//cylinder implementations
		virtual Cylinder* getParent();
		virtual const Cylinder* getParent() const;
		virtual bool isRemoved() const;
		virtual Position getPosition() const;
		virtual Tile* getTile();
		virtual const Tile* getTile() const;
		virtual Item* getItem();
		virtual const Item* getItem() const;
		virtual Creature* getCreature();
		virtual const Creature* getCreature() const;

		virtual ReturnValue __queryAdd(int32_t index, const Item* item, uint32_t count,
			uint32_t flags) const;
		virtual ReturnValue __queryMaxCount(int32_t index, const Item* item, uint32_t count,
			uint32_t& maxQueryCount, uint32_t flags) const;
		virtual ReturnValue __queryRemove(const Item* item, uint32_t count, uint32_t flags) const {return RET_NOTPOSSIBLE;}
		virtual Cylinder* __queryDestination(int32_t& index, const Item* item, Item** destItem,
			uint32_t& flags) {return this;}

		virtual void __addThing(Creature* actor, Item* item) {__addThing(actor, 0, item);}
		virtual void __addThing(Creature* actor, int32_t index, Item* item);

		virtual void __updateThing(Item* item, uint16_t itemId, uint32_t count) {}
		virtual void __replaceThing(uint32_t index, Item* item) {}

		virtual void __removeThing(Item* item, uint32_t count) {}

		virtual void postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
			int32_t index, cylinderlink_t link = LINK_OWNER)
			{if(getParent()) getParent()->postAddNotification(actor, thing,
				oldParent, index, LINK_PARENT);}
		virtual void postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
			int32_t index, bool isCompleteRemoval, cylinderlink_t link = LINK_OWNER)
			{if(getParent()) getParent()->postRemoveNotification(actor, thing,
				newParent, index, isCompleteRemoval, LINK_PARENT);}

		bool canSend(const Item* item) const {return (item->getId() == ITEM_PARCEL || item->getId() == ITEM_LETTER);}
		bool sendItem(Creature* actor, Item* item);

		bool getDepotId(const std::string& townString, uint32_t& depotId);
		bool getRecipient(Item* item, std::string& name, uint32_t& depotId);


	private:

		LOGGER_DECLARATION;

};

#endif // _MAILBOX_H
