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

#include "itemattributes.h"
#include "fileloader.h"

ItemAttributes::ItemAttributes(const ItemAttributes& o)
{
	if(o.attributes)
		attributes = new AttributeMap(*o.attributes);
}

void ItemAttributes::createAttributes()
{
	if(!attributes)
		attributes = new AttributeMap;
}

void ItemAttributes::eraseAttribute(const std::string& key)
{
	if(!attributes)
		return;
	
	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		attributes->erase(it);
}

void ItemAttributes::setAttribute(const std::string& key, boost::any value)
{
	createAttributes();
	(*attributes)[key].set(value);
}

boost::any ItemAttributes::getAttribute(const std::string& key) const
{
	if(!attributes)
		return boost::any();

	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		return it->second.get();

	return boost::any();
}

void ItemAttributes::setAttribute(const std::string& key, const std::string& value)
{
	createAttributes();
	(*attributes)[key].set(value);
}

void ItemAttributes::setAttribute(const std::string& key, int32_t value)
{
	createAttributes();
	(*attributes)[key].set(value);
}

void ItemAttributes::setAttribute(const std::string& key, float value)
{
	createAttributes();
	(*attributes)[key].set(value);
}

void ItemAttributes::setAttribute(const std::string& key, bool value)
{
	createAttributes();
	(*attributes)[key].set(value);
}

const std::string* ItemAttributes::getStringAttribute(const std::string& key) const
{
	if(!attributes)
		return nullptr;

	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		return it->second.getString();

	return nullptr;
}

const int32_t* ItemAttributes::getIntegerAttribute(const std::string& key) const
{
	if(!attributes)
		return nullptr;

	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		return it->second.getInteger();

	return nullptr;
}

const float* ItemAttributes::getFloatAttribute(const std::string& key) const
{
	if(!attributes)
		return nullptr;

	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		return it->second.getFloat();

	return nullptr;
}

const bool* ItemAttributes::getBooleanAttribute(const std::string& key) const
{
	if(!attributes)
		return nullptr;

	AttributeMap::iterator it = attributes->find(key);
	if(it != attributes->end())
		return it->second.getBoolean();

	return nullptr;
}

ItemAttribute& ItemAttribute::operator=(const ItemAttribute& o)
{
	if(&o == this)
		return *this;

	data = o.data;

	return *this;
}

void ItemAttribute::clear() {
	set(false);
}

void ItemAttribute::set(const std::string& s)
{
	data = s;
}

void ItemAttribute::set(int32_t i)
{
	data = i;
}

void ItemAttribute::set(float f)
{
	data = f;
}

void ItemAttribute::set(bool b)
{
	data = b;
}

void ItemAttribute::set(boost::any a)
{
	if(a.empty()) {
		clear();
		return;
	}

	if(a.type() == typeid(std::string))
	{
		data = boost::any_cast<std::string>(a);
	}
	else if(a.type() == typeid(int32_t))
	{
		data = boost::any_cast<int32_t>(a);
	}
	else if(a.type() == typeid(float))
	{
		data = boost::any_cast<float>(a);
	}
	else if(a.type() == typeid(bool))
	{
		data = boost::any_cast<bool>(a);
	}
	else {
		clear();
	}
}

const std::string* ItemAttribute::getString() const
{
	if(data.which() != TypeString)
		return nullptr;

	const std::string& value = boost::get<std::string>(data);
	return &value;
}

const int32_t* ItemAttribute::getInteger() const
{
	if(data.which() != TypeInt)
		return nullptr;

	const int32_t& value = boost::get<int32_t>(data);
	return &value;
}

const float* ItemAttribute::getFloat() const
{
	if(data.which() != TypeFloat)
		return nullptr;

	const float& value = boost::get<float>(data);
	return &value;
}

const bool* ItemAttribute::getBoolean() const
{
	if(data.which() != TypeBool)
		return nullptr;

	const bool& value = boost::get<bool>(data);
	return &value;
}

boost::any ItemAttribute::get() const
{
	switch (data.which()) {
	case TypeBool:
		return getBoolean();

	case TypeFloat:
		return getFloat();

	case TypeInt:
		return getInteger();

	case TypeString:
		return getString();
	}

	return boost::any();
}

bool ItemAttributes::unserializeMap(PropStream& stream)
{
	uint16_t n;
	if(!stream.GET_USHORT(n))
		return true;

	createAttributes();
	while(n--)
	{
		std::string key;
		if(!stream.GET_STRING(key))
			return false;

		ItemAttribute attr;
		if(!attr.unserialize(stream))
			return false;

		(*attributes)[key] = attr;
	}

	return true;
}

void ItemAttributes::serializeMap(PropWriteStream& stream) const
{
	stream.ADD_USHORT((uint16_t)std::min((size_t)0xFFFF, attributes->size()));
	AttributeMap::const_iterator it = attributes->begin();
	for(int32_t i = 0; it != attributes->end() && i <= 0xFFFF; ++it, ++i)
	{
		std::string key = it->first;
		if(key.size() > 0xFFFF)
			stream.ADD_STRING(key.substr(0, 0xFFFF));
		else
			stream.ADD_STRING(key);

		it->second.serialize(stream);
	}
}

bool ItemAttribute::unserialize(PropStream& stream)
{
	uint8_t type = 0;
	stream.GET_UCHAR(type);

	switch (type)
	{
		case SerializedTypeString:
		{
			std::string v;
			if(!stream.GET_LSTRING(v))
				return false;

			set(v);
			break;
		}
		case SerializedTypeInt:
		{
			int32_t v;
			if(!stream.GET_VALUE(v))
				return false;

			set(v);
			break;
		}
		case SerializedTypeFloat:
		{
			float v;
			if(!stream.GET_VALUE(v))
				return false;

			set(v);
			break;
		}
		case SerializedTypeBool:
		{
			uint8_t v;
			if(!stream.GET_UCHAR(v))
				return false;

			set(v != 0);
			break;
		}

		default:
			break;
	}

	return true;
}

void ItemAttribute::serialize(PropWriteStream& stream) const
{
	SerializedType type;
	switch (data.which()) {
	case TypeBool:
		type = SerializedTypeBool;
		break;
	case TypeFloat:
		type = SerializedTypeFloat;
		break;
	case TypeInt:
		type = SerializedTypeInt;
		break;
	case TypeString:
		type = SerializedTypeString;
		break;
	default:
		type = SerializedTypeNone;
	}

	stream.ADD_UCHAR((uint8_t)type);
	switch(type)
	{
		case SerializedTypeString:
			stream.ADD_LSTRING(*getString());
			break;
		case SerializedTypeInt:
			stream.ADD_ULONG(*(uint32_t*)getInteger());
			break;
		case SerializedTypeFloat:
			stream.ADD_ULONG(*(uint32_t*)getFloat());
			break;
		case SerializedTypeBool:
			stream.ADD_UCHAR(*(uint8_t*)getBoolean());
			break;
		case SerializedTypeNone:
			break;
	}
}
