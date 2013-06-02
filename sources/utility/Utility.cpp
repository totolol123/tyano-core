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
#include "utility/Utility.hpp"


namespace ts {

template<typename T>
bool _lexicalStringCast(const std::string& string, T& value) {
	try {
		value = boost::lexical_cast<T>(string);
	}
	catch (boost::bad_lexical_cast& e) {
		return false;
	}

	return true;
}


bool operator >> (const std::string& string, bool& value) {
	if (string == "no") {
		value = false;
		return true;
	}
	if (string == "yes") {
		value = true;
		return true;
	}

	return false;
}


bool operator >> (const std::string& string, int8_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, int16_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, int32_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, int64_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, uint8_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, uint16_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, uint32_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, uint64_t& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, float& value) {
	return _lexicalStringCast(string, value);
}


bool operator >> (const std::string& string, double& value) {
	return _lexicalStringCast(string, value);
}

} // namespace ts
