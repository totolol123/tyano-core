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

#include "cylinder.h"
#include "position.h"
#include "item.h"

VirtualCylinder VirtualCylinder::virtualCylinder;


ReturnValue VirtualCylinder::__queryAdd(int32_t index, const Item* item, uint32_t count,
	uint32_t flags) const {return RET_NOTPOSSIBLE;}
ReturnValue VirtualCylinder::__queryMaxCount(int32_t index, const Item* item, uint32_t count,
	uint32_t& maxQueryCount, uint32_t flags) const {return RET_NOTPOSSIBLE;}
ReturnValue VirtualCylinder::__queryRemove(const Item* item, uint32_t count,
	uint32_t flags) const {return (item->getParent() == this ? RET_NOERROR : RET_NOTPOSSIBLE);}
Cylinder* VirtualCylinder::__queryDestination(int32_t& index, const Item* item, Item** destItem,
	uint32_t& flags) {return nullptr;}

Position VirtualCylinder::getPosition() const {return Position();}
