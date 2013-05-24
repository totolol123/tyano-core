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

#ifndef _PROTOCOLOLD_H
#define _PROTOCOLOLD_H

#include "protocol.h"

class Connection;
class NetworkMessage;
class OutputMessage;

typedef std::shared_ptr<Connection>    Connection_ptr;


class ProtocolOld : public Protocol
{
	public:
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
		static uint32_t protocolOldCount;
#endif
		virtual void onRecvFirstMessage(NetworkMessage& msg);

		ProtocolOld(const Connection_ptr& connection): Protocol(connection)
		{
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
			protocolOldCount++;
#endif
		}

		virtual ~ProtocolOld()
		{
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
			protocolOldCount--;
#endif
		}

		enum {isSingleSocket = false};
		enum {hasChecksum = false};

	private:
		virtual void deleteProtocolTask();

		void disconnectClient(uint8_t error, const char* message);
		bool parseFirstPacket(NetworkMessage& msg);


		LOGGER_DECLARATION;
};

class ProtocolOldLogin : public ProtocolOld
{
	public:
		ProtocolOldLogin(const Connection_ptr& connection) : ProtocolOld(connection) {}

		enum {protocolId = 0x01};
		static const char* protocolName() {return "old login protocol";}
};

class ProtocolOldGame : public ProtocolOld
{
	public:
		ProtocolOldGame(const Connection_ptr& connection) : ProtocolOld(connection) {}

		enum {protocolId = 0x0A};
		static const char* protocolName() {return "old game protocol";}
};

#endif // _PROTOCOLOLD_H
