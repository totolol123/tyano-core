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

#ifndef _DEPOT_H
#define _DEPOT_H

#include "container.h"
#include "item/DepotKind.hpp"


class Depot : public Container {

private:

	typedef ts::item::DepotKind    DepotKind;
	typedef ts::item::DepotKindPC  DepotKindPC;


public:

	Depot(const DepotKindPC& kind);

	const DepotKind& getKind() const;






	public:

		virtual ~Depot() {}

		virtual Depot* getDepot() {return this;}
		virtual const Depot* getDepot() const {return this;}

		//serialization
		virtual Attr_ReadValue readAttr(AttrTypes_t attr, PropStream& propStream);

		uint32_t getDepotId() const;

		void setMaxDepotLimit(uint32_t count) {depotLimit = count;}

		virtual ReturnValue __queryAdd(int32_t index, const Thing* thing, uint32_t count,
			uint32_t flags) const;

		virtual ReturnValue __queryMaxCount(int32_t index, const Thing* thing, uint32_t count,
			uint32_t& maxQueryCount, uint32_t flags) const;

		virtual void postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
			int32_t index, cylinderlink_t link = LINK_OWNER);
		virtual void postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
			int32_t index, bool isCompleteRemoval, cylinderlink_t link = LINK_OWNER);

		//overrides
		virtual bool canRemove() const {return false;}

	private:
		uint32_t depotLimit;
};

#endif // _DEPOT_H
