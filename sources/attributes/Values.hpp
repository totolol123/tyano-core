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

#ifndef _ATTRIBUTES_VALUES_HPP
#define _ATTRIBUTES_VALUES_HPP

namespace ts {
namespace attributes {

	class      Attribute;
	class      Scheme;
	enum class Type : uint8_t;

	typedef std::shared_ptr<Scheme>  SchemeP;


	class Values {

	public:

		typedef std::unordered_map<const Attribute*,boost::any>  Entries;
		typedef Entries::value_type                              Entry;


		Values(const SchemeP& scheme);

		bool               contains   (const std::string& name) const;
		const bool*        getBoolean (const std::string& name) const;
		const Entries*     getEntries () const;
		const Entry*       getEntry   (const std::string& name) const;
		const float*       getFloat   (const std::string& name) const;
		const int32_t*     getInteger (const std::string& name) const;
		const std::string* getString  (const std::string& name) const;
		bool               isEmpty    () const;
		void               remove     (const std::string& name);
		void               set        (const std::string& name, bool value);
		void               set        (const std::string& name, float value);
		void               set        (const std::string& name, int32_t value);
		void               set        (const std::string& name, const std::string& value);

		Values& operator = (const Values& values);
		Values& operator = (Values&& values);


	private:

		typedef std::unique_ptr<Entries> EntriesP;


		Values(const Values&) = delete;
		Values(Values&&) = delete;


		template <typename T>
		const T* _get(const std::string& name, Type type) const;

		template <typename T>
		void _set(const std::string& name, Type type, const T& value);


		LOGGER_DECLARATION;


		EntriesP      _entries;
		const SchemeP _scheme;

	};

} // namespace attributes
} // namespace ts

#endif // _ATTRIBUTES_VALUES_HPP
