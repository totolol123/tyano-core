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

#include "admin.h"
#include "rsa.h"
#include "tools.h"

#include "configmanager.h"
#include "game.h"

#include "connection.h"
#include "outputmessage.h"
#include "networkmessage.h"

#include "dispatcher.h"
#include "house.h"
#include "luascript.h"
#include "town.h"
#include "iologindata.h"
#include "player.h"
#include "task.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "textlogger.h"

#include "server.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t ProtocolAdmin::protocolAdminCount = 0;
#endif


LOGGER_DEFINITION(Admin);


Admin::Admin()
	: m_interface("Admin")
{
	m_enabled = m_onlyLocalHost = m_requireLogin = true;
	m_requireEncryption = false;
	m_currrentConnections = 0;
	m_key_RSA1024XTEA = nullptr;
	m_maxConnections = 1;
	m_password = "";
}


Admin::~Admin() {
	delete m_key_RSA1024XTEA;
}



void ProtocolAdmin::onRecvFirstMessage(NetworkMessage& msg)
{
	Admin& admin = server.admin();

	if(!admin.enabled())
	{
		getConnection()->close();
		return;
	}

	m_state = NO_CONNECTED;
	if(!admin.allowIP(getIP()))
	{
		addLogLine(LogType::EVENT, "ip not allowed");
		getConnection()->close();
		return;
	}

	if(!admin.addConnection())
	{
		addLogLine(LogType::EVENT, "cannot add new connection");
		getConnection()->close();
		return;
	}

	addLogLine(LogType::EVENT, "sending HELLO");
	if(OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false))
	{
		TRACK_MESSAGE(output);
		output->AddByte(AP_MSG_HELLO);
		output->AddU32(1); //version
		output->AddString("OTADMIN");
		output->AddU16(admin.getProtocolPolicy()); //security policy
		output->AddU32(admin.getProtocolOptions()); //protocol options(encryption, ...)
		OutputMessagePool::getInstance()->send(output);
	}

	m_lastCommand = time(nullptr);
	m_state = ENCRYPTION_NO_SET;
}

