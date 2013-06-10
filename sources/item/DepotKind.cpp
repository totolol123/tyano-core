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
#include "item/DepotKind.hpp"

#include "item/Class.hpp"


namespace ts { namespace item {

const std::string DepotKind::ATTRIBUTE_DEPOTID("depotid");


DepotKind::DepotKind(const ClassPC& clazz, KindId id)
	: ContainerKind(std::static_pointer_cast<const ContainerKind::ClassT>(clazz), id)
{}




template<>
ClassDescriptor<DepotKind>::DynamicAttributesP ClassDescriptor<DepotKind>::getDynamicAttributes() {
	using attributes::Type;

	auto attributes = Base::getDynamicAttributes();
	attributes->emplace(DepotKind::ATTRIBUTE_DEPOTID, Type::INTEGER);

	return attributes;
}


template<>
std::string ClassDescriptor<DepotKind>::getId() {
	return "depot";
}


template<>
std::string ClassDescriptor<DepotKind>::getName() {
	return "Depot";
}

}} // namespace ts::item
