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

#ifndef _ITEM_CLASS_HPP
#define _ITEM_CLASS_HPP

#include "attributes/Attribute.hpp"
#include "attributes/Scheme.hpp"
#include "utility/Traits.hpp"

class Item;


namespace ts { namespace item {

class Kind;
class Class;

template<typename _Kind>
class ClassDescriptor;

template<typename _Kind>
class ConcreteClass;

typedef std::shared_ptr<Class>        ClassP;
typedef std::shared_ptr<const Class>  ClassPC;
typedef uint16_t                      KindId;


class Class : public std::enable_shared_from_this<Class> {

public:

	typedef attributes::Scheme                       DynamicAttributeScheme;
	typedef std::shared_ptr<DynamicAttributeScheme>  DynamicAttributeSchemeP;
	typedef Kind                                     KindT;
	typedef std::shared_ptr<KindT>                   KindP;


	Class (const std::string& id, const std::string& name, const DynamicAttributeSchemeP& dynamicAttributeScheme);
	virtual ~Class();

	virtual KindP                   createKind                (KindId id) const = 0;
			KindP                   createKindFromXmlNode     (xmlNode& node) const;
			DynamicAttributeSchemeP getDynamicAttributeScheme () const;
			const std::string&      getId                     () const;
			const std::string&      getName                   () const;


private:

	Class(const Class&) = delete;
	Class(Class&&) = delete;

	void processKindParameter (KindT& kind, const std::string& name, const std::string& value, xmlNodePtr deprecatedNode = nullptr) const;


	LOGGER_DECLARATION;

	const DynamicAttributeSchemeP _dynamicAttributeScheme;
	const std::string             _id;
	const std::string             _name;

	template<typename T>
	friend class ConcreteClass;

};



template<>
class ConcreteClass<Kind> : public Class {

public:

	ConcreteClass(const std::string& id, const std::string& name, const DynamicAttributeSchemeP& dynamicAttributeScheme)
		: Class(id, name, dynamicAttributeScheme)
	{
		// empty
	}


private:

	template<typename _Kind>
	struct hierarchy_resolver {
		typedef typename parallel_hierarchy<_Kind,Kind>::type  _baseKind;
		typedef ConcreteClass<_baseKind>                       type;
	};

};




template<typename _Kind>
class ConcreteClass : public ConcreteClass<Kind>::hierarchy_resolver<_Kind>::type {

	static_assert(std::is_base_of<Kind,_Kind>::value, "template argument _Kind must be a specialization of class item::Kind");


private:

	typedef ConcreteClass<_Kind>  This;


public:

	typedef typename ConcreteClass<Kind>::hierarchy_resolver<_Kind>::type  Base;
	typedef typename Base::DynamicAttributeSchemeP                         DynamicAttributeSchemeP;
	typedef _Kind                                                          KindT;
	typedef std::shared_ptr<KindT>                                         KindP;


	ConcreteClass(const std::string& id, const std::string& name, const DynamicAttributeSchemeP& dynamicAttributeScheme)
		: ConcreteClass(id, name, dynamicAttributeScheme)
	{
		// empty
	}


	virtual ~ConcreteClass() {
		// empty
	}


	virtual Class::KindP createKind(KindId id) const {
		return std::static_pointer_cast<Class::KindT>(newKind(id));
	}


	KindP newKind(KindId id) const {
		return std::make_shared<KindT>(std::static_pointer_cast<const This>(Class::shared_from_this()), id);
	}


	KindP createKindFromXmlNode(xmlNode& node) const {
		return Base::createKindFromXmlNode(node);
	}
};



template<>
class ClassDescriptor<Kind> {

private:

	template<typename _Kind>
	struct hierarchy_resolver {
		typedef typename parallel_hierarchy<_Kind,Kind>::type  _baseKind;
		typedef ClassDescriptor<_baseKind>                     type;
	};


protected:

	typedef attributes::Attribute                 DynamicAttribute;
	typedef std::unordered_set<DynamicAttribute>  DynamicAttributes;
	typedef std::unique_ptr<DynamicAttributes>    DynamicAttributesP;

	static DynamicAttributesP getDynamicAttributes();
	static std::string        getId();
	static std::string        getName();

};



template <typename _Kind>
class ClassDescriptor : public ClassDescriptor<Kind>::hierarchy_resolver<_Kind>::type {

	static_assert(std::is_base_of<Kind,_Kind>::value, "template argument _Kind must be a specialization of class item::Kind");


protected:

	typedef typename ClassDescriptor<Kind>::hierarchy_resolver<_Kind>::type  Base;
	typedef typename Base::DynamicAttributesP                                DynamicAttributesP;

	static DynamicAttributesP getDynamicAttributes();
	static std::string        getId();
	static std::string        getName();


public:

	typedef ConcreteClass<_Kind>    Class;
	typedef std::shared_ptr<Class>  ClassP;


	static ClassP create() {
		return std::make_shared<Class>(getId(), getName(), std::make_shared<attributes::Scheme>(getDynamicAttributes()));
	}
};

}} // namespace ts::item

#endif // _ITEM_CLASS_HPP
