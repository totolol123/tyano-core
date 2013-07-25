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

#include "protocolgame.h"
#include "textlogger.h"

#include "waitlist.h"
#include "player.h"

#include "connection.h"
#include "networkmessage.h"
#include "outputmessage.h"

#include "iologindata.h"
#include "ioban.h"

#include "group.h"
#include "items.h"
#include "tile.h"
#include "house.h"
#include "outfit.h"

#include "account.h"
#include "actions.h"
#include "creatureevent.h"
#include "quests.h"

#include "chat.h"
#include "configmanager.h"
#include "dispatcher.h"
#include "game.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"
#include "task.h"


LOGGER_DEFINITION(ProtocolGame);


ProtocolGame::ProtocolGame(const Connection_ptr& connection)
	: Protocol(connection)
{
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	protocolGameCount++;
#endif
	m_eventConnect = 0;
	m_debugAssertSent = m_acceptPackets = false;
}


ProtocolGame::~ProtocolGame() {
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	protocolGameCount--;
#endif
}


template<class FunctionType>
void ProtocolGame::addGameTaskInternal(uint32_t delay, const FunctionType& func)
{
	if(delay > 0)
		server.dispatcher().addTask(Task::create(Milliseconds(delay), func));
	else
		server.dispatcher().addTask(Task::create(func));
}

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t ProtocolGame::protocolGameCount = 0;
#endif

void ProtocolGame::setPlayer(Player* p)
{
	player = p;
}

void ProtocolGame::releaseProtocol()
{
	if(player && player->client == this)
		player->client = nullptr;

	Protocol::releaseProtocol();
}

void ProtocolGame::deleteProtocolTask()
{
	player.reset();

	Protocol::deleteProtocolTask();
}

bool ProtocolGame::login(const std::string& name, uint32_t id, const std::string& password,
	OperatingSystem_t operatingSystem, uint16_t version, bool gamemaster)
{
	LOGt("ProtocolGame(" << name << ")::login(name = '" << name << "', id = " << id << ")");

	//dispatcher thread
	PlayerVector players = server.game().getPlayersByName(name);
	Player* _player = nullptr;
	if(!players.empty())
		_player = players[random_range<uint32_t>(0, (players.size() - 1))];

	if(!_player || name == "Account Manager" || server.configManager().getNumber(ConfigManager::ALLOW_CLONES) > (int32_t)players.size())
	{
		auto io = *IOLoginData::getInstance();

		AccountP account;
		if (name != "Account Manager") {
			auto accountId = io.getAccountIdByName(name);
			if (accountId == 0) {
				disconnectClient(0x14, "Your character could not be loaded.");
				return false;
			}

			account = io.loadAccount(accountId, true);
			if (account == nullptr) {
				disconnectClient(0x14, "Your character could not be loaded.");
				return false;
			}
		}

		player = new Player(account, name, this);

		player->setID();
		if(!io.loadPlayer(player.get(), name, true))
		{
			disconnectClient(0x14, "Your character could not be loaded.");
			return false;
		}

		Ban ban;
		ban.value = player->getID();
		ban.param = PLAYERBAN_BANISHMENT;

		ban.type = BAN_PLAYER;
		if(IOBan::getInstance()->getData(ban) && !player->hasFlag(PlayerFlag_CannotBeBanned))
		{
			bool deletion = ban.expires < 0;
			std::string name_ = "Automatic ";
			if(!ban.adminId)
				name_ += (deletion ? "deletion" : "banishment");
			else
				IOLoginData::getInstance()->getNameByGuid(ban.adminId, name_, true);

			char buffer[500 + ban.comment.length()];
			sprintf(buffer, "Your character has been %s at:\n%s by: %s,\nfor the following reason:\n%s.\nThe action taken was:\n%s.\nThe comment given was:\n%s.\nYour %s%s.",
				(deletion ? "deleted" : "banished"), formatDateShort(ban.added).c_str(), name_.c_str(),
				getReason(ban.reason).c_str(), getAction(ban.action, false).c_str(), ban.comment.c_str(),
				(deletion ? "character won't be undeleted" : "banishment will be lifted at:\n"),
				(deletion ? "." : formatDateShort(ban.expires, true).c_str()));

			disconnectClient(0x14, buffer);
			return false;
		}

		if(IOBan::getInstance()->isPlayerBanished(player->getGUID(), PLAYERBAN_LOCK) && id != 1)
		{
			if(server.configManager().getBool(ConfigManager::NAMELOCK_MANAGER))
			{
				player->name = "Account Manager";
				player->accountManager = MANAGER_NAMELOCK;

				player->managerNumber = id;
				player->managerString2 = name;
			}
			else
			{
				disconnectClient(0x14, "Your character has been namelocked.");
				return false;
			}
		}
		else if(player->getName() == "Account Manager" && server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER))
		{
			if(id != 1)
			{
				player->accountManager = MANAGER_ACCOUNT;
				player->managerNumber = id;
			}
			else
				player->accountManager = MANAGER_NEW;
		}

		if(gamemaster && !player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges))
		{
			disconnectClient(0x14, "You are not a gamemaster! Turn off the gamemaster mode in your IP changer.");
			return false;
		}

		if(!player->hasFlag(PlayerFlag_CanAlwaysLogin))
		{
			if(server.game().getGameState() == GAME_STATE_CLOSING)
			{
				disconnectClient(0x14, "Gameworld is just going down, please come back later.");
				return false;
			}

			if(server.game().getGameState() == GAME_STATE_CLOSED)
			{
				disconnectClient(0x14, "Gameworld is currently closed, please come back later.");
				return false;
			}
		}

		if(server.configManager().getBool(ConfigManager::ONE_PLAYER_ON_ACCOUNT) && !player->isAccountManager() &&
			!IOLoginData::getInstance()->hasCustomFlag(id, PlayerCustomFlag_CanLoginMultipleCharacters))
		{
			bool found = false;
			PlayerVector tmp = server.game().getPlayersByAccount(id);
			for(PlayerVector::iterator it = tmp.begin(); it != tmp.end(); ++it)
			{
				if((*it)->getName() != name)
					continue;

				found = true;
				break;
			}

			if(tmp.size() > 0 && !found)
			{
				disconnectClient(0x14, "You may only login with one character\nof your account at the same time.");
				return false;
			}
		}

		if(!WaitingList::getInstance()->login(player.get()))
		{
			if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
			{
				TRACK_MESSAGE(output);
				std::stringstream ss;
				ss << "Too many players online.\n" << "You are ";

				int32_t slot = WaitingList::getInstance()->getSlot(player.get());
				if(slot)
				{
					ss << "at ";
					if(slot > 0)
						ss << slot;
					else
						ss << "unknown";

					ss << " place on the waiting list.";
				}
				else
					ss << "awaiting connection...";

				output->AddByte(0x16);
				output->AddString(ss.str());
				output->AddByte(WaitingList::getTime(slot));
				OutputMessagePool::getInstance()->send(output);
			}

			getConnection()->close();
			return false;
		}

		if(!IOLoginData::getInstance()->loadPlayer(player.get(), name))
		{
			disconnectClient(0x14, "Your character could not be loaded.");
			return false;
		}

		player->setOperatingSystem(operatingSystem);
		player->setClientVersion(version);
		if(!server.game().placeCreature(player.get(), player->getLoginPosition()) && !server.game().placeCreature(player.get(), player->getMasterPosition(), false, true))
		{
			disconnectClient(0x14, "Temple position is wrong. Contact with the administration.");
			return false;

		}

		player->lastIP = player->getIP();
		player->lastLoad = OTSYS_TIME();
		player->lastLogin = std::max(time(nullptr), player->lastLogin + 1);

		m_acceptPackets = true;

		auto channels = server.chat().getChannelList(player.get());
		for (auto channel : channels) {
			if (channel->isAutojoin()) {
				channel = server.chat().addUserToChannel(player.get(), channel->getId());
				if (channel != nullptr) {
					if(channel->getId() != CHANNEL_RVR)
						player->sendChannel(channel->getId(), channel->getName());
					else
						player->sendRuleViolationsChannel(channel->getId());
				}
			}
		}

		return true;
	}
	else if(_player->client)
	{
		if(m_eventConnect || !server.configManager().getBool(ConfigManager::REPLACE_KICK_ON_LOGIN))
		{
			//A task has already been scheduled just bail out (should not be overriden)
			disconnectClient(0x14, "You are already logged in.");
			return false;
		}

		server.chat().removeUserFromAllChannels(_player);
		_player->disconnect();
		_player->isConnecting = true;

		addRef();
		m_eventConnect = server.scheduler().addTask(SchedulerTask::create(
			Milliseconds(1000), std::bind(&ProtocolGame::connect, this, _player->getID(), operatingSystem, version)));
		return true;
	}

	addRef();
	return connect(_player->getID(), operatingSystem, version);
}

bool ProtocolGame::logout(bool displayEffect, bool forceLogout)
{
	LOGt("ProtocolGame(" << player->getName() << ")::logout(displayEffect = '" << (displayEffect ? "true" : "false") << "', forceLogout = " << (forceLogout ? "true" : "false") << ")");

	//dispatcher thread
	if(!player)
		return false;

	if(!player->isRemoved())
	{
		if(!forceLogout)
		{
			if(!IOLoginData::getInstance()->hasCustomFlag(player->getAccount()->getId(), PlayerCustomFlag_CanLogoutAnytime))
			{
				if(player->getTile() != nullptr && player->getTile()->hasFlag(TILESTATE_NOLOGOUT))
				{
					player->sendCancelMessage(RET_YOUCANNOTLOGOUTHERE);
					return false;
				}

				if(player->hasCondition(CONDITION_INFIGHT))
				{
					player->sendCancelMessage(RET_YOUMAYNOTLOGOUTDURINGAFIGHT);
					return false;
				}

				if(!server.creatureEvents().playerLogout(player.get(), false)) //let the script handle the error message
					return false;
			}
			else
				server.creatureEvents().playerLogout(player.get(), false);
		}
		else if(!server.creatureEvents().playerLogout(player.get(), true))
			return false;
	}
	else
		displayEffect = false;

	if(displayEffect && !player->isGhost())
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);

	if(Connection_ptr connection = getConnection())
		connection->close();

	return server.game().removeCreature(player.get());
}

bool ProtocolGame::connect(uint32_t playerId, OperatingSystem_t operatingSystem, uint16_t version)
{
	unRef();
	m_eventConnect = 0;

	Player* _player = server.game().getPlayerByID(playerId);
	if(!_player || _player->isRemoved() || _player->client)
	{
		disconnectClient(0x14, "You are already logged in.");
		return false;
	}

	player = _player;
	player->isConnecting = false;

	player->client = this;
	player->sendCreatureAppear(player.get(), BOOST_CURRENT_FUNCTION);

	player->setOperatingSystem(operatingSystem);
	player->setClientVersion(version);

	player->lastIP = player->getIP();
	player->lastLoad = OTSYS_TIME();
	player->lastLogin = std::max(time(nullptr), player->lastLogin + 1);

	m_acceptPackets = true;
	return true;
}

void ProtocolGame::disconnect()
{
	if(getConnection())
		getConnection()->close();
}

void ProtocolGame::disconnectClient(uint8_t error, const char* message)
{
	if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
	{
		TRACK_MESSAGE(output);
		output->AddByte(error);
		output->AddString(message);
		OutputMessagePool::getInstance()->send(output);
	}

	disconnect();
}

void ProtocolGame::onConnect()
{
	if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
	{
		TRACK_MESSAGE(output);
		enableChecksum();

		output->AddByte(0x1F);
		output->AddU16(random_range(0, 0xFFFF));
		output->AddU16(0x00);
		output->AddByte(random_range(0, 0xFF));

		OutputMessagePool::getInstance()->send(output);
	}
}

void ProtocolGame::onRecvFirstMessage(NetworkMessage& msg)
{
	parseFirstPacket(msg);
}

