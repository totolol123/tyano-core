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

enum class slots_t : uint8_t;
class      StackPosition;


enum class Direction : uint8_t {
	NORTH,
	EAST,
	SOUTH,
	WEST,
	SOUTH_WEST,
	SOUTH_EAST,
	NORTH_WEST,
	NORTH_EAST,
	NONE = std::numeric_limits<uint8_t>::max(),
};



class Position {

public:

	static bool areInRange (const Position& range, const Position& a, const Position& b);
	static bool isValid    (int_fast32_t x, int_fast32_t y, int_fast32_t z);

	Position          ();
	Position          (const Position& position);
	Position          (Position&& position) = default;
	explicit Position (uint16_t x, uint16_t y, uint8_t z);

	uint32_t              distanceTo   (const Position& position) const;
	bool                  isNeighborOf (const Position& position) const;
	bool                  isValid      () const;
	std::vector<Position> neighbors    (uint16_t distance) const;
	void                  neighbors    (uint16_t distance, std::vector<Position>& target) const;

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

	Position          (const StackPosition& position);
	explicit Position (uint16_t x, uint16_t y, uint8_t z, bool internalInvalid);

	Position& operator =  (const StackPosition& position);
	bool      operator == (const StackPosition& position) const;
	bool      operator != (const StackPosition& position) const;
	Position  operator +  (const StackPosition& position) const;
	Position  operator -  (const StackPosition& position) const;

};



class StackPosition : public Position {

public:

	static bool isValid (int_fast32_t x, int_fast32_t y, int_fast32_t z, int_fast32_t index);
	static bool isValid (const Position& position, int_fast32_t index);

	StackPosition          ();
	StackPosition          (const StackPosition& position);
	StackPosition          (StackPosition&& position) = default;
	explicit StackPosition (uint16_t x, uint16_t y, uint8_t z, uint8_t index);
	explicit StackPosition (const Position& position, uint8_t index);

	bool            isValid      () const;
	Position&       withoutIndex ();
	const Position& withoutIndex () const;

	StackPosition& operator =  (const StackPosition& position);
	bool           operator == (const StackPosition& position) const;
	bool           operator != (const StackPosition& position) const;


	static const StackPosition INVALID;
	static const uint8_t    MAX_INDEX;
	static const uint8_t    MIN_INDEX;

	uint8_t index;


private:

	StackPosition          (const Position& position);
	explicit StackPosition (const Position& position, uint8_t index, bool internalInvalid);

	StackPosition& operator =  (const Position& position);
	bool           operator == (const Position& position) const;
	bool           operator != (const Position& position) const;
	Position       operator +  (const Position& position) const;
	Position       operator -  (const Position& position) const;

};



class ExtendedPosition {

public:

	enum class Type {
		NOWHERE         = 0,
		BACKPACK        = 1,
		BACKPACK_SEARCH = 2,
		POSITION        = 3,
		SLOT            = 4,
		STACK_POSITION  = 5,
	};


	static ExtendedPosition forBackpack       (uint8_t openedContainerId, uint8_t containerItemIndex);
	static ExtendedPosition forBackpackSearch (uint16_t clientItemKindId, int8_t fluidType);
	static ExtendedPosition forPosition       (const Position& position);
	static ExtendedPosition forSlot           (slots_t slot);
	static ExtendedPosition forStackPosition  (const StackPosition& stackPosition);
	static ExtendedPosition nowhere           ();

	ExtendedPosition (const ExtendedPosition& position) = default;

	uint16_t      getClientItemKindId   () const;
	uint8_t       getContainerItemIndex () const;
	int8_t        getFluidType          () const;
	uint8_t       getOpenedContainerId  () const;
	Position      getPosition           (bool convertsStackPosititon = false) const;
	slots_t       getSlot               () const;
	StackPosition getStackPosition      (bool convertsPosition = false) const;
	Type          getType               () const;
	bool          hasPosition           (bool convertsStackPosititon = false) const;

	ExtendedPosition& operator =  (const ExtendedPosition& position) = default;
	bool              operator == (const ExtendedPosition& position) const;
	bool              operator != (const ExtendedPosition& position) const;


private:

	struct Backpack {
		uint8_t containerItemIndex;
		uint8_t openedContainerId;
	};

	struct BackpackSearch {
		uint16_t clientItemKindId;
		int8_t   fluidType;
	};

	struct Nowhere {};


	typedef boost::variant<Nowhere,Backpack,BackpackSearch,Position,slots_t,StackPosition>  Value;


	Value   _value;


	explicit ExtendedPosition (Value value);

	// to prevent mistakes
	static ExtendedPosition forPosition (const StackPosition& position);

};



namespace std {

template<>
struct hash<Position> : public function<size_t(const Position&)> {
	result_type operator()(argument_type position) const {
		return hash<typeof(position.x)>()(position.x) ^ hash<typeof(position.y)>()(position.y) ^ hash<typeof(position.z)>()(position.z);
	}
};

template<>
struct hash<StackPosition> : public function<size_t(const StackPosition&)> {
	result_type operator()(argument_type position) const {
		return hash<Position>()(position) ^ hash<typeof(position.index)>()(position.index);
	}
};

} // namespace std



std::ostream& operator << (std::ostream& stream, Direction direction);
std::ostream& operator << (std::ostream& stream, const Position& position);
std::ostream& operator << (std::ostream& stream, const StackPosition& position);

#endif // _POSITION_H
