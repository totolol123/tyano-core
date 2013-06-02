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

#ifndef _XML_XML_HPP
#define _XML_XML_HPP

namespace ts {
namespace xml {

	template<typename T>
	typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
	attribute(xmlAttr& attribute, T& value);

	template<typename T>
	typename std::enable_if<std::is_same<T,std::string>::value,bool>::type
	attribute(xmlAttr& attribute, T& value);

	xmlAttrPtr attribute(xmlNode& node, const char* name);

	template<typename T>
	typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
	attribute(xmlNode& node, const char* name, T& value);

	template<typename T>
	typename std::enable_if<std::is_same<T,std::string>::value,bool>::type
	attribute(xmlNode& node, const char* name, T& value);

	bool hasAttribute(xmlNode& node, const char* name);

	std::string name(xmlNode& node);

} // namespace xml
} // namespace ts

#endif // _XML_XML_HPP