bool ProtocolGame::parseFirstPacket(NetworkMessage& msg)
{
	if(server.game().getGameState() == GAME_STATE_SHUTDOWN)
	{
		getConnection()->close();
		return false;
	}

	OperatingSystem_t operatingSystem = (OperatingSystem_t)msg.GetU16();
	uint16_t version = msg.GetU16();
	if(!RSA_decrypt(msg))
	{
		getConnection()->close();
		return false;
	}

	uint32_t key[4] = {msg.GetU32(), msg.GetU32(), msg.GetU32(), msg.GetU32()};
	enableXTEAEncryption();
	setXTEAKey(key);

	bool gamemaster = msg.GetByte();
	std::string name = msg.GetString(), character = msg.GetString(), password = msg.GetString();

	msg.SkipBytes(6); //841- wtf?
	if(version < CLIENT_VERSION_MIN || version > CLIENT_VERSION_MAX)
	{
		disconnectClient(0x14, CLIENT_VERSION_STRING);
		return false;
	}

	if(name.empty())
	{
		if(!server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER))
		{
			disconnectClient(0x14, "Invalid account name.");
			return false;
		}

		name = "1";
		password = "1";
	}

	if(server.game().getGameState() < GAME_STATE_NORMAL)
	{
		disconnectClient(0x14, "Gameworld is just starting up, please wait.");
		return false;
	}

	if(server.game().getGameState() == GAME_STATE_MAINTAIN)
	{
		disconnectClient(0x14, "Gameworld is under maintenance, please re-connect in a while.");
		return false;
	}

	if(ConnectionManager::getInstance()->isDisabled(getIP(), protocolId))
	{
		disconnectClient(0x14, "Too many connections attempts from your IP address, please try again later.");
		return false;
	}

	if(IOBan::getInstance()->isIpBanished(getIP()))
	{
		disconnectClient(0x14, "Your IP is banished!");
		return false;
	}

	uint32_t id = 1;
	if(!IOLoginData::getInstance()->getAccountId(name, id))
	{
		ConnectionManager::getInstance()->addAttempt(getIP(), protocolId, false);
		disconnectClient(0x14, "Invalid account name.");
		return false;
	}

	LOGi("Account '" << name << "' is trying to log in...");

	std::string hash;
	if(!IOLoginData::getInstance()->getPassword(id, hash, character) || !encryptTest(password, hash))
	{
		ConnectionManager::getInstance()->addAttempt(getIP(), protocolId, false);
		disconnectClient(0x14, "Invalid password.");
		return false;
	}

	Ban ban;
	ban.value = id;

	ban.type = BAN_ACCOUNT;
	if(IOBan::getInstance()->getData(ban) && !IOLoginData::getInstance()->hasFlag(id, PlayerFlag_CannotBeBanned))
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

		disconnectClient(0x14, buffer);
		return false;
	}

	ConnectionManager::getInstance()->addAttempt(getIP(), protocolId, true);
	server.dispatcher().addTask(Task::create(std::bind(
		&ProtocolGame::login, this, character, id, password, operatingSystem, version, gamemaster)));
	return true;
}

void ProtocolGame::parsePacket(NetworkMessage &msg)
{
	if(!player || !m_acceptPackets || server.game().getGameState() == GAME_STATE_SHUTDOWN
		|| msg.getMessageLength() <= 0)
		return;

	uint8_t recvbyte = msg.GetByte();
	//a dead player cannot performs actions
	if(player->isRemoved() && recvbyte != 0x14)
		return;

	if(player->isAccountManager())
	{
		switch(recvbyte)
		{
			case 0x14:
				parseLogout(msg);
				break;

			case 0x96:
				parseSay(msg);
				break;

			default:
				sendCancelWalk();
				break;
		}
	}
	else
	{
		switch(recvbyte)
		{
			case 0x14: // logout
				parseLogout(msg);
				break;

			case 0x1E: // keep alive / ping response
				parseReceivePing(msg);
				break;

			case 0x64: // move with steps
				parseAutoWalk(msg);
				break;

			case 0x65: // move north
			case 0x66: // move east
			case 0x67: // move south
			case 0x68: // move west
				parseMove(msg, (Direction)(recvbyte - 0x65));
				break;

			case 0x69: // stop-autowalk
				addGameTask(&Game::playerStopAutoWalk, player->getID());
				break;

			case 0x6A:
				parseMove(msg, Direction::NORTH_EAST);
				break;

			case 0x6B:
				parseMove(msg, Direction::SOUTH_EAST);
				break;

			case 0x6C:
				parseMove(msg, Direction::SOUTH_WEST);
				break;

			case 0x6D:
				parseMove(msg, Direction::NORTH_WEST);
				break;

			case 0x6F: // turn north
			case 0x70: // turn east
			case 0x71: // turn south
			case 0x72: // turn west
				parseTurn(msg, (Direction)(recvbyte - 0x6F));
				break;

			case 0x78: // throw item
				parseThrow(msg);
				break;

			case 0x79: // description in shop window
				parseLookInShop(msg);
				break;

			case 0x7A: // player bought from shop
				parsePlayerPurchase(msg);
				break;

			case 0x7B: // player sold to shop
				parsePlayerSale(msg);
				break;

			case 0x7C: // player closed shop window
				parseCloseShop(msg);
				break;

			case 0x7D: // Request trade
				parseRequestTrade(msg);
				break;

			case 0x7E: // Look at an item in trade
				parseLookInTrade(msg);
				break;

			case 0x7F: // Accept trade
				parseAcceptTrade(msg);
				break;

			case 0x80: // close/cancel trade
				parseCloseTrade();
				break;

			case 0x82: // use item
				parseUseItem(msg);
				break;

			case 0x83: // use item
				parseUseItemEx(msg);
				break;

			case 0x84: // battle window
				parseBattleWindow(msg);
				break;

			case 0x85: //rotate item
				parseRotateItem(msg);
				break;

			case 0x87: // close container
				parseCloseContainer(msg);
				break;

			case 0x88: //"up-arrow" - container
				parseUpArrowContainer(msg);
				break;

			case 0x89:
				parseTextWindow(msg);
				break;

			case 0x8A:
				parseHouseWindow(msg);
				break;

			case 0x8C: // throw item
				parseLookAt(msg);
				break;

			case 0x96: // say something
				parseSay(msg);
				break;

			case 0x97: // request channels
				parseGetChannels(msg);
				break;

			case 0x98: // open channel
				parseOpenChannel(msg);
				break;

			case 0x99: // close channel
				parseCloseChannel(msg);
				break;

			case 0x9A: // open priv
				parseOpenPriv(msg);
				break;

			case 0x9B: //process report
				parseProcessRuleViolation(msg);
				break;

			case 0x9C: //gm closes report
				parseCloseRuleViolation(msg);
				break;

			case 0x9D: //player cancels report
				parseCancelRuleViolation(msg);
				break;

			case 0x9E: // close NPC
				parseCloseNpc(msg);
				break;

			case 0xA0: // set attack and follow mode
				parseFightModes(msg);
				break;

			case 0xA1: // attack
				parseAttack(msg);
				break;

			case 0xA2: //follow
				parseFollow(msg);
				break;

			case 0xA3: // invite party
				parseInviteToParty(msg);
				break;

			case 0xA4: // join party
				parseJoinParty(msg);
				break;

			case 0xA5: // revoke party
				parseRevokePartyInvite(msg);
				break;

			case 0xA6: // pass leadership
				parsePassPartyLeadership(msg);
				break;

			case 0xA7: // leave party
				parseLeaveParty(msg);
				break;

			case 0xA8: // share exp
				parseSharePartyExperience(msg);
				break;

			case 0xAA:
				parseCreatePrivateChannel(msg);
				break;

			case 0xAB:
				parseChannelInvite(msg);
				break;

			case 0xAC:
				parseChannelExclude(msg);
				break;

			case 0xBE: // cancel move
				parseCancelMove(msg);
				break;

			case 0xC9: //client request to resend the tile
				parseUpdateTile(msg);
				break;

			case 0xCA: //client request to resend the container (happens when you store more than container maxsize)
				parseUpdateContainer(msg);
				break;

			case 0xD2: // request outfit
				if((!player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges) || !server.configManager().getBool(
					ConfigManager::DISABLE_OUTFITS_PRIVILEGED)) && (server.configManager().getBool(ConfigManager::ALLOW_CHANGEOUTFIT)
					|| server.configManager().getBool(ConfigManager::ALLOW_CHANGECOLORS) || server.configManager().getBool(ConfigManager::ALLOW_CHANGEADDONS)))
					parseRequestOutfit(msg);
				break;

			case 0xD3: // set outfit
				if((!player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges) || !server.configManager().getBool(ConfigManager::DISABLE_OUTFITS_PRIVILEGED))
					&& (server.configManager().getBool(ConfigManager::ALLOW_CHANGECOLORS) || server.configManager().getBool(ConfigManager::ALLOW_CHANGEOUTFIT)))
				parseSetOutfit(msg);
				break;

			case 0xDC:
				parseAddVip(msg);
				break;

			case 0xDD:
				parseRemoveVip(msg);
				break;

			case 0xE6:
				parseBugReport(msg);
				break;

			case 0xE7:
				parseViolationWindow(msg);
				break;

			case 0xE8:
				parseDebugAssert(msg);
				break;

			case 0xF0:
				parseQuests(msg);
				break;

			case 0xF1:
				parseQuestInfo(msg);
				break;

			default: {
				if (server.configManager().getBool(ConfigManager::BAN_UNKNOWN_BYTES)) {
					int64_t banTime = -1;
					ViolationAction_t action = ACTION_BANISHMENT;

					AccountP account = player->getAccount();
					if (account == nullptr) {
						return;
					}

					uint32_t warnings = account->getWarnings() + 1;

					if(static_cast<signed>(warnings) >= server.configManager().getNumber(ConfigManager::WARNINGS_TO_DELETION))
						action = ACTION_DELETION;
					else if(static_cast<signed>(warnings) >= server.configManager().getNumber(ConfigManager::WARNINGS_TO_FINALBAN))
					{
						banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::FINALBAN_LENGTH);
						action = ACTION_BANFINAL;
					}
					else
						banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::BAN_LENGTH);

					if(IOBan::getInstance()->addAccountBanishment(account->getId(), banTime, 13, action,
						"Sending unknown packets to the server.", 0, player->getGUID()))
					{
						account->setWarnings(warnings);
						IOLoginData::getInstance()->saveAccount(*account);
						player->sendTextMessage(MSG_INFO_DESCR, "You have been banished.");

						server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_GREEN);
						server.scheduler().addTask(SchedulerTask::create(Milliseconds(1000), std::bind(
							&Game::kickPlayer, &server.game(), player->getID(), false)));
					}
				}

				std::stringstream hex, s;
				hex << "0x" << std::hex << (int16_t)recvbyte << std::dec;
				s << player->getName() << " sent unknown byte: " << hex << std::endl;

				LOG_MESSAGE(LogType::NOTICE, s.str(), "PLAYER")
				DeprecatedLogger::getInstance()->eFile(getFilePath(FileType::LOG, "bots/" + player->getName() + ".log").c_str(),
					"[" + formatDate() + "] Received byte " + hex.str(), false);
				break;
			}
		}
	}
}

void ProtocolGame::GetTileDescription(const Tile* tile, NetworkMessage_ptr msg)
{
	if(!tile)
		return;

	LOGt("ProtocolGame(" << player->getName() << ")::GetTileDescription(tile.position = " << tile->getPosition() << ")");


	int32_t count = 0;
	if(tile->ground)
	{
		msg->AddItem(tile->ground.get());
		count++;
	}

	invalidateCreaturesAtPosition(tile->getPosition());

	const TileItemVector* items = tile->getItemList();
	const CreatureVector* creatures = tile->getCreatures();

	ItemVector::const_iterator it;
	if(items)
	{
		for(it = items->getBeginTopItem(); (it != items->getEndTopItem()); ++it, ++count) {
			assert(count < 10);
			msg->AddItem((*it).get());
		}
	}

	if(creatures)
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			assert(count < 10);

			Creature& creature = **cit;

			if(!player->canSeeCreature(&creature))
				continue;

			AddCreature(msg, *cit, StackPosition(creature.getPosition(), count));
			count++;
		}
	}

	if(items)
	{
		for(it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it, ++count) {
			assert(count < 10);
			msg->AddItem((*it).get());
		}
	}
}

