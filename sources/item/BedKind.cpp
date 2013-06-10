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
#include "item/BedKind.hpp"

#include "item/Class.hpp"
#include "position.h"
#include "tools.h"


namespace ts { namespace item {

LOGGER_DEFINITION(ts::item::BedKind);

const std::string BedKind::ATTRIBUTE_SLEEPSTART("sleepstart");


BedKind::BedKind(const ClassPC& clazz, KindId id)
	: Kind(std::static_pointer_cast<const Kind::ClassT>(clazz), id),
	  _counterpartDirection(Direction::NORTH),
	  _femaleOccupiedBedId(0),
	  _freeBedId(0),
	  _maleOccupiedBedId(0)
{}


bool BedKind::assemble() {
	if (!Kind::assemble()) {
		return false;
	}

	if (_femaleOccupiedBedId == 0 && _maleOccupiedBedId == 0 && _freeBedId == 0) {
		LOGe("Bed has none of 'femaleOccupiedBedId', 'maleOccupiedBedId' or 'freeBedId' set.");
		return false;
	}

	if ((_femaleOccupiedBedId != 0) != (_maleOccupiedBedId != 0)) {
		LOGw("Bed should have both 'femaleOccupiedBedId' and 'maleOccupiedBedId' set, not just one of them. Will do that for you.");
		if (_femaleOccupiedBedId != 0) {
			_maleOccupiedBedId = _femaleOccupiedBedId;
		}
		else {
			_femaleOccupiedBedId = _maleOccupiedBedId;
		}
	}

	return true;
}


bool BedKind::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
	if (Kind::setParameter(name, value, deprecatedNode)) {
		return true;
	}

	if (name == "counterpartDirection" || name == "partnerdirection") {
		_checkDeprecatedParameter(name, "counterpartDirection");

		Direction direction;
		if (getDirection(value, direction)) {
			_counterpartDirection = direction;
		}
		else {
			if (getDeprecatedDirection(value, direction)) {
				LOGw("Attribute '" << name << "' uses deprecated value '" << value << "'. Valid values are " << getDirectionNames());
				_counterpartDirection = direction;
			}
			else {
				LOGe("Attribute '" << name << "' must be a direction (" << getDirectionNames() << ")");
			}
		}
	}
	else if (name == "femaleOccupiedBedId" || name == "femaletransformto") {
		_setParameter(name, value, _femaleOccupiedBedId, "femaleOccupiedBedId");
	}
	else if (name == "freeBedId" || name == "transformto") {
		_setParameter(name, value, _freeBedId, "freeBedId");
	}
	else if (name == "maleOccupiedBedId" || name == "maletransformto") {
		_setParameter(name, value, _maleOccupiedBedId, "maleOccupiedBedId");
	}
	else {
		return false;
	}

	return true;
}




template<>
ClassDescriptor<BedKind>::DynamicAttributesP ClassDescriptor<BedKind>::getDynamicAttributes() {
	using attributes::Type;

	auto attributes = Base::getDynamicAttributes();
	attributes->emplace(BedKind::ATTRIBUTE_SLEEPSTART, Type::INTEGER);

	return attributes;
}


template<>
std::string ClassDescriptor<BedKind>::getId() {
	return "bed";
}


template<>
std::string ClassDescriptor<BedKind>::getName() {
	return "Bed";
}

}} // namespace ts::item
