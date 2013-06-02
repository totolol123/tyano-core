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
#include "xml/Xml.hpp"

#include "Utility/Utility.hpp"


namespace ts {
namespace xml {

template<typename T>
struct default_delete {
public:
	void operator()(T* pointer) const {
		xmlFree(pointer);
	}
};


template<typename T>
class unique_ptr : public std::unique_ptr<T,default_delete<T>> {

public:

	constexpr unique_ptr() noexcept {
	}


	explicit unique_ptr(T* p) noexcept
		: unique_ptr(p)
	{}


	constexpr unique_ptr(std::nullptr_t) noexcept
		: unique_ptr(nullptr)
	{}


	operator T*() {
		return unique_ptr::get();
	}

};


unique_ptr<char> _attribute(xmlAttr& attribute) {
	return unique_ptr<char>(reinterpret_cast<char*>(xmlNodeListGetString(attribute.doc, attribute.children, 1)));
}


unique_ptr<char> _attribute(xmlNode& node, const char* name) {
	if (name == nullptr) {
		assert(name != nullptr);
		return nullptr;
	}

	return unique_ptr<char>(reinterpret_cast<char*>(xmlGetProp(&node, reinterpret_cast<const xmlChar*>(name))));
}


template<>
bool attribute<bool>(xmlAttr& attribute, bool& value) {
	auto string = _attribute(attribute);
	if (string == nullptr) {
		return false;
	}

	if (std::strcmp(string, "no") == 0) {
		value = false;
		return true;
	}
	if (std::strcmp(string, "yes") == 0) {
		value = true;
		return true;
	}

	return true;
}


template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
attribute(xmlAttr& attribute, T& value) {
	auto string = _attribute(attribute);
	if (string == nullptr) {
		return false;
	}

	try {
		value = boost::lexical_cast<T>(string.get());
	}
	catch (boost::bad_lexical_cast& e) {
		return false;
	}

	return true;
}


template<typename T>
typename std::enable_if<std::is_same<T,std::string>::value,bool>::type
attribute(xmlAttr& attribute, T& value) {
	auto string = _attribute(attribute);
	if (string == nullptr) {
		return false;
	}

	value = string;
	return true;
}


xmlAttrPtr attribute(xmlNode& node, const char* name) {
	if (name == nullptr) {
		assert(name != nullptr);
		return nullptr;
	}

	return xmlHasProp(&node, reinterpret_cast<const xmlChar*>(name));
}


template<>
bool attribute<bool>(xmlNode& node, const char* name, bool& value) {
	auto string = _attribute(node, name);
	if (string == nullptr) {
		return false;
	}

	if (std::strcmp(string, "no") == 0) {
		value = false;
		return true;
	}
	if (std::strcmp(string, "yes") == 0) {
		value = true;
		return true;
	}

	return true;
}


template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
attribute(xmlNode& node, const char* name, T& value) {
	auto string = _attribute(node, name);
	if (string == nullptr) {
		return false;
	}

	try {
		value = boost::lexical_cast<T>(string.get());
	}
	catch (boost::bad_lexical_cast& e) {
		return false;
	}

	return true;
}


template<typename T>
typename std::enable_if<std::is_same<T,std::string>::value,bool>::type
attribute(xmlNode& node, const char* name, T& value) {
	auto string = _attribute(node, name);
	if (string == nullptr) {
		return false;
	}

	value = string;
	return true;
}


bool hasAttribute(xmlNode& node, const char* name) {
	return (attribute(node, name) != nullptr);
}


std::string name(xmlNode& node) {
	return reinterpret_cast<const char*>(node.name);
}



template bool attribute<bool>(xmlAttr& attribute, bool& value);
template bool attribute<std::int8_t>(xmlAttr& attribute, int8_t& value);
template bool attribute<std::int16_t>(xmlAttr& attribute, int16_t& value);
template bool attribute<std::int32_t>(xmlAttr& attribute, int32_t& value);
template bool attribute<std::int64_t>(xmlAttr& attribute, int64_t& value);
template bool attribute<std::uint8_t>(xmlAttr& attribute, uint8_t& value);
template bool attribute<std::uint16_t>(xmlAttr& attribute, uint16_t& value);
template bool attribute<std::uint32_t>(xmlAttr& attribute, uint32_t& value);
template bool attribute<std::uint64_t>(xmlAttr& attribute, uint64_t& value);
template bool attribute<float>(xmlAttr& attribute, float& value);
template bool attribute<double>(xmlAttr& attribute, double& value);
template bool attribute<std::string>(xmlAttr& attribute, std::string& value);

template bool attribute<bool>(xmlNode& node, const char* name, bool& value);
template bool attribute<std::int8_t>(xmlNode& node, const char* name, int8_t& value);
template bool attribute<std::int16_t>(xmlNode& node, const char* name, int16_t& value);
template bool attribute<std::int32_t>(xmlNode& node, const char* name, int32_t& value);
template bool attribute<std::int64_t>(xmlNode& node, const char* name, int64_t& value);
template bool attribute<std::uint8_t>(xmlNode& node, const char* name, uint8_t& value);
template bool attribute<std::uint16_t>(xmlNode& node, const char* name, uint16_t& value);
template bool attribute<std::uint32_t>(xmlNode& node, const char* name, uint32_t& value);
template bool attribute<std::uint64_t>(xmlNode& node, const char* name, uint64_t& value);
template bool attribute<float>(xmlNode& node, const char* name, float& value);
template bool attribute<double>(xmlNode& node, const char* name, double& value);
template bool attribute<std::string>(xmlNode& node, const char* name, std::string& value);

} // namespace xml
} // namespace ts
