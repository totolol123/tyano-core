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
#include "item/ContainerKind.hpp"

#include "item/Class.hpp"


namespace ts { namespace item {

LOGGER_DEFINITION(ts::item::ContainerKind);


ContainerKind::ContainerKind(const ClassPC& clazz, KindId id)
	: Kind(std::static_pointer_cast<const Kind::ClassT>(clazz), id),
	  _capacity(0)
{}


bool ContainerKind::assemble() {
	if (!Kind::assemble()) {
		return false;
	}

	if (_capacity == 0) {
		LOGe("Container has no capacity set.");
		return false;
	}

	return true;
}


bool ContainerKind::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
	if (Kind::setParameter(name, value, deprecatedNode)) {
		return true;
	}

	if (name == "capacity" || name == "containersize") {
		_setParameter(name, value, _capacity, "capacity");
	}
	else {
		return false;
	}

	return true;
}




template<>
ClassDescriptor<ContainerKind>::DynamicAttributesP ClassDescriptor<ContainerKind>::getDynamicAttributes() {
	return Base::getDynamicAttributes();
}


template<>
std::string ClassDescriptor<ContainerKind>::getId() {
	return "container";
}


template<>
std::string ClassDescriptor<ContainerKind>::getName() {
	return "Container";
}

}} // namespace ts::item
