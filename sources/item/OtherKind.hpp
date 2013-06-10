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

#ifndef _ITEM_OTHERKIND_HPP
#define _ITEM_OTHERKIND_HPP

#include "item/Kind.hpp"


namespace ts { namespace item {

class OtherKind;

typedef std::shared_ptr<OtherKind>        OtherKindP;
typedef std::shared_ptr<const OtherKind>  OtherKindPC;


class OtherKind : public Kind {

public:

	typedef const ConcreteClass<OtherKind>  ClassT;
	typedef std::shared_ptr<const ClassT>   ClassPC;


	OtherKind(const ClassPC& clazz, KindId id);

	virtual ItemP newItem () const;

};

}} // namespace ts::item

#endif // _ITEM_OTHERKIND_HPP
