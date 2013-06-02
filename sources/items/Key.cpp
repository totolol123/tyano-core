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
#include "items/Key.hpp"

using namespace ts;
using namespace ts::items;


Key::ClassAttributesP Key::getClassAttributes() {
	return Item::getClassAttributes();
}


const std::string& Key::getClassId() {
	static const std::string id("key");
	return id;
}


const std::string& Key::getClassName() {
	static const std::string name("Key");
	return name;
}
