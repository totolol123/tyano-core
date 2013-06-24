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

#ifndef _HOUSETILE_H
#define _HOUSETILE_H

#include "tile.h"

class House;


class HouseTile : public DynamicTile {

public:

	virtual ReturnValue testAddCreature (const Creature& creature, uint32_t flags = 0) const;




		HouseTile(int32_t x, int32_t y, int32_t z, House* _house);
		virtual ~HouseTile() {}

		//cylinder implementations
		virtual ReturnValue __queryAdd(int32_t index, const Item* thing, uint32_t count,
			uint32_t flags) const;

		virtual void __addThing(Creature* actor, int32_t index, Item* item);
		virtual void __internalAddThing(uint32_t index, Item* item);

		House* getHouse() {return house;}

	private:
		void updateHouse(Item* item);

		LOGGER_DECLARATION;

		House* house;
};

#endif // _HOUSETILE_H
