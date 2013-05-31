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

#ifndef _ATTRIBUTES_SCHEME_HPP
#define _ATTRIBUTES_SCHEME_HPP

namespace attributes {

	class Attribute;
	class Scheme;

	typedef std::shared_ptr<Scheme>  SchemeP;


	class Scheme {

	public:

		typedef std::unordered_set<Attribute>  Attributes;
		typedef std::unique_ptr<Attributes>    AttributesP;


		Scheme(AttributesP attributes);
		~Scheme();

		const Attribute* getAttributeByName (const std::string& name) const;


	private:

		typedef std::unordered_map<std::reference_wrapper<const std::string>,const Attribute*,std::hash<std::string>,std::equal_to<std::string>>  AttributesByName;


		static AttributesByName mapAttributesByName (const Attributes& attributes);


		Scheme(const Scheme&) = delete;
		Scheme(Scheme&&) = delete;


		const AttributesP      _attributes;
		const AttributesByName _attributesByName;

	};

} // namespace attributes

#endif // _ATTRIBUTES_SCHEME_HPP
