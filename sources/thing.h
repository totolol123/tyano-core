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

#ifndef _THING_H
#define _THING_H

#include "deprecated.h"

class Creature;
class Cylinder;
class Item;
class Position;
class Tile;

class Thing : public ReferenceCounted {
	protected:
		Thing(): parent(nullptr) {}

	public:
		virtual ~Thing() {}

		virtual std::string getDescription(int32_t lookDistance) const = 0;

		Cylinder* getParent() {return parent;}
		const Cylinder* getParent() const {return parent;}

		virtual void setParent(Cylinder* cylinder) {
			parent = cylinder;
		}

		Cylinder* getTopParent();
		const Cylinder* getTopParent() const;

		virtual Tile* getTile();
		virtual const Tile* getTile() const;

		virtual Position getPosition() const;
		virtual int32_t getThrowRange() const = 0;
		virtual bool isPushable() const = 0;

		virtual Item* getItem() {return nullptr;}
		virtual const Item* getItem() const {return nullptr;}
		virtual Creature* getCreature() {return nullptr;}
		virtual const Creature* getCreature() const {return nullptr;}

		virtual bool isRemoved() const;

	private:
		Cylinder* parent;
};

#endif // _THING_H