void ProtocolGame::GetMapDescription(int32_t x, int32_t y, int32_t z,
	int32_t width, int32_t height, NetworkMessage_ptr msg)
{
	LOGt("ProtocolGame(" << player->getName() << ")::GetMapDescription(position = " << Position(x,y,z) << ", width = " << width << ", height = " << height << ")");

	int32_t skip = -1, startz, endz, zstep = 0;
	if(z > 7)
	{
		startz = z - 2;
		endz = std::min((int32_t)MAP_MAX_LAYERS - 1, z + 2);
		zstep = 1;
	}
	else
	{
		startz = 7;
		endz = 0;
		zstep = -1;
	}

	for(int32_t nz = startz; nz != endz + zstep; nz += zstep)
		GetFloorDescription(msg, x, y, nz, width, height, z - nz, skip);

	if(skip >= 0)
	{
		msg->AddByte(skip);
		msg->AddByte(0xFF);
		//cc += skip;
	}
}

void ProtocolGame::GetFloorDescription(NetworkMessage_ptr msg, int32_t x, int32_t y, int32_t z,
		int32_t width, int32_t height, int32_t offset, int32_t& skip)
{
	LOGt("ProtocolGame(" << player->getName() << ")::GetFloorDescription(position = " << Position(x,y,z) << ", width = " << width << ", height = " << height << ", offset = " << offset << ")");

	Tile* tile = nullptr;
	for(int32_t nx = 0; nx < width; nx++)
	{
		for(int32_t ny = 0; ny < height; ny++)
		{
			int_fast32_t finalX = x + nx + offset;
			int_fast32_t finalY = y + ny + offset;

			if(Position::isValid(finalX, finalY, z) && (tile = server.game().getTile(Position(finalX, finalY, z))))
			{
				if(skip >= 0)
				{
					msg->AddByte(skip);
					msg->AddByte(0xFF);
				}

				skip = 0;
				GetTileDescription(tile, msg);
			}
			else
			{
				++skip;
				if(skip == 0xFF)
				{
					msg->AddByte(0xFF);
					msg->AddByte(0xFF);
					skip = -1;
				}
			}
		}
	}
}


void ProtocolGame::correctRegisteredCreature(const CreatureP& creature, const StackPosition& previousPosition, const StackPosition& newPosition, const CreatureValidationResult& validationResult) {
	if (creature == player) {
		NetworkMessage_ptr message = getOutputBuffer();
		if (message) {
			LOGf("\tI moved the player to " << newPosition << " and re-sent the map.");
			LOGf(std::setfill('-') << std::setw(90) << "");

			RemoveTileCreature(message, validationResult.expectedPosition);
			AddMapDescription(message, newPosition);

			invalidateDistantCreatures();
		}

		return;
	}

	if (canSee(creature.get())) {
		if (validationResult.registered) {
			Game& game = server.game();

			if (validationResult.expectedPosition.isValid() && newPosition.withoutIndex() != validationResult.expectedPosition.withoutIndex()) {
				if (previousPosition.withoutIndex() == validationResult.expectedPosition.withoutIndex()) {
					LOGf("\tI reloaded the tiles at " << previousPosition << " and " << newPosition << ".");
					LOGf(std::setfill('-') << std::setw(90) << "");

					sendUpdateTile(game.getTile(previousPosition), previousPosition);
					sendUpdateTile(game.getTile(newPosition), newPosition);
				}
				else {
					if (previousPosition.withoutIndex() != validationResult.expectedPosition.withoutIndex()) {
						LOGf("\tI simulated that the creature was beamed to the new target " << newPosition << ".");
						LOGf(std::setfill('-') << std::setw(90) << "");

						sendMoveCreature(creature, game.getTile(newPosition), newPosition, game.getTile(validationResult.expectedPosition), validationResult.expectedPosition, false, BOOST_CURRENT_FUNCTION);
					}
				}
			}
			else {
				LOGf("\tI reloaded the tile at " << newPosition << ".");
				LOGf(std::setfill('-') << std::setw(90) << "");

				sendUpdateTile(game.getTile(newPosition), newPosition);
			}
		}
		else {
			LOGf("\tI simulated that the creature just appeared at " << newPosition << ".");
			LOGf(std::setfill('-') << std::setw(90) << "");

			sendAddCreature(creature, newPosition, BOOST_CURRENT_FUNCTION);
		}
	}
	else if (!validationResult.registered) {
		LOGf("\tI prevented an invalid update at " << newPosition << ".");
		LOGf(std::setfill('-') << std::setw(90) << "");
	}
	else {
		LOGf("\tI tried to re-synchronize the invalid data.");
		LOGf(std::setfill('-') << std::setw(90) << "");
		invalidateCreature(creature);
	}
}


bool ProtocolGame::invalidateCreature(const CreatureP& creature) {
	auto iterator = _registeredCreatures.find(creature);
	if (iterator == _registeredCreatures.end()) {
		// creature is not known
		return false;
	}

	iterator->second = StackPosition::INVALID;

	return true;
}


void ProtocolGame::invalidateCreaturesAtPosition(const Position& position) {
	NetworkMessage_ptr message = getOutputBuffer();

	for (auto& pair : _registeredCreatures) {
		if (pair.second.withoutIndex() == position) {
			pair.second = StackPosition::INVALID;
		}
	}
}


void ProtocolGame::invalidateDistantCreatures() {
	NetworkMessage_ptr message = getOutputBuffer();

	for (auto& pair : _registeredCreatures) {
		if (pair.second.isValid() && !canSee(pair.second)) {
			pair.second = StackPosition::INVALID;
		}
	}
}


bool ProtocolGame::registerCreature(const CreatureP& creature, const StackPosition& position, uint32_t& removedCreatureId) {
	removedCreatureId = 0;
	
	if (_registeredCreatures.find(creature) != _registeredCreatures.end()) {
		// creature is already known
		return false;
	}

	if (_registeredCreatures.size() >= 250) {
		// too many creatures registered. remove one!

		Position playerPosition = player->getPosition();
		uint32_t farthestDistance = 0;
		const CreatureP* creatureToRemove = nullptr;

		for (const auto& entry : _registeredCreatures) {
			if (entry.first == player) {
				// bad idea
				continue;
			}

			if (!canSee(entry.first.get())) {
				// cool, creature is gone anyway
				creatureToRemove = &entry.first;
				break;
			}

			uint32_t distance = playerPosition.distanceTo(entry.second);
			if (distance > farthestDistance) {
				// alternatively let's remove the creature which is farthest away
				creatureToRemove = &entry.first;
				farthestDistance = distance;
			}
		}

		if (creatureToRemove != nullptr) {
			removedCreatureId = (*creatureToRemove)->getID();
			_registeredCreatures.erase(*creatureToRemove);
		}
	}

	// add the new creature
	_registeredCreatures[creature] = position;

	return true;
}


void ProtocolGame::updateCreaturesAtPosition(const Position& position) {
	NetworkMessage_ptr message = getOutputBuffer();

	Tile* tile = server.game().getTile(position);

	StackPosition creaturePosition(position, 0);

	CreatureVector* creatures = tile->getCreatures();
	if (creatures != nullptr) {
		for (const auto& creature : *creatures) {
			if (!player->canSeeCreature(creature.get())) {
				continue;
			}

			creaturePosition.index = tile->getClientIndexOfThing(player.get(), creature.get());
			updateRegisteredCreature(creature, creaturePosition);
		}
	}
}


bool ProtocolGame::updateRegisteredCreature(const CreatureP& creature, const StackPosition& position) {
	auto iterator = _registeredCreatures.find(creature);
	if (iterator == _registeredCreatures.end()) {
		// creature is not known
		return false;
	}

	LOGt("Update " << *creature << " to " << position);

	_registeredCreatures[creature] = position;

	return true;
}


ProtocolGame::CreatureValidationResult ProtocolGame::validateRegisteredCreature(const CreatureP& creature, const StackPosition& previousPosition, const StackPosition& newPosition, bool mustExist, const char* callSource) const {
	auto iterator = _registeredCreatures.find(creature);

	bool registered = (iterator != _registeredCreatures.end());
	if (registered) {
		if (iterator->second == previousPosition || (!iterator->second.isValid() && !canSee(previousPosition) && (!mustExist || !canSee(creature.get())))) {
			// creature is still at the position we expect it
			return CreatureValidationResult(true, previousPosition, false);
		}
	}
	else if (!mustExist || !canSee(creature.get())) {
		// creature not yet registered
		return CreatureValidationResult();
	}

	std::ostringstream expectedPosition;
	if (registered) {
		expectedPosition << iterator->second;
		if (iterator->second.isValid() && !canSee(iterator->second)) {
			expectedPosition << " (too far away)";
		}
	}
	else {
		expectedPosition << "nowhere";
	}

	// creature has moved but we forgot to tell the client!
	LOGf(std::setfill('-') << std::setw(90) << "");
	LOGf("\tInternal creature movement data invalid when sending an update to the client.");
	LOGf("\tI'll try my best to prevent the client from crashing - this should never happen!");
	LOGf("\tPlease report this to a developer on " << X_PROJECT_ISSUES_URL);
	LOGf("");
	LOGf("\tPlayer:");
	LOGf("\t\tName:     " << player->getName());
	LOGf("\t\tPosition: " << player->getPosition());
	LOGf("\tCreature:");
	LOGf("\t\tID:       " << creature->getID());
	LOGf("\t\tName:     " << creature->getName() << (creature->isRemoved() ? " (removed)" : (player->canSeeCreature(creature.get()) ? "": " (invisible)")));
	LOGf("\t\tPrevious: " << previousPosition << (previousPosition.isValid() && !canSee(previousPosition) ? " (too far away)" : ""));
	LOGf("\t\tExpected: " << expectedPosition.str());
	LOGf("\t\tNew:      " << newPosition << (newPosition.isValid() && !canSee(newPosition) ? " (too far away)" : ""));
	LOGf("");
	LOGf("\tSource: " << callSource);
	LOGf("");

	return CreatureValidationResult(registered, registered ? iterator->second : StackPosition::INVALID, true);
}


bool ProtocolGame::canSee(const Creature* c) const
{
	return !c->isRemoved() && (c == player || (player->canSeeCreature(c) && canSee(c->getPosition())));
}

bool ProtocolGame::canSee(const Position& pos) const
{
	return canSee(pos.x, pos.y, pos.z);
}

bool ProtocolGame::canSee(uint16_t x, uint16_t y, uint16_t z) const
{
	const Position& myPos = player->getPosition();
	if(myPos.z <= 7)
	{
		//we are on ground level or above (7 -> 0), view is from 7 -> 0
		if(z > 7)
			return false;
	}
	else if(myPos.z >= 8 && std::abs(myPos.z - z) > 2) //we are underground (8 -> 15), view is +/- 2 from the floor we stand on
		return false;

	//negative offset means that the action taken place is on a lower floor than ourself
	int32_t offsetz = myPos.z - z;
	return ((x >= myPos.x - 8 + offsetz) && (x <= myPos.x + 9 + offsetz) &&
		(y >= myPos.y - 6 + offsetz) && (y <= myPos.y + 7 + offsetz));
}

//********************** Parse methods *******************************//
void ProtocolGame::parseLogout(NetworkMessage& msg)
{
	server.dispatcher().addTask(Task::create(std::bind(&ProtocolGame::logout, this, true, false)));
}

void ProtocolGame::parseCreatePrivateChannel(NetworkMessage& msg)
{
	addGameTask(&Game::playerCreatePrivateChannel, player->getID());
}

void ProtocolGame::parseChannelInvite(NetworkMessage& msg)
{
	const std::string name = msg.GetString();
	addGameTask(&Game::playerChannelInvite, player->getID(), name);
}

