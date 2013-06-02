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

#ifndef _ITEMS_KEY_HPP
#define _ITEMS_KEY_HPP

#include "item.h"


namespace ts {
namespace items {

	class Key : public Item {

	public:

		static ClassAttributesP   getClassAttributes();
		static const std::string& getClassId();
		static const std::string& getClassName();

	};

} // namespace items
} // namespace ts

#endif // _ITEMS_KEY_HPP