void ProtocolAdmin::parsePacket(NetworkMessage& msg)
{
	Game& game = server.game();
	Admin& admin = server.admin();

	if(game.getGameState() == GAME_STATE_SHUTDOWN)
	{
		getConnection()->close();
		return;
	}

	uint8_t recvbyte = msg.GetByte();
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	TRACK_MESSAGE(output);
	switch(m_state)
	{
		case ENCRYPTION_NO_SET:
		{
			if(admin.requireEncryption())
			{
				if((time(nullptr) - m_startTime) > 30000)
				{
					getConnection()->close();
					addLogLine(LogType::EVENT, "encryption timeout");
					return;
				}

				if(recvbyte != AP_MSG_ENCRYPTION && recvbyte != AP_MSG_KEY_EXCHANGE)
				{
					output->AddByte(AP_MSG_ERROR);
					output->AddString("encryption needed");
					OutputMessagePool::getInstance()->send(output);

					getConnection()->close();
					addLogLine(LogType::EVENT, "wrong command while ENCRYPTION_NO_SET");
					return;
				}
			}
			else
				m_state = NO_LOGGED_IN;

			break;
		}

		case NO_LOGGED_IN:
		{
			if(admin.requireLogin())
			{
				if((time(nullptr) - m_startTime) > 30000)
				{
					//login timeout
					getConnection()->close();
					addLogLine(LogType::EVENT, "login timeout");
					return;
				}

				if(m_loginTries > 3)
				{
					output->AddByte(AP_MSG_ERROR);
					output->AddString("too many login tries");
					OutputMessagePool::getInstance()->send(output);

					getConnection()->close();
					addLogLine(LogType::EVENT, "too many login tries");
					return;
				}

				if(recvbyte != AP_MSG_LOGIN)
				{
					output->AddByte(AP_MSG_ERROR);
					output->AddString("you are not logged in");
					OutputMessagePool::getInstance()->send(output);

					getConnection()->close();
					addLogLine(LogType::EVENT, "wrong command while NO_LOGGED_IN");
					return;
				}
			}
			else
				m_state = LOGGED_IN;

			break;
		}

		case LOGGED_IN:
			break;

		default:
		{
			getConnection()->close();
			addLogLine(LogType::EVENT, "no valid connection state!!!");
			return;
		}
	}

	m_lastCommand = time(nullptr);
	switch(recvbyte)
	{
		case AP_MSG_LOGIN:
		{
			if(m_state == NO_LOGGED_IN && admin.requireLogin())
			{
				std::string password = msg.GetString();
				if(admin.passwordMatch(password))
				{
					m_state = LOGGED_IN;
					output->AddByte(AP_MSG_LOGIN_OK);
					addLogLine(LogType::EVENT, "login ok");
				}
				else
				{
					m_loginTries++;
					output->AddByte(AP_MSG_LOGIN_FAILED);
					output->AddString("wrong password");
					addLogLine(LogType::EVENT, "login failed.("+ password + ")");
				}
			}
			else
			{
				output->AddByte(AP_MSG_LOGIN_FAILED);
				output->AddString("cannot login");
				addLogLine(LogType::EVENT, "wrong state at login");
			}

			break;
		}

		case AP_MSG_ENCRYPTION:
		{
			if(m_state == ENCRYPTION_NO_SET && admin.requireEncryption())
			{
				uint8_t keyType = msg.GetByte();
				switch(keyType)
				{
					case ENCRYPTION_RSA1024XTEA:
					{
						RSA* rsa = admin.getRSAKey(ENCRYPTION_RSA1024XTEA);
						if(!rsa)
						{
							output->AddByte(AP_MSG_ENCRYPTION_FAILED);
							addLogLine(LogType::EVENT, "no valid server key type");
							break;
						}

						if(RSA_decrypt(rsa, msg))
						{
							m_state = NO_LOGGED_IN;
							uint32_t k[4]= {msg.GetU32(), msg.GetU32(), msg.GetU32(), msg.GetU32()};

							//use for in/out the new key we have
							enableXTEAEncryption();
							setXTEAKey(k);

							output->AddByte(AP_MSG_ENCRYPTION_OK);
							addLogLine(LogType::EVENT, "encryption ok");
						}
						else
						{
							output->AddByte(AP_MSG_ENCRYPTION_FAILED);
							output->AddString("wrong encrypted packet");
							addLogLine(LogType::EVENT, "wrong encrypted packet");
						}

						break;
					}

					default:
					{
						output->AddByte(AP_MSG_ENCRYPTION_FAILED);
						output->AddString("no valid key type");

						addLogLine(LogType::EVENT, "no valid client key type");
						break;
					}
				}
			}
			else
			{
				output->AddByte(AP_MSG_ENCRYPTION_FAILED);
				output->AddString("cannot set encryption");
				addLogLine(LogType::EVENT, "cannot set encryption");
			}

			break;
		}

		case AP_MSG_KEY_EXCHANGE:
		{
			if(m_state == ENCRYPTION_NO_SET && admin.requireEncryption())
			{
				uint8_t keyType = msg.GetByte();
				switch(keyType)
				{
					case ENCRYPTION_RSA1024XTEA:
					{
						RSA* rsa = admin.getRSAKey(ENCRYPTION_RSA1024XTEA);
						if(!rsa)
						{
							output->AddByte(AP_MSG_KEY_EXCHANGE_FAILED);
							addLogLine(LogType::EVENT, "no valid server key type");
							break;
						}

						output->AddByte(AP_MSG_KEY_EXCHANGE_OK);
						output->AddByte(ENCRYPTION_RSA1024XTEA);

						char RSAPublicKey[128];
						rsa->getPublicKey(RSAPublicKey);

						output->AddBytes(RSAPublicKey, 128);
						break;
					}

					default:
					{
						output->AddByte(AP_MSG_KEY_EXCHANGE_FAILED);
						addLogLine(LogType::EVENT, "no valid client key type");
						break;
					}
				}
			}
			else
			{
				output->AddByte(AP_MSG_KEY_EXCHANGE_FAILED);
				output->AddString("cannot get public key");
				addLogLine(LogType::EVENT, "cannot get public key");
			}

			break;
		}

		case AP_MSG_COMMAND:
		{
			if(m_state != LOGGED_IN)
			{
				addLogLine(LogType::EVENT, "recvbyte == AP_MSG_COMMAND && m_state != LOGGED_IN !!!");
				break;
			}

			uint8_t command = msg.GetByte();
			switch(command)
			{
				case CMD_SAVE_SERVER:
				case CMD_SHALLOW_SAVE_SERVER:
				{
					addLogLine(LogType::EVENT, "saving server");
					server.dispatcher().addTask(Task::create(std::bind(
						&Game::saveGameState, &game, (command == CMD_SHALLOW_SAVE_SERVER))));

					output->AddByte(AP_MSG_COMMAND_OK);
					break;
				}

				case CMD_CLOSE_SERVER:
				{
					addLogLine(LogType::EVENT, "closing server");
					server.dispatcher().addTask(Task::create(std::bind(
						&Game::setGameState, &game, GAME_STATE_CLOSED)));

					output->AddByte(AP_MSG_COMMAND_OK);
					break;
				}

				case CMD_OPEN_SERVER:
				{
					addLogLine(LogType::EVENT, "opening server");
					server.dispatcher().addTask(Task::create(std::bind(
						&Game::setGameState, &game, GAME_STATE_NORMAL)));

					output->AddByte(AP_MSG_COMMAND_OK);
					break;
				}

				case CMD_SHUTDOWN_SERVER:
				{
					addLogLine(LogType::EVENT, "shutting down server");
					server.dispatcher().addTask(Task::create(std::bind(
						&Game::setGameState, &game, GAME_STATE_SHUTDOWN)));

					output->AddByte(AP_MSG_COMMAND_OK);
					break;
				}

				case CMD_PAY_HOUSES:
				{
					server.dispatcher().addTask(Task::create(std::bind(
						&ProtocolAdmin::adminCommandPayHouses, this)));
					break;
				}

				case CMD_RELOAD_SCRIPTS:
				{
					const int8_t reload = msg.GetByte();
					server.dispatcher().addTask(Task::create(std::bind(
						&ProtocolAdmin::adminCommandReload, this, reload)));
					break;
				}

				case CMD_KICK:
				{
					const std::string param = msg.GetString();
					server.dispatcher().addTask(Task::create(std::bind(
						&ProtocolAdmin::adminCommandKickPlayer, this, param)));
					break;
				}

				case CMD_SETOWNER:
				{
					const std::string param = msg.GetString();
					server.dispatcher().addTask(Task::create(std::bind(
						&ProtocolAdmin::adminCommandSetOwner, this, param)));
					break;
				}

				case CMD_SEND_MAIL:
				{
					const std::string xmlData = msg.GetString();
					server.dispatcher().addTask(Task::create(std::bind(
						&ProtocolAdmin::adminCommandSendMail, this, xmlData)));
					break;
				}

				case CMD_BROADCAST:
				{
					const std::string param = msg.GetString();
					addLogLine(LogType::EVENT, "broadcasting: " + param);
					server.dispatcher().addTask(Task::create(std::bind(
						&Game::broadcastMessage, &game, param, MSG_STATUS_WARNING)));

					output->AddByte(AP_MSG_COMMAND_OK);
					break;
				}

				case CMD_SCRIPT:
				{
					uint8_t flags = msg.GetByte();

					m_scriptBuffer << msg.GetString();

					if ((flags & 0x01) != 0) {
						// commit script
						std::string script = m_scriptBuffer.str();

						m_scriptBuffer.clear();
						m_scriptBuffer.str("");

						server.dispatcher().addTask(Task::create(std::bind(&ProtocolAdmin::adminCommandExecuteScript, this, script)));
					}
					else {
						output->AddByte(AP_MSG_COMMAND_OK);
					}

					break;
				}

				default:
				{
					output->AddByte(AP_MSG_COMMAND_FAILED);
					output->AddString("not known server command");
					addLogLine(LogType::EVENT, "not known server command");
				}
			}
			break;
		}

		case AP_MSG_PING:
			output->AddByte(AP_MSG_PING_OK);
			break;

		case AP_MSG_KEEP_ALIVE:
			break;

		default:
		{
			output->AddByte(AP_MSG_ERROR);
			output->AddString("not known command byte");

			addLogLine(LogType::EVENT, "not known command byte");
			break;
		}
	}

	if(output->getMessageLength() > 0)
		OutputMessagePool::getInstance()->send(output);
}

