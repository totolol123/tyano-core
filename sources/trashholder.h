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

#ifndef _TRASHHOLDER_H
#define _TRASHHOLDER_H

#include "cylinder.h"
#include "item.h"


class TrashHolder : public Item, public Cylinder
{
	public:

		static ClassAttributesP   getClassAttributes();
		static const std::string& getClassName();

		TrashHolder(const ItemKindPC& kind, MagicEffect_t _effect = MAGIC_EFFECT_NONE): Item(kind), effect(_effect) {}
		virtual ~TrashHolder() {}

		virtual TrashHolder* getTrashHolder() {return this;}
		virtual const TrashHolder* getTrashHolder() const {return this;}

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

		virtual ReturnValue __queryAdd(int32_t index, const Thing* thing, uint32_t count,
			uint32_t flags) const {return RET_NOERROR;}
		virtual ReturnValue __queryMaxCount(int32_t index, const Thing* thing, uint32_t count,
			uint32_t& maxQueryCount, uint32_t flags) const {return RET_NOERROR;}
		virtual ReturnValue __queryRemove(const Thing* thing, uint32_t count,
			uint32_t flags) const {return RET_NOTPOSSIBLE;}
		virtual Cylinder* __queryDestination(int32_t& index, const Thing* thing, Item** destItem,
			uint32_t& flags) {return this;}

		virtual void __addThing(Creature* actor, Thing* thing) {return __addThing(actor, 0, thing);}
		virtual void __addThing(Creature* actor, int32_t index, Thing* thing);

		virtual void __updateThing(Thing* thing, uint16_t itemId, uint32_t count) {}
		virtual void __replaceThing(uint32_t index, Thing* thing) {}

		virtual void __removeThing(Thing* thing, uint32_t count) {}

		virtual void postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
			int32_t index, cylinderlink_t link = LINK_OWNER);
		virtual void postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
			int32_t index, bool isCompleteRemoval, cylinderlink_t link = LINK_OWNER);

		MagicEffect_t getEffect() const {return effect;}

	private:
		MagicEffect_t effect;
};

#endif // _TRASHHOLDER_H
