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

#ifndef _TELEPORTER_H
#define _TELEPORTER_H

#include "item.h"
#include "position.h"

class Creature;


class Teleporter : public Item {

public:

	static ClassAttributesP   getClassAttributes ();
	static const std::string& getClassName();

	Teleporter          (const ItemKindPC& kind);
	virtual ~Teleporter ();

	virtual Teleporter*       asTeleporter               ();
	virtual const Teleporter* asTeleporter               () const;
	        Tile*             getCreatureDestinationTile (const Creature& creature) const;
	        Tile*             getItemDestinationTile     (const Item& item) const;
	        const Position&   getDestination             () const;
	virtual Attr_ReadValue    readAttr                   (AttrTypes_t attribute, PropStream& stream);
	virtual void              serializeAttr              (PropWriteStream& stream) const;
	        void              setDestination             (const Position& destination);


private:

	LOGGER_DECLARATION;

	Position _destination;

};

#endif // _TELEPORTER_H