void ProtocolAdmin::deleteProtocolTask()
{
	addLogLine(LogType::EVENT, "end connection");
	server.admin().removeConnection();
	Protocol::deleteProtocolTask();
}

void ProtocolAdmin::adminCommandPayHouses()
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	Houses::getInstance()->payHouses();
	addLogLine(LogType::EVENT, "pay houses ok");

	TRACK_MESSAGE(output);
	output->AddByte(AP_MSG_COMMAND_OK);
	OutputMessagePool::getInstance()->send(output);
}

void ProtocolAdmin::adminCommandReload(int8_t reload)
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	server.game().reloadInfo((ReloadInfo_t)reload);
	addLogLine(LogType::EVENT, "reload ok");

	TRACK_MESSAGE(output);
	output->AddByte(AP_MSG_COMMAND_OK);
	OutputMessagePool::getInstance()->send(output);
}

void ProtocolAdmin::adminCommandKickPlayer(const std::string& param)
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	TRACK_MESSAGE(output);
	Player* player = nullptr;
	if(server.game().getPlayerByNameWildcard(param, player) == RET_NOERROR)
	{
		server.scheduler().addTask(SchedulerTask::create(Milliseconds(SCHEDULER_MINTICKS), std::bind(&Game::kickPlayer, &server.game(), player->getID(), false)));
		addLogLine(LogType::EVENT, "kicking player " + player->getName());
		output->AddByte(AP_MSG_COMMAND_OK);
	}
	else
	{
		addLogLine(LogType::EVENT, "failed setting kick for player " + param);
		output->AddByte(AP_MSG_COMMAND_FAILED);
		output->AddString("player is not online");
	}

	OutputMessagePool::getInstance()->send(output);
}


