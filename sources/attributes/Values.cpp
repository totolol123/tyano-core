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
#include "attributes/Values.hpp"

#include "attributes/Attribute.hpp"
#include "attributes/Scheme.hpp"


LOGGER_DEFINITION(attributes::Values);

using namespace attributes;



Values::Values(const SchemeP& scheme)
	: _scheme(scheme)
{
	assert(scheme);
}


bool Values::contains(const std::string& name) const {
	auto attribute = _scheme->getAttributeByName(name);
	if (attribute == nullptr) {
		LOGe("Cannot access non-existent attribute '" << name << "'.");
		return nullptr;
	}

	return (_entries != nullptr && _entries->find(attribute) != _entries->cend());
}


template <typename T>
const T* Values::_get(const std::string& name, Type type) const {
	auto entry = getEntry(name);
	if (entry == nullptr) {
		return nullptr;
	}

	auto& attribute = *entry->first;
	if (attribute.getType() != type) {
		LOGe("Cannot access " << Attribute::getTypeName(attribute.getType()) << " attribute '" << attribute.getName() << "' as " << Attribute::getTypeName(type) << ".");
		return nullptr;
	}

	return boost::any_cast<T>(&entry->second);
}


const bool* Values::getBoolean(const std::string& name) const {
	return _get<bool>(name, Type::BOOLEAN);
}


const Values::Entries* Values::getEntries() const {
	return _entries.get();
}


const Values::Entry* Values::getEntry(const std::string& name) const {
	auto attribute = _scheme->getAttributeByName(name);
	if (attribute == nullptr) {
		LOGe("Cannot access non-existent attribute '" << name << "'.");
		return nullptr;
	}

	if (_entries == nullptr) {
		return nullptr;
	}

	auto it = _entries->find(attribute);
	if (it == _entries->cend()) {
		return nullptr;
	}

	return &(*it);
}


const float* Values::getFloat(const std::string& name) const {
	return _get<float>(name, Type::FLOAT);
}


const int32_t* Values::getInteger(const std::string& name) const {
	return _get<int32_t>(name, Type::INTEGER);
}


const std::string* Values::getString(const std::string& name) const {
	return _get<std::string>(name, Type::STRING);
}


Values& Values::operator =(const Values& values) {
	if (&values == this) {
		return *this;
	}

	if (values._scheme != _scheme) {
		LOGe("Cannot copy attribute values between different schemes.");
		return *this;
	}

	if (values._entries != nullptr) {
		_entries = EntriesP(new Entries(*values._entries));
	}
	else {
		_entries = nullptr;
	}

	return *this;
}


Values& Values::operator =(Values&& values) {
	if (values._scheme != _scheme) {
		LOGe("Cannot move attribute values between different schemes.");
		return *this;
	}

	_entries = std::move(values._entries);

	return *this;
}


bool Values::isEmpty() const {
	return (_entries == nullptr || _entries->empty());
}


void Values::remove(const std::string& name) {
	auto attribute = _scheme->getAttributeByName(name);
	if (attribute == nullptr) {
		LOGe("Cannot remove non-existent attribute '" << name << "'.");
		return;
	}

	if (_entries == nullptr) {
		return;
	}

	_entries->erase(attribute);
	if (_entries->empty()) {
		_entries = nullptr;
	}
}


template <typename T>
void Values::_set(const std::string& name, Type type, const T& value) {
	auto attribute = _scheme->getAttributeByName(name);
	if (attribute == nullptr) {
		LOGe("Cannot set non-existent attribute '" << name << "'.");
		return;
	}

	if (attribute->getType() != type) {
		LOGe("Cannot set " << Attribute::getTypeName(attribute->getType()) << " attribute '" << attribute->getName() << "' to " << Attribute::getTypeName(type) << ".");
		return;
	}

	if (_entries == nullptr) {
		_entries = EntriesP(new Entries);
	}

	(*_entries)[attribute] = value;
}


void Values::set(const std::string& name, bool value) {
	_set<bool>(name, Type::BOOLEAN, value);
}


void Values::set(const std::string& name, float value) {
	_set<float>(name, Type::FLOAT, value);
}


void Values::set(const std::string& name, int32_t value) {
	_set<int32_t>(name, Type::INTEGER, value);
}


void Values::set(const std::string& name, const std::string& value) {
	_set<std::string>(name, Type::STRING, value);
}
