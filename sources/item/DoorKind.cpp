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
#include "item/DoorKind.hpp"

#include "item/Class.hpp"


namespace ts { namespace item {

const std::string DoorKind::ATTRIBUTE_DOORID("doorid");


DoorKind::DoorKind(const ClassPC& clazz, KindId id)
	: Kind(std::static_pointer_cast<const Kind::ClassT>(clazz), id)
{}




template<>
ClassDescriptor<DoorKind>::DynamicAttributesP ClassDescriptor<DoorKind>::getDynamicAttributes() {
	using attributes::Type;

	auto attributes = Base::getDynamicAttributes();
	attributes->emplace(DoorKind::ATTRIBUTE_DOORID, Type::INTEGER);

	return attributes;
}


template<>
std::string ClassDescriptor<DoorKind>::getId() {
	return "door";
}


template<>
std::string ClassDescriptor<DoorKind>::getName() {
	return "Door";
}

}} // namespace ts::item
