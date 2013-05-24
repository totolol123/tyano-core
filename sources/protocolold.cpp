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

#include "protocolold.h"
#include "rsa.h"

#include "outputmessage.h"
#include "connection.h"

#include "game.h"
#include "server.h"


LOGGER_DEFINITION(ProtocolOld);


#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t ProtocolOld::protocolOldCount = 0;
#endif


void ProtocolOld::deleteProtocolTask()
{
	LOGt("ProtocolOld::deleteProtocolTask()");

	Protocol::deleteProtocolTask();
}

void ProtocolOld::disconnectClient(uint8_t error, const char* message)
{
	if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
	{
		TRACK_MESSAGE(output);
		output->AddByte(error);
		output->AddString(message);
		OutputMessagePool::getInstance()->send(output);
	}

	getConnection()->close();
}

bool ProtocolOld::parseFirstPacket(NetworkMessage& msg)
{
	if(server.game().getGameState() == GAME_STATE_SHUTDOWN)
	{
		getConnection()->close();
		return false;
	}

	/*uint16_t operatingSystem = */msg.GetU16();
	uint16_t version = msg.GetU16();
	msg.SkipBytes(12);
	if(version <= 760)
		disconnectClient(0x0A, CLIENT_VERSION_STRING);

	if(!RSA_decrypt(msg))
	{
		getConnection()->close();
		return false;
	}

	uint32_t key[4] = {msg.GetU32(), msg.GetU32(), msg.GetU32(), msg.GetU32()};
	enableXTEAEncryption();
	setXTEAKey(key);

	if(version <= 822)
		disableChecksum();

	disconnectClient(0x0A, CLIENT_VERSION_STRING);
	return false;
}

void ProtocolOld::onRecvFirstMessage(NetworkMessage& msg)
{
	parseFirstPacket(msg);
}

