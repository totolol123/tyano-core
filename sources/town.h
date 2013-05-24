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

#ifndef _TOWN_H
#define _TOWN_H

#include "position.h"


class Town;

typedef std::unique_ptr<Town>  TownP;


class Town
{
	public:

		static TownP create(uint32_t id) { return TownP(new Town(id)); }

		Position getPosition() const {return position;}
		std::string getName() const {return name;}

		void setPosition(const Position& pos) {position = pos;}
		void setName(const std::string& townName) {name = townName;}

		uint32_t getID() const {return id;}

	private:
		Town(uint32_t townId) {id = townId;}

		uint32_t id;
		std::string name;
		Position position;
};


class Towns {

private:

	typedef std::map<uint32_t,TownP> TownMap;


public:

	Town* addTown (TownP&& town);
	Town* getTown (const std::string& name) const;
	Town* getTown (uint32_t id) const;

private:

	TownMap _towns;

};

#endif // _TOWN_H
