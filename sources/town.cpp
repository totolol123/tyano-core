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
#include "town.h"


Town* Towns::addTown(TownP&& town) {
	assert(town);

	if (_towns.find(town->getID()) != _towns.end()) {
		return nullptr;
	}

	Town* townPointer = town.get();

	_towns[town->getID()] = std::move(town);

	return townPointer;
}


Town* Towns::getTown(const std::string& name) const {
	for (auto it = _towns.cbegin(); it != _towns.cend(); ++it) {
		if (boost::iequals(it->second->getName(), name)) {
			return it->second.get();
		}
	}

	return nullptr;
}


Town* Towns::getTown(uint32_t id) const {
	auto it = _towns.find(id);
	if (it == _towns.end()) {
		return nullptr;
	}

	return it->second.get();
}
