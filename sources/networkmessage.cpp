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

#include "networkmessage.h"
#include "position.h"
#include "rsa.h"

#include "container.h"
#include "creature.h"
#include "items.h"
#include "map.h"
#include "player.h"
#include "server.h"


LOGGER_DEFINITION(NetworkMessage);



int32_t NetworkMessage::decodeHeader()
{
	int32_t size = (int32_t)(m_MsgBuf[0] | m_MsgBuf[1] << 8);
	m_MsgSize = size;
	return size;
}

/******************************************************************************/
std::string NetworkMessage::GetString(uint16_t size/* = 0*/)
{
	if(!size)
		size = GetU16();

	if(size >= (NETWORKMESSAGE_MAXSIZE - m_ReadPos))
		return std::string();

	char* v = (char*)(m_MsgBuf + m_ReadPos);
	m_ReadPos += size;
	return std::string(v, size);
}

Position NetworkMessage::GetPosition()
{
	Position pos;
	pos.x = GetU16();
	pos.y = GetU16();
	pos.z = GetByte();

	assert(pos.isValid());

	return pos;
}


ExtendedPosition NetworkMessage::GetExtendedPosition(bool detailed /* = true */) {
	uint16_t x = GetU16();
	uint16_t y = GetU16();
	uint8_t  z = GetByte();
	uint16_t spriteId;
	uint8_t index;

	if (detailed) {
		spriteId = GetSpriteId();
		index = GetByte();
	}
	else {
		spriteId = 0;
		index = 0;
	}

	if (x == 0xFFFF) {
		if ((y & 0x40) != 0) {
			// index in backpack
			// spriteId is set to the client's item kind ID - do we need that?

			if (z != index) {
				// this indeed happens, e.g. when moving items between contains ('z' is destination container item index, 'index' is origin container item index)
			}

			if ((y & ~0x4F) != 0) {
				LOGw("Received invalid position " << x << "/" << y << "/" << z << " from player.");
				return ExtendedPosition::nowhere();
			}

			uint8_t backpack = y & 0x0F;
			index = z;

			return ExtendedPosition::forBackpack(backpack, index);
		}
		else if (y == 0 && z == 0) {
			// item to find in backpacks

			if (!detailed) {
				LOGw("Received unexpected backpack search from player.");
				return ExtendedPosition::nowhere();
			}

			ItemKindPC kind = server.items().getKindByClientId(spriteId);
			if (kind == nullptr) {
				LOGw("Received backpack search for unknown client item kind ID " << spriteId << ".");
				return ExtendedPosition::nowhere();
			}

			int8_t fluidType = -1;
			if (kind->isFluidContainer()) {
				if (index < sizeof(reverseFluidMap) / sizeof(int8_t)) {
					fluidType = reverseFluidMap[index];
				}
				else {
					LOGw("Received invalid fluid type " << index << " from player.");
					fluidType = -1;
				}
			}

			return ExtendedPosition::forBackpackSearch(spriteId, fluidType);
		}
		else {
			// player slot

			// spriteId is set to the client's item kind ID - do we need that?
			assert(index == 0);

			if (y < +slots_t::FIRST || y > +slots_t::LAST) {
				LOGw("Received invalid slot " << y << " from player.");
				return ExtendedPosition::nowhere();
			}

			return ExtendedPosition::forSlot(slots_t(y));
		}
	}

	// spriteId is set to the client's item kind ID - do we need that?

	if (!StackPosition::isValid(x, y, z, index)) {
		LOGw("Received invalid position " << x << "/" << y << "/" << z << "#" << index << " from player.");
		return ExtendedPosition::nowhere();
	}

	return ExtendedPosition::forStackPosition(StackPosition(x, y, z, index));
}


/******************************************************************************/

void NetworkMessage::AddString(const char* value)
{
	uint32_t stringLen = (uint32_t)strlen(value);
	if(!canAdd(stringLen + 2) || stringLen > 8192)
		return;

	AddU16(stringLen);
	strcpy((char*)(m_MsgBuf + m_ReadPos), value);
	m_ReadPos += stringLen;
	m_MsgSize += stringLen;
}

void NetworkMessage::AddBytes(const char* bytes, uint32_t size)
{
	if(!canAdd(size) || size > 8192)
		return;

	memcpy(m_MsgBuf + m_ReadPos, bytes, size);
	m_ReadPos += size;
	m_MsgSize += size;
}

void NetworkMessage::AddPaddingBytes(uint32_t n)
{
	if(!canAdd(n))
		return;

	memset((void*)&m_MsgBuf[m_ReadPos], 0x33, n);
	m_MsgSize = m_MsgSize + n;
}

void NetworkMessage::AddPosition(const Position& pos) {
	assert(pos.isValid());

	AddU16(pos.x);
	AddU16(pos.y);
	AddByte(pos.z);
}

void NetworkMessage::AddPositionEx(const StackPosition& position) {
	assert(position.isValid());

	AddU16(position.x);
	AddU16(position.y);
	AddByte(position.z);
	AddByte(position.index);
}

void NetworkMessage::AddItem(const Item* item)
{
	ItemKindPC kind = item->getKind();
	AddU16(kind->clientId);
	if(kind->stackable || kind->isRune())
		AddByte(item->getSubType());
	else if(kind->isSplash() || kind->isFluidContainer())
		AddByte(fluidMap[item->getSubType() % 8]);
}

void NetworkMessage::AddItemId(const Item *item)
{
	AddU16(item->getKind()->clientId);
}

void NetworkMessage::AddItemId(uint16_t itemId)
{
	ItemKindPC kind = server.items()[itemId];
	AddU16(kind ? kind->clientId : 0);
}
