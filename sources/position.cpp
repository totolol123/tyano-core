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
#include "position.h"

#include "const.h"


const uint16_t Position::MAX_X = 60000;
const uint16_t Position::MAX_Y = 60000;
const uint8_t  Position::MAX_Z = 15;
const uint16_t Position::MIN_X = 0;
const uint16_t Position::MIN_Y = 0;
const uint8_t  Position::MIN_Z = 0;
const Position Position::INVALID(std::numeric_limits<typeof(x)>::max(), std::numeric_limits<typeof(y)>::max(), std::numeric_limits<typeof(z)>::max(), true);



Position::Position()
	: x(0),
	  y(0),
	  z(0)
{}


Position::Position(uint16_t x, uint16_t y, uint8_t z)
	: x(x),
	  y(y),
	  z(z)
{
	assert(isValid());
}


Position::Position(uint16_t x, uint16_t y, uint8_t z, bool)
	: x(x),
	  y(y),
	  z(z)
{}


Position::Position(const Position& position)
	: x(position.x),
	  y(position.y),
	  z(position.z)
{
	assert(isValid() || *this == INVALID);
}


bool Position::areInRange(const Position& range, const Position& a, const Position& b) {
	return std::abs(static_cast<int32_t>(a.x) - static_cast<int32_t>(b.x)) <= range.x
			&& std::abs(static_cast<int32_t>(a.y) - static_cast<int32_t>(b.y)) <= range.y
			&& std::abs(static_cast<int16_t>(a.z) - static_cast<int16_t>(b.z)) <= range.z;
}


uint32_t Position::distanceTo(const Position& position) const {
	auto dx = std::abs(position.x - x);
	auto dy = std::abs(position.y - y);
	auto dz = std::abs(position.z - z);

	return std::max(dx, std::max(dy, dz));
}


bool Position::isNeighborOf(const Position& position) const {
	if (position.z != z) {
		return false;
	}

	auto dx = std::abs(position.x - x);
	if (dx > 1) {
		return false;
	}

	auto dy = std::abs(position.y - y);
	if (dy > 1) {
		return false;
	}

	if (dx + dy == 0) {
		return false;
	}

	return true;
}


bool Position::isValid() const {
	return isValid(x, y, z);
}


bool Position::isValid(int_fast32_t x, int_fast32_t y, int_fast32_t z) {
	return (x >= MIN_X && x <= MAX_X && y >= MIN_Y && y <= MAX_Y && z >= MIN_Z && z <= MAX_Z);
}


std::vector<Position> Position::neighbors(uint16_t distance) const {
	std::vector<Position> target;
	neighbors(distance, target);

	return target;
}


void Position::neighbors(uint16_t distance, std::vector<Position>& target) const {
	if (distance == 0) {
		return;
	}

	auto startOffset = -static_cast<int32_t>(distance);
	auto endOffset   = static_cast<int32_t>(distance);

	auto neighbor = *this;
	for (auto xOffset = startOffset; xOffset <= endOffset; ++xOffset) {
		auto x = this->x + xOffset;
		if (x < static_cast<int32_t>(MIN_X) || x > static_cast<int32_t>(MAX_X)) {
			continue;
		}

		neighbor.x = static_cast<int16_t>(x);

		int32_t yOffsetIncrease;
		if (xOffset == startOffset || xOffset == endOffset) {
			// we're on the left or the right border - go through all y tiles
			yOffsetIncrease = 1;
		}
		else {
			// we're somewhere in between - go through top and bottom tile only
			yOffsetIncrease = endOffset - startOffset;
		}

		for (auto yOffset = startOffset; yOffset <= endOffset; yOffset += yOffsetIncrease) {
			auto y = this->y + yOffset;
			if (y < static_cast<int32_t>(MIN_Y) || y > static_cast<int32_t>(MAX_Y)) {
				continue;
			}

			neighbor.y = static_cast<int16_t>(y);

			target.push_back(neighbor);
		}
	}
}


Position& Position::operator = (const Position& position) {
	assert(position.isValid() || position == INVALID);

	x = position.x;
	y = position.y;
	z = position.z;

	return *this;
}


