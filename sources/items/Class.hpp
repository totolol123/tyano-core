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
#include "utility/Traits.hpp"

class Item;


namespace ts {
namespace items {

	class Kind;

	template<typename _Kind>
	class Class;


	template<>
	class Class<Kind> : public std::enable_shared_from_this<Class<Kind>> {

	public:

		typedef Kind                    KindT;
		typedef std::shared_ptr<KindT>  KindP;


		Class(const std::string& id, const std::string& name, const attributes::SchemeP& attributesScheme);
		virtual ~Class();

		virtual KindP       createKind          () const = 0;
		attributes::SchemeP getAttributesScheme () const;
		const std::string&  getId               () const;
		const std::string&  getName             () const;


	private:

		template<typename _Kind>
		struct hierarchy_resolver {
			typedef typename parallel_hierarchy<_Kind,Kind>::type  _baseKind;
			typedef Class<_baseKind>                               type;
		};


		Class(const Class&) = delete;
		Class(Class&&) = delete;

		KindP createKindFromXmlNode (xmlNodePtr node) const;
		void  processKindParameter  (KindT& kind, const std::string& name, const std::string& value, xmlNodePtr deprecatedNode = nullptr) const;

		LOGGER_DECLARATION;

		const attributes::SchemeP _attributesScheme;
		const std::string         _id;
		const std::string         _name;

		template<class T>
		friend class Class;

	};


	template<typename _Kind>
	class Class : public Class<Kind>::hierarchy_resolver<_Kind>::type {

		static_assert(std::is_base_of<Kind,_Kind>::value, "template argument _Kind must be a specialization of class items::Kind");


	public:

		typedef _Kind                   KindT;
		typedef std::shared_ptr<KindT>  KindP;


		Class(const std::string& id, const std::string& name, const attributes::SchemeP& attributesScheme)
			: Class(id, name, attributesScheme)
		{
		}


		virtual ~Class() {
		}


		virtual Class<Kind>::KindP createKind() const {
			return std::make_shared<KindT>(Class<Kind>::shared_from_this());
		}


		KindP createKindFromXmlNode(xmlNodePtr node) const {
			return Class::createKindFromXmlNode(node);
		}
	};


//
//	template <typename Item>
//	ClassP makeClass() {
//		static_assert(sizeof(Item) > 0, "template argument Item must must be a complete type, not a forward declaration");
//		static_assert(std::is_base_of<::Item,Item>::value, "template argument Item must be a specialization of class Item");
//
//		return std::make_shared<Class>(Item::getClassId(), Item::getClassName(), attributes::SchemeP(new attributes::Scheme(Item::getClassAttributes())));
//	}

} // namespace items
} // namespace ts

#endif // _ITEMS_CLASS_HPP