void ProtocolGame::parseChannelExclude(NetworkMessage& msg)
{
	const std::string name = msg.GetString();
	addGameTask(&Game::playerChannelExclude, player->getID(), name);
}

void ProtocolGame::parseGetChannels(NetworkMessage& msg)
{
	addGameTask(&Game::playerRequestChannels, player->getID());
}

void ProtocolGame::parseOpenChannel(NetworkMessage& msg)
{
	uint16_t channelId = msg.GetU16();
	addGameTask(&Game::playerOpenChannel, player->getID(), channelId);
}

void ProtocolGame::parseCloseChannel(NetworkMessage& msg)
{
	uint16_t channelId = msg.GetU16();
	addGameTask(&Game::playerCloseChannel, player->getID(), channelId);
}

void ProtocolGame::parseOpenPriv(NetworkMessage& msg)
{
	const std::string receiver = msg.GetString();
	addGameTask(&Game::playerOpenPrivateChannel, player->getID(), receiver);
}

void ProtocolGame::parseProcessRuleViolation(NetworkMessage& msg)
{
	const std::string reporter = msg.GetString();
	addGameTask(&Game::playerProcessRuleViolation, player->getID(), reporter);
}

void ProtocolGame::parseCloseRuleViolation(NetworkMessage& msg)
{
	const std::string reporter = msg.GetString();
	addGameTask(&Game::playerCloseRuleViolation, player->getID(), reporter);
}

void ProtocolGame::parseCancelRuleViolation(NetworkMessage& msg)
{
	addGameTask(&Game::playerCancelRuleViolation, player->getID());
}

void ProtocolGame::parseCloseNpc(NetworkMessage& msg)
{
	addGameTask(&Game::playerCloseNpcChannel, player->getID());
}

void ProtocolGame::parseCancelMove(NetworkMessage& msg)
{
	addGameTask(&Game::playerCancelAttackAndFollow, player->getID());
}

void ProtocolGame::parseReceivePing(NetworkMessage& msg)
{
	addGameTask(&Game::playerReceivePing, player->getID());
}

void ProtocolGame::parseAutoWalk(NetworkMessage& msg)
{
	// first we get all directions...
	std::deque<Direction> path;
	size_t dirCount = msg.GetByte();
	for(size_t i = 0; i < dirCount; ++i)
	{
		uint8_t rawDir = msg.GetByte();
		Direction dir = Direction::SOUTH;
		switch(rawDir)
		{
			case 1:
				dir = Direction::EAST;
				break;
			case 2:
				dir = Direction::NORTH_EAST;
				break;
			case 3:
				dir = Direction::NORTH;
				break;
			case 4:
				dir = Direction::NORTH_WEST;
				break;
			case 5:
				dir = Direction::WEST;
				break;
			case 6:
				dir = Direction::SOUTH_WEST;
				break;
			case 7:
				dir = Direction::SOUTH;
				break;
			case 8:
				dir = Direction::SOUTH_EAST;
				break;
			default:
				continue;
		}

		path.push_back(dir);
	}

	addGameTask(&Game::playerAutoWalk, player->getID(), path);
}

void ProtocolGame::parseMove(NetworkMessage& msg, Direction dir)
{
	addGameTask(&Game::playerMove, player->getID(), dir);
}

void ProtocolGame::parseTurn(NetworkMessage& msg, Direction dir)
{
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerTurn, player->getID(), dir);
}

void ProtocolGame::parseRequestOutfit(NetworkMessage& msg)
{
	addGameTask(&Game::playerRequestOutfit, player->getID());
}

void ProtocolGame::parseSetOutfit(NetworkMessage& msg)
{
	Outfit_t newOutfit = player->defaultOutfit;
	if(server.configManager().getBool(ConfigManager::ALLOW_CHANGEOUTFIT))
		newOutfit.lookType = msg.GetU16();
	else
		msg.SkipBytes(2);

	if(server.configManager().getBool(ConfigManager::ALLOW_CHANGECOLORS))
	{
		newOutfit.lookHead = msg.GetByte();
		newOutfit.lookBody = msg.GetByte();
		newOutfit.lookLegs = msg.GetByte();
		newOutfit.lookFeet = msg.GetByte();
	}
	else
		msg.SkipBytes(4);

	if(server.configManager().getBool(ConfigManager::ALLOW_CHANGEADDONS))
		newOutfit.lookAddons = msg.GetByte();
	else
		msg.SkipBytes(1);

	addGameTask(&Game::playerChangeOutfit, player->getID(), newOutfit);
}

void ProtocolGame::parseUseItem(NetworkMessage& msg)
{
	ExtendedPosition position = msg.GetExtendedPosition();
	uint8_t openContainerId = msg.GetByte();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerUseItem, player->getID(), position, openContainerId);
}

void ProtocolGame::parseUseItemEx(NetworkMessage& msg) {
	ExtendedPosition origin = msg.GetExtendedPosition();
	ExtendedPosition destination = msg.GetExtendedPosition();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerUseItemEx, player->getID(),
		origin, destination);
}

void ProtocolGame::parseBattleWindow(NetworkMessage& msg)
{
	ExtendedPosition origin = msg.GetExtendedPosition();
	uint32_t creatureId = msg.GetU32();

	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerUseBattleWindow, player->getID(), origin, creatureId);
}

void ProtocolGame::parseCloseContainer(NetworkMessage& msg)
{
	uint8_t cid = msg.GetByte();
	addGameTask(&Game::playerCloseContainer, player->getID(), cid);
}

void ProtocolGame::parseUpArrowContainer(NetworkMessage& msg)
{
	uint8_t cid = msg.GetByte();
	addGameTask(&Game::playerMoveUpContainer, player->getID(), cid);
}

void ProtocolGame::parseUpdateTile(NetworkMessage& msg)
{
}

void ProtocolGame::parseUpdateContainer(NetworkMessage& msg)
{
	uint8_t cid = msg.GetByte();
	addGameTask(&Game::playerUpdateContainer, player->getID(), cid);
}

void ProtocolGame::parseThrow(NetworkMessage& msg)
{
	ExtendedPosition origin = msg.GetExtendedPosition();
	ExtendedPosition destination = msg.GetExtendedPosition(false);
	uint8_t count = msg.GetByte();

	if (origin.hasPosition(true) && destination.hasPosition(true) && origin.getPosition(true) == destination.getPosition(true)) {
		return;
	}

	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerMoveThing,
		player->getID(), origin, destination, count);
}

void ProtocolGame::parseLookAt(NetworkMessage& msg)
{
	ExtendedPosition position = msg.GetExtendedPosition();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerLookAt, player->getID(), position);
}

void ProtocolGame::parseSay(NetworkMessage& msg)
{
	std::string receiver;
	uint16_t channelId = 0;

	SpeakClasses type = (SpeakClasses)msg.GetByte();
	switch(type)
	{
		case SPEAK_PRIVATE:
		case SPEAK_PRIVATE_RED:
		case SPEAK_RVR_ANSWER:
			receiver = msg.GetString();
			break;

		case SPEAK_CHANNEL_Y:
		case SPEAK_CHANNEL_RN:
		case SPEAK_CHANNEL_RA:
			channelId = msg.GetU16();
			break;

		default:
			break;
	}

	const std::string text = msg.GetString();
	if(text.length() > 255) //client limit
	{
		std::stringstream s;
		s << text.length();

		DeprecatedLogger::getInstance()->eFile("bots/" + player->getName() + ".log", "Attempt to send message with size " + s.str() + " - client is limited to 255 characters.", true);
		return;
	}

	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerSay, player->getID(), channelId, type, receiver, text);
}

void ProtocolGame::parseFightModes(NetworkMessage& msg)
{
	uint8_t rawFightMode = msg.GetByte(); //1 - offensive, 2 - balanced, 3 - defensive
	uint8_t rawChaseMode = msg.GetByte(); //0 - stand while fightning, 1 - chase opponent
	uint8_t rawSecureMode = msg.GetByte(); //0 - can't attack unmarked, 1 - can attack unmarked

	chaseMode_t chaseMode = CHASEMODE_STANDSTILL;
	if(rawChaseMode == 1)
		chaseMode = CHASEMODE_FOLLOW;

	fightMode_t fightMode = FIGHTMODE_ATTACK;
	if(rawFightMode == 2)
		fightMode = FIGHTMODE_BALANCED;
	else if(rawFightMode == 3)
		fightMode = FIGHTMODE_DEFENSE;

	secureMode_t secureMode = SECUREMODE_OFF;
	if(rawSecureMode == 1)
		secureMode = SECUREMODE_ON;

	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerSetFightModes, player->getID(), fightMode, chaseMode, secureMode);
}

void ProtocolGame::parseAttack(NetworkMessage& msg)
{
	uint32_t creatureId = msg.GetU32();
	msg.GetU32();
	msg.GetU32();
	addGameTask(&Game::playerSetAttackedCreature, player->getID(), creatureId);
}

void ProtocolGame::parseFollow(NetworkMessage& msg)
{
	uint32_t creatureId = msg.GetU32();
	addGameTask(&Game::playerFollowCreature, player->getID(), creatureId);
}

void ProtocolGame::parseTextWindow(NetworkMessage& msg)
{
	uint32_t windowTextId = msg.GetU32();
	const std::string newText = msg.GetString();
	addGameTask(&Game::playerWriteItem, player->getID(), windowTextId, newText);
}

void ProtocolGame::parseHouseWindow(NetworkMessage &msg)
{
	uint8_t doorId = msg.GetByte();
	uint32_t id = msg.GetU32();
	const std::string text = msg.GetString();
	addGameTask(&Game::playerUpdateHouseWindow, player->getID(), doorId, id, text);
}

void ProtocolGame::parseLookInShop(NetworkMessage &msg)
{
	uint16_t id = msg.GetU16();
	uint16_t count = msg.GetByte();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerLookInShop, player->getID(), id, count);
}

void ProtocolGame::parsePlayerPurchase(NetworkMessage &msg)
{
	uint16_t id = msg.GetU16();
	uint16_t count = msg.GetByte();
	uint16_t amount = msg.GetByte();
	bool ignoreCap = msg.GetByte();
	bool inBackpacks = msg.GetByte();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerPurchaseItem, player->getID(), id, count, amount, ignoreCap, inBackpacks);
}

void ProtocolGame::parsePlayerSale(NetworkMessage &msg)
{
	uint16_t id = msg.GetU16();
	uint16_t count = msg.GetByte();
	uint16_t amount = msg.GetByte();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerSellItem, player->getID(), id, count, amount);
}

void ProtocolGame::parseCloseShop(NetworkMessage &msg)
{
	addGameTask(&Game::playerCloseShop, player->getID());
}

void ProtocolGame::parseRequestTrade(NetworkMessage& msg)
{
	ExtendedPosition position = msg.GetExtendedPosition();
	uint32_t playerId = msg.GetU32();
	addGameTask(&Game::playerRequestTrade, player->getID(), position, playerId);
}

void ProtocolGame::parseAcceptTrade(NetworkMessage& msg)
{
	addGameTask(&Game::playerAcceptTrade, player->getID());
}

void ProtocolGame::parseLookInTrade(NetworkMessage& msg)
{
	bool counter = msg.GetByte();
	int32_t index = msg.GetByte();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerLookInTrade, player->getID(), counter, index);
}

void ProtocolGame::parseCloseTrade()
{
	addGameTask(&Game::playerCloseTrade, player->getID());
}

void ProtocolGame::parseAddVip(NetworkMessage& msg)
{
	const std::string name = msg.GetString();
	if(name.size() > 32)
		return;

	addGameTask(&Game::playerRequestAddVip, player->getID(), name);
}

void ProtocolGame::parseRemoveVip(NetworkMessage& msg)
{
	uint32_t guid = msg.GetU32();
	addGameTask(&Game::playerRequestRemoveVip, player->getID(), guid);
}