bool Position::operator == (const Position& position) const {
	return (x == position.x && y == position.y && z == position.z);
}


bool Position::operator != (const Position& position) const {
	return (x != position.x || y != position.y || z != position.z);
}


Position Position::operator + (const Position& position) const {
	assert(isValid());
	assert(position.isValid());
	assert(MAX_X - position.x <= x && MAX_Y - position.y <= y && MAX_Z - position.z <= z);

	return Position(x + position.x, y + position.y, z + position.z);
}


Position Position::operator - (const Position& position) const {
	assert(isValid());
	assert(position.isValid());
	assert(position.x <= x && position.y <= y && position.z <= z);

	return Position(x - position.x, y - position.y, z - position.z);
}




const uint8_t    StackPosition::MAX_INDEX = 9;
const uint8_t    StackPosition::MIN_INDEX = 0;
const StackPosition StackPosition::INVALID(Position::INVALID, std::numeric_limits<typeof(index)>::max(), true);



StackPosition::StackPosition()
	: index(0)
{}


StackPosition::StackPosition(uint16_t x, uint16_t y, uint8_t z, uint8_t index)
	: Position(x, y, z),
	  index(index)
{}


StackPosition::StackPosition(const Position& position, uint8_t index)
	: Position(position),
	  index(index)
{
	assert(isValid());
}


StackPosition::StackPosition(const Position& position, uint8_t index, bool)
	: Position(position),
	  index(index)
{
}


StackPosition::StackPosition(const StackPosition& position)
	: Position(position.withoutIndex()),
	  index(position.index)
{
	assert(isValid() || *this == INVALID);
}


bool StackPosition::isValid(int_fast32_t x, int_fast32_t y, int_fast32_t z, int_fast32_t index) {
	return (Position::isValid(x, y, z) && index >= MIN_INDEX && index <= MAX_INDEX);
}


bool StackPosition::isValid(const Position& position, int_fast32_t index) {
	return (position.isValid() && index >= MIN_INDEX && index <= MAX_INDEX);
}


bool StackPosition::isValid() const {
	return (isValid(*this, index));
}


Position& StackPosition::withoutIndex() {
	return static_cast<Position&>(*this);
}


const Position& StackPosition::withoutIndex() const {
	return static_cast<const Position&>(*this);
}


StackPosition& StackPosition::operator = (const StackPosition& position) {
	assert(position.isValid() || position == INVALID);

	withoutIndex() = position.withoutIndex();
	index = position.index;

	return *this;
}


bool StackPosition::operator == (const StackPosition& position) const {
	return (withoutIndex() == position.withoutIndex() && index == position.index);
}


bool StackPosition::operator != (const StackPosition& position) const {
	return (withoutIndex() != position.withoutIndex() || index != position.index);
}




ExtendedPosition::ExtendedPosition(Value value)
	: _value(value)
{}


ExtendedPosition ExtendedPosition::forBackpack(uint8_t openedContainerId, uint8_t containerItemIndex) {
	Backpack backpack;
	backpack.containerItemIndex = containerItemIndex;
	backpack.openedContainerId = openedContainerId;

	return ExtendedPosition(backpack);
}


ExtendedPosition ExtendedPosition::forBackpackSearch(uint16_t clientItemKindId, int8_t fluidType) {
	BackpackSearch backpackSearch;
	backpackSearch.clientItemKindId = clientItemKindId;
	backpackSearch.fluidType = fluidType;

	return ExtendedPosition(backpackSearch);
}


ExtendedPosition ExtendedPosition::forPosition(const Position& position) {
	return ExtendedPosition(position);
}


ExtendedPosition ExtendedPosition::forSlot(slots_t slot) {
	return ExtendedPosition(slot);
}


ExtendedPosition ExtendedPosition::forStackPosition(const StackPosition& stackPosition) {
	return ExtendedPosition(stackPosition);
}


uint16_t ExtendedPosition::getClientItemKindId() const {
	if (getType() != Type::BACKPACK_SEARCH) {
		assert(getType() == Type::BACKPACK_SEARCH);
		return 0;
	}

	return boost::get<BackpackSearch>(_value).clientItemKindId;
}


