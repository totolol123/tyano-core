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
	return std::abs(static_cast<int32_t>(position.x) - static_cast<int32_t>(x))
	     + std::abs(static_cast<int32_t>(position.y) - static_cast<int32_t>(y))
	     + std::abs(static_cast<int32_t>(position.z) - static_cast<int32_t>(z));
}


bool Position::isValid() const {
	return isValid(x, y, z);
}


bool Position::isValid(uint16_t x, uint16_t y, uint8_t z) {
	return (x <= MAX_X && y <= MAX_Y && z <= MAX_Z);
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




const uint8_t    PositionEx::MAX_INDEX = 9;
const uint8_t    PositionEx::MIN_INDEX = 0;
const PositionEx PositionEx::INVALID(Position::INVALID, std::numeric_limits<typeof(index)>::max(), true);



PositionEx::PositionEx()
	: index(0)
{}


PositionEx::PositionEx(uint16_t x, uint16_t y, uint8_t z, uint8_t index)
	: Position(x, y, z),
	  index(0)
{}


PositionEx::PositionEx(const Position& position, uint8_t index)
	: Position(position),
	  index(index)
{
	assert(isValid());
}


PositionEx::PositionEx(const Position& position, uint8_t index, bool)
	: Position(position),
	  index(index)
{
}


PositionEx::PositionEx(const PositionEx& position)
	: Position(position.withoutIndex()),
	  index(position.index)
{
	assert(isValid() || *this == INVALID);
}


bool PositionEx::isValid(uint16_t x, uint16_t y, uint8_t z, uint8_t index) {
	return (Position::isValid(x, y, z) && index <= MAX_INDEX);
}


bool PositionEx::isValid(const Position& position, uint8_t index) {
	return (position.isValid() && index <= MAX_INDEX);
}


bool PositionEx::isValid() const {
	return (isValid(*this, index));
}


Position& PositionEx::withoutIndex() {
	return static_cast<Position&>(*this);
}


const Position& PositionEx::withoutIndex() const {
	return static_cast<const Position&>(*this);
}


PositionEx& PositionEx::operator = (const PositionEx& position) {
	assert(position.isValid() || position == INVALID);

	withoutIndex() = position.withoutIndex();
	index = position.index;

	return *this;
}


bool PositionEx::operator == (const PositionEx& position) const {
	return (withoutIndex() == position.withoutIndex() && index == position.index);
}


bool PositionEx::operator != (const PositionEx& position) const {
	return (withoutIndex() != position.withoutIndex() || index != position.index);
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
			return stream << "north-eest";

		case Direction::NORTH_EAST:
			return stream << "north-east";
	}

	return stream << "invalid";
}


std::ostream& operator << (std::ostream& stream, const PositionEx& position) {
	if (position.isValid()) {
		return stream << position.withoutIndex() << "#" << position.index;
	}
	else {
		return stream << "invalid";
	}
}
