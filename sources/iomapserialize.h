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

#ifndef _IOMAPSERIALIZE_H
#define _IOMAPSERIALIZE_H

class Container;
class Cylinder;
class Database;
class DBResult;
class House;
class Item;
class Map;
class PropStream;
class PropWriteStream;
class Tile;

typedef std::list<std::pair<Container*,int32_t>>                         ContainerStackList;
typedef std::map<int32_t, std::pair<boost::intrusive_ptr<Item>,int32_t>> ItemMap;


class IOMapSerialize
{
	public:
		static IOMapSerialize* getInstance()
		{
			static IOMapSerialize instance;
			return &instance;
		}

		bool loadMap(Map* map);
		bool saveMap(const Map* map);

		bool updateAuctions();

		bool loadHouses();
		bool updateHouses();
		bool saveHouses();

		bool saveHouse(Database& db, House* house);

	protected:
		IOMapSerialize() {}

		// Relational storage uses a row for each item/tile
		bool loadMapRelational(Map* map);
		bool saveMapRelational(const Map* map);
	
		// Binary storage uses a giant BLOB field for storing everything
		bool loadMapBinary(Map* map);
		bool saveMapBinary(const Map* map);

		bool loadItems(Database& db, DBResult* result, Cylinder* parent, bool depotTransfer);
		bool saveItems(Database& db, uint32_t& tileId, uint32_t houseId, const Tile* tile);

		bool loadContainer(PropStream& propStream, Container* container);
		bool loadItem(PropStream& propStream, Cylinder* parent, bool depotTransfer);

		bool saveTile(PropWriteStream& stream, const Tile* tile);
		bool saveItem(PropWriteStream& stream, const Item* item);


		LOGGER_DECLARATION;
};

#endif // _IOMAPSERIALIZE_H
