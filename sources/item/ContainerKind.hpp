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

#ifndef _ITEM_CONTAINERKIND_HPP
#define _ITEM_CONTAINERKIND_HPP

#include "item/Kind.hpp"


namespace ts { namespace item {

class ContainerKind;

typedef std::shared_ptr<ContainerKind>        ContainerKindP;
typedef std::shared_ptr<const ContainerKind>  ContainerKindPC;


class ContainerKind : public Kind {

public:

	typedef const ConcreteClass<ContainerKind>  ClassT;
	typedef std::shared_ptr<const ClassT>       ClassPC;


	ContainerKind(const ClassPC& clazz, KindId id);

	virtual ItemP newItem () const;


protected:

	virtual bool assemble     ();
	virtual bool setParameter (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);


private:

	LOGGER_DECLARATION;

	// 2 bytes
	uint16_t _capacity;  // XML/containersize

};

}} // namespace ts::item

#endif // _ITEM_CONTAINERKIND_HPP
