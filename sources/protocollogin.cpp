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

#include "protocollogin.h"
#include "tools.h"
#include "rsa.h"

#include "iologindata.h"
#include "ioban.h"

#include "outputmessage.h"
#include "connection.h"

#include "account.h"
#include "configmanager.h"
#include "game.h"
#include "server.h"


LOGGER_DEFINITION(ProtocolLogin);


extern IpList serverIps;

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t ProtocolLogin::protocolLoginCount = 0;
#endif

void ProtocolLogin::deleteProtocolTask()
{
	LOGt("ProtocolLogin::deleteProtocolTask()");

	Protocol::deleteProtocolTask();
}

void ProtocolLogin::disconnectClient(uint8_t error, const char* message)
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(output)
	{
		TRACK_MESSAGE(output);
		output->AddByte(error);
		output->AddString(message);
		OutputMessagePool::getInstance()->send(output);
	}

	getConnection()->close();
}

bool ProtocolLogin::parseFirstPacket(NetworkMessage& msg)
{
	if(server.game().getGameState() == GAME_STATE_SHUTDOWN)
	{
		getConnection()->close();
		return false;
	}

	uint32_t clientIp = getConnection()->getIP();
	/*uint16_t operatingSystem = msg.GetU16();*/msg.SkipBytes(2);
	uint16_t version = msg.GetU16();

	msg.SkipBytes(12);
	if(!RSA_decrypt(msg))
	{
		getConnection()->close();
		return false;
	}

	uint32_t key[4] = {msg.GetU32(), msg.GetU32(), msg.GetU32(), msg.GetU32()};
	enableXTEAEncryption();
	setXTEAKey(key);

	std::string name = msg.GetString(), password = msg.GetString();
	if(name.empty())
	{
		if(!server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER))
		{
			disconnectClient(0x0A, "Invalid account name.");
			return false;
		}

		name = "1";
		password = "1";
	}

	if(version < CLIENT_VERSION_MIN || version > CLIENT_VERSION_MAX)
	{
		disconnectClient(0x0A, CLIENT_VERSION_STRING);
		return false;
	}

	if(server.game().getGameState() < GAME_STATE_NORMAL)
	{
		disconnectClient(0x0A, "Server is just starting up, please wait.");
		return false;
	}

	if(server.game().getGameState() == GAME_STATE_MAINTAIN)
	{
		disconnectClient(0x0A, "Server is under maintenance, please re-connect in a while.");
		return false;
	}

	if(ConnectionManager::getInstance()->isDisabled(clientIp, protocolId))
	{
		disconnectClient(0x0A, "Too many connections attempts from your IP address, please try again later.");
		return false;
	}

	if(IOBan::getInstance()->isIpBanished(clientIp))
	{
		disconnectClient(0x0A, "Your IP is banished!");
		return false;
	}

	uint32_t id = 1;
	if(!IOLoginData::getInstance()->getAccountId(name, id))
	{
		ConnectionManager::getInstance()->addAttempt(clientIp, protocolId, false);
		disconnectClient(0x0A, "Invalid account name.");
		return false;
	}

	AccountP account = IOLoginData::getInstance()->loadAccount(id);
	if (account == nullptr) {
		disconnectClient(0x0A, "Invalid account name.");
		return false;
	}

	if(!encryptTest(password, account->getPassword()))
	{
		ConnectionManager::getInstance()->addAttempt(clientIp, protocolId, false);
		disconnectClient(0x0A, "Invalid password.");
		return false;
	}

	Ban ban;
	ban.value = account->getId();

	ban.type = BAN_ACCOUNT;
	if(IOBan::getInstance()->getData(ban) && !IOLoginData::getInstance()->hasFlag(account->getId(), PlayerFlag_CannotBeBanned))
	{
		bool deletion = ban.expires < 0;
		std::string name_ = "Automatic ";
		if(!ban.adminId)
			name_ += (deletion ? "deletion" : "banishment");
		else
			IOLoginData::getInstance()->getNameByGuid(ban.adminId, name_, true);

		char buffer[500 + ban.comment.length()];
		sprintf(buffer, "Your account has been %s at:\n%s by: %s,\nfor the following reason:\n%s.\nThe action taken was:\n%s.\nThe comment given was:\n%s.\nYour %s%s.",
			(deletion ? "deleted" : "banished"), formatDateShort(ban.added).c_str(), name_.c_str(),
			getReason(ban.reason).c_str(), getAction(ban.action, false).c_str(), ban.comment.c_str(),
			(deletion ? "account won't be undeleted" : "banishment will be lifted at:\n"),
			(deletion ? "." : formatDateShort(ban.expires, true).c_str()));

		disconnectClient(0x0A, buffer);
		return false;
	}

	//Remove premium days
	IOLoginData::getInstance()->removePremium(*account);

	const Account::Characters& characters = account->getCharacters();
	if(!server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER) && characters.empty())
	{
		disconnectClient(0x0A, std::string("This account does not contain any character yet.\nCreate a new character on the "
			+ server.configManager().getString(ConfigManager::SERVER_NAME) + " website at " + server.configManager().getString(ConfigManager::URL) + ".").c_str());
		return false;
	}

	ConnectionManager::getInstance()->addAttempt(clientIp, protocolId, true);
	if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
	{
		TRACK_MESSAGE(output);
		output->AddByte(0x14);

		char motd[1300];
		sprintf(motd, "%d\n%s", server.game().getMotdId(), server.configManager().getString(ConfigManager::MOTD).c_str());
		output->AddString(motd);

		uint32_t serverIp = serverIps[0].first;
		for(IpList::iterator it = serverIps.begin(); it != serverIps.end(); ++it)
		{
			if((it->first & it->second) != (clientIp & it->second))
				continue;

			serverIp = it->first;
			break;
		}

		//Add char list
		output->AddByte(0x64);
		if(server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER) && id != 1)
		{
			output->AddByte(characters.size() + 1);
			output->AddString("Account Manager");
			output->AddString(server.configManager().getString(ConfigManager::SERVER_NAME));
			output->AddU32(serverIp);
			output->AddU16(server.configManager().getNumber(ConfigManager::GAME_PORT));
		}
		else
			output->AddByte((uint8_t)characters.size());

		for (auto it = characters.cbegin(); it != characters.cend(); ++it) {
			auto& character = *it;

#ifndef __LOGIN_SERVER__
			output->AddString(character->getName());
			output->AddString(character->getType());
			output->AddU32(serverIp);
			output->AddU16(server.configManager().getNumber(ConfigManager::GAME_PORT));
#else
			if(version < it->second->getVersionMin() || version > it->second->getVersionMax())
				continue;

			output->AddString(it->first);
			output->AddString(it->second->getName());
			output->AddU32(it->second->getAddress());
			output->AddU16(it->second->getPort());
#endif
		}

		//Add premium days
		if(server.configManager().getBool(ConfigManager::FREE_PREMIUM))
			output->AddU16(std::numeric_limits<uint16_t>::max()); //client displays free premium
		else
			output->AddU16(account->getPremiumDays());

		OutputMessagePool::getInstance()->send(output);
	}

	getConnection()->close();
	return true;
}
