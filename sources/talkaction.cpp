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
#include "talkaction.h"

#include "iologindata.h"
#include "ioban.h"

#include "player.h"
#include "npc.h"

#include "house.h"
#include "housetile.h"
#include "monster.h"
#include "town.h"

#include "account.h"
#include "ioguild.h"
#include "status.h"
#include "textlogger.h"
#include "vocation.h"
#include "party.h"

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
#include "outputmessage.h"
#include "connection.h"
#include "admin.h"
#include "protocolgame.h"
#include "protocollogin.h"
#include "protocolold.h"
#endif

#include "condition.h"
#include "configmanager.h"
#include "game.h"
#include "chat.h"
#include "tools.h"
#include "server.h"


LOGGER_DEFINITION(TalkActions);


TalkActions::TalkActions()
	: m_interface("TalkAction Interface")
{}


void TalkActions::clear()
{
	talksMap.clear();
	m_interface.reInitState();
}

TalkActionP TalkActions::getEvent(const std::string& nodeName)
{
	if(asLowerCaseString(nodeName) == "talkaction")
		return std::make_shared<TalkAction>(&m_interface);

	return nullptr;
}

bool TalkActions::registerEvent(const TalkActionP& talkAction, xmlNodePtr p, bool override)
{
	std::string sep;
	if(!readXMLString(p, "separator", sep) || sep.empty())
		sep = ";";

	StringVector strVector = explodeString(talkAction->getWords(), sep);
	for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
	{
		trimString(*it);
		talkAction->setWords(*it);
		if(talksMap.find(*it) != talksMap.end())
		{
			if(!override)
			{
				LOGw("[TalkAction::configureEvent] Duplicate registered talkaction with words: " << (*it));
				continue;
			}
		}

		talksMap[(*it)] = talkAction;
	}

	return true;
}

bool TalkActions::onPlayerSay(Creature* creature, uint16_t channelId, const std::string& words, bool ignoreAccess)
{
	std::string cmdstring[TALKFILTER_LAST] = words, paramstring[TALKFILTER_LAST] = "";
	size_t loc = words.find('"', 0);
	if(loc != std::string::npos)
	{
		cmdstring[TALKFILTER_QUOTATION] = std::string(words, 0, loc);
		paramstring[TALKFILTER_QUOTATION] = std::string(words, (loc + 1), (words.size() - (loc - 1)));
		trimString(cmdstring[TALKFILTER_QUOTATION]);
	}

	loc = words.find(" ", 0);
	if(loc != std::string::npos)
	{
		cmdstring[TALKFILTER_WORD] = std::string(words, 0, loc);
		paramstring[TALKFILTER_WORD] = std::string(words, (loc + 1), (words.size() - (loc - 1)));

		size_t sloc = words.find(" ", ++loc);
		if(sloc != std::string::npos)
		{
			cmdstring[TALKFILTER_WORD_SPACED] = std::string(words, 0, sloc);
			paramstring[TALKFILTER_WORD_SPACED] = std::string(words, (sloc + 1), (words.size() - (sloc - 1)));
		}
	}

	TalkActionP talkAction;
	for(TalkActionsMap::iterator it = talksMap.begin(); it != talksMap.end(); ++it)
	{
		if(it->first == cmdstring[it->second->getFilter()] || (!it->second->isSensitive() &&
			boost::iequals(it->first, cmdstring[it->second->getFilter()])))
		{
			talkAction = it->second;
			break;
		}
	}

	if(!talkAction || (talkAction->getChannel() != -1 && talkAction->getChannel() != channelId))
		return false;

	Player* player = creature->getPlayer();
	StringVector exceptions = talkAction->getExceptions();
	if(player && ((!ignoreAccess && std::find(exceptions.begin(), exceptions.end(), asLowerCaseString(
		player->getName())) == exceptions.end() && talkAction->getAccess() > player->getAccess())
		|| player->isAccountManager()))
	{
		if(player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges))
		{
			player->sendTextMessage(MSG_STATUS_SMALL, "You cannot execute this talkaction.");
			return true;
		}

		return false;
	}

	if(talkAction->isLogged())
	{
		if(player)
			player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "> " + words);

		DeprecatedLogger::getInstance()->eFile("talkactions/" + creature->getName() + ".log", words, true);
	}

	if(talkAction->isScripted())
		return talkAction->executeSay(creature, cmdstring[talkAction->getFilter()], paramstring[talkAction->getFilter()], channelId);

	if(TalkFunction* function = talkAction->getFunction())
		return function(creature, cmdstring[talkAction->getFilter()], paramstring[talkAction->getFilter()]);

	return false;
}