void ProtocolAdmin::adminCommandExecuteScript(const std::string& script) {
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if (output == nullptr) {
		return;
	}

	TRACK_MESSAGE(output);

	addLogLine(LogType::EVENT, "Excuting script: " + script);

	std::string result;
	if (server.admin().executeScript(script, result)) {
		output->AddByte(AP_MSG_COMMAND_OK);
	}
	else {
		output->AddByte(AP_MSG_COMMAND_FAILED);
	}

	output->AddString(result);

	OutputMessagePool::getInstance()->send(output);
}


void ProtocolAdmin::adminCommandSetOwner(const std::string& param)
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	StringVector params = explodeString(param, ",");
	for(StringVector::iterator it = params.begin(); it != params.end(); ++it)
		trimString(*it);

	TRACK_MESSAGE(output);
	int32_t houseId = atoi(params[0].c_str());
	if(houseId > 0)
	{
		size_t size = params.size();
		if(size > 1)
		{
			std::string name = params[1];
			bool clean = true;
			if(size > 2)
				clean = booleanString(params[2]);

			if(House* house = Houses::getInstance()->getHouse(houseId))
			{
				uint32_t guid;
				if(IOLoginData::getInstance()->getGuidByName(guid, name))
				{
					if(house->setOwnerEx(guid, clean))
					{
						addLogLine(LogType::EVENT, "Set " + name + " as new owner of house with id " + house->getName());
						output->AddByte(AP_MSG_COMMAND_OK);
					}
					else
					{
						addLogLine(LogType::EVENT, "Failed setting " + name + " as new owner of house with id " + house->getName());
						output->AddByte(AP_MSG_COMMAND_FAILED);
						output->AddString("failed while setting owner");
					}
				}
				else
				{
					addLogLine(LogType::EVENT, "Could not find player with name: " + name);
					output->AddByte(AP_MSG_COMMAND_FAILED);
					output->AddString("such player does not exists");
				}
			}
			else
			{
				addLogLine(LogType::EVENT, "Could not find house with id: " + houseId);
				output->AddByte(AP_MSG_COMMAND_FAILED);
				output->AddString("such house does not exists");
			}
		}
		else
		{
			addLogLine(LogType::EVENT, "Not enough params given, param data: " + param);
			output->AddByte(AP_MSG_COMMAND_FAILED);
			output->AddString("not enough params");
		}
	}
	else
	{
		addLogLine(LogType::EVENT, "Specified house id is not a valid one: " + params[0]);
		output->AddByte(AP_MSG_COMMAND_FAILED);
		output->AddString("invalid house id");
	}

	OutputMessagePool::getInstance()->send(output);
}

