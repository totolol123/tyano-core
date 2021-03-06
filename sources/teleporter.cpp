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
#include "teleporter.h"

#include "fileloader.h"
#include "game.h"
#include "server.h"
#include "tile.h"

LOGGER_DEFINITION(Teleporter);



Teleporter::Teleporter(const ItemKindPC& kind)
	: Item(kind)
{}


Teleporter::~Teleporter() {
}


Teleporter* Teleporter::asTeleporter() {
	return this;
}


const Teleporter* Teleporter::asTeleporter() const {
	return this;
}


auto Teleporter::getClassAttributes() -> ClassAttributesP {
	return Item::getClassAttributes();
}


const std::string& Teleporter::getClassName() {
	static const std::string name("Teleporter");
	return name;
}


const Position& Teleporter::getDestination() const {
	return _destination;
}


Attr_ReadValue Teleporter::readAttr(AttrTypes_t attribute, PropStream& stream) {
	switch (attribute) {
	case ATTR_TELE_DEST: {
		uint16_t x, y;
		uint8_t z;

		if (!stream.GET_VALUE(x) || !stream.GET_VALUE(y) || !stream.GET_VALUE(z)) {
			return ATTR_READ_ERROR;
		}

		if (!Position::isValid(x, y, z)) {
			LOGe("Teleporter at position " << getPosition() << " has invalid destination " << x << "/" << y << "/" << z << ".");
			return ATTR_READ_ERROR;
		}

		_destination = Position(x, y, z);
		return ATTR_READ_CONTINUE;
	}

	default:
		return Item::readAttr(attribute, stream);
	}
}


void Teleporter::serializeAttr(PropWriteStream& stream) const {
	Item::serializeAttr(stream);

	stream.ADD_UCHAR(ATTR_TELE_DEST);
	stream.ADD_VALUE(static_cast<uint16_t>(_destination.x));
	stream.ADD_VALUE(static_cast<uint16_t>(_destination.y));
	stream.ADD_VALUE(static_cast<uint8_t>(_destination.z));
}


void Teleporter::setDestination(const Position& destination) {
	_destination = destination;
}