LOGGER_DEFINITION(TalkAction);


TalkAction::TalkAction(LuaScriptInterface* _interface):
Event(_interface)
{
	m_function = nullptr;
	m_filter = TALKFILTER_WORD;
	m_access = 0;
	m_channel = -1;
	m_logged = m_hidden = false;
	m_sensitive = true;
}


bool TalkAction::configureEvent(xmlNodePtr p)
{
	std::string strValue;
	if(readXMLString(p, "words", strValue))
		m_words = strValue;
	else
	{
		LOGe("[TalkAction::configureEvent] No words for TalkAction.");
		return false;
	}

	if(readXMLString(p, "filter", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "quotation")
			m_filter = TALKFILTER_QUOTATION;
		else if(tmpStrValue == "word")
			m_filter = TALKFILTER_WORD;
		else if(tmpStrValue == "word-spaced")
			m_filter = TALKFILTER_WORD_SPACED;
		else
			LOGw("[TalkAction::configureEvent] Unknown filter for TalkAction: " << strValue << ", using default.");
	}

	int32_t intValue;
	if(readXMLInteger(p, "access", intValue))
		m_access = intValue;

	if(readXMLInteger(p, "channel", intValue))
		m_channel = intValue;

	if(readXMLString(p, "log", strValue) || readXMLString(p, "logged", strValue))
		m_logged = booleanString(strValue);

	if(readXMLString(p, "hide", strValue) || readXMLString(p, "hidden", strValue))
		m_hidden = booleanString(strValue);

	if(readXMLString(p, "case-sensitive", strValue) || readXMLString(p, "casesensitive", strValue) || readXMLString(p, "sensitive", strValue))
		m_sensitive = booleanString(strValue);

	if(readXMLString(p, "exception", strValue))
		m_exceptions = explodeString(asLowerCaseString(strValue), ";");

	return true;
}

bool TalkAction::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "housebuy")
		m_function = houseBuy;
 	else if(tmpFunctionName == "housesell")
		m_function = houseSell;
	else if(tmpFunctionName == "housekick")
		m_function = houseKick;
	else if(tmpFunctionName == "housedoorlist")
		m_function = houseDoorList;
	else if(tmpFunctionName == "houseguestlist")
		m_function = houseGuestList;
	else if(tmpFunctionName == "housesubownerlist")
		m_function = houseSubOwnerList;
 	else if(tmpFunctionName == "guildjoin")
		m_function = guildJoin;
 	else if(tmpFunctionName == "guildcreate")
		m_function = guildCreate;
	else if(tmpFunctionName == "thingproporties")
		m_function = thingProporties;
	else if(tmpFunctionName == "banishmentinfo")
		m_function = banishmentInfo;
	else if(tmpFunctionName == "diagnostics")
		m_function = diagnostics;
	else if(tmpFunctionName == "addskill")
		m_function = addSkill;
	else if(tmpFunctionName == "ghost")
		m_function = ghost;
	else
	{
		LOGw("[TalkAction::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

int32_t TalkAction::executeSay(Creature* creature, const std::string& words, std::string param, uint16_t channel)
{
	//onSay(cid, words, param, channel)
	if(m_interface->reserveEnv())
	{
		trimString(param);
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;
			scriptstream << "local cid = " << env->addThing(creature) << std::endl;

			scriptstream << "local words = \"" << words << "\"" << std::endl;
			scriptstream << "local param = \"" << param << "\"" << std::endl;
			scriptstream << "local channel = " << channel << std::endl;

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());

			lua_State* L = m_interface->getState();
			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));

			lua_pushstring(L, words.c_str());
			lua_pushstring(L, param.c_str());
			lua_pushnumber(L, channel);

			bool result = m_interface->callFunction(4);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[TalkAction::executeSay] Call stack overflow.");
		return 0;
	}
}

