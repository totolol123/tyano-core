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

#ifndef _ITEM_BEDKIND_HPP
#define _ITEM_BEDKIND_HPP

#include "item/Kind.hpp"


namespace ts { namespace creature {

enum class Gender : uint8_t;

}} // namespace ts::creature



namespace ts { namespace item {

class BedKind;

typedef std::shared_ptr<BedKind>        BedKindP;
typedef std::shared_ptr<const BedKind>  BedKindPC;


class BedKind : public Kind {

private:

	typedef creature::Gender  Gender;


public:

	typedef const ConcreteClass<BedKind>   ClassT;
	typedef std::shared_ptr<const ClassT>  ClassPC;


	BedKind(const ClassPC& clazz, KindId id);

	        Direction getCounterpartDirection () const;
	        KindId    getFreeBedId            () const;
	        KindId    getOccupiedBedId        (Gender gender) const;
	virtual ItemP     newItem                 () const;


	static const std::string ATTRIBUTE_SLEEPSTART;


protected:

	virtual bool assemble     ();
	virtual bool setParameter (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);


private:

	LOGGER_DECLARATION;

	// 4 bytes
	Direction _counterpartDirection;  // XML/partnerdirection

	// 2 bytes
	KindId _femaleOccupiedBedId;  // XML/femaleTransformTo
	KindId _freeBedId;            // XML/transformTo
	KindId _maleOccupiedBedId;    // XML/maleTransformTo

};

}} // namespace ts::item

#endif // _ITEM_BEDKIND_HPP
