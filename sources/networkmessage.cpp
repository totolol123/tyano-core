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
#include "player.h"
#include "server.h"

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
	return pos;
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

void NetworkMessage::AddPosition(const Position& pos)
{
	AddU16(pos.x);
	AddU16(pos.y);
	AddByte(pos.z);
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