void ProtocolGame::parseRotateItem(NetworkMessage& msg)
{
	ExtendedPosition position = msg.GetExtendedPosition();
	addGameTaskTimed(DISPATCHER_TASK_EXPIRATION, &Game::playerRotateItem, player->getID(), position);
}

void ProtocolGame::parseDebugAssert(NetworkMessage& msg)
{
	if(m_debugAssertSent)
		return;

	std::stringstream s;
	s << "----- " << formatDate() << " - " << player->getName() << " (" << convertIPAddress(getIP())
		<< ") -----" << std::endl << msg.GetString() << std::endl << msg.GetString()
		<< std::endl << msg.GetString() << std::endl << msg.GetString()
		<< std::endl << std::endl;

	m_debugAssertSent = true;
	DeprecatedLogger::getInstance()->iFile(LogFile::CLIENT_ASSERTION, s.str(), false);
}

void ProtocolGame::parseBugReport(NetworkMessage& msg)
{
	std::string comment = msg.GetString();
	addGameTask(&Game::playerReportBug, player->getID(), comment);
}

void ProtocolGame::parseInviteToParty(NetworkMessage& msg)
{
	uint32_t targetId = msg.GetU32();
	addGameTask(&Game::playerInviteToParty, player->getID(), targetId);
}

void ProtocolGame::parseJoinParty(NetworkMessage& msg)
{
	uint32_t targetId = msg.GetU32();
	addGameTask(&Game::playerJoinParty, player->getID(), targetId);
}

void ProtocolGame::parseRevokePartyInvite(NetworkMessage& msg)
{
	uint32_t targetId = msg.GetU32();
	addGameTask(&Game::playerRevokePartyInvitation, player->getID(), targetId);
}

void ProtocolGame::parsePassPartyLeadership(NetworkMessage& msg)
{
	uint32_t targetId = msg.GetU32();
	addGameTask(&Game::playerPassPartyLeadership, player->getID(), targetId);
}

void ProtocolGame::parseLeaveParty(NetworkMessage& msg)
{
	addGameTask(&Game::playerLeaveParty, player->getID());
}

void ProtocolGame::parseSharePartyExperience(NetworkMessage& msg)
{
	bool activate = msg.GetByte();
	uint8_t unknown = msg.GetByte(); //TODO: find out what is this byte
	addGameTask(&Game::playerSharePartyExperience, player->getID(), activate, unknown);
}

void ProtocolGame::parseQuests(NetworkMessage& msg)
{
	addGameTask(&Game::playerQuests, player->getID());
}

void ProtocolGame::parseQuestInfo(NetworkMessage& msg)
{
	uint16_t questId = msg.GetU16();
	addGameTask(&Game::playerQuestInfo, player->getID(), questId);
}

void ProtocolGame::parseViolationWindow(NetworkMessage& msg)
{
	std::string target = msg.GetString();
	uint8_t reason = msg.GetByte();
	ViolationAction_t action = (ViolationAction_t)msg.GetByte();
	std::string comment = msg.GetString();
	std::string statement = msg.GetString();
	uint32_t statementId = (uint32_t)msg.GetU16();
	bool ipBanishment = msg.GetByte();
	addGameTask(&Game::playerViolationWindow, player->getID(), target, reason, action, comment, statement, statementId, ipBanishment);
}

//********************** Send methods *******************************//
void ProtocolGame::sendOpenPrivateChannel(const std::string& receiver)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendOpenPrivateChannel(receiver = '" << receiver << "')");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAD);
		msg->AddString(receiver);
	}
}

void ProtocolGame::sendCreatureOutfit(const Creature* creature, const Outfit_t& outfit)
{
//	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureOutfit(" << creature << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x8E);
		msg->AddU32(creature->getID());
		AddCreatureOutfit(msg, creature, outfit);
	}
}

void ProtocolGame::sendCreatureLight(const Creature* creature)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureLight(" << creature << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddCreatureLight(msg, creature);
	}
}

void ProtocolGame::sendWorldLight(const LightInfo& lightInfo)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendWorldLight()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddWorldLight(msg, lightInfo);
	}
}

void ProtocolGame::sendCreatureShield(const Creature* creature)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureShield(" << creature << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x91);
		msg->AddU32(creature->getID());
		msg->AddByte(player->getPartyShield(creature));
	}
}

void ProtocolGame::sendCreatureSkull(const Creature* creature)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureSkull(" << creature << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x90);
		msg->AddU32(creature->getID());
		msg->AddByte(player->getSkullClient(creature));
	}
}

void ProtocolGame::sendCreatureSquare(const Creature* creature, SquareColor_t color)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureSquare(" << creature << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x86);
		msg->AddU32(creature->getID());
		msg->AddByte((uint8_t)color);
	}
}

void ProtocolGame::sendTutorial(uint8_t tutorialId)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendTutorial(tutorialId = '" << tutorialId << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xDC);
		msg->AddByte(tutorialId);
	}
}

void ProtocolGame::sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendAddMarker(position = " << pos << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xDD);
		msg->AddPosition(pos);
		msg->AddByte(markType);
		msg->AddString(desc);
	}
}

void ProtocolGame::sendReLoginWindow()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendReLoginWindow()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x28);
	}
}

void ProtocolGame::sendStats()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendStats()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddPlayerStats(msg);
	}
}

void ProtocolGame::sendTextMessage(MessageClasses mClass, const std::string& message)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendTextMessage()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddTextMessage(msg, mClass, message);
	}
}

void ProtocolGame::sendClosePrivate(uint16_t channelId)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendClosePrivate(channelId = " << channelId << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		if(channelId == CHANNEL_GUILD || channelId == CHANNEL_PARTY)
			server.chat().removeUserFromChannel(player.get(), channelId);

		msg->AddByte(0xB3);
		msg->AddU16(channelId);
	}
}

void ProtocolGame::sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatePrivateChannel(channelId = " << channelId << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xB2);
		msg->AddU16(channelId);
		msg->AddString(channelName);
	}
}

void ProtocolGame::sendChannelsDialog()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendChannelsDialog()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAB);
		ChannelList list = server.chat().getChannelList(player.get());
		msg->AddByte(list.size());
		for(ChannelList::iterator it = list.begin(); it != list.end(); ++it)
		{
			if(ChatChannel* channel = (*it))
			{
				msg->AddU16(channel->getId());
				msg->AddString(channel->getName());
			}
		}
	}
}

void ProtocolGame::sendChannel(uint16_t channelId, const std::string& channelName)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendChannel(channelId = " << channelId << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAC);
		msg->AddU16(channelId);
		msg->AddString(channelName);
	}
}

void ProtocolGame::sendRuleViolationsChannel(uint16_t channelId)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRuleViolationsChannel(channelId = " << channelId << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAE);
		msg->AddU16(channelId);
		for(RuleViolationsMap::const_iterator it = server.game().getRuleViolations().begin(); it != server.game().getRuleViolations().end(); ++it)
		{
			RuleViolation& rvr = *it->second;
			if(rvr.isOpen && rvr.reporter)
				AddCreatureSpeak(msg, rvr.reporter, SPEAK_RVR_CHANNEL, rvr.text, channelId, rvr.time);
		}
	}
}

void ProtocolGame::sendRemoveReport(const std::string& name)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRemoveReport()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAF);
		msg->AddString(name);
	}
}

void ProtocolGame::sendRuleViolationCancel(const std::string& name)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRuleViolationCancel()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xB0);
		msg->AddString(name);
	}
}

void ProtocolGame::sendLockRuleViolation()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendLockRuleViolation()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xB1);
	}
}

void ProtocolGame::sendIcons(int32_t icons)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendIcons()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xA2);
		msg->AddU16(icons);
	}
}

void ProtocolGame::sendContainer(uint32_t cid, const Container* container, bool hasParent)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendContainer(cid = " << cid << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x6E);
		msg->AddByte(cid);

		msg->AddItemId(container);
		msg->AddString(container->getName());
		msg->AddByte(container->capacity());

		msg->AddByte(hasParent ? 0x01 : 0x00);
		msg->AddByte(std::min(container->size(), (uint32_t)255));

		ItemList::const_iterator cit = container->getItems();
		for(uint32_t i = 0; cit != container->getEnd() && i < 255; ++cit, ++i)
			msg->AddItem((*cit).get());
	}
}

void ProtocolGame::sendShop(const ShopInfoList& shop)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendShop()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x7A);
		msg->AddByte(std::min(shop.size(), (size_t)255));

		ShopInfoList::const_iterator it = shop.begin();
		for(uint32_t i = 0; it != shop.end() && i < 255; ++it, ++i)
			AddShopItem(msg, (*it));
	}
}

void ProtocolGame::sendCloseShop()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCloseShop()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x7C);
	}
}

void ProtocolGame::sendGoods(const ShopInfoList& shop)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendGoods()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		uint64_t money = server.game().getMoney(player.get());
		if (money > 2000000000UL) {
			money = 2000000000UL;
		}

		TRACK_MESSAGE(msg);
		msg->AddByte(0x7B);
		msg->AddU32(static_cast<uint32_t>(money));

		std::map<uint32_t, uint32_t> goodsMap;
		for(ShopInfoList::const_iterator sit = shop.begin(); sit != shop.end(); ++sit)
		{
			if(sit->sellPrice < 0)
				continue;

			int8_t subType = -1;
			if(sit->subType)
			{
				ItemKindPC kind = server.items()[sit->itemId];
				if(kind && kind->hasSubType() && !kind->stackable)
					subType = sit->subType;
			}

			uint32_t count = player->__getItemTypeCount(sit->itemId, subType);
			if (count == 1) {
				count = player->__getItemTypeCount(sit->itemId, subType, true, false);
			}

			if(count > 0)
				goodsMap[sit->itemId] = count;
		}

		msg->AddByte(std::min(goodsMap.size(), (size_t)255));
		std::map<uint32_t, uint32_t>::const_iterator it = goodsMap.begin();
		for(uint32_t i = 0; it != goodsMap.end() && i < 255; ++it, ++i)
		{
			msg->AddItemId(it->first);
			msg->AddByte(std::min(it->second, (uint32_t)255));
		}
	}
}

void ProtocolGame::sendTradeItemRequest(const Player* player, const Item* item, bool ack)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendTradeItemRequest(player = '" << player->getName() << "')");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		if(ack)
			msg->AddByte(0x7D);
		else
			msg->AddByte(0x7E);

		msg->AddString(player->getName());
		if(const Container* container = item->getContainer())
		{
			msg->AddByte(container->getItemHoldingCount() + 1);
			msg->AddItem(item);
			for(ContainerIterator it = container->begin(); it != container->end(); ++it)
				msg->AddItem(*it);
		}
		else
		{
			msg->AddByte(1);
			msg->AddItem(item);
		}
	}
}

void ProtocolGame::sendCloseTrade()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCloseTrade()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x7F);
	}
}

void ProtocolGame::sendCloseContainer(uint32_t cid)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCloseContainer(cid = " << cid << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x6F);
		msg->AddByte(cid);
	}
}

void ProtocolGame::sendCreatureTurn(const CreatureP& creature, const StackPosition& position)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureTurn(" << creature << ", position = " << position << ") - canSee = " << (canSee(creature.get()) ? "true" : "false"));

	if(!canSee(creature.get()))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x6B);
		msg->AddPositionEx(position);
		msg->AddU16(0x63); /*99*/
		msg->AddU32(creature->getID());
		msg->AddByte(+creature->getDirection());

		updateRegisteredCreature(creature, position);

		if (creature == player) {
			invalidateDistantCreatures();
		}
	}
}

void ProtocolGame::sendCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text, Position* pos/* = nullptr*/)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureSay(" << creature << "");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddCreatureSpeak(msg, creature, type, text, 0, 0, pos);
	}
}

void ProtocolGame::sendToChannel(const Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId, uint32_t time /*= 0*/)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendToChannel(" << creature << ")");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddCreatureSpeak(msg, creature, type, text, channelId, time);
	}
}