bool TalkAction::houseBuy(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !server.configManager().getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = server.game().getTile(pos);
	if(!tile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	HouseTile* houseTile = tile->getHouseTile();
	if(!houseTile)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	House* house = houseTile->getHouse();
	if(!house)
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->getDoorByPosition(pos))
	{
		player->sendCancel("You have to be looking at door of flat you would like to purchase.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(player->getGUID()))
		{
			player->sendCancel("You already rent another house.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint16_t accountHouses = server.configManager().getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(accountHouses > 0 && Houses::getInstance()->getHousesCount(player->getAccount()->getId()) >= accountHouses)
		{
			char buffer[80];
			sprintf(buffer, "You may own only %d house%s per account.", accountHouses, (accountHouses != 1 ? "s" : ""));

			player->sendCancel(buffer);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = server.configManager().getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(player->getLevel() < levelToBuyHouse)
		{
			char buffer[90];
			sprintf(buffer, "You have to be at least Level %d to purchase a house.", levelToBuyHouse);
			player->sendCancel(buffer);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}
	else
	{
		if(!player->getGuildId() || player->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("You have to be at least a guild leader to purchase a hall.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(player->getGuildId()))
		{
			player->sendCancel("Your guild rents already another hall.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	if(house->getOwner())
	{
		player->sendCancel("This flat is already owned by someone else.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(server.game().getMoney(player) < house->getPrice() || !server.game().removeMoney(player, house->getPrice()))
	{
		player->sendCancel("You do not have enough money.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	house->setOwnerEx(player->getGUID(), true);
	std::string ret = "You have successfully bought this ";
	if(house->isGuild())
		ret += "hall";
	else
		ret += "house";

	ret += ", remember to leave money at ";
	if(house->isGuild())
		ret += "guild owner ";

	if(server.configManager().getBool(ConfigManager::BANK_SYSTEM))
		ret += "bank or ";

	ret += "depot of this town for rent.";
	player->sendTextMessage(MSG_INFO_DESCR, ret.c_str());

	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	return false;
}

bool TalkAction::houseSell(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !server.configManager().getBool(ConfigManager::HOUSE_BUY_AND_SELL))
		return false;

	House* house = Houses::getInstance()->getHouseByPlayerId(player->getGUID());
	if(!house && (!player->getGuildId() || !(house = Houses::getInstance()->getHouseByGuildId(player->getGuildId()))))
	{
		player->sendCancel("You do not rent any flat.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(house->isGuild() && player->getGuildLevel() != GUILDLEVEL_LEADER)
	{
		player->sendCancel("You have to be at least a guild leader to sell this hall.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Player* tradePartner = nullptr;
	ReturnValue ret = server.game().getPlayerByNameWildcard(param, tradePartner);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(tradePartner == player)
	{
		player->sendCancel("You cannot trade with yourself.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!house->isGuild())
	{
		if(Houses::getInstance()->getHouseByPlayerId(tradePartner->getGUID()))
		{
			player->sendCancel("Trade player already rents another house.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint16_t housesPerAccount = server.configManager().getNumber(ConfigManager::HOUSES_PER_ACCOUNT);
		if(housesPerAccount > 0 && Houses::getInstance()->getHousesCount(tradePartner->getAccount()->getId()) >= housesPerAccount)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has reached limit of %d house%s per account.", housesPerAccount, (housesPerAccount != 1 ? "s" : ""));

			player->sendCancel(buffer);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		uint32_t levelToBuyHouse = server.configManager().getNumber(ConfigManager::LEVEL_TO_BUY_HOUSE);
		if(tradePartner->getLevel() < levelToBuyHouse)
		{
			char buffer[100];
			sprintf(buffer, "Trade player has to be at least Level %d to buy house.", levelToBuyHouse);

			player->sendCancel(buffer);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}
	else
	{
		if(!tradePartner->getGuildId() || tradePartner->getGuildLevel() != GUILDLEVEL_LEADER)
		{
			player->sendCancel("Trade player has to be at least a guild leader to buy a hall.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(Houses::getInstance()->getHouseByGuildId(tradePartner->getGuildId()))
		{
			player->sendCancel("Trade player's guild already rents another hall.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	if(!Position::areInRange<3,3,0>(tradePartner->getPosition(), player->getPosition()))
	{
		player->sendCancel("Trade player is too far away.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!Houses::getInstance()->payRent(player, house, 0))
	{
		player->sendCancel("You have to pay a pre-rent first.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	boost::intrusive_ptr<Item> transferItem = TransferItem::createTransferItem(house);
	player->transferContainer.__addThing(nullptr, transferItem.get());

	player->transferContainer.setParent(player);
	player->transferContainer.retain();

	if(!server.game().internalStartTrade(player, tradePartner, transferItem.get()))
		transferItem->onTradeEvent(ON_TRADE_CANCEL, player, nullptr);

	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	return false;
}

bool TalkAction::houseKick(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	Player* targetPlayer = nullptr;
	if(server.game().getPlayerByNameWildcard(param, targetPlayer) != RET_NOERROR)
		targetPlayer = player;

	House* house = Houses::getInstance()->getHouseByPlayer(targetPlayer);
	if(!house || !house->kickPlayer(player, targetPlayer))
	{
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		player->sendCancelMessage(RET_NOTPOSSIBLE);
	}
	else
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);

	return false;
}

bool TalkAction::houseDoorList(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(!house)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Door* door = house->getDoorByPosition(getNextPosition(player->getDirection(), player->getPosition()));
	if(door && house->canEditAccessList(door->getDoorId(), player))
	{
		player->setEditHouse(house, door->getDoorId());
		player->sendHouseWindow(house, door->getDoorId());
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return false;
}

bool TalkAction::houseGuestList(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(house && house->canEditAccessList(GUEST_LIST, player))
	{
		player->setEditHouse(house, GUEST_LIST);
		player->sendHouseWindow(house, GUEST_LIST);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return false;
}

bool TalkAction::houseSubOwnerList(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	House* house = Houses::getInstance()->getHouseByPlayer(player);
	if(house && house->canEditAccessList(SUBOWNER_LIST, player))
	{
		player->setEditHouse(house, SUBOWNER_LIST);
		player->sendHouseWindow(house, SUBOWNER_LIST);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return false;
}

bool TalkAction::guildJoin(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !server.configManager().getBool(ConfigManager::INGAME_GUILD_MANAGEMENT))
		return false;

	std::string param_ = param;
	trimString(param_);
	if(!player->getGuildId())
	{
		uint32_t guildId;
		if(IOGuild::getInstance()->getGuildId(guildId, param_))
		{
			if(player->isGuildInvited(guildId))
			{
				IOGuild::getInstance()->joinGuild(player, guildId);
				player->sendTextMessage(MSG_INFO_DESCR, "You have joined the guild.");

				char buffer[80];
				sprintf(buffer, "%s has joined the guild.", player->getName().c_str());
				if(ChatChannel* guildChannel = server.chat().getChannel(player, 0x00))
					guildChannel->talk(player, SPEAK_CHANNEL_RA, buffer);
			}
			else
				player->sendCancel("You are not invited to that guild.");
		}
		else
			player->sendCancel("There's no guild with that name.");
	}
	else
		player->sendCancel("You are already in a guild.");

	return true;
}

bool TalkAction::guildCreate(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || !server.configManager().getBool(ConfigManager::INGAME_GUILD_MANAGEMENT))
		return false;

	if(player->getGuildId())
	{
		player->sendCancel("You are already in a guild.");
		return true;
	}

	std::string param_ = param;
	trimString(param_);
	if(!isValidName(param_))
	{
		player->sendCancel("That guild name contains illegal characters, please choose another name.");
		return true;
	}

	const uint32_t minLength = server.configManager().getNumber(ConfigManager::MIN_GUILDNAME);
	const uint32_t maxLength = server.configManager().getNumber(ConfigManager::MAX_GUILDNAME);
	if(param_.length() < minLength)
	{
		player->sendCancel("That guild name is too short, please select a longer name.");
		return true;
	}

	if(param_.length() > maxLength)
	{
		player->sendCancel("That guild name is too long, please select a shorter name.");
		return true;
	}

	uint32_t guildId;
	if(IOGuild::getInstance()->getGuildId(guildId, param_))
	{
		player->sendCancel("There is already a guild with that name.");
		return true;
	}

	const uint32_t levelToFormGuild = server.configManager().getNumber(ConfigManager::LEVEL_TO_FORM_GUILD);
	if(player->getLevel() < levelToFormGuild)
	{
		char buffer[70 + levelToFormGuild];
		sprintf(buffer, "You have to be at least Level %d to form a guild.", levelToFormGuild);
		player->sendCancel(buffer);
		return true;
	}

	player->setGuildName(param_);
	IOGuild::getInstance()->createGuild(player);

	char buffer[50 + maxLength];
	sprintf(buffer, "You have formed guild \"%s\"!", param_.c_str());
	player->sendTextMessage(MSG_INFO_DESCR, buffer);
	return true;
}

bool TalkAction::thingProporties(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	const Position& pos = getNextPosition(player->getDirection(), player->getPosition());
	Tile* tile = server.game().getTile(pos);
	if(!tile)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No tile found.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return true;
	}

	Thing* thing = tile->getTopVisibleThing(creature);
	if(!thing)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "No object found.");
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return true;
	}

	boost::char_separator<char> sep(" ");
	tokenizer tokens(param, sep);

	std::string invalid;
	for(tokenizer::iterator it = tokens.begin(); it != tokens.end();)
	{
		std::string action = parseParams(it, tokens.end());
		toLowerCaseString(action);
		if(Item* item = thing->getItem())
		{
			if(action == "set")
			{
				std::string key = parseParams(it, tokens.end()), value = parseParams(it, tokens.end());
				if(atoi(value.c_str()) || value == "0")
					item->getAttributes().set(key, atoi(value.c_str()));
				else
					item->getAttributes().set(key, value);
			}
			else if(action == "erase" || action == "remove")
				item->getAttributes().remove(parseParams(it, tokens.end()));
			else if(action == "action" || action == "actionid" || action == "aid")
			{
				int32_t tmp = atoi(parseParams(it, tokens.end()).c_str());
				if(tmp > 0)
					item->setActionId(tmp);
				else
					item->resetActionId();
			}
			else if(action == "unique" || action == "uniqueid" || action == "uid")
			{
				int32_t tmp = atoi(parseParams(it, tokens.end()).c_str());
				if(tmp >= 1000 || tmp <= 0xFFFF)
					item->setUniqueId(tmp);
			}
			else
			{
				std::stringstream s;
				s << action << " (" << parseParams(it, tokens.end()) << ")";
				invalid += s.str();
				break;
			}
		}
		else if(Creature* _creature = thing->getCreature())
		{
			if(action == "health")
				_creature->changeHealth(atoi(parseParams(it, tokens.end()).c_str()), nullptr);
			else if(action == "maxhealth")
				_creature->changeMaxHealth(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "mana")
				_creature->changeMana(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "maxmana")
				_creature->changeMaxMana(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "basespeed")
				_creature->setBaseSpeed(atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "droploot")
				_creature->setDropLoot((lootDrop_t)atoi(parseParams(it, tokens.end()).c_str()));
			else if(action == "lossskill")
				_creature->setLossSkill(booleanString(parseParams(it, tokens.end())));
			else if(action == "cannotmove")
				_creature->setNoMove(booleanString(parseParams(it, tokens.end())));
			else if(action == "skull")
			{
				_creature->setSkull(getSkull(parseParams(it, tokens.end())));
				server.game().updateCreatureSkull(_creature);
			}
			else if(action == "speaktype")
				_creature->setSpeakType((SpeakClasses)atoi(parseParams(it, tokens.end()).c_str()));
			else if(Player* _player = _creature->getPlayer())
			{
				if(action == "fyi")
					_player->sendFYIBox(parseParams(it, tokens.end()).c_str());
				else if(action == "tutorial")
					_player->sendTutorial(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildlevel")
					_player->setGuildLevel((GuildLevel_t)atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildrank")
					_player->setRankId(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "guildnick")
					_player->setGuildNick(parseParams(it, tokens.end()).c_str());
				else if(action == "group")
					_player->setGroupId(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "vocation")
					_player->setVocation(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "sex" || action == "gender")
					_player->setSex(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "stamina")
					_player->setStaminaMinutes(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "town" || action == "temple")
				{
					if(Town* town = server.towns().getTown(parseParams(it, tokens.end())))
					{
						_player->setMasterPosition(town->getPosition());
						_player->setTown(town->getID());
					}
				}
				else if(action == "balance")
					_player->balance = atoi(parseParams(it, tokens.end()).c_str());
				else if(action == "marriage" || action == "partner")
					_player->marriage = atoi(parseParams(it, tokens.end()).c_str());
				else if(action == "rates")
					_player->rates[atoi(parseParams(it, tokens.end()).c_str())] = atof(
						parseParams(it, tokens.end()).c_str());
				else if(action == "idle")
					_player->setIdleTime(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "capacity" || action == "cap")
					_player->setCapacity(atoi(parseParams(it, tokens.end()).c_str()));
				else if(action == "execute")
					server.talkActions().onPlayerSay(_player, atoi(parseParams(it, tokens.end()).c_str()),
						parseParams(it, tokens.end()), booleanString(parseParams(it, tokens.end())));
				else if(action == "saving" || action == "save")
					_player->switchSaving();
				else
				{
					std::stringstream s;
					s << action << " (" << parseParams(it, tokens.end()) << ")";
					invalid += s.str();
					break;
				}
			}
			else
			{
				std::stringstream s;
				s << action << " (" << parseParams(it, tokens.end()) << ")";
				invalid += s.str();
				break;
			}
		}
	}

	const SpectatorList& list = server.game().getSpectators(pos);
	SpectatorList::const_iterator it;

	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTile(tile, pos);
	}

	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTile(tile, pos);

	server.game().addMagicEffect(pos, MAGIC_EFFECT_WRAPS_GREEN);
	if(invalid.empty())
		return true;

	std::string tmp = "Following action was invalid: " + invalid;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, tmp.c_str());
	return true;
}

bool TalkAction::banishmentInfo(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	StringVector params = explodeString(param, ",");
	std::string what = "Account";
	trimString(params[0]);

	Ban ban;
	ban.type = BAN_ACCOUNT;
	if(params.size() > 1)
	{
		trimString(params[1]);
		if(params[0].substr(0, 1) == "p")
		{
			what = "Character";
			ban.type = BAN_PLAYER;
			ban.param = PLAYERBAN_BANISHMENT;

			ban.value = atoi(params[1].c_str());
			if(!ban.value)
			{
				IOLoginData::getInstance()->getGuidByName(ban.value, params[1], true);
				if(!ban.value)
					ban.value = IOLoginData::getInstance()->getAccountIdByName(params[1]);
			}
		}
		else
		{
			ban.value = atoi(params[1].c_str());
			if(!ban.value)
			{
				IOLoginData::getInstance()->getAccountId(params[1], ban.value);
				if(!ban.value)
					ban.value = IOLoginData::getInstance()->getAccountIdByName(params[1]);
			}
		}
	}
	else
	{
		ban.value = atoi(params[0].c_str());
		if(!ban.value)
		{
			IOLoginData::getInstance()->getAccountId(params[0], ban.value);
			if(!ban.value)
				ban.value = IOLoginData::getInstance()->getAccountIdByName(params[0]);
		}
	}

	if(!ban.value)
	{
		toLowerCaseString(what);
		player->sendCancel("Invalid " + what + (std::string)" name or id.");
		return true;
	}

	if(!IOBan::getInstance()->getData(ban))
	{
		player->sendCancel("That player or account is not banished or deleted.");
		return true;
	}

	bool deletion = ban.expires < 0;
	std::string admin = "Automatic ";
	if(!ban.adminId)
		admin += (deletion ? "deletion" : "banishment");
	else
		IOLoginData::getInstance()->getNameByGuid(ban.adminId, admin, true);

	std::string end = "Banishment will be lifted at:\n";
	if(deletion)
		end = what + (std::string)" won't be undeleted";

	char buffer[500 + ban.comment.length()];
	sprintf(buffer, "%s has been %s at:\n%s by: %s,\nfor the following reason:\n%s.\nThe action taken was:\n%s.\nThe comment given was:\n%s.\n%s%s.",
		what.c_str(), (deletion ? "deleted" : "banished"), formatDateShort(ban.added).c_str(), admin.c_str(), getReason(ban.reason).c_str(),
		getAction(ban.action, false).c_str(), ban.comment.c_str(), end.c_str(), (deletion ? "." : formatDateShort(ban.expires, true).c_str()));

	player->sendFYIBox(buffer);
	return true;
}

bool TalkAction::diagnostics(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	std::stringstream text;
	text << "Server diagonostic:\n";
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	text.str("");
	text << "World:" << "\n";
	text << "--------------------\n";
	text << "Player: " << server.game().getPlayersOnline() << " (" << Player::playerCount << ")" << std::endl;
	text << "Npc: " << server.game().getNpcsOnline() << " (" << Npc::npcCount << ")" << std::endl << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	text.str("");
	text << "Protocols:" << "\n";
	text << "--------------------\n";
	text << "ProtocolGame: " << ProtocolGame::protocolGameCount << std::endl;
	text << "ProtocolLogin: " << ProtocolLogin::protocolLoginCount << std::endl;
	text << "ProtocolAdmin: " << ProtocolAdmin::protocolAdminCount << std::endl;
	text << "ProtocolStatus: " << ProtocolStatus::protocolStatusCount << std::endl;
	text << "ProtocolOld: " << ProtocolOld::protocolOldCount << std::endl << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	text.str("");
	text << "Connections:\n";
	text << "--------------------\n";
	text << "Active connections: " << Connection::connectionCount << "\n";
	text << "Total message pool: " << OutputMessagePool::getInstance()->getTotalMessageCount() << std::endl;
	text << "Auto message pool: " << OutputMessagePool::getInstance()->getAutoMessageCount() << std::endl;
	text << "Queued message pool: " << OutputMessagePool::getInstance()->getQueuedMessageCount() << std::endl;
	text << "Free message pool: " << OutputMessagePool::getInstance()->getAvailableMessageCount() << std::endl << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

	text.str("");
	text << "Libraries:\n";
	text << "--------------------\n";
	text << "Platform: " << BOOST_PLATFORM << std::endl;
	text << "Compiler: " << BOOST_COMPILER << std::endl;
	text << "Boost: " << BOOST_VERSION << std::endl;
	text << "ASIO: " << BOOST_ASIO_VERSION << std::endl;
	text << "XML: " << XML_DEFAULT_VERSION << std::endl;
	text << "Lua: " << LUA_VERSION << std::endl;
	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, text.str().c_str());

#endif
	return true;
}

bool TalkAction::addSkill(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	StringVector params = explodeString(param, ",");
	if(params.size() < 2)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "Command requires at least 2 parameters.");
		return true;
	}

	int32_t amount = 1;
	if(params.size() > 2)
	{
		std::string tmp = params[2];
		trimString(tmp);
		amount = atoi(tmp.c_str());
	}

	std::string name = params[0], skill = params[1];
	trimString(name);
	trimString(skill);

	Player* target = nullptr;
	ReturnValue ret = server.game().getPlayerByNameWildcard(name, target);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		return true;
	}

	if(skill[0] == 'l' || skill[0] == 'e') {
		int64_t diff = Player::getExpForLevel((int32_t)target->getLevel() + amount) - target->getExperience();
		if (diff >= 0) {
			target->addExperience((uint64_t)diff);
		} else {
			target->removeExperience((uint64_t)-diff);
		}
	}
	else if(skill[0] == 'm') {
		amount = std::max(1, amount);
		target->addManaSpent((uint64_t)(target->getVocation()->getReqMana(target->getMagicLevel() +
			amount) - target->getSpentMana()), false);
	}
	else
	{
		amount = std::max(1, amount);
		skills_t skillId = getSkillId(skill);
		target->addSkillAdvance(skillId, (uint32_t)(target->getVocation()->getReqSkillTries(skillId, target->getSkill(skillId,
			SKILL_LEVEL) + amount) - target->getSkill(skillId, SKILL_TRIES)), false);
	}

	return true;
}

bool TalkAction::ghost(Creature* creature, const std::string& cmd, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(player->hasFlag(PlayerFlag_CannotBeSeen))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Command disabled for players with special, invisibility flag.");
		return true;
	}

	SpectatorList::iterator it;
	SpectatorList list = server.game().getSpectators(player->getPosition());
	Player* tmpPlayer = nullptr;

	Condition* condition = nullptr;
	if((condition = player->getCondition(CONDITION_GAMEMASTER, CONDITIONID_DEFAULT, GAMEMASTER_INVISIBLE)))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "You are visible again.");
		IOLoginData::getInstance()->updateOnlineStatus(player->getGUID(), true);
		for(AutoList<Player>::iterator pit = Player::autoList.begin(); pit != Player::autoList.end(); ++pit)
		{
			if(!pit->second->canSeeCreature(player))
				pit->second->notifyLogIn(player);
		}

		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->canSeeCreature(player))
				tmpPlayer->sendMagicEffect(player->getPosition(), MAGIC_EFFECT_TELEPORT);
		}

		player->removeCondition(condition);
		server.game().internalCreatureChangeVisible(creature, VISIBLE_GHOST_APPEAR);
	}
	else if((condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_GAMEMASTER, -1, 0, false, GAMEMASTER_INVISIBLE)))
	{
		player->addCondition(condition);
		server.game().internalCreatureChangeVisible(creature, VISIBLE_GHOST_DISAPPEAR);
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()) && !tmpPlayer->canSeeCreature(player))
				tmpPlayer->sendMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		}

		for(AutoList<Player>::iterator pit = Player::autoList.begin(); pit != Player::autoList.end(); ++pit)
		{
			if(!pit->second->canSeeCreature(player))
				pit->second->notifyLogOut(player);
		}

		IOLoginData::getInstance()->updateOnlineStatus(player->getGUID(), false);
		if(player->isTrading())
			server.game().internalCloseTrade(player);

		player->clearPartyInvitations();
		if(player->getParty())
			player->getParty()->leave(player);

		player->sendTextMessage(MSG_INFO_DESCR, "You are now invisible.");
	}

	return true;
}
