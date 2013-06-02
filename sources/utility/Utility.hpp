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

#ifndef _UTILITY_UTILITY_HPP
#define _UTILITY_UTILITY_HPP

namespace ts {

	bool operator >> (const std::string& string, bool& value);
	bool operator >> (const std::string& string, int8_t& value);
	bool operator >> (const std::string& string, int16_t& value);
	bool operator >> (const std::string& string, int32_t& value);
	bool operator >> (const std::string& string, int64_t& value);
	bool operator >> (const std::string& string, uint8_t& value);
	bool operator >> (const std::string& string, uint16_t& value);
	bool operator >> (const std::string& string, uint32_t& value);
	bool operator >> (const std::string& string, uint64_t& value);
	bool operator >> (const std::string& string, float& value);
	bool operator >> (const std::string& string, double& value);

} // namespace ts

#endif // _UTILITY_UTILITY_HPP