void ProtocolGame::sendCancel(const std::string& message)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCancel()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddTextMessage(msg, MSG_STATUS_SMALL, message);
	}
}

void ProtocolGame::sendCancelTarget()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCancelTarget()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xA3);
		msg->AddU32(0);
		}
}

void ProtocolGame::sendChangeSpeed(const Creature* creature, uint32_t speed)
{
	LOGt("ProtocolGame(" << player->getName() << ")::setChangeSpeed(" << creature << ", speed = " << speed << ") - canSee = " << (canSee(creature) ? "true" : "false"));

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x8F);
		msg->AddU32(creature->getID());
		msg->AddU16(speed);
	}
}

void ProtocolGame::sendCancelWalk()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCancelWalk()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xB5);
		msg->AddByte(+player->getDirection());
	}
}

void ProtocolGame::sendSkills()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendSkills()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddPlayerSkills(msg);
	}
}

void ProtocolGame::sendPing()
{
//	LOGt("ProtocolGame(" << player->getName() << ")::sendPing()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x1E);
	}
}

void ProtocolGame::sendDistanceShoot(const Position& from, const Position& to, uint8_t type)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendDistanceShoot(from = " << from << ", to = " << to << ", type = " << static_cast<uint32_t>(type) << ") - canSee(from) = " << (canSee(from) ? "true" : "false") << ", canSee(from) = " << (canSee(to) ? "true" : "false"));

	if(type > SHOOT_EFFECT_LAST || (!canSee(from) && !canSee(to)))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddDistanceShoot(msg, from, to, type);
	}
}

void ProtocolGame::sendMagicEffect(const Position& pos, uint8_t type)
{
//	LOGt("ProtocolGame(" << player->getName() << ")::sendMagicEffect(pos = " << pos << ", type = " << static_cast<uint32_t>(type) << ") - canSee = " << (canSee(pos) ? "true" : "false"));

	if(type > MAGIC_EFFECT_LAST || !canSee(pos))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddMagicEffect(msg, pos, type);
	}
}

void ProtocolGame::sendAnimatedText(const Position& pos, uint8_t color, std::string text)
{
//	LOGt("ProtocolGame(" << player->getName() << ")::sendAnimatedText(pos = " << pos << ") - canSee = " << (canSee(pos) ? "true" : "false"));

	if(!canSee(pos))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddAnimatedText(msg, pos, color, text);
	}
}

void ProtocolGame::sendCreatureHealth(const Creature* creature)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendCreatureHealth(" << creature << ")");

	if(!canSee(creature))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddCreatureHealth(msg, creature);
	}
}

void ProtocolGame::sendFYIBox(const std::string& message)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendFYIBox()");

	if(message.empty() || message.length() > 1018) //Prevent client debug when message is empty or length is > 1018 (not confirmed)
	{
		LOGe("ProtocolGame(" << player->getName() << ")::sendFYIBox() - Trying to send an empty or too large message.");
		return;
	}

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x15);
		msg->AddString(message);
	}
}

//tile
void ProtocolGame::sendAddTileItem(const Tile* tile, const StackPosition& position, const Item* item, const char* callSource)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendAddTileItem(position = " << position << ", item = '" << item->getName() << "') - canSee = " << (canSee(position) ? "true" : "false"));

	if(!canSee(position))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddTileItem(msg, position, item);

		Tile* tile = server.game().getTile(position);
		if (tile->getCreatures() != nullptr) {
			StackPosition creaturePosition(position);
			for (const auto& creature : boost::adaptors::reverse(*tile->getCreatures())) {
				if (!player->canSeeCreature(creature.get())) {
					continue;
				}

				creaturePosition.index = tile->getClientIndexOfThing(player.get(), creature.get());
				if (position.index >= creaturePosition.index) {
					continue;
				}

				// existing creatures move up by 1 stackpos

				StackPosition previousPosition(creaturePosition, creaturePosition.index - 1);
				CreatureValidationResult validationResult = validateRegisteredCreature(creature, previousPosition, creaturePosition, true, callSource);
				if (validationResult.expectedPositionIsWrong) {
					correctRegisteredCreature(creature, previousPosition, creaturePosition, validationResult);
				}
				else {
					updateRegisteredCreature(creature, creaturePosition);
				}
			}
		}
	}
}

void ProtocolGame::sendUpdateTileItem(const Tile* tile, const StackPosition& position, const Item* item)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendUpdateTileItem(position = " << position << ", item = '" << item->getName() << "') - canSee = " << (canSee(position) ? "true" : "false"));

	if(!canSee(position))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		UpdateTileItem(msg, position, item);
	}
}

void ProtocolGame::sendRemoveTileItem(const Tile* tile, const StackPosition& position, const Item* item, const char* callSource)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRemoveTileItem(position = " << position << ") - canSee = " << (canSee(position) ? "true" : "false"));

	if(!canSee(position))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);

		RemoveTileItem(msg, position);

		if (tile->getCreatures() != nullptr) {
			StackPosition creaturePosition(position);
			for (const auto& creature : boost::adaptors::reverse(*tile->getCreatures())) {
				if (!player->canSeeCreature(creature.get())) {
					continue;
				}

				creaturePosition.index = tile->getClientIndexOfThing(player.get(), creature.get());
				if (position.index > creaturePosition.index) {
					// existing creature move down by 1 stackpos
					continue;
				}

				StackPosition previousPosition(creaturePosition, creaturePosition.index + 1);
				CreatureValidationResult validationResult = validateRegisteredCreature(creature, previousPosition, creaturePosition, true, callSource);
				if (validationResult.expectedPositionIsWrong) {
					correctRegisteredCreature(creature, previousPosition, creaturePosition, validationResult);
				}
				else {
					updateRegisteredCreature(creature, creaturePosition);
				}
			}
		}
	}
}

void ProtocolGame::sendUpdateTile(const Tile* tile, const Position& pos)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendUpdateTile(pos = " << pos << ") - canSee = " << (canSee(pos) ? "true" : "false"));

	if(!canSee(pos))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x69);
		msg->AddPosition(pos);
		if(tile)
		{
			GetTileDescription(tile, msg);
			msg->AddByte(0x00);
			msg->AddByte(0xFF);
		}
		else
		{
			msg->AddByte(0x01);
			msg->AddByte(0xFF);
		}
	}
}


void ProtocolGame::sendAddCreature(const CreatureP& creature, const StackPosition& position, const char* callSource) {
	assert(creature != nullptr);
	assert(callSource != nullptr);

	LOGt("ProtocolGame(" << player->getName() << ")::sendAddCreature(" << creature << ", position = " << position << ") - canSee = " << (canSee(creature.get()) ? "true" : "false"));

	CreatureValidationResult validationResult = validateRegisteredCreature(creature, StackPosition::INVALID, position, false, callSource);
	if (validationResult.registered) {
		if (validationResult.expectedPositionIsWrong) {
			correctRegisteredCreature(creature, position, position, validationResult);
		}

		if (validationResult.expectedPosition.isValid()) {
			// the position is already known
			return;
		}
	}

	if(!canSee(creature.get()))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(!msg)
		return;

	TRACK_MESSAGE(msg);
	if(creature != player)
	{
		AddTileCreature(msg, position, creature);
		updateCreaturesAtPosition(position);
		return;
	}

	msg->AddByte(0x0A);
	msg->AddU32(player->getID());
	msg->AddU16(0x32);

	msg->AddByte(player->hasFlag(PlayerFlag_CanReportBugs));
	if(Group* group = player->getGroup())
	{
		int32_t reasons = group->getViolationReasons();
		if(reasons > 1)
		{
			msg->AddByte(0x0B);
			for(int32_t i = 0; i < 20; ++i)
			{
				if(i < 4)
					msg->AddByte(group->getNameViolationFlags());
				else if(i < reasons)
					msg->AddByte(group->getStatementViolationFlags());
				else
					msg->AddByte(0x00);
			}
		}
	}

	AddMapDescription(msg, position);
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
		AddInventoryItem(msg, (slots_t)i, player->getInventoryItem((slots_t)i));

	AddPlayerStats(msg);
	AddPlayerSkills(msg);

	//gameworld light-settings
	LightInfo lightInfo;
	server.game().getWorldLightInfo(lightInfo);
	AddWorldLight(msg, lightInfo);

	//player light level
	AddCreatureLight(msg, creature.get());
	player->sendIcons();
	for(VIPListSet::iterator it = player->VIPList.begin(); it != player->VIPList.end(); it++)
	{
		std::string vipName;
		if(IOLoginData::getInstance()->getNameByGuid((*it), vipName))
		{
			PlayerP tmpPlayer = server.game().getPlayerByName(vipName);
			sendVIP((*it), vipName, (tmpPlayer && player->canSeeCreature(tmpPlayer.get())));
		}
	}
}

void ProtocolGame::sendRemoveCreature(const CreatureP& creature, StackPosition position, const char* callSource) {
	LOGt("ProtocolGame(" << player->getName() << ")::sendRemoveCreature(" << creature << ", position = " << position << ") - canSee = " << (canSee(creature.get()) ? "true" : "false"));

	CreatureValidationResult validationResult = validateRegisteredCreature(creature, position, StackPosition::INVALID, false, callSource);
	if (validationResult.expectedPositionIsWrong) {
		correctRegisteredCreature(creature, position, position, validationResult);
		return;
	}

	if(!canSee(position))
		return;

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		RemoveTileCreature(msg, position);

		invalidateCreature(creature);
		updateCreaturesAtPosition(position);
	}
}


void ProtocolGame::sendMoveCreature(const CreatureP& creature, const Tile* newTile, const StackPosition& newPosition, const Tile* oldTile, StackPosition oldPosition, bool teleport, const char* callSource) {
	LOGt("ProtocolGame(" << player->getName() << ")::sendMoveCreature(" << creature << ", newPosition = " << newPosition << ", oldPosition = " << oldPosition << ") - canSee(new) = " << (canSee(newPosition) ? "true" : "false") << ", canSee(old) = " << (canSee(oldPosition) ? "true" : "false"));

	assert(creature != nullptr);
	assert(newTile != nullptr);
	assert(oldTile != nullptr);
	assert(callSource != nullptr);

	CreatureValidationResult validationResult = validateRegisteredCreature(creature, oldPosition, newPosition, canSee(oldPosition), callSource);
	if (validationResult.expectedPositionIsWrong) {
		correctRegisteredCreature(creature, oldPosition, newPosition, validationResult);
		return;
	}

	if(creature == player)
	{
		LOGt("creature == player");
		NetworkMessage_ptr msg = getOutputBuffer();
		if(msg)
		{
			TRACK_MESSAGE(msg);
			if(teleport)
			{
				LOGt("teleport");
				RemoveTileCreature(msg, oldPosition);
				AddMapDescription(msg, newPosition);
			}
			else
			{
				if(oldPosition.z != 7 || newPosition.z < 8)
				{
					LOGt("move");
					msg->AddByte(0x6D);
					msg->AddPositionEx(oldPosition);
					msg->AddPosition(newPosition);
				}
				else
					RemoveTileCreature(msg, oldPosition);

				if(newPosition.z > oldPosition.z)
					MoveDownCreature(msg, creature.get(), newPosition, oldPosition, oldPosition.index);
				else if(newPosition.z < oldPosition.z)
					MoveUpCreature(msg, creature.get(), newPosition, oldPosition, oldPosition.index);

				if(oldPosition.y > newPosition.y) // north, for old x
				{
					msg->AddByte(0x65);
					GetMapDescription(oldPosition.x - 8, newPosition.y - 6, newPosition.z, 18, 1, msg);
				}
				else if(oldPosition.y < newPosition.y) // south, for old x
				{
					msg->AddByte(0x67);
					GetMapDescription(oldPosition.x - 8, newPosition.y + 7, newPosition.z, 18, 1, msg);
				}

				if(oldPosition.x < newPosition.x) // east, [with new y]
				{
					msg->AddByte(0x66);
					GetMapDescription(newPosition.x + 9, newPosition.y - 6, newPosition.z, 1, 14, msg);
				}
				else if(oldPosition.x > newPosition.x) // west, [with new y]
				{
					msg->AddByte(0x68);
					GetMapDescription(newPosition.x - 8, newPosition.y - 6, newPosition.z, 1, 14, msg);
				}

				updateCreaturesAtPosition(newPosition);
			}

			updateCreaturesAtPosition(oldPosition);
			invalidateDistantCreatures();
		}
	}
	else if(canSee(oldPosition) && canSee(newPosition))
	{
		if(!player->canSeeCreature(creature.get()))
			return;

		NetworkMessage_ptr msg = getOutputBuffer();
		if(msg)
		{
			TRACK_MESSAGE(msg);
			if(!teleport && (oldPosition.z != 7 || newPosition.z < 8))
			{
				LOGt("move");
				msg->AddByte(0x6D);
				msg->AddPositionEx(oldPosition);
				msg->AddPosition(newPosition);
			}
			else
			{
				LOGt("teleport");
				RemoveTileCreature(msg, oldPosition);
				AddTileCreature(msg, newPosition, creature);
			}

			updateCreaturesAtPosition(oldPosition);
			updateCreaturesAtPosition(newPosition);
		}
	}
	else if(canSee(oldPosition))
	{
		if(!player->canSeeCreature(creature.get()))
			return;
		LOGt("gone");

		NetworkMessage_ptr msg = getOutputBuffer();
		if(msg)
		{
			TRACK_MESSAGE(msg);
			RemoveTileCreature(msg, oldPosition);

			invalidateCreature(creature);
			updateCreaturesAtPosition(oldPosition);
		}
	}
	else if(canSee(creature.get()) && player->canSeeCreature(creature.get()))
	{
		LOGt("came");
		NetworkMessage_ptr msg = getOutputBuffer();
		if(msg)
		{
			TRACK_MESSAGE(msg);
			AddTileCreature(msg, newPosition, creature);

			updateCreaturesAtPosition(newPosition);
		}
	}
}

