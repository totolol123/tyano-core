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

#ifndef _ITEMS_CLASS_HPP
#define _ITEMS_CLASS_HPP

#include "attributes/Attribute.hpp"
#include "attributes/Scheme.hpp"

class Item;


namespace items {

	class Class;

	typedef std::shared_ptr<Class>  ClassP;


	class Class {

	public:

		Class(const std::string& name, const attributes::SchemeP& attributesScheme);
		~Class();

		attributes::SchemeP getAttributesScheme () const;
		const std::string&  getName             () const;


	private:

		Class(const Class&) = delete;
		Class(Class&&) = delete;


		const attributes::SchemeP _attributesScheme;
		const std::string         _name;

	};



	template <typename _Item>
	ClassP makeClass() {
		static_assert(sizeof(_Item) > 0, "template argument Item must must be a complete type, not a forward declaration");
		static_assert(std::is_base_of<Item,_Item>::value, "template argument Item must be a specialization of class Item");

		return std::make_shared<Class>(_Item::getClassName(), attributes::SchemeP(new attributes::Scheme(_Item::getClassAttributes())));
	}

} // namespace items

#endif // _ITEMS_CLASS_HPP
