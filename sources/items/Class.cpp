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
#include "items/Class.hpp"

#include "attributes/Scheme.hpp"

using namespace items;


Class::Class(const std::string& name, const attributes::SchemeP& attributesScheme)
	: _attributesScheme(attributesScheme),
	  _name(name)
{
	assert(attributesScheme != nullptr);
}


Class::~Class() {
}


attributes::SchemeP Class::getAttributesScheme() const {
	return _attributesScheme;
}


const std::string& Class::getName() const {
	return _name;
}