//inventory
void ProtocolGame::sendAddInventoryItem(slots_t slot, const Item* item)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendAddInventoryItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddInventoryItem(msg, slot, item);
	}
}

void ProtocolGame::sendUpdateInventoryItem(slots_t slot, const Item* item)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendUpdateInventoryItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		UpdateInventoryItem(msg, slot, item);
	}
}

void ProtocolGame::sendRemoveInventoryItem(slots_t slot)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRemoveInventoryItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		RemoveInventoryItem(msg, slot);
	}
}

//containers
void ProtocolGame::sendAddContainerItem(uint8_t cid, const Item* item)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendAddContainerItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		AddContainerItem(msg, cid, item);
	}
}

void ProtocolGame::sendUpdateContainerItem(uint8_t cid, uint8_t slot, const Item* item)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendUpdateContainerItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		UpdateContainerItem(msg, cid, slot, item);
	}
}

void ProtocolGame::sendRemoveContainerItem(uint8_t cid, uint8_t slot)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendRemoveContainerItem()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		RemoveContainerItem(msg, cid, slot);
	}
}

void ProtocolGame::sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxLen, bool canWrite)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendTextWindow()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x96);
		msg->AddU32(windowTextId);
		msg->AddItemId(item);
		if(canWrite)
		{
			msg->AddU16(maxLen);
			msg->AddString(item->getText());
		}
		else
		{
			msg->AddU16(item->getText().size());
			msg->AddString(item->getText());
		}

		const std::string& writer = item->getWriter();
		if(writer.size())
			msg->AddString(writer);
		else
			msg->AddString("");

		time_t writtenDate = item->getDate();
		if(writtenDate > 0)
			msg->AddString(formatDate(writtenDate));
		else
			msg->AddString("");
	}
}

void ProtocolGame::sendTextWindow(uint32_t windowTextId, uint32_t itemId, const std::string& text)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendTextWindow()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x96);
		msg->AddU32(windowTextId);
		msg->AddItemId(itemId);

		msg->AddU16(text.size());
		msg->AddString(text);

		msg->AddString("");
		msg->AddString("");
	}
}

void ProtocolGame::sendHouseWindow(uint32_t windowTextId, House* _house,
	uint32_t listId, const std::string& text)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendHouseWindow()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0x97);
		msg->AddByte(0x00);
		msg->AddU32(windowTextId);
		msg->AddString(text);
	}
}

void ProtocolGame::sendOutfitWindow()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendOutfitWindow()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xC8);
		AddCreatureOutfit(msg, player.get(), player->getDefaultOutfit(), true);

		std::list<Outfit> outfitList;
		for(OutfitMap::iterator it = player->outfits.begin(); it != player->outfits.end(); ++it)
		{
			if(player->canWearOutfit(it->first, it->second.addons))
				outfitList.push_back(it->second);
		}

 		if(outfitList.size())
		{
			msg->AddByte((size_t)std::min((size_t)OUTFITS_MAX_NUMBER, outfitList.size()));
			std::list<Outfit>::iterator it = outfitList.begin();
			for(int32_t i = 0; it != outfitList.end() && i < OUTFITS_MAX_NUMBER; ++it, ++i)
			{
				msg->AddU16(it->lookType);
				msg->AddString(it->name);
				if(player->hasCustomFlag(PlayerCustomFlag_CanWearAllAddons))
					msg->AddByte(0x03);
				else
					msg->AddByte(it->addons);
			}
		}
		else
		{
			msg->AddByte(1);
			msg->AddU16(player->getDefaultOutfit().lookType);
			msg->AddString("Outfit");
			msg->AddByte(player->getDefaultOutfit().lookAddons);
		}

		player->hasRequestedOutfit(true);
	}
}

void ProtocolGame::sendQuests()
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendQuests()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xF0);

		msg->AddU16(Quests::getInstance()->getQuestCount(player.get()));
		for(QuestList::const_iterator it = Quests::getInstance()->getFirstQuest(); it != Quests::getInstance()->getLastQuest(); ++it)
		{
			if(!(*it)->isStarted(player.get()))
				continue;

			msg->AddU16((*it)->getId());
			msg->AddString((*it)->getName());
			msg->AddByte((*it)->isCompleted(player.get()));
		}
	}
}

void ProtocolGame::sendQuestInfo(Quest* quest)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendQuestInfo()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xF1);
		msg->AddU16(quest->getId());

		msg->AddByte(quest->getMissionCount(player.get()));
		for(MissionList::const_iterator it = quest->getFirstMission(); it != quest->getLastMission(); ++it)
		{
			if(!(*it)->isStarted(player.get()))
				continue;

			msg->AddString((*it)->getName(player.get()));
			msg->AddString((*it)->getDescription(player.get()));
		}
	}
}

void ProtocolGame::sendVIPLogIn(uint32_t guid)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendVIPLogIn()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xD3);
		msg->AddU32(guid);
	}
}

void ProtocolGame::sendVIPLogOut(uint32_t guid)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendVIPLogOut()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xD4);
		msg->AddU32(guid);
	}
}

void ProtocolGame::sendVIP(uint32_t guid, const std::string& name, bool isOnline)
{
	LOGt("ProtocolGame(" << player->getName() << ")::sendVIP()");

	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xD2);
		msg->AddU32(guid);
		msg->AddString(name);
		msg->AddByte(isOnline ? 1 : 0);
	}
}

////////////// Add common messages
void ProtocolGame::AddMapDescription(NetworkMessage_ptr msg, const Position& pos)
{
	msg->AddByte(0x64);
	msg->AddPosition(player->getPosition());
	GetMapDescription(pos.x - 8, pos.y - 6, pos.z, 18, 14, msg);
}

void ProtocolGame::AddTextMessage(NetworkMessage_ptr msg, MessageClasses mclass, const std::string& message)
{
	msg->AddByte(0xB4);
	msg->AddByte(mclass);
	msg->AddString(message);
}

void ProtocolGame::AddAnimatedText(NetworkMessage_ptr msg, const Position& pos,
	uint8_t color, const std::string& text)
{
	msg->AddByte(0x84);
	msg->AddPosition(pos);
	msg->AddByte(color);
	msg->AddString(text);
}

void ProtocolGame::AddMagicEffect(NetworkMessage_ptr msg,const Position& pos, uint8_t type)
{
	msg->AddByte(0x83);
	msg->AddPosition(pos);
	msg->AddByte(type + 1);
}

void ProtocolGame::AddDistanceShoot(NetworkMessage_ptr msg, const Position& from, const Position& to,
	uint8_t type)
{
	msg->AddByte(0x85);
	msg->AddPosition(from);
	msg->AddPosition(to);
	msg->AddByte(type + 1);
}

void ProtocolGame::AddCreature(NetworkMessage_ptr msg, const CreatureP& creature, const StackPosition& position) {
	LOGt("ProtocolGame(" << player->getName() << ")::AddCreature(" << creature << ", position = " << position << ")");

	uint32_t removedCreatureId;
	bool added = registerCreature(creature, position, removedCreatureId);

	if(added)
	{
		LOGt("added");
		msg->AddU16(0x61);
		msg->AddU32(removedCreatureId);
		msg->AddU32(creature->getID());
		msg->AddString(creature->getHideName() ? "" : creature->getName());
	}
	else
	{
		LOGt("updated");
		msg->AddU16(0x62);
		msg->AddU32(creature->getID());

		updateRegisteredCreature(creature, position);
	}

	if(!creature->getHideHealth())
		msg->AddByte((int32_t)std::ceil(((float)creature->getHealth()) * 100 / std::max(creature->getMaxHealth(), (int32_t)1)));
	else
		msg->AddByte(0x00);

	msg->AddByte((uint8_t)creature->getDirection());
	AddCreatureOutfit(msg, creature.get(), creature->getCurrentOutfit());

	LightInfo lightInfo;
	creature->getCreatureLight(lightInfo);
	msg->AddByte(player->hasCustomFlag(PlayerCustomFlag_HasFullLight) ? 0xFF : lightInfo.level);
	msg->AddByte(lightInfo.color);

	msg->AddU16(creature->getStepSpeed());
	msg->AddByte(player->getSkullClient(creature.get()));
	msg->AddByte(player->getPartyShield(creature.get()));
	if(added)
		msg->AddByte(0x00); // war emblem

	msg->AddByte(!player->canWalkthrough(creature.get()));
}

void ProtocolGame::AddPlayerStats(NetworkMessage_ptr msg)
{
	msg->AddByte(0xA0);
	msg->AddU16(player->getHealth());
	msg->AddU16(player->getPlayerInfo(PLAYERINFO_MAXHEALTH));
	msg->AddU32(uint32_t(player->getFreeCapacity() * 100));
	uint64_t experience = player->getExperience();
	if(experience > 0x7FFFFFFF) // client debugs after 2,147,483,647 exp
		msg->AddU32(0x7FFFFFFF);
	else
		msg->AddU32(experience);

	msg->AddU16(player->getPlayerInfo(PLAYERINFO_LEVEL));
	msg->AddByte(player->getPlayerInfo(PLAYERINFO_LEVELPERCENT));
	msg->AddU16(player->getPlayerInfo(PLAYERINFO_MANA));
	msg->AddU16(player->getPlayerInfo(PLAYERINFO_MAXMANA));
	msg->AddByte(player->getPlayerInfo(PLAYERINFO_MAGICLEVEL));
	msg->AddByte(player->getPlayerInfo(PLAYERINFO_MAGICLEVELPERCENT));
	msg->AddByte(player->getPlayerInfo(PLAYERINFO_SOUL));
	msg->AddU16(player->getStaminaMinutes());
}

void ProtocolGame::AddPlayerSkills(NetworkMessage_ptr msg)
{
	msg->AddByte(0xA1);
	msg->AddByte(player->getSkill(SKILL_FIST, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_FIST, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_CLUB, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_CLUB, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_SWORD, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_SWORD, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_AXE, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_AXE, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_DIST, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_DIST, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_SHIELD, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_SHIELD, SKILL_PERCENT));
	msg->AddByte(player->getSkill(SKILL_FISH, SKILL_LEVEL));
	msg->AddByte(player->getSkill(SKILL_FISH, SKILL_PERCENT));
}

