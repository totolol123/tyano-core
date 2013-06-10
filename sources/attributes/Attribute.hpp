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

#ifndef _ATTRIBUTES_ATTRIBUTE_HPP
#define _ATTRIBUTES_ATTRIBUTE_HPP

namespace ts {
namespace attributes {

	class Attribute;

	typedef std::shared_ptr<Attribute>        AttributeP;
	typedef std::shared_ptr<const Attribute>  AttributePC;


	enum class Type : uint8_t {
		BOOLEAN,
		FLOAT,
		INTEGER,
		STRING,
	};



	class Attribute {

	public:

		static const std::string& getTypeName(Type type);

		Attribute(const std::string& name, Type type);

		const std::string& getName () const;
		Type               getType () const;


	private:

		Attribute(const Attribute&) = delete;
		Attribute(Attribute&&) = delete;


		const std::string _name;
		const Type        _type;

	};

} // namespace attributes
} // namespace ts



namespace std {

	template <>
	struct equal_to<ts::attributes::Attribute> : public function<bool(const ts::attributes::Attribute&, const ts::attributes::Attribute&)> {
		result_type operator()(first_argument_type a, second_argument_type b) const {
			return equal_to<std::string>()(a.getName(), b.getName());
		}
	};

	template <>
	struct hash<ts::attributes::Attribute> : public function<size_t(const ts::attributes::Attribute&)> {
		result_type operator()(argument_type attribute) const {
			return hash<std::string>()(attribute.getName());
		}
	};

} // namespace std

#endif // _ATTRIBUTES_ATTRIBUTE_HPP
