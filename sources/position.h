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

#ifndef _POSITION_H
#define _POSITION_H

class PositionEx;


enum class Direction : uint8_t {
	NORTH,
	EAST,
	SOUTH,
	WEST,
	SOUTH_WEST,
	SOUTH_EAST,
	NORTH_WEST,
	NORTH_EAST,
};



class Position {

public:

	static bool areInRange (const Position& range, const Position& a, const Position& b);
	static bool isValid    (uint16_t x, uint16_t y, uint8_t z);

	Position();
	Position(const Position& position);
	explicit Position(uint16_t x, uint16_t y, uint8_t z);

	uint32_t distanceTo (const Position& position) const;
	bool     isValid    () const;

	Position& operator =  (const Position& position);
	bool      operator == (const Position& position) const;
	bool      operator != (const Position& position) const;
	Position  operator +  (const Position& position) const;
	Position  operator -  (const Position& position) const;


	static const Position INVALID;
	static const uint16_t MAX_X;
	static const uint16_t MAX_Y;
	static const uint8_t  MAX_Z;
	static const uint16_t MIN_X;
	static const uint16_t MIN_Y;
	static const uint8_t  MIN_Z;

	uint16_t x;
	uint16_t y;
	uint8_t  z;


	template<uint16_t rangeX, uint16_t rangeY, uint8_t rangeZ>
	inline static bool areInRange(const Position& a, const Position& b) {
		return std::abs(static_cast<int32_t>(a.x) - static_cast<int32_t>(b.x)) <= rangeX
				&& std::abs(static_cast<int32_t>(a.y) - static_cast<int32_t>(b.y)) <= rangeY
				&& std::abs(static_cast<int16_t>(a.z) - static_cast<int16_t>(b.z)) <= rangeZ;
	}

	template<uint16_t rangeX, uint16_t rangeY>
	inline static bool areInRange(const Position& a, const Position& b) {
		return std::abs(static_cast<int32_t>(a.x) - static_cast<int32_t>(b.x)) <= rangeX
				&& std::abs(static_cast<int32_t>(a.y) - static_cast<int32_t>(b.y)) <= rangeY;
	}


private:

	Position(const PositionEx& position);
	explicit Position(uint16_t x, uint16_t y, uint8_t z, bool internalInvalid);

	Position& operator =  (const PositionEx& position);
	bool      operator == (const PositionEx& position) const;
	bool      operator != (const PositionEx& position) const;
	Position  operator +  (const PositionEx& position) const;
	Position  operator -  (const PositionEx& position) const;

};



class PositionEx : public Position {

public:

	static bool isValid (uint16_t x, uint16_t y, uint8_t z, uint8_t index);
	static bool isValid (const Position& position, uint8_t index);

	PositionEx();
	PositionEx(const PositionEx& position);
	explicit PositionEx(uint16_t x, uint16_t y, uint8_t z, uint8_t index);
	explicit PositionEx(const Position& position, uint8_t index);

	bool            isValid      () const;
	Position&       withoutIndex ();
	const Position& withoutIndex () const;

	PositionEx& operator =  (const PositionEx& position);
	bool        operator == (const PositionEx& position) const;
	bool        operator != (const PositionEx& position) const;


	static const PositionEx INVALID;
	static const uint8_t    MAX_INDEX;
	static const uint8_t    MIN_INDEX;

	uint8_t index;


private:

	PositionEx(const Position& position);
	explicit PositionEx(const Position& position, uint8_t index, bool internalInvalid);

	PositionEx& operator =  (const Position& position);
	bool        operator == (const Position& position) const;
	bool        operator != (const Position& position) const;
	Position    operator +  (const Position& position) const;
	Position    operator -  (const Position& position) const;

};



namespace std {
	template<>
	struct hash<Position> : public function<size_t(const Position&)> {
		result_type operator()(argument_type position) const {
			return hash<typeof(position.x)>()(position.x) ^ hash<typeof(position.y)>()(position.y) ^ hash<typeof(position.z)>()(position.z);
		}
	};

	template<>
	struct hash<PositionEx> : public function<size_t(const PositionEx&)> {
		result_type operator()(argument_type position) const {
			return hash<Position>()(position) ^ hash<typeof(position.index)>()(position.index);
		}
	};
}



std::ostream& operator << (std::ostream& stream, Direction direction);
std::ostream& operator << (std::ostream& stream, const Position& position);
std::ostream& operator << (std::ostream& stream, const PositionEx& position);

#endif // _POSITION_H