void ProtocolGame::AddCreatureSpeak(NetworkMessage_ptr msg, const Creature* creature, SpeakClasses type,
	std::string text, uint16_t channelId, uint32_t time/*= 0*/, Position* pos/* = nullptr*/)
{
	msg->AddByte(0xAA);
	if(creature)
	{
		const Player* speaker = creature->getPlayer();
		if(speaker)
		{
			msg->AddU32(++server.chat().statement);
			server.chat().statementMap[server.chat().statement] = text;
		}
		else
			msg->AddU32(0x00);

		if(creature->getSpeakType() != SPEAK_CLASS_NONE)
			type = creature->getSpeakType();

		switch(type)
		{
			case SPEAK_CHANNEL_RA:
				msg->AddString("");
				break;
			case SPEAK_RVR_ANSWER:
				msg->AddString("Gamemaster");
				break;
			default:
				msg->AddString(!creature->getHideName() ? creature->getName() : "");
				break;
		}

		if(speaker && type != SPEAK_RVR_ANSWER && !speaker->isAccountManager()
			&& !speaker->hasCustomFlag(PlayerCustomFlag_HideLevel))
			msg->AddU16(speaker->getPlayerInfo(PLAYERINFO_LEVEL));
		else
			msg->AddU16(0x00);

	}
	else
	{
		msg->AddU32(0x00);
		msg->AddString("");
		msg->AddU16(0x00);
	}

	msg->AddByte(type);
	switch(type)
	{
		case SPEAK_SAY:
		case SPEAK_WHISPER:
		case SPEAK_YELL:
		case SPEAK_MONSTER_SAY:
		case SPEAK_MONSTER_YELL:
		case SPEAK_PRIVATE_NP:
		{
			if(pos)
				msg->AddPosition(*pos);
			else if(creature)
				msg->AddPosition(creature->getPosition());
			else
				msg->AddPosition(Position(0,0,7));

			break;
		}

		case SPEAK_CHANNEL_Y:
		case SPEAK_CHANNEL_RN:
		case SPEAK_CHANNEL_RA:
		case SPEAK_CHANNEL_O:
		case SPEAK_CHANNEL_W:
			msg->AddU16(channelId);
			break;

		case SPEAK_RVR_CHANNEL:
		{
			msg->AddU32(uint32_t(OTSYS_TIME() / 1000 & 0xFFFFFFFF) - time);
			break;
		}

		default:
			break;
	}

	msg->AddString(text);
}

void ProtocolGame::AddCreatureHealth(NetworkMessage_ptr msg,const Creature* creature)
{
	msg->AddByte(0x8C);
	msg->AddU32(creature->getID());
	if(!creature->getHideHealth())
		msg->AddByte((int32_t)std::ceil(((float)creature->getHealth()) * 100 / std::max(creature->getMaxHealth(), (int32_t)1)));
	else
		msg->AddByte(0x00);
}

void ProtocolGame::AddCreatureOutfit(NetworkMessage_ptr msg, const Creature* creature, const Outfit_t& outfit, bool outfitWindow/* = false*/)
{
	if(outfitWindow || !creature->getPlayer() || (!creature->isInvisible() && (!creature->isGhost()
		|| !server.configManager().getBool(ConfigManager::GHOST_INVISIBLE_EFFECT))))
	{
		msg->AddU16(outfit.lookType);
		if(outfit.lookType)
		{
			msg->AddByte(outfit.lookHead);
			msg->AddByte(outfit.lookBody);
			msg->AddByte(outfit.lookLegs);
			msg->AddByte(outfit.lookFeet);
			msg->AddByte(outfit.lookAddons);
		}
		else if(outfit.lookTypeEx)
			msg->AddItemId(outfit.lookTypeEx);
		else
			msg->AddU16(outfit.lookTypeEx);
	}
	else
		msg->AddU32(0x00);
}

void ProtocolGame::AddWorldLight(NetworkMessage_ptr msg, const LightInfo& lightInfo)
{
	msg->AddByte(0x82);
	msg->AddByte((player->hasCustomFlag(PlayerCustomFlag_HasFullLight) ? 0xFF : lightInfo.level));
	msg->AddByte(lightInfo.color);
}

void ProtocolGame::AddCreatureLight(NetworkMessage_ptr msg, const Creature* creature)
{
	LightInfo lightInfo;
	creature->getCreatureLight(lightInfo);
	msg->AddByte(0x8D);
	msg->AddU32(creature->getID());
	msg->AddByte((player->hasCustomFlag(PlayerCustomFlag_HasFullLight) ? 0xFF : lightInfo.level));
	msg->AddByte(lightInfo.color);
}

//tile
void ProtocolGame::AddTileItem(NetworkMessage_ptr msg, const StackPosition& position, const Item* item)
{
	msg->AddByte(0x6A);
	msg->AddPositionEx(position);
	msg->AddItem(item);
}

void ProtocolGame::AddTileCreature(NetworkMessage_ptr msg, const StackPosition& position, const CreatureP& creature)
{
	LOGt("ProtocolGame(" << player->getName() << ")::AddTileCreature(" << creature << ", position = " << position << ")");

	msg->AddByte(0x6A);
	msg->AddPositionEx(position);

	AddCreature(msg, creature, position);
}

void ProtocolGame::UpdateTileItem(NetworkMessage_ptr msg, const StackPosition& position, const Item* item)
{
	msg->AddByte(0x6B);
	msg->AddPositionEx(position);
	msg->AddItem(item);
}

void ProtocolGame::RemoveTileItem(NetworkMessage_ptr msg, const StackPosition& position)
{
	msg->AddByte(0x6C);
	msg->AddPositionEx(position);
}

void ProtocolGame::RemoveTileCreature(NetworkMessage_ptr msg, const StackPosition& position) {
	LOGt("ProtocolGame(" << player->getName() << ")::RemoveTileCreature(position = " << position << ")");

	msg->AddByte(0x6C);
	msg->AddPositionEx(position);
}

void ProtocolGame::MoveUpCreature(NetworkMessage_ptr msg, const Creature* creature,
	const Position& newPos, const Position& oldPos, uint32_t oldStackpos)
{
	if(creature != player)
		return;

	msg->AddByte(0xBE); //floor change up
	if(newPos.z == 7) //going to surface
	{
		int32_t skip = -1;
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 5, 18, 14, 3, skip); //(floor 7 and 6 already set)
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 4, 18, 14, 4, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 3, 18, 14, 5, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 2, 18, 14, 6, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 1, 18, 14, 7, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, 0, 18, 14, 8, skip);
		if(skip >= 0)
		{
			msg->AddByte(skip);
			msg->AddByte(0xFF);
		}
	}
	else if(newPos.z > 7) //underground, going one floor up (still underground)
	{
		int32_t skip = -1;
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, oldPos.z - 3, 18, 14, 3, skip);
		if(skip >= 0)
		{
			msg->AddByte(skip);
			msg->AddByte(0xFF);
		}
	}

	//moving up a floor up makes us out of sync
	//west
	msg->AddByte(0x68);
	GetMapDescription(oldPos.x - 8, oldPos.y + 1 - 6, newPos.z, 1, 14, msg);

	//north
	msg->AddByte(0x65);
	GetMapDescription(oldPos.x - 8, oldPos.y - 6, newPos.z, 18, 1, msg);
}

void ProtocolGame::MoveDownCreature(NetworkMessage_ptr msg, const Creature* creature,
	const Position& newPos, const Position& oldPos, uint32_t oldStackpos)
{
	if(creature != player)
		return;

	msg->AddByte(0xBF); //floor change down
	if(newPos.z == 8) //going from surface to underground
	{
		int32_t skip = -1;
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, newPos.z, 18, 14, -1, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, newPos.z + 1, 18, 14, -2, skip);
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, newPos.z + 2, 18, 14, -3, skip);
		if(skip >= 0)
		{
			msg->AddByte(skip);
			msg->AddByte(0xFF);
		}
	}
	else if(newPos.z > oldPos.z && newPos.z > 8 && newPos.z < 14) //going further down
	{
		int32_t skip = -1;
		GetFloorDescription(msg, oldPos.x - 8, oldPos.y - 6, newPos.z + 2, 18, 14, -3, skip);
		if(skip >= 0)
		{
			msg->AddByte(skip);
			msg->AddByte(0xFF);
		}
	}

	//moving down a floor makes us out of sync
	//east
	msg->AddByte(0x66);
	GetMapDescription(oldPos.x + 9, oldPos.y - 1 - 6, newPos.z, 1, 14, msg);

	//south
	msg->AddByte(0x67);
	GetMapDescription(oldPos.x - 8, oldPos.y + 7, newPos.z, 18, 1, msg);
}

//inventory
void ProtocolGame::AddInventoryItem(NetworkMessage_ptr msg, slots_t slot, const Item* item)
{
	if(item)
	{
		msg->AddByte(0x78);
		msg->AddByte(+slot);
		msg->AddItem(item);
	}
	else
		RemoveInventoryItem(msg, slot);
}

void ProtocolGame::RemoveInventoryItem(NetworkMessage_ptr msg, slots_t slot)
{
	msg->AddByte(0x79);
	msg->AddByte(+slot);
}

void ProtocolGame::UpdateInventoryItem(NetworkMessage_ptr msg, slots_t slot, const Item* item)
{
	AddInventoryItem(msg, slot, item);
}

//containers
void ProtocolGame::AddContainerItem(NetworkMessage_ptr msg, uint8_t cid, const Item* item)
{
	msg->AddByte(0x70);
	msg->AddByte(cid);
	msg->AddItem(item);
}

void ProtocolGame::UpdateContainerItem(NetworkMessage_ptr msg, uint8_t cid, uint8_t slot, const Item* item)
{
	msg->AddByte(0x71);
	msg->AddByte(cid);
	msg->AddByte(slot);
	msg->AddItem(item);
}

void ProtocolGame::RemoveContainerItem(NetworkMessage_ptr msg, uint8_t cid, uint8_t slot)
{
	msg->AddByte(0x72);
	msg->AddByte(cid);
	msg->AddByte(slot);
}

void ProtocolGame::sendChannelMessage(std::string author, std::string text, SpeakClasses type, uint8_t channel)
{
	NetworkMessage_ptr msg = getOutputBuffer();
	if(msg)
	{
		TRACK_MESSAGE(msg);
		msg->AddByte(0xAA);
		msg->AddU32(0x00);
		msg->AddString(author);
		msg->AddU16(0x00);
		msg->AddByte(type);
		msg->AddU16(channel);
		msg->AddString(text);
	}
}

void ProtocolGame::AddShopItem(NetworkMessage_ptr msg, const ShopInfo item)
{
	ItemKindPC kind = server.items()[item.itemId];
	if (kind) {
		msg->AddU16(kind->clientId);
		if(kind->isSplash() || kind->isFluidContainer())
			msg->AddByte(fluidMap[item.subType % 8]);
		else if(kind->stackable || kind->charges)
			msg->AddByte(item.subType);
		else
			msg->AddByte(0x01);

		msg->AddString(item.itemName);
		msg->AddU32(uint32_t(kind->weight * 100));
		msg->AddU32(item.buyPrice);
		msg->AddU32(item.sellPrice);
	}
	else {
		// TODO improve this

		msg->AddU16(0);
		msg->AddByte(0x01);
		msg->AddString("");
		msg->AddU32(0);
		msg->AddU32(0);
		msg->AddU32(0);
	}
}



ProtocolGame::CreatureValidationResult::CreatureValidationResult()
	: expectedPositionIsWrong(false),
	  registered(false)
{}


ProtocolGame::CreatureValidationResult::CreatureValidationResult(bool registered, const StackPosition& expectedPosition, bool expectedPositionIsWrong)
	: expectedPosition(expectedPosition),
	  expectedPositionIsWrong(expectedPositionIsWrong),
	  registered(registered)
{}
