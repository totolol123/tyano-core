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
#include "attributes/Attribute.hpp"

using namespace ts;
using namespace ts::attributes;


Attribute::Attribute(const std::string& name, Type type)
	: _name(name),
	  _type(type)
{}


const std::string& Attribute::getName() const {
	return _name;
}


Type Attribute::getType() const {
	return _type;
}


const std::string& Attribute::getTypeName(Type type) {
	switch (type) {
		case Type::BOOLEAN: {
			static std::string name("boolean");
			return name;
		}

		case Type::FLOAT: {
			static std::string name("float");
			return name;
		}

		case Type::INTEGER: {
			static std::string name("integer");
			return name;
		}

		case Type::STRING: {
			static std::string name("string");
			return name;
		}
	}

	static std::string name("<invalid>");
	return name;
}