uint8_t ExtendedPosition::getContainerItemIndex() const {
	if (getType() != Type::BACKPACK) {
		assert(getType() == Type::BACKPACK);
		return 0;
	}

	return boost::get<Backpack>(_value).containerItemIndex;
}


int8_t ExtendedPosition::getFluidType() const {
	if (getType() != Type::BACKPACK_SEARCH) {
		assert(getType() == Type::BACKPACK_SEARCH);
		return 0;
	}

	return boost::get<BackpackSearch>(_value).fluidType;
}


uint8_t ExtendedPosition::getOpenedContainerId() const {
	if (getType() != Type::BACKPACK) {
		assert(getType() == Type::BACKPACK);
		return 0;
	}

	return boost::get<Backpack>(_value).openedContainerId;
}


Position ExtendedPosition::getPosition(bool convertsStackPosititon) const {
	switch (getType()) {
	case Type::POSITION:
		return boost::get<Position>(_value);

	case Type::STACK_POSITION:
		if (!convertsStackPosititon) {
			assert(getType() == Type::POSITION || (convertsStackPosititon && getType() == Type::STACK_POSITION));
			return Position();
		}

		return boost::get<StackPosition>(_value).withoutIndex();

	case Type::BACKPACK:
	case Type::BACKPACK_SEARCH:
	case Type::NOWHERE:
	case Type::SLOT:
		break;
	}

	assert(getType() == Type::POSITION || (convertsStackPosititon && getType() == Type::STACK_POSITION));
	return Position();
}


slots_t ExtendedPosition::getSlot() const {
	if (getType() != Type::SLOT) {
		assert(getType() == Type::SLOT);
		return slots_t::HEAD;
	}

	return boost::get<slots_t>(_value);
}


StackPosition ExtendedPosition::getStackPosition(bool convertsPosititon) const {
	switch (getType()) {
	case Type::STACK_POSITION:
		return boost::get<StackPosition>(_value);

	case Type::POSITION:
		if (!convertsPosititon) {
			assert(getType() == Type::STACK_POSITION || (convertsPosititon && getType() == Type::POSITION));
			return StackPosition();
		}

		return StackPosition(boost::get<Position>(_value), 0);

	case Type::BACKPACK:
	case Type::BACKPACK_SEARCH:
	case Type::NOWHERE:
	case Type::SLOT:
		break;
	}

	assert(getType() == Type::STACK_POSITION || (convertsPosititon && getType() == Type::POSITION));
	return StackPosition();
}


ExtendedPosition::Type ExtendedPosition::getType() const {
	return static_cast<Type>(_value.which());
}


bool ExtendedPosition::hasPosition(bool convertsStackPosition) const {
	switch (getType()) {
	case Type::POSITION:
		return true;

	case Type::STACK_POSITION:
		return convertsStackPosition;

	case Type::BACKPACK:
	case Type::BACKPACK_SEARCH:
	case Type::NOWHERE:
	case Type::SLOT:
		return false;
	}

	return false;
}


ExtendedPosition ExtendedPosition::nowhere() {
	return ExtendedPosition(Nowhere());
}




std::ostream& operator << (std::ostream& stream, const Position& position) {
	if (position.isValid()) {
		return stream << position.x << "/" << position.y << "/" << position.z;
	}
	else {
		return stream << "invalid";
	}
}


std::ostream& operator << (std::ostream& stream, Direction direction) {
	switch (direction) {
		case Direction::NONE:
			return stream << "none";

		case Direction::NORTH:
			return stream << "north";

		case Direction::EAST:
			return stream << "east";

		case Direction::WEST:
			return stream << "west";

		case Direction::SOUTH:
			return stream << "south";

		case Direction::SOUTH_WEST:
			return stream << "south-west";

		case Direction::SOUTH_EAST:
			return stream << "south-east";

		case Direction::NORTH_WEST:
			return stream << "north-west";

		case Direction::NORTH_EAST:
			return stream << "north-east";
	}

	return stream << "invalid";
}


std::ostream& operator << (std::ostream& stream, const StackPosition& position) {
	if (position.isValid()) {
		return stream << position.withoutIndex() << "#" << position.index;
	}
	else {
		return stream << "invalid";
	}
}