void ProtocolAdmin::adminCommandSendMail(const std::string& xmlData)
{
	OutputMessage_ptr output = OutputMessagePool::getInstance()->getOutputMessage(this, false);
	if(!output)
		return;

	std::string name;
	uint32_t depotId;

	TRACK_MESSAGE(output);
	if(boost::intrusive_ptr<Item> mailItem = Admin::createMail(xmlData, name, depotId))
	{
		if(IOLoginData::getInstance()->playerMail(nullptr, name, depotId, mailItem.get()))
		{
			addLogLine(LogType::EVENT, "sent mailbox to " + name);
			output->AddByte(AP_MSG_COMMAND_OK);
		}
		else
		{
			addLogLine(LogType::EVENT, "failed sending mailbox to " + name);
			output->AddByte(AP_MSG_COMMAND_FAILED);
			output->AddString("could not send the box");
		}
	}
	else
	{
		addLogLine(LogType::EVENT, "failed parsing mailbox");
		output->AddByte(AP_MSG_COMMAND_FAILED);
		output->AddString("could not parse the box");
	}

	OutputMessagePool::getInstance()->send(output);
}

bool Admin::loadFromXml()
{
	xmlDocPtr doc = xmlParseFile(getFilePath(FileType::XML, "admin.xml").c_str());
	if(!doc)
	{
		LOGe("[Admin::loadFromXml] Cannot load admin file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr p, q, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"otadmin"))
	{
		LOGe("[Admin::loadFromXml] Malformed admin file.");
		xmlFreeDoc(doc);
		return false;
	}

	std::string strValue;
	if(readXMLString(root, "enabled", strValue))
		m_enabled = booleanString(strValue);

	int32_t intValue;
	p = root->children;
	while(p)
	{
		if(xmlStrEqual(p->name, (const xmlChar*)"security"))
		{
			if(readXMLString(p, "onlylocalhost", strValue))
				m_onlyLocalHost = booleanString(strValue);
			if(readXMLInteger(p, "maxconnections", intValue) && intValue > 0)
				m_maxConnections = intValue;
			if(readXMLString(p, "loginrequired", strValue))
				m_requireLogin = booleanString(strValue);
			if(readXMLString(p, "loginpassword", strValue))
				m_password = strValue;
			else if(m_requireLogin)
				LOGw("[Admin::loadFromXml] Login required, but no password specified - using default.");
		}
		else if(xmlStrEqual(p->name, (const xmlChar*)"encryption"))
		{
			if(readXMLString(p, "required", strValue))
				m_requireEncryption = booleanString(strValue);

			q = p->children;
			while(q)
			{
				if(xmlStrEqual(q->name, (const xmlChar*)"key"))
				{
					if(readXMLString(q, "type", strValue))
					{
						if(asLowerCaseString(strValue) == "rsa1024xtea")
						{
							if(readXMLString(q, "file", strValue))
							{
								m_key_RSA1024XTEA = new RSA();
								if(!m_key_RSA1024XTEA->setKey(getFilePath(FileType::XML, strValue)))
								{
									delete m_key_RSA1024XTEA;
									m_key_RSA1024XTEA = nullptr;
									LOGe("[Admin::loadFromXml] Could not load RSA key from file " << getFilePath(FileType::XML, strValue));
								}
							}
							else
								LOGe("[Admin::loadFromXml] Missing file for RSA1024XTEA key.");
						}
						else
							LOGe("[Admin::loadFromXml] " << strValue << " is not a valid key type.");
					}
				}

				q = q->next;
			}
		}

		p = p->next;
	}

	xmlFreeDoc(doc);
	return true;
}

bool Admin::addConnection()
{
	if(m_currrentConnections >= m_maxConnections)
		return false;

	m_currrentConnections++;
	return true;
}


bool Admin::executeScript(const std::string& script, std::string& result) {
	m_interface.initState();

	return m_interface.call(script, result);
}


void Admin::removeConnection()
{
	if(m_currrentConnections > 0)
		m_currrentConnections--;
}

uint16_t Admin::getProtocolPolicy()
{
	uint16_t policy = 0;
	if(requireLogin())
		policy |= REQUIRE_LOGIN;
	if(requireEncryption())
		policy |= REQUIRE_ENCRYPTION;

	return policy;
}

uint32_t Admin::getProtocolOptions()
{
	uint32_t ret = 0;
	if(requireEncryption() && m_key_RSA1024XTEA)
		ret |= ENCRYPTION_RSA1024XTEA;

	return ret;
}

RSA* Admin::getRSAKey(uint8_t type)
{
	switch(type)
	{
		case ENCRYPTION_RSA1024XTEA:
			return m_key_RSA1024XTEA;

		default:
			break;
	}

	return nullptr;
}

boost::intrusive_ptr<Item> Admin::createMail(const std::string xmlData, std::string& name, uint32_t& depotId)
{
	xmlDocPtr doc = xmlParseMemory(xmlData.c_str(), xmlData.length());
	if(!doc)
		return nullptr;

	xmlNodePtr root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*)"mail"))
		return nullptr;

	int32_t intValue;
	std::string strValue;

	int32_t itemId = ITEM_PARCEL;
	if(readXMLString(root, "to", strValue))
		name = strValue;

	if(readXMLString(root, "town", strValue))
	{
		Town* town = server.towns().getTown(strValue);
		if(!town)
			return nullptr;

		depotId = town->getID();
	}
	else if(!IOLoginData::getInstance()->getDefaultTownByName(name, depotId)) //use the players default town
		return nullptr;

	if(readXMLInteger(root, "id", intValue))
		itemId = intValue;

	boost::intrusive_ptr<Item> mailItem = Item::CreateItem(itemId);
	mailItem->setParent(&VirtualCylinder::virtualCylinder);
	if(Container* mailContainer = mailItem->getContainer())
	{
		xmlNodePtr node = root->children;
		while(node)
		{
			if(node->type != XML_ELEMENT_NODE)
			{
				node = node->next;
				continue;
			}

			if(!Item::loadItem(node, mailContainer))
			{
				delete mailContainer;
				return nullptr;
			}

			node = node->next;
		}
	}

	return mailItem;
}

bool Admin::allowIP(uint32_t ip)
{
	if(!m_onlyLocalHost)
		return !ConnectionManager::getInstance()->isDisabled(ip, 0xFE);

	if(ip == 0x0100007F) //127.0.0.1
		return true;

	if(server.configManager().getBool(ConfigManager::ADMIN_LOGS_ENABLED))
		LOG_MESSAGE(LogType::EVENT, "forbidden connection try", "ADMIN " + convertIPAddress(ip));

	return false;
}

bool Admin::passwordMatch(const std::string& password)
{
	//prevent empty password login
	if(!m_password.length())
		return false;

	return password == m_password;
}

void ProtocolAdmin::addLogLine(LogType type, std::string message)
{
	if(server.configManager().getBool(ConfigManager::ADMIN_LOGS_ENABLED))
		LOG_MESSAGE(type, message, "ADMIN " + convertIPAddress(getIP()))
}
