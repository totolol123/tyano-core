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

#ifndef _ITEMATTRIBUTES_H
#define _ITEMATTRIBUTES_H

class PropWriteStream;
class PropStream;

class ItemAttribute
{
	public:
		ItemAttribute() {}
		ItemAttribute(const ItemAttribute& o) {*this = o;}
		~ItemAttribute() {clear();}

		ItemAttribute(const std::string& s): data(s) {};
		ItemAttribute(int32_t i): data(i) {}
		ItemAttribute(float f): data(f) {}
		ItemAttribute(bool b): data(b) {}

		ItemAttribute& operator=(const ItemAttribute& o);

		void serialize(PropWriteStream& stream) const;
		bool unserialize(PropStream& stream);

		void clear();

		void set(const std::string& s);
		void set(int32_t i);
		void set(float f);
		void set(bool b);
		void set(boost::any a);

		const std::string* getString() const;
		const int32_t* getInteger() const;
		const float* getFloat() const;
		const bool* getBoolean() const;
		boost::any get() const;

	private:

		typedef enum {
			TypeBool,
			TypeFloat,
			TypeInt,
			TypeString,
		} Type;

		typedef enum {
			SerializedTypeNone = 0,
			SerializedTypeString = 1,
			SerializedTypeInt = 2,
			SerializedTypeFloat = 3,
			SerializedTypeBool = 4
		} SerializedType;

		boost::variant<bool,float,int32_t,std::string> data;
};

class ItemAttributes
{
	public:
		ItemAttributes(): attributes(nullptr) {}
		ItemAttributes(const ItemAttributes &i);
		virtual ~ItemAttributes() {delete attributes;}

		void serializeMap(PropWriteStream& stream) const;
		bool unserializeMap(PropStream& stream);

		void eraseAttribute(const std::string& key);
		void setAttribute(const std::string& key, boost::any value);
		boost::any getAttribute(const std::string& key) const;

		void setAttribute(const std::string& key, const std::string& value);
		void setAttribute(const std::string& key, int32_t value);
		void setAttribute(const std::string& key, float value);
		void setAttribute(const std::string& key, bool value);

		const std::string* getStringAttribute(const std::string& key) const;
		const int32_t* getIntegerAttribute(const std::string& key) const;
		const float* getFloatAttribute(const std::string& key) const;
		const bool* getBooleanAttribute(const std::string& key) const;

		bool hasStringAttribute(const std::string& key) const {return getStringAttribute(key);}
		bool hasIntegerAttribute(const std::string& key) const {return getIntegerAttribute(key);}
		bool hasFloatAttribute(const std::string& key) const {return getFloatAttribute(key);}
		bool hasBooleanAttribute(const std::string& key) const {return getBooleanAttribute(key);}

	protected:
		void createAttributes();

		typedef std::map<std::string, ItemAttribute> AttributeMap;
		AttributeMap* attributes;
};

#endif // _ITEMATTRIBUTES_H
