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
#include "game.h"

#include "configmanager.h"
#ifdef __LOGIN_SERVER__
#include "gameservers.h"
#endif
#include "service.h"
#include "chat.h"

#include "account.h"
#include "dispatcher.h"
#include "luascript.h"
#include "creature.h"
#include "combat.h"
#include "tile.h"
#include "monster.h"
#include "npc.h"
#include "database.h"
#include "iologindata.h"
#include "ioban.h"
#include "ioguild.h"
#include "party.h"

#include "items.h"
#include "container.h"
#include "monsters.h"

#include "house.h"
#include "quests.h"

#include "actions.h"
#include "globalevent.h"
#include "movement.h"
#include "raids.h"
#include "scriptmanager.h"
#include "spells.h"
#include "talkaction.h"
#include "weapons.h"
#include "spawn.h"

#include "vocation.h"
#include "condition.h"
#include "creatureevent.h"
#include "group.h"
#include "player.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"


LOGGER_DEFINITION(Game);


Game::Game()
{
	gameState = GAME_STATE_NORMAL;
	worldType = WORLD_TYPE_PVP;
	services = nullptr;
	map = nullptr;
	playersRecord = lastStageLevel = 0;
	for(int32_t i = 0; i < 3; i++)
		globalSaveMessage[i] = false;

	//(1440 minutes/day) * 10 seconds event interval / (3600 seconds/day)
	lightHourDelta = 1440 * 10 / 3600;
	lightHour = SUNRISE + (SUNSET - SUNRISE) / 2;
	lightLevel = LIGHT_LEVEL_DAY;
	lightState = LIGHT_STATE_DAY;

	lastMotdId = lastHighscoreCheck = lastBucket = checkLightEvent = checkDecayEvent = saveEvent = 0;
}

Game::~Game()
{
	delete map;
}

void Game::clear() {
	Npc::clear();
	ScriptEnviroment::clearAll();
	Houses::getInstance()->clear();

	server.monsters().clear();

	if (map) {
		delete map;
		map = nullptr;
	}

	cleanup();

	for (size_t bucket = 0; bucket < EVENT_DECAYBUCKETS; ++bucket) {
		decayItems[bucket].clear();
	}

	blacklist.clear();
}

void Game::start(ServiceManager* servicer)
{
	checkDecayEvent = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(EVENT_DECAYINTERVAL),
		std::bind(&Game::checkDecay, this)));
	checkLightEvent = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(EVENT_LIGHTINTERVAL),
		std::bind(&Game::checkLight, this)));

	services = servicer;
	if(server.configManager().getBool(ConfigManager::GLOBALSAVE_ENABLED) && server.configManager().getNumber(ConfigManager::GLOBALSAVE_H) >= 1
		&& server.configManager().getNumber(ConfigManager::GLOBALSAVE_H) <= 24)
	{
		int32_t prepareGlobalSaveHour = server.configManager().getNumber(ConfigManager::GLOBALSAVE_H) - 1, hoursLeft = 0, minutesLeft = 0, minutesToRemove = 0;
		bool ignoreEvent = false;

		time_t timeNow = time(nullptr);
		const tm* theTime = localtime(&timeNow);
		if(theTime->tm_hour > prepareGlobalSaveHour)
		{
			hoursLeft = 24 - (theTime->tm_hour - prepareGlobalSaveHour);
			if(theTime->tm_min > 55 && theTime->tm_min <= 59)
				minutesToRemove = theTime->tm_min - 55;
			else
				minutesLeft = 55 - theTime->tm_min;
		}
		else if(theTime->tm_hour == prepareGlobalSaveHour)
		{
			if(theTime->tm_min >= 55 && theTime->tm_min <= 59)
			{
				if(theTime->tm_min >= 57)
					setGlobalSaveMessage(0, true);

				if(theTime->tm_min == 59)
					setGlobalSaveMessage(1, true);

				prepareGlobalSave();
				ignoreEvent = true;
			}
			else
				minutesLeft = 55 - theTime->tm_min;
		}
		else
		{
			hoursLeft = prepareGlobalSaveHour - theTime->tm_hour;
			if(theTime->tm_min > 55 && theTime->tm_min <= 59)
				minutesToRemove = theTime->tm_min - 55;
			else
				minutesLeft = 55 - theTime->tm_min;
		}

		uint32_t hoursLeftInMs = 60000 * 60 * hoursLeft, minutesLeftInMs = 60000 * (minutesLeft - minutesToRemove);
		if(!ignoreEvent && (hoursLeftInMs + minutesLeftInMs) > 0)
			saveEvent = server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(hoursLeftInMs + minutesLeftInMs),
				std::bind(&Game::prepareGlobalSave, this)));
	}
}

void Game::loadGameState()
{
	ScriptEnviroment::loadGameState();
	loadMotd();
	loadPlayersRecord();
	checkHighscores();
}

void Game::setGameState(GameState_t newState)
{
	if(gameState == GAME_STATE_SHUTDOWN)
		return; //this cannot be stopped

	if(gameState != newState)
	{
		gameState = newState;
		switch(newState)
		{
			case GAME_STATE_INIT:
			{
				Spawns::getInstance()->startup();
				LOGi("Loading raids...");
				Raids::getInstance()->loadFromXml();
				Raids::getInstance()->startup();
				LOGi("Loading quests...");
				Quests::getInstance()->loadFromXml();

				loadGameState();
				server.globalEvents().startup();

				IOBan::getInstance()->clearTemporials();
				if(server.configManager().getBool(ConfigManager::REMOVE_PREMIUM_ON_INIT))
					IOLoginData::getInstance()->updatePremiumDays();
				break;
			}

			case GAME_STATE_SHUTDOWN:
			{
				server.globalEvents().execute(GLOBAL_EVENT_SHUTDOWN);

				AutoList<Player> players = Player::autoList;
				for (auto it = players.begin(); it != players.end(); ++it) {
					it->second->kickPlayer(true, true);
				}

				saveGameState(false);
				server.dispatcher().addTask(Task::create(std::bind(&Game::shutdown, this)));

				server.scheduler().stop();
				server.dispatcher().stop();
				break;
			}

			case GAME_STATE_CLOSED:
			{
				AutoList<Player>::iterator it = Player::autoList.begin();
				while(it != Player::autoList.end()) //kick all players who not allowed to stay
				{
					if(!it->second->hasFlag(PlayerFlag_CanAlwaysLogin))
					{
						it->second->kickPlayer(true, true);
						it = Player::autoList.begin();
					}
					else
						++it;
				}

				saveGameState(false);
				break;
			}

			case GAME_STATE_NORMAL:
			case GAME_STATE_MAINTAIN:
			case GAME_STATE_STARTUP:
			case GAME_STATE_CLOSING:
			default:
				break;
		}
	}
}

void Game::saveGameState(bool shallow)
{
	LOGi("Saving server...");

	uint64_t start = OTSYS_TIME();
	if(gameState == GAME_STATE_NORMAL)
		setGameState(GAME_STATE_MAINTAIN);

	Houses::getInstance()->payHouses();

	IOLoginData* io = IOLoginData::getInstance();
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		it->second->loginPosition = it->second->getPosition();
		io->savePlayer(it->second.get(), false, shallow);
	}

	std::string storage = "relational";
	if(server.configManager().getBool(ConfigManager::HOUSE_STORAGE))
		storage = "binary";

	map->save();
	ScriptEnviroment::saveGameState();
	if(gameState == GAME_STATE_MAINTAIN)
		setGameState(GAME_STATE_NORMAL);

	LOGi("Server saved in " << (OTSYS_TIME() - start) / (1000.) << " seconds using " << storage << " house storage.");
}

int32_t Game::loadMap(std::string filename)
{
	if(!map)
		map = new Map;

	return map->load(getFilePath(FileType::OTHER, std::string("world/" + filename + ".otbm")));
}

void Game::cleanMap(uint32_t& count)
{
	uint64_t start = OTSYS_TIME();
	uint32_t tiles = 0; count = 0;

	int32_t marked = -1;
	if(gameState == GAME_STATE_NORMAL)
		setGameState(GAME_STATE_MAINTAIN);

	Tile* tile = nullptr;
	ItemVector::iterator tit;
	if(server.configManager().getBool(ConfigManager::STORE_TRASH))
	{
		marked = trash.size();
		Trash::iterator it = trash.begin();
		if(server.configManager().getBool(ConfigManager::CLEAN_PROTECTED_ZONES))
		{
			for(; it != trash.end(); ++it)
			{
				if(!(tile = getTile(*it)))
					continue;

				tile->resetFlag(TILESTATE_TRASHED);
				if(tile->hasFlag(TILESTATE_HOUSE) || !tile->getItemList())
					continue;

				++tiles;
				tit = tile->getItemList()->begin();
				while(tile->getItemList() && tit != tile->getItemList()->end())
				{
					if((*tit)->isMoveable() && !(*tit)->isLoadedFromMap()
						&& !(*tit)->isScriptProtected())
					{
						internalRemoveItem(nullptr, (*tit).get());
						if(tile->getItemList())
							tit = tile->getItemList()->begin();

						++count;
					}
					else
						++tit;
				}
			}

			trash.clear();
		}
		else
		{
			for(; it != trash.end(); ++it)
			{
				if(!(tile = getTile(*it)))
					continue;

				tile->resetFlag(TILESTATE_TRASHED);
				if(tile->hasFlag(TILESTATE_PROTECTIONZONE) || !tile->getItemList())
					continue;

				++tiles;
				tit = tile->getItemList()->begin();
				while(tile->getItemList() && tit != tile->getItemList()->end())
				{
					if((*tit)->isMoveable() && !(*tit)->isLoadedFromMap()
						&& !(*tit)->isScriptProtected())
					{
						internalRemoveItem(nullptr, (*tit).get());
						if(tile->getItemList())
							tit = tile->getItemList()->begin();

						++count;
					}
					else
						++tit;
				}
			}

			trash.clear();
		}
	}
	else if(server.configManager().getBool(ConfigManager::CLEAN_PROTECTED_ZONES))
	{
		for(uint16_t z = 0; z < (uint16_t)MAP_MAX_LAYERS; z++)
		{
			for(uint16_t y = 1; y <= map->getHeight(); y++)
			{
				for(uint16_t x = 1; x <= map->getWidth(); x++)
				{
					if(!(tile = getTile(x, y, z)) || tile->hasFlag(TILESTATE_HOUSE) || !tile->getItemList())
						continue;

					++tiles;
					tit = tile->getItemList()->begin();
					while(tile->getItemList() && tit != tile->getItemList()->end())
					{
						if((*tit)->isMoveable() && !(*tit)->isLoadedFromMap()
							&& !(*tit)->isScriptProtected())
						{
							internalRemoveItem(nullptr, (*tit).get());
							if(tile->getItemList())
								tit = tile->getItemList()->begin();

							++count;
						}
						else
							++tit;
					}
				}
			}
		}
	}
	else
	{
		for(uint16_t z = 0; z < (uint16_t)MAP_MAX_LAYERS; z++)
		{
			for(uint16_t y = 1; y <= map->getHeight(); y++)
			{
				for(uint16_t x = 1; x <= map->getWidth(); x++)
				{
					if(!(tile = getTile(x, y, z)) || tile->hasFlag(TILESTATE_PROTECTIONZONE) || !tile->getItemList())
						continue;

					++tiles;
					tit = tile->getItemList()->begin();
					while(tile->getItemList() && tit != tile->getItemList()->end())
					{
						if((*tit)->isMoveable() && !(*tit)->isLoadedFromMap()
							&& !(*tit)->isScriptProtected())
						{
							internalRemoveItem(nullptr, (*tit).get());
							if(tile->getItemList())
								tit = tile->getItemList()->begin();

							++count;
						}
						else
							++tit;
					}
				}
			}
		}
	}

	if(gameState == GAME_STATE_MAINTAIN)
		setGameState(GAME_STATE_NORMAL);

	LOGi("CLEAN: Removed " << count << " item" << (count != 1 ? "s" : "")
		<< " from " << tiles << " tile" << (tiles != 1 ? "s" : "") << " (" << marked << " were marked)"
		<< " in " << (OTSYS_TIME() - start) / (1000.) << " seconds.");
}

void Game::proceduralRefresh(RefreshTiles::iterator* it/* = nullptr*/)
{
	if(!it)
		it = new RefreshTiles::iterator(refreshTiles.begin());

	// Refresh 250 tiles each cycle
	refreshMap(it, 250);
	if((*it) != refreshTiles.end())
	{
		delete it;
		return;
	}

	// Refresh some items every 100 ms until all tiles has been checked
	// For 100k tiles, this would take 100000/2500 = 40s = half a minute
	server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(100),
		std::bind(&Game::proceduralRefresh, this, it)));
}

void Game::refreshMap(RefreshTiles::iterator* it/* = nullptr*/, uint32_t limit/* = 0*/)
{
	RefreshTiles::iterator end = refreshTiles.end();
	if(!it)
	{
		RefreshTiles::iterator begin = refreshTiles.begin();
		it = &begin;
	}

	Tile* tile = nullptr;
	TileItemVector* items = nullptr;

	Item* item = nullptr;
	for(uint32_t cleaned = 0, downItemsSize = 0; (*it) != end &&
		(limit ? (cleaned < limit) : true); ++(*it), ++cleaned)
	{
		if(!(tile = (*it)->first))
			continue;

		if((items = tile->getItemList()))
		{
			downItemsSize = tile->getDownItemCount();
			for(int32_t i = downItemsSize - 1; i >= 0; --i)
			{
				if((item = items->at(i)))
				{
					if(internalRemoveItem(nullptr, item) != RET_NOERROR)
					{
						LOGw("Could not refresh item: " << item->getId() << " at position " << tile->getPosition());
					}
				}
			}
		}

		cleanup();
		TileItemVector list = (*it)->second.list;
		for(ItemVector::reverse_iterator it = list.rbegin(); it != list.rend(); ++it)
		{
			boost::intrusive_ptr<Item> item = (*it)->clone();
			if(internalAddItem(nullptr, tile, item.get(), INDEX_WHEREEVER, FLAG_NOLIMIT) == RET_NOERROR)
			{
				if(item->getUniqueId() != 0)
					ScriptEnviroment::addUniqueThing(item.get());

				startDecay(item.get());
			}
			else
			{
				LOGw("Could not refresh item: " << item->getId() << " at position: " << tile->getPosition());
			}
		}
	}
}


Cylinder* Game::internalGetCylinder(Player* player, const ExtendedPosition& position) {
	typedef ExtendedPosition::Type  Type;

	switch (position.getType()) {
	case Type::POSITION:
	case Type::STACK_POSITION:
		return getTile(position.getPosition(true));

	case Type::BACKPACK:
		return player->getContainer(position.getOpenedContainerId());

	case Type::BACKPACK_SEARCH:
	case Type::SLOT:
		return player;

	case Type::NOWHERE:
		break;
	}

	return nullptr;
}


Thing* Game::internalGetThing(Player* player, const ExtendedPosition& position, stackposType_t type/* = STACKPOS_NORMAL*/) {
	typedef ExtendedPosition::Type  Type;

	switch (position.getType()) {
	case Type::POSITION:
	case Type::STACK_POSITION: {

		Tile* tile = getTile(position.getPosition(true));
		if(!tile)
			return nullptr;

		if(type == STACKPOS_LOOK)
			return tile->getTopVisibleThing(player);

		Thing* thing = nullptr;
		switch(type)
		{
			case STACKPOS_MOVE:
			{
				Item* item = tile->getTopDownItem();
				if(item && item->isMoveable())
					thing = item;
				else
					thing = tile->getTopVisibleCreature(player);

				break;
			}

			case STACKPOS_USE:
			{
				thing = tile->getTopDownItem();
				break;
			}

			case STACKPOS_USEITEM:
			{
				Item* downItem = tile->getTopDownItem();
				Item* item = tile->getItemByTopOrder(2);
				if(item && server.actions().hasAction(item))
				{
					if(!downItem || (!item->getKind()->hasHeight && !item->getKind()->allowPickupable))
						thing = item;
				}

				if(!thing)
					thing = downItem;

				if(!thing)
					thing = tile->getTopTopItem();

				if(!thing)
					thing = tile->ground.get();

				break;
			}

			default:
				thing = tile->__getThing(position.getType() == ExtendedPosition::Type::STACK_POSITION ? position.getStackPosition().index : 0);
				break;
		}

		if(player && thing && thing->getItem())
		{
			if(tile->hasProperty(ISVERTICAL))
			{
				if(player->getPosition().x + 1 == tile->getPosition().x)
					thing = nullptr;
			}
			else if(tile->hasProperty(ISHORIZONTAL) && player->getPosition().y + 1 == tile->getPosition().y)
				thing = nullptr;
		}

		return thing;
	}

	case Type::BACKPACK: {
		Container* parentcontainer = player->getContainer(position.getOpenedContainerId());
		if (parentcontainer == nullptr) {
			return nullptr;
		}

		return parentcontainer->getItem(position.getContainerItemIndex());
	}

	case Type::BACKPACK_SEARCH: {
		ItemKindPC kind = server.items().getKindByClientId(position.getClientItemKindId());
		if (!kind)
			return nullptr;

		return findItemOfType(player, kind->id, true, position.getFluidType());
	}

	case Type::SLOT: {
		return player->getInventoryItem(position.getSlot());
	}

	case Type::NOWHERE:
		break;
	} // switch

	return nullptr;
}

ExtendedPosition Game::internalGetPosition(Item* item, const ExtendedPosition& position)
{
	if(Cylinder* topParent = item->getTopParent())
	{
		if(Player* player = dynamic_cast<Player*>(topParent))
		{
			Container* container = dynamic_cast<Container*>(item->getParent());
			if(container)
			{
				return ExtendedPosition::forBackpack(player->getContainerID(container), container->__getIndexOfThing(item));
			}
			else
			{
				return ExtendedPosition::forSlot(slots_t(player->__getIndexOfThing(item)));
			}
		}
		else if(Tile* tile = topParent->getTile())
		{
			return ExtendedPosition::forStackPosition(StackPosition(tile->getPosition(), tile->__getIndexOfThing(item)));
		}
	}

	return ExtendedPosition::nowhere();
}

Creature* Game::getCreatureByID(uint32_t id)
{
	if(!id)
		return nullptr;

	AutoList<Creature>::iterator it = autoList.find(id);
	if(it != autoList.end() && !it->second->isRemoved())
		return it->second.get();

	return nullptr; //just in case the player doesnt exist
}

Player* Game::getPlayerByID(uint32_t id)
{
	if(!id)
		return nullptr;

	AutoList<Player>::iterator it = Player::autoList.find(id);
	if(it != Player::autoList.end() && !it->second->isRemoved())
		return it->second.get();

	return nullptr; //just in case the player doesnt exist
}

Creature* Game::getCreatureByName(std::string s)
{
	if(s.empty())
		return nullptr;

	toLowerCaseString(s);
	for(AutoList<Creature>::iterator it = autoList.begin(); it != autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && asLowerCaseString(it->second->getName()) == s)
			return it->second.get();
	}

	return nullptr; //just in case the creature doesnt exist
}


Map* Game::getMap() {
	return map;
}


const Map* Game::getMap() const {
	return map;
}


PlayerP Game::getPlayerByName(std::string s)
{
	if(s.empty())
		return nullptr;

	toLowerCaseString(s);
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && asLowerCaseString(it->second->getName()) == s)
			return it->second.get();
	}

	return nullptr;
}

PlayerP Game::getPlayerByNameEx(const std::string& s)
{
	PlayerP player = getPlayerByName(s);
	if(player)
		return player;

	std::string name = s;
	if(!IOLoginData::getInstance()->playerExists(name))
		return nullptr;

	player = new Player(name, nullptr);
	if(IOLoginData::getInstance()->loadPlayer(player.get(), name))
		return player;

	LOGe("[Game::getPlayerByNameEx] Cannot load player: " << name);

	return nullptr;
}

PlayerP Game::getPlayerByGuid(uint32_t guid)
{
	if(!guid)
		return nullptr;

	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && it->second->getGUID() == guid)
			return it->second.get();
	}

	return nullptr;
}

PlayerP Game::getPlayerByGuidEx(uint32_t guid)
{
	PlayerP player = getPlayerByGuid(guid);
	if(player)
		return player;

	std::string name;
	if(!IOLoginData::getInstance()->getNameByGuid(guid, name))
		return nullptr;

	player = new Player(name, nullptr);
	if(IOLoginData::getInstance()->loadPlayer(player.get(), name))
		return player;

	LOGe("[Game::getPlayerByGuidEx] Cannot load player: " << name);

	return nullptr;
}

ReturnValue Game::getPlayerByNameWildcard(std::string s, Player*& player)
{
	player = nullptr;
	if(s.empty())
		return RET_PLAYERWITHTHISNAMEISNOTONLINE;

	char tmp = *s.rbegin();
	if(tmp != '~' && tmp != '*')
	{
		player = getPlayerByName(s).get();
		if(!player)
			return RET_PLAYERWITHTHISNAMEISNOTONLINE;

		return RET_NOERROR;
	}

	Player* last = nullptr;
	s = s.substr(0, s.length() - 1);

	toLowerCaseString(s);
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(it->second->isRemoved())
			continue;

		std::string name = asLowerCaseString(it->second->getName());
		if(name.substr(0, s.length()) != s)
			continue;

		if(last)
			return RET_NAMEISTOOAMBIGUOUS;

		last = it->second.get();
	}

	if(!last)
		return RET_PLAYERWITHTHISNAMEISNOTONLINE;

	player = last;
	return RET_NOERROR;
}

Player* Game::getPlayerByAccount(uint32_t acc)
{
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && it->second->getAccount() == acc)
			return it->second.get();
	}

	return nullptr;
}

PlayerVector Game::getPlayersByName(std::string s)
{
	toLowerCaseString(s);
	PlayerVector players;
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && asLowerCaseString(it->second->getName()) == s)
			players.push_back(it->second.get());
	}

	return players;
}

PlayerVector Game::getPlayersByAccount(uint32_t acc)
{
	PlayerVector players;
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && it->second->getAccount() == acc)
			players.push_back(it->second.get());
	}

	return players;
}

PlayerVector Game::getPlayersByIP(uint32_t ip, uint32_t mask)
{
	PlayerVector players;
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
	{
		if(!it->second->isRemoved() && (it->second->getIP() & mask) == (ip & mask))
			players.push_back(it->second.get());
	}

	return players;
}

bool Game::internalPlaceCreature(Creature* creature, const Position& pos, bool extendedPos /*= false*/, bool forced /*= false*/)
{
	if(creature->getParent())
		return false;

	if(!map->placeCreature(pos, creature, extendedPos, forced))
		return false;

	creature->setID();

	autoList[creature->getID()] = creature;
	creature->addList();
	return true;
}

bool Game::placeCreature(const CreatureP& creature, const Position& pos, bool extendedPos /*= false*/, bool forced /*= false*/)
{
	Player* tmpPlayer = nullptr;
	if((tmpPlayer = creature->getPlayer()) && !tmpPlayer->storedConditionList.empty())
	{
		for(ConditionList::iterator it = tmpPlayer->storedConditionList.begin(); it != tmpPlayer->storedConditionList.end(); ++it)
		{
			if((*it)->getType() == CONDITION_MUTED && ((*it)->getTicks() - (
				(time(nullptr) - tmpPlayer->getLastLogout()) * 1000)) <= 0)
				continue;

			tmpPlayer->addCondition(*it);
		}

		tmpPlayer->storedConditionList.clear();
	}

	if(!internalPlaceCreature(creature.get(), pos, extendedPos, forced))
		return false;

	if (!creature->isAlive()) {
		return false;
	}

	SpectatorList::iterator it;
	SpectatorList list;

	getSpectators(list, creature->getPosition(), false, true);
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureAppear(creature.get(), BOOST_CURRENT_FUNCTION);
	}

	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureAppear(creature);

	if (!creature->isAlive()) {
		return false;
	}

	creature->setLastPosition(pos);
	creature->getParent()->postAddNotification(nullptr, creature.get(), nullptr, creature->getParent()->__getIndexOfThing(creature.get()));

	if (!creature->isAlive()) {
		return false;
	}

	creature->onPlacedCreature();
	return true;
}

ReturnValue Game::placeSummon(Creature* creature, const std::string& name)
{
	boost::intrusive_ptr<Monster> monster = Monster::create(name);
	if(!monster)
		return RET_NOTPOSSIBLE;

	// Place the monster
	if (!monster->convince(creature, true)) {
		return RET_NOTPOSSIBLE;
	}

	if(placeCreature(monster.get(), creature->getPosition(), true))
		return RET_NOERROR;

	monster->release();

	return RET_NOTENOUGHROOM;
}

bool Game::removeCreature(Creature* creature, bool isLogout /*= true*/)
{
	if(creature->isRemoved())
		return false;

	creature->willRemove();

	Tile* tile = creature->getTile();
	if (tile != nullptr) {
		SpectatorList list;

		SpectatorList::iterator it;
		getSpectators(list, tile->getPosition(), false, true);

		Player* player = nullptr;
		std::vector<uint32_t> oldStackPosVector;
		for(it = list.begin(); it != list.end(); ++it)
		{
			if((player = (*it)->getPlayer()) && player->canSeeCreature(creature))
				oldStackPosVector.push_back(tile->getClientIndexOfThing(player, creature));
		}

		int32_t oldIndex = tile->__getIndexOfThing(creature);

		map->onCreatureMoved(creature, creature->getTile(), nullptr);

		autorelease(creature);

		//send to client
		uint32_t i = 0;
		for(it = list.begin(); it != list.end(); ++it)
		{
			if(!(player = (*it)->getPlayer()) || !player->canSeeCreature(creature))
				continue;

			player->sendCreatureDisappear(creature, oldStackPosVector[i], BOOST_CURRENT_FUNCTION);
			++i;
		}

		//event method
		for(it = list.begin(); it != list.end(); ++it)
			(*it)->onCreatureDisappear(creature, isLogout);

		tile->__removeThing(creature, 0);
		tile->postRemoveNotification(nullptr, creature, nullptr, oldIndex, true);

		assert(creature->getParent() == nullptr);
	}

	creature->didRemove();

	autoList.erase(creature->getID());

	return true;
}

bool Game::playerMoveThing(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Thing* thing = internalGetThing(player, origin, STACKPOS_MOVE);
	Cylinder* toCylinder = internalGetCylinder(player, destination);
	if(!thing || !toCylinder)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(Creature* movingCreature = thing->getCreature())
	{
		uint32_t delay = server.configManager().getNumber(ConfigManager::PUSH_CREATURE_DELAY);
		if(Position::areInRange<1,1,0>(movingCreature->getPosition(), player->getPosition()) && delay > 0)
		{
			player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerMoveCreature, this,
					player->getID(), movingCreature->getID(), movingCreature->getPosition(), toCylinder->getPosition())));
		}
		else
			playerMoveCreature(playerId, movingCreature->getID(), movingCreature->getPosition(), toCylinder->getPosition());
	}
	else if(thing->getItem())
		playerMoveItem(playerId, origin, destination, count);

	return true;
}

bool Game::playerMoveCreature(uint32_t playerId, uint32_t movingCreatureId,
	const Position& movingCreaturePos, const Position& toPos)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || player->hasFlag(PlayerFlag_CannotMoveCreatures))
		return false;

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();

		player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerMoveCreature,
				this, playerId, movingCreatureId, movingCreaturePos, toPos)));
		return false;
	}

	Creature* movingCreature = getCreatureByID(movingCreatureId);
	if(!movingCreature || movingCreature->isRemoved() || movingCreature->getNoMove())
		return false;

	player->setNextActionTask(nullptr);
	if(!Position::areInRange<1,1,0>(movingCreaturePos, player->getPosition()) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
	{
		//need to walk to the creature first before moving it
		std::list<Direction> listDir;
		if(getPathToEx(player, movingCreaturePos, listDir, 0, 1, true, true))
		{
			server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(player->getStepDuration()), std::bind(&Game::playerMoveCreature, this,
					playerId, movingCreatureId, movingCreaturePos, toPos)));
			return true;
		}

		player->sendCancelMessage(RET_THEREISNOWAY);
		return false;
	}

	Tile* toTile = map->getTile(toPos);
	if(!toTile)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if((!movingCreature->isPushable() && !player->hasFlag(PlayerFlag_CanPushAllCreatures)) || !player->canSeeCreature(movingCreature))
	{
		player->sendCancelMessage(RET_NOTMOVEABLE);
		return false;
	}

	//check throw distance
	const Position& pos = movingCreature->getPosition();
	if(!player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere) && ((std::abs(pos.x - toPos.x) > movingCreature->getThrowRange()) || (std::abs(
		pos.y - toPos.y) > movingCreature->getThrowRange()) || (std::abs(
		pos.z - toPos.z) * 4 > movingCreature->getThrowRange())))
	{
		player->sendCancelMessage(RET_DESTINATIONOUTOFREACH);
		return false;
	}

	if(player != movingCreature)
	{
		if(toTile->hasProperty(BLOCKPATH))
		{
			player->sendCancelMessage(RET_NOTENOUGHROOM);
			return false;
		}

		if((movingCreature->getZone() == ZONE_PROTECTION || movingCreature->getZone() == ZONE_NOPVP)
			&& !toTile->hasFlag(TILESTATE_NOPVPZONE) && !toTile->hasFlag(TILESTATE_PROTECTIONZONE)
			&& !player->hasFlag(PlayerFlag_IgnoreProtectionZone))
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			return false;
		}

		if(toTile->getCreatures() && !toTile->getCreatures()->empty()
			&& !player->hasFlag(PlayerFlag_CanPushAllCreatures))
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			return false;
		}
	}

	bool deny = false;
	CreatureEventList pushEvents = player->getCreatureEvents(CREATURE_EVENT_PUSH);
	for(CreatureEventList::iterator it = pushEvents.begin(); it != pushEvents.end(); ++it)
	{
		if(!(*it)->executePush(player, movingCreature) && !deny)
			deny = true;
	}

	if(deny)
		return false;

	ReturnValue ret = internalMoveCreature(player, movingCreature, movingCreature->getTile(), toTile);
	if(ret != RET_NOERROR)
	{
		if(!player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar) || !player->hasCustomFlag(PlayerCustomFlag_CanMoveAnywhere))
		{
			player->sendCancelMessage(ret);
			return false;
		}

		if(!toTile->ground)
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			return true;
		}

		internalTeleport(movingCreature, toTile->getPosition(), true);
		return true;
	}

	if(Player* movingPlayer = movingCreature->getPlayer())
	{
		uint64_t delay = OTSYS_TIME() + movingPlayer->getStepDuration();
		if(delay > movingPlayer->getNextActionTime())
			movingPlayer->setNextAction(delay);
	}

	return true;
}

ReturnValue Game::internalMoveCreature(Creature* creature, Direction direction, uint32_t flags/* = 0*/)
{
	const Position& currentPos = creature->getPosition();
	Cylinder* fromTile = creature->getTile();
	Cylinder* toTile = nullptr;

	Position destPos = getNextPosition(direction, currentPos);
	if(direction < Direction::SOUTH_WEST && creature->getPlayer())
	{
		Tile* tmpTile = nullptr;
		if(currentPos.z != 8 && creature->getTile()->hasHeight(3)) //try go up
		{
			if((!(tmpTile = map->getTile(Position(currentPos.x, currentPos.y, currentPos.z - 1)))
				|| (!tmpTile->ground && !tmpTile->hasProperty(BLOCKSOLID))) &&
				(tmpTile = map->getTile(Position(destPos.x, destPos.y, destPos.z - 1)))
				&& tmpTile->ground && !tmpTile->hasProperty(BLOCKSOLID))
			{
				flags = flags | FLAG_IGNOREBLOCKITEM | FLAG_IGNOREBLOCKCREATURE;
				destPos.z--;
			}
		}
		else if(currentPos.z != 7 && (!(tmpTile = map->getTile(destPos)) || (!tmpTile->ground &&
			!tmpTile->hasProperty(BLOCKSOLID))) && (tmpTile = map->getTile(Position(
			destPos.x, destPos.y, destPos.z + 1))) && tmpTile->hasHeight(3)) //try go down
		{
			flags = flags | FLAG_IGNOREBLOCKITEM | FLAG_IGNOREBLOCKCREATURE;
			destPos.z++;
		}
	}

	ReturnValue ret = RET_NOTPOSSIBLE;
	if((toTile = map->getTile(destPos)))
		ret = internalMoveCreature(nullptr, creature, fromTile, toTile, flags);

	if(ret != RET_NOERROR)
	{
		if(Player* player = creature->getPlayer())
		{
			player->sendCancelMessage(ret);
			player->sendCancelWalk();
		}
	}

	return ret;
}

ReturnValue Game::internalMoveCreature(Creature* actor, Creature* creature, Cylinder* fromCylinder, Cylinder* toCylinder, uint32_t flags/* = 0*/)
{
	//check if we can move the creature to the destination
	ReturnValue ret = toCylinder->__queryAdd(0, creature, 1, flags);
	if(ret != RET_NOERROR)
		return ret;

	fromCylinder->getTile()->moveCreature(actor, creature, toCylinder);
	if(creature->getParent() != toCylinder)
		return RET_NOERROR;

	Item* toItem = nullptr;
	Cylinder* subCylinder = nullptr;

	int32_t n = 0, tmp = 0;
	while((subCylinder = toCylinder->__queryDestination(tmp, creature, &toItem, flags)) != toCylinder)
	{
		toCylinder->getTile()->moveCreature(actor, creature, subCylinder);
		if(creature->getParent() != subCylinder) //could happen if a script move the creature
			 break;

		toCylinder = subCylinder;
		flags = 0;
		if(++n >= MAP_MAX_LAYERS) //to prevent infinite loop
			break;
	}

	return RET_NOERROR;
}

bool Game::playerMoveItem(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || player->hasFlag(PlayerFlag_CannotMoveItems))
		return false;

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();

		player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerMoveItem, this,
				playerId, origin, destination, count)));
		return false;
	}

	player->setNextActionTask(nullptr);
	Cylinder* fromCylinder = internalGetCylinder(player, origin);

	Thing* thing = internalGetThing(player, origin, STACKPOS_MOVE);
	if(!thing || !thing->getItem())
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	Cylinder* toCylinder = internalGetCylinder(player, destination);

	if(!fromCylinder || !toCylinder || !item || (origin.getType() == ExtendedPosition::Type::BACKPACK_SEARCH && item->getClientID() != origin.getClientItemKindId()))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(!player->hasCustomFlag(PlayerCustomFlag_CanPushAllItems) && (!item->isPushable() || (item->isLoadedFromMap() &&
		(item->getUniqueId() != 0 || (item->getActionId() != 0 && item->getContainer())))))
	{
		player->sendCancelMessage(RET_NOTMOVEABLE);
		return false;
	}

	const Position& mapFromPos = fromCylinder->getTile()->getPosition();
	const Position& mapToPos = toCylinder->getTile()->getPosition();

	const Position& playerPos = player->getPosition();
	if(playerPos.z > mapFromPos.z && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		return false;
	}

	if(playerPos.z < mapFromPos.z && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		return false;
	}

	if(!Position::areInRange<1,1,0>(playerPos, mapFromPos) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
	{
		//need to walk to the item first before using it
		std::list<Direction> listDir;
		if(getPathToEx(player, item->getPosition(), listDir, 0, 1, true, true))
		{
			server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(player->getStepDuration()), std::bind(&Game::playerMoveItem, this,
					playerId, origin, destination, count)));
			return true;
		}

		player->sendCancelMessage(RET_THEREISNOWAY);
		return false;
	}

	//hangable item specific code
	if(item->isHangable() && toCylinder->getTile()->hasProperty(SUPPORTHANGABLE))
	{
		//destination supports hangable objects so need to move there first
		if(toCylinder->getTile()->hasProperty(ISVERTICAL))
		{
			if(player->getPosition().x + 1 == mapToPos.x)
			{
				player->sendCancelMessage(RET_NOTPOSSIBLE);
				return false;
			}
		}
		else if(toCylinder->getTile()->hasProperty(ISHORIZONTAL))
		{
			if(player->getPosition().y + 1 == mapToPos.y)
			{
				player->sendCancelMessage(RET_NOTPOSSIBLE);
				return false;
			}
		}

		if(!Position::areInRange<1,1,0>(playerPos, mapToPos) && !player->hasCustomFlag(PlayerCustomFlag_CanMoveFromFar))
		{
			Position walkPos = mapToPos;
			if(toCylinder->getTile()->hasProperty(ISVERTICAL))
				walkPos.x -= -1;

			if(toCylinder->getTile()->hasProperty(ISHORIZONTAL))
				walkPos.y -= -1;

			ExtendedPosition position = origin;
			if(position.hasPosition(true) && Position::areInRange<1,1,0>(mapFromPos, player->getPosition()) && !Position::areInRange<1,1,0>(mapFromPos, walkPos)) {
				//need to pickup the item first
				Item* moveItem = nullptr;
				ReturnValue ret = internalMoveItem(player, fromCylinder, player, INDEX_WHEREEVER, item, count, &moveItem);
				if(ret != RET_NOERROR)
				{
					player->sendCancelMessage(ret);
					return false;
				}

				//changing the position since its now in the inventory of the player
				position = internalGetPosition(moveItem, position);
			}

			std::list<Direction> listDir;
			if(map->getPathTo(player, walkPos, listDir))
			{
				server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(player->getStepDuration()), std::bind(&Game::playerMoveItem, this,
						playerId, position, destination, count)));
				return true;
			}

			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	if(!player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		if((std::abs(playerPos.x - mapToPos.x) > item->getThrowRange()) ||
			(std::abs(playerPos.y - mapToPos.y) > item->getThrowRange()) ||
			(std::abs(mapFromPos.z - mapToPos.z) * 4 > item->getThrowRange()))
		{
			player->sendCancelMessage(RET_DESTINATIONOUTOFREACH);
			return false;
		}
	}

	if(!canThrowObjectTo(mapFromPos, mapToPos) && !player->hasCustomFlag(PlayerCustomFlag_CanThrowAnywhere))
	{
		player->sendCancelMessage(RET_CANNOTTHROW);
		return false;
	}

	uint8_t destinationIndex = 0;
	if (destination.getType() == ExtendedPosition::Type::STACK_POSITION) {
		destinationIndex = destination.getStackPosition().index;
	}
	else if (destination.getType() == ExtendedPosition::Type::BACKPACK) {
		destinationIndex = destination.getContainerItemIndex();
	}
	else if (destination.getType() == ExtendedPosition::Type::SLOT) {
		destinationIndex = +destination.getSlot();
	}

	ReturnValue ret = internalMoveItem(player, fromCylinder, toCylinder, destinationIndex, item, count, nullptr);
	if(ret == RET_NOERROR)
		return true;

	player->sendCancelMessage(ret);
	return false;
}

ReturnValue Game::internalMoveItem(Creature* actor, Cylinder* fromCylinder, Cylinder* toCylinder,
	int32_t index, Item* item, uint32_t count, Item** _moveItem, uint32_t flags /*= 0*/)
{
	if(!toCylinder)
		return RET_NOTPOSSIBLE;

	if(!item->getParent())
	{
		assert(fromCylinder == item->getParent());
		return internalAddItem(actor, toCylinder, item, INDEX_WHEREEVER, FLAG_NOLIMIT);
	}

	Item* toItem = nullptr;
	Cylinder* subCylinder = nullptr;

	int32_t floor = 0;
	while((subCylinder = toCylinder->__queryDestination(index, item, &toItem, flags)) != toCylinder)
	{
		toCylinder = subCylinder;
		flags = 0;
		//to prevent infinite loop
		if(++floor >= MAP_MAX_LAYERS)
			break;
	}

	//destination is the same as the source?
	if(item == toItem)
		return RET_NOERROR; //silently ignore move

	//check if we can add this item
	ReturnValue ret = toCylinder->__queryAdd(index, item, count, flags);
	if(ret == RET_NEEDEXCHANGE)
	{
		//check if we can add it to source cylinder
		int32_t fromIndex = fromCylinder->__getIndexOfThing(item);

		ret = fromCylinder->__queryAdd(fromIndex, toItem, toItem->getItemCount(), 0);
		if(ret == RET_NOERROR)
		{
			//check how much we can move
			uint32_t maxExchangeQueryCount = 0;
			ReturnValue retExchangeMaxCount = fromCylinder->__queryMaxCount(-1, toItem, toItem->getItemCount(), maxExchangeQueryCount, 0);

			if(retExchangeMaxCount != RET_NOERROR && maxExchangeQueryCount == 0)
				return retExchangeMaxCount;

			if((toCylinder->__queryRemove(toItem, toItem->getItemCount(), flags) == RET_NOERROR) && ret == RET_NOERROR)
			{
				int32_t oldToItemIndex = toCylinder->__getIndexOfThing(toItem);

				autorelease(toItem);
				toCylinder->__removeThing(toItem, toItem->getItemCount());

				fromCylinder->__addThing(actor, toItem);
				if(oldToItemIndex != -1)
					toCylinder->postRemoveNotification(actor, toItem, fromCylinder, oldToItemIndex, true);

				int32_t newToItemIndex = fromCylinder->__getIndexOfThing(toItem);
				if(newToItemIndex != -1)
					fromCylinder->postAddNotification(actor, toItem, toCylinder, newToItemIndex);

				ret = toCylinder->__queryAdd(index, item, count, flags);
				toItem = nullptr;
			}
		}
	}

	if(ret != RET_NOERROR)
		return ret;

	//check how much we can move
	uint32_t maxQueryCount = 0;
	ReturnValue retMaxCount = toCylinder->__queryMaxCount(index, item, count, maxQueryCount, flags);
	if(retMaxCount != RET_NOERROR && !maxQueryCount)
		return retMaxCount;

	uint32_t m = maxQueryCount, n = 0;
	if(item->isStackable())
		m = std::min((uint32_t)count, m);

	boost::intrusive_ptr<Item> moveItem = item;
	//check if we can remove this item
	ret = fromCylinder->__queryRemove(item, m, flags);
	if(ret != RET_NOERROR)
		return ret;

	//remove the item
	int32_t itemIndex = fromCylinder->__getIndexOfThing(item);
	// requires netcode & container code update
//	if (fromCylinder == toCylinder && index > itemIndex) {
//		--index;
//	}

	autorelease(item);
	fromCylinder->__removeThing(item, m);

	bool isCompleteRemoval = item->isRemoved();
	Item* updateItem = nullptr;
	//update item(s)
	if(item->isStackable())
	{
		if(toItem && toItem->getId() == item->getId())
		{
			n = std::min((uint32_t)100 - toItem->getItemCount(), m);
			toCylinder->__updateThing(toItem, toItem->getId(), toItem->getItemCount() + n);
			updateItem = toItem;
		}

		if(m - n > 0)
			moveItem = Item::CreateItem(item->getId(), m - n);
		else
			moveItem = nullptr;
	}

	//add item
	if(moveItem /*m - n > 0*/)
		toCylinder->__addThing(actor, index, moveItem.get());

	if(itemIndex != -1)
		fromCylinder->postRemoveNotification(actor, item, toCylinder, itemIndex, isCompleteRemoval);

	if(moveItem)
	{
		int32_t moveItemIndex = toCylinder->__getIndexOfThing(moveItem.get());
		if(moveItemIndex != -1)
			toCylinder->postAddNotification(actor, moveItem.get(), fromCylinder, moveItemIndex);
	}

	if(updateItem)
	{
		int32_t updateItemIndex = toCylinder->__getIndexOfThing(updateItem);
		if(updateItemIndex != -1)
			toCylinder->postAddNotification(actor, updateItem, fromCylinder, updateItemIndex);
	}

	if(_moveItem)
	{
		if(moveItem)
			*_moveItem = moveItem.get();
		else
			*_moveItem = item;
	}

	//we could not move all, inform the player
	if(item->isStackable() && maxQueryCount < count)
		return retMaxCount;

	return ret;
}

ReturnValue Game::internalAddItem(Creature* actor, Cylinder* toCylinder, Item* item, int32_t index /*= INDEX_WHEREEVER*/,
	uint32_t flags /*= 0*/, bool test /*= false*/)
{
	if(!toCylinder || !item)
		return RET_NOTPOSSIBLE;

	Item* toItem = nullptr;
	toCylinder = toCylinder->__queryDestination(index, item, &toItem, flags);

	ReturnValue ret = toCylinder->__queryAdd(index, item, item->getItemCount(), flags);
	if(ret != RET_NOERROR)
		return ret;

	uint32_t maxQueryCount = 0;
	ret = toCylinder->__queryMaxCount(index, item, item->getItemCount(), maxQueryCount, flags);
	if(ret != RET_NOERROR)
		return ret;

	if(!test)
	{
		uint32_t m = maxQueryCount;
		if(item->isStackable())
			m = std::min((uint32_t)item->getItemCount(), maxQueryCount);

		boost::intrusive_ptr<Item> moveItem = item;
		if(item->isStackable() && toItem && toItem->getId() == item->getId())
		{
			uint32_t n = std::min((uint32_t)100 - toItem->getItemCount(), m);
			toCylinder->__updateThing(toItem, toItem->getId(), toItem->getItemCount() + n);
			if(m - n > 0)
			{
				if(m - n != item->getItemCount())
					moveItem = Item::CreateItem(item->getId(), m - n);
			}
			else
			{
				moveItem = nullptr;
				if(item->getParent() != &VirtualCylinder::virtualCylinder)
				{
					item->onRemoved();
				}
			}
		}

		if(moveItem)
		{
			toCylinder->__addThing(actor, index, moveItem.get());
			int32_t moveItemIndex = toCylinder->__getIndexOfThing(moveItem.get());
			if(moveItemIndex != -1)
				toCylinder->postAddNotification(actor, moveItem.get(), nullptr, moveItemIndex);
		}
		else
		{
			int32_t itemIndex = toCylinder->__getIndexOfThing(item);
			if(itemIndex != -1)
				toCylinder->postAddNotification(actor, item, nullptr, itemIndex);
		}
	}

	return RET_NOERROR;
}

ReturnValue Game::internalRemoveItem(Creature* actor, Item* item, int32_t count /*= -1*/, bool test /*= false*/, uint32_t flags /*= 0*/)
{
	Cylinder* cylinder = item->getParent();
	if(!cylinder)
		return RET_NOTPOSSIBLE;

	if(count == -1)
		count = item->getItemCount();

	//check if we can remove this item
	ReturnValue ret = cylinder->__queryRemove(item, count, flags | FLAG_IGNORENOTMOVEABLE);
	if(ret != RET_NOERROR)
		return ret;

	if(!item->canRemove())
		return RET_NOTPOSSIBLE;

	if(!test)
	{
		//remove the item
		int32_t index = cylinder->__getIndexOfThing(item);

		autorelease(item);
		cylinder->__removeThing(item, count);

		bool isCompleteRemoval = false;
		if(item->isRemoved())
		{
			isCompleteRemoval = true;
		}

		cylinder->postRemoveNotification(actor, item, nullptr, index, isCompleteRemoval);
	}

	item->onRemoved();
	return RET_NOERROR;
}

ReturnValue Game::internalPlayerAddItem(Creature* actor, Player* player, Item* item, bool dropOnMap /*= true*/)
{
	ReturnValue ret = internalAddItem(actor, player, item);
	if(ret != RET_NOERROR && dropOnMap)
		ret = internalAddItem(actor, player->getTile(), item, INDEX_WHEREEVER, FLAG_NOLIMIT);

	return ret;
}

Item* Game::findItemOfType(Cylinder* cylinder, uint16_t itemId,
	bool depthSearch /*= true*/, int32_t subType /*= -1*/)
{
	if(!cylinder)
		return nullptr;

	std::list<Container*> listContainer;
	Container* tmpContainer = nullptr;

	Thing* thing = nullptr;
	Item* item = nullptr;
	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex();)
	{
		if((thing = cylinder->__getThing(i)) && (item = thing->getItem()))
		{
			if(item->getId() == itemId && (subType == -1 || subType == item->getSubType()))
				return item;
			else
			{
				++i;
				if(depthSearch && (tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
		else
			++i;
	}

	while(listContainer.size() > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(int32_t i = 0; i < (int32_t)container->size();)
		{
			Item* item = container->getItem(i);
			if(item->getId() == itemId && (subType == -1 || subType == item->getSubType()))
				return item;
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
	}

	return nullptr;
}

bool Game::removeItemOfType(Cylinder* cylinder, uint16_t itemId, int32_t count, int32_t subType /*= -1*/, bool includeSlots /* = true */)
{
	if(!cylinder || ((int32_t)cylinder->__getItemTypeCount(itemId, subType, true, includeSlots) < count))
		return false;

	if(count <= 0)
		return true;

	std::list<Container*> listContainer;
	Container* tmpContainer = nullptr;

	Player* player = dynamic_cast<Player*>(cylinder);

	Thing* thing = nullptr;
	Item* item = nullptr;
	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex() && count > 0;)
	{
		if((thing = cylinder->__getThing(i)) && (item = thing->getItem()))
		{
			if(item->getId() == itemId && (includeSlots || player == nullptr))
			{
				if(item->isStackable())
				{
					if(item->getItemCount() > count)
					{
						internalRemoveItem(nullptr, item, count);
						count = 0;
					}
					else
					{
						count -= item->getItemCount();
						internalRemoveItem(nullptr, item);
					}
				}
				else if(subType == -1 || subType == item->getSubType())
				{
					--count;
					internalRemoveItem(nullptr, item);
				}
				else
					++i;
			}
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
		else
			++i;
	}

	while(listContainer.size() > 0 && count > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(int32_t i = 0; i < (int32_t)container->size() && count > 0;)
		{
			Item* item = container->getItem(i);
			if(item->getId() == itemId)
			{
				if(item->isStackable())
				{
					if(item->getItemCount() > count)
					{
						internalRemoveItem(nullptr, item, count);
						count = 0;
					}
					else
					{
						count-= item->getItemCount();
						internalRemoveItem(nullptr, item);
					}
				}
				else if(subType == -1 || subType == item->getSubType())
				{
					--count;
					internalRemoveItem(nullptr, item);
				}
			}
			else
			{
				++i;
				if((tmpContainer = item->getContainer()))
					listContainer.push_back(tmpContainer);
			}
		}
	}

	return (count == 0);
}

uint64_t Game::getMoney(const Cylinder* cylinder)
{
	if(!cylinder)
		return 0;

	std::list<Container*> listContainer;
	Container* tmpContainer = nullptr;

	Thing* thing = nullptr;
	Item* item = nullptr;

	uint64_t moneyCount = 0;
	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex(); ++i)
	{
		if(!(thing = cylinder->__getThing(i)))
			continue;

		if(!(item = thing->getItem()))
			continue;

		if((tmpContainer = item->getContainer()))
			listContainer.push_back(tmpContainer);
		else if(item->getWorth() > 0)
			moneyCount += item->getWorth();
	}

	while(listContainer.size() > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(ItemList::const_iterator it = container->getItems(); it != container->getEnd(); ++it)
		{
			item = (*it).get();
			if((tmpContainer = item->getContainer()))
				listContainer.push_back(tmpContainer);
			else if(item->getWorth() > 0)
				moneyCount += item->getWorth();
		}
	}

	return moneyCount;
}

bool Game::removeMoney(Cylinder* cylinder, uint64_t money, uint32_t flags /*= 0*/)
{
	if(!cylinder)
		return false;

	if(money <= 0)
		return true;

	typedef std::multimap<uint64_t, Item*> MoneyMultiMap;
	MoneyMultiMap moneyMap;

	std::list<Container*> listContainer;
	Container* tmpContainer = nullptr;

	Thing* thing = nullptr;
	Item* item = nullptr;

	uint64_t moneyCount = 0;
	for(int32_t i = cylinder->__getFirstIndex(); i < cylinder->__getLastIndex(); ++i)
	{
		if(!(thing = cylinder->__getThing(i)))
			continue;

		if(!(item = thing->getItem()))
			continue;

		if((tmpContainer = item->getContainer()))
			listContainer.push_back(tmpContainer);
		else if(item->getWorth() > 0)
		{
			moneyCount += item->getWorth();
			moneyMap.insert(std::make_pair(item->getWorth(), item));
		}
	}

	while(listContainer.size() > 0)
	{
		Container* container = listContainer.front();
		listContainer.pop_front();
		for(int32_t i = 0; i < (int32_t)container->size(); i++)
		{
			Item* item = container->getItem(i);
			if((tmpContainer = item->getContainer()))
				listContainer.push_back(tmpContainer);
			else if(item->getWorth() > 0)
			{
				moneyCount += item->getWorth();
				moneyMap.insert(std::make_pair(item->getWorth(), item));
			}
		}
	}

	// Not enough money
	if(moneyCount < static_cast<unsigned>(money))
		return false;

	for(MoneyMultiMap::iterator mit = moneyMap.begin(); mit != moneyMap.end() && money > 0; ++mit)
	{
		Item* item = mit->second;
		if(!item)
			continue;

		internalRemoveItem(nullptr, item);
		if(mit->first > money)
		{
			// Remove a monetary value from an item
			uint64_t remaind = mit->first - money;
			addMoney(cylinder, remaind, flags);
			money = 0;
		}
		else
			money -= mit->first;
	}

	return money == 0;
}

void Game::addMoney(Cylinder* cylinder, uint64_t money, uint32_t flags /*= 0*/)
{
	Items::WorthMap moneyMap = server.items().getWorth();
	uint64_t tmp = 0;
	for(Items::WorthMap::reverse_iterator it = moneyMap.rbegin(); it != moneyMap.rend(); ++it)
	{
		tmp = money / it->first;
		money -= tmp * it->first;

		while (tmp > 0)
		{
			boost::intrusive_ptr<Item> remaindItem = Item::CreateItem(it->second, std::min(UINT64_C(100), tmp));
			if(internalAddItem(nullptr, cylinder, remaindItem.get(), INDEX_WHEREEVER, flags) != RET_NOERROR)
				internalAddItem(nullptr, cylinder->getTile(), remaindItem.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);

			tmp -= std::min(UINT64_C(100), tmp);
		}
	}
}

ItemP Game::transformItem(const ItemP& item, uint16_t newId, int32_t newCount /*= -1*/)
{
	if(item->getId() == newId && (newCount == -1 || (newCount == item->getSubType() && newCount != 0)))
		return item;

	Cylinder* cylinder = item->getParent();
	if(!cylinder)
		return nullptr;

	int32_t itemIndex = cylinder->__getIndexOfThing(item.get());
	if(itemIndex == -1)
	{
		LOGt("Error: transformItem, itemIndex == -1");
		return item;
	}

	if(!item->canTransform())
		return item;

	ItemKindPC newKind = server.items()[newId];
	if (!newKind) {
		return item;
	}

	ItemKindPC oldKind = item->getKind();
	if(oldKind->alwaysOnTop != newKind->alwaysOnTop)
	{
		//This only occurs when you transform items on tiles from a downItem to a topItem (or vice versa)
		//Remove the old, and add the new
		ReturnValue ret = internalRemoveItem(nullptr, item.get());
		if(ret != RET_NOERROR)
			return item;

		ItemP newItem;
		if(newCount == -1)
			newItem = Item::CreateItem(newId);
		else
			newItem = Item::CreateItem(newId, newCount);

		if(!newItem)
			return nullptr;

		newItem->copyAttributes(item.get());
		if(internalAddItem(nullptr, cylinder, newItem.get(), INDEX_WHEREEVER, FLAG_NOLIMIT) == RET_NOERROR)
			return newItem;

		return nullptr;
	}

	if(oldKind->type == newKind->type)
	{
		//Both items has the same type so we can safely change id/subtype
		if(!newCount && (item->isStackable() || item->hasCharges()))
		{
			if(!item->isStackable() && (!item->getDefaultDuration() || item->getDuration() <= 0))
			{
				int32_t tmpId = newId;
				if(oldKind->id == newKind->id)
					tmpId = oldKind->decayTo;

				if(tmpId != -1)
				{
					return transformItem(item, tmpId);
				}
			}

			internalRemoveItem(nullptr, item.get());
			return nullptr;
		}

		uint16_t itemId = item->getId();
		int32_t count = item->getSubType();

		cylinder->postRemoveNotification(nullptr, item.get(), cylinder, itemIndex, false);
		if(oldKind->id != newKind->id)
		{
			itemId = newId;
			if(oldKind->group != newKind->group)
				item->setDefaultSubtype();
		}

		if(newCount != -1 && newKind->hasSubType())
			count = newCount;

		cylinder->__updateThing(item.get(), itemId, count);
		cylinder->postAddNotification(nullptr, item.get(), cylinder, itemIndex);
		return item;
	}

	//Replacing the the old item with the new while maintaining the old position
	ItemP newItem = nullptr;
	if(newCount == -1)
		newItem = Item::CreateItem(newId);
	else
		newItem = Item::CreateItem(newId, newCount);

	if(!newItem)
	{
		LOGe("[Game::transformItem] Item of type " << item->getId() << " transforming into invalid type " << newId);
		return nullptr;
	}

	cylinder->__replaceThing(itemIndex, newItem.get());
	cylinder->postAddNotification(nullptr, newItem.get(), cylinder, itemIndex);

	item->setParent(nullptr);
	cylinder->postRemoveNotification(nullptr, item.get(), cylinder, itemIndex, true);

	return newItem;
}

ReturnValue Game::internalTeleport(Thing* thing, const Position& newPos, bool pushMove, uint32_t flags /*= 0*/)
{
	if(newPos == thing->getPosition())
		return RET_NOERROR;

	if(thing->isRemoved())
		return RET_NOTPOSSIBLE;

	if(Tile* toTile = map->getTile(newPos))
	{
		if(Creature* creature = thing->getCreature())
		{
			if(Position::areInRange<1,1,0>(creature->getPosition(), newPos) && pushMove)
				creature->getTile()->moveCreature(nullptr, creature, toTile, false);
			else
				creature->getTile()->moveCreature(nullptr, creature, toTile, true);

			return RET_NOERROR;
		}

		if(Item* item = thing->getItem())
			return internalMoveItem(nullptr, item->getParent(), toTile, INDEX_WHEREEVER, item, item->getItemCount(), nullptr, flags);
	}

	return RET_NOTPOSSIBLE;
}

//Implementation of player invoked events
bool Game::playerMove(uint32_t playerId, Direction dir)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->getNoMove())
	{
		player->sendCancelWalk();
		return false;
	}

	player->stopWalk();
	int32_t delay = player->getWalkDelay(dir);
	if(delay > 0)
	{
		player->setNextAction(OTSYS_TIME() + player->getStepDuration(dir) - SCHEDULER_MINTICKS);
		player->setNextWalkTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerMove, this, playerId, dir)));

		return false;
	}

	player->setFollowCreature(nullptr);
	player->onWalk(dir);

	player->setIdleTime(0);
	return internalMoveCreature(player, dir) == RET_NOERROR;
}

bool Game::playerBroadcastMessage(Player* player, SpeakClasses type, const std::string& text)
{
	if(!player->hasFlag(PlayerFlag_CanBroadcast) || type < SPEAK_CLASS_FIRST || type > SPEAK_CLASS_LAST)
		return false;

	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
		it->second->sendCreatureSay(player, type, text);

	//TODO: event handling - onCreatureSay
	LOGi(player->getName() << " broadcasted: \"" << text << "\".");
	return true;
}

bool Game::playerCreatePrivateChannel(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->isPremium())
		return false;

	ChatChannel* channel = server.chat().createChannel(player, 0xFFFF);
	if(!channel || !channel->addUser(player))
		return false;

	player->sendCreatePrivateChannel(channel->getId(), channel->getName());
	return true;
}

bool Game::playerChannelInvite(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	PrivateChatChannel* channel = server.chat().getPrivateChannel(player);
	if(!channel)
		return false;

	PlayerP invitePlayer = getPlayerByName(name);
	if(!invitePlayer)
		return false;

	channel->invitePlayer(player, invitePlayer.get());
	return true;
}

bool Game::playerChannelExclude(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	PrivateChatChannel* channel = server.chat().getPrivateChannel(player);
	if(!channel)
		return false;

	PlayerP excludePlayer = getPlayerByName(name);
	if(!excludePlayer)
		return false;

	channel->excludePlayer(player, excludePlayer.get());
	return true;
}

bool Game::playerRequestChannels(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->sendChannelsDialog();
	return true;
}

bool Game::playerOpenChannel(uint32_t playerId, uint16_t channelId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	ChatChannel* channel = server.chat().addUserToChannel(player, channelId);
	if(!channel)
	{
		return false;
	}

	if(channel->getId() != CHANNEL_RVR)
		player->sendChannel(channel->getId(), channel->getName());
	else
		player->sendRuleViolationsChannel(channel->getId());

	return true;
}

bool Game::playerCloseChannel(uint32_t playerId, uint16_t channelId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	server.chat().removeUserFromChannel(player, channelId);
	return true;
}

bool Game::playerOpenPrivateChannel(uint32_t playerId, std::string& receiver)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(IOLoginData::getInstance()->playerExists(receiver))
		player->sendOpenPrivateChannel(receiver);
	else
		player->sendCancel("A player with this name does not exist.");

	return true;
}

bool Game::playerProcessRuleViolation(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->hasFlag(PlayerFlag_CanAnswerRuleViolations))
		return false;

	PlayerP reporter = getPlayerByName(name);
	if(!reporter)
		return false;

	RuleViolationsMap::iterator it = ruleViolations.find(reporter->getID());
	if(it == ruleViolations.end())
		return false;

	RuleViolation& rvr = *it->second;
	if(!rvr.isOpen)
		return false;

	rvr.isOpen = false;
	rvr.gamemaster = player;
	if(ChatChannel* channel = server.chat().getChannelById(CHANNEL_RVR))
	{
		UsersMap tmpMap = channel->getUsers();
		for(UsersMap::iterator tit = tmpMap.begin(); tit != tmpMap.end(); ++tit)
			tit->second->sendRemoveReport(reporter->getName());
	}

	return true;
}

bool Game::playerCloseRuleViolation(uint32_t playerId, const std::string& name)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	PlayerP reporter = getPlayerByName(name);
	if(!reporter)
		return false;

	return closeRuleViolation(reporter.get());
}

bool Game::playerCancelRuleViolation(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	return cancelRuleViolation(player);
}

bool Game::playerCloseNpcChannel(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	SpectatorList list;
	SpectatorList::iterator it;

	getSpectators(list, player->getPosition());
	Npc* npc = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((npc = (*it)->getNpc()))
			npc->onPlayerCloseChannel(player);
	}

	return true;
}

bool Game::playerReceivePing(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->receivePing();
	return true;
}

bool Game::playerAutoWalk(uint32_t playerId, std::list<Direction>& listDir)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->setIdleTime(0);
	if(player->hasCondition(CONDITION_GAMEMASTER, GAMEMASTER_TELEPORT))
	{
		Position pos = player->getPosition();
		for(std::list<Direction>::iterator it = listDir.begin(); it != listDir.end(); ++it)
			pos = getNextPosition((*it), pos);

		pos = getClosestFreeTile(player, pos, true, false);
		if(!pos.x || !pos.y)
		{
			player->sendCancelWalk();
			return false;
		}

		internalCreatureTurn(player, getDirectionTo(player->getPosition(), pos, false));
		internalTeleport(player, pos, false);
		return true;
	}

	player->setNextWalkTask(nullptr);
	return player->startAutoWalk(listDir);
}

bool Game::playerStopAutoWalk(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->stopWalk();
	return true;
}

bool Game::playerUseItemEx(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	bool isHotkey = (origin.getType() == ExtendedPosition::Type::BACKPACK_SEARCH);
	if(isHotkey && !server.configManager().getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED))
		return false;

	Thing* thing = internalGetThing(player, origin, STACKPOS_USEITEM);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || !item->isUseable())
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ReturnValue ret = RET_NOERROR;

	if (origin.hasPosition(true)) {
		ret = server.actions().canUse(player, origin.getPosition(true));
	}
	if (ret == RET_NOERROR) {
		ret = server.actions().canUseEx(player, destination, item);
	}

	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY && origin.hasPosition(true) && destination.hasPosition(true))
		{
			ExtendedPosition position = origin;
			if(Position::areInRange<1,1,0>(origin.getPosition(true), player->getPosition())
					&& !Position::areInRange<1,1,0>(origin.getPosition(true), destination.getPosition(true)))
			{
				Item* moveItem = nullptr;
				ReturnValue retTmp = internalMoveItem(player, item->getParent(), player,
					INDEX_WHEREEVER, item, item->getItemCount(), &moveItem);
				if(retTmp != RET_NOERROR)
				{
					player->sendCancelMessage(retTmp);
					return false;
				}

				//changing the position since its now in the inventory of the player
				position = internalGetPosition(moveItem, origin);
			}

			std::list<Direction> listDir;
			if(getPathToEx(player, destination.getPosition(true), listDir, 0, 1, true, true, 10))
			{
				server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(400), std::bind(&Game::playerUseItemEx, this,
						playerId, position, destination)));
				return true;
			}

			ret = RET_THEREISNOWAY;
		}

		player->sendCancelMessage(ret);
		return false;
	}

	if(isHotkey)
		showHotkeyUseMessage(player, item);

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();

		player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerUseItemEx, this,
				playerId, origin, destination)));
		return false;
	}

	player->setIdleTime(0);
	player->setNextActionTask(nullptr);

	return server.actions().useItemEx(player, origin, destination, item);
}

bool Game::playerUseItem(uint32_t playerId, const ExtendedPosition& origin, uint8_t openContainerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	bool isHotkey = (origin.getType() == ExtendedPosition::Type::BACKPACK_SEARCH);
	if(isHotkey && !server.configManager().getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED))
		return false;

	Thing* thing = internalGetThing(player, origin, STACKPOS_USEITEM);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || item->isUseable())
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ReturnValue ret = RET_NOERROR;
	if (origin.hasPosition(true)) {
		ret = server.actions().canUse(player, origin.getPosition(true));
	}

	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY)
		{
			std::list<Direction> listDir;
			if(getPathToEx(player, origin.getPosition(true), listDir, 0, 1, true, true))
			{
				server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(400), std::bind(&Game::playerUseItem, this,
						playerId, origin, openContainerId)));
				return true;
			}

			ret = RET_THEREISNOWAY;
		}

		player->sendCancelMessage(ret);
		return false;
	}

	if(isHotkey)
		showHotkeyUseMessage(player, item);

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();

		player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerUseItem, this,
				playerId, origin, openContainerId)));
		return false;
	}

	player->setIdleTime(0);
	player->setNextActionTask(nullptr);
	return server.actions().useItem(player, item, origin, openContainerId);
}

bool Game::playerUseBattleWindow(uint32_t playerId, const ExtendedPosition& origin, uint32_t creatureId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Creature* creature = getCreatureByID(creatureId);
	if(!creature)
		return false;

	if(!Position::areInRange<7,5,0>(creature->getPosition(), player->getPosition()))
		return false;

	bool isHotkey = (origin.getType() == ExtendedPosition::Type::BACKPACK_SEARCH);
	if(!server.configManager().getBool(ConfigManager::AIMBOT_HOTKEY_ENABLED) && (creature->getPlayer() || isHotkey))
	{
		player->sendCancelMessage(RET_DIRECTPLAYERSHOOT);
		return false;
	}

	Thing* thing = internalGetThing(player, origin, STACKPOS_USE);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Item* item = thing->getItem();
	if(!item || (origin.getType() == ExtendedPosition::Type::BACKPACK_SEARCH && item->getClientID() != origin.getClientItemKindId()))
	{
		player->sendCancelMessage(RET_CANNOTUSETHISOBJECT);
		return false;
	}

	ReturnValue ret = RET_NOERROR;
	if (origin.hasPosition(true)) {
		ret = server.actions().canUse(player, origin.getPosition(true));
	}

	if(ret != RET_NOERROR)
	{
		if(ret == RET_TOOFARAWAY)
		{
			std::list<Direction> listDir;
			if(getPathToEx(player, item->getPosition(), listDir, 0, 1, true, true))
			{
				server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(400), std::bind(&Game::playerUseBattleWindow, this,
						playerId, origin, creatureId)));
				return true;
			}

			ret = RET_THEREISNOWAY;
		}

		player->sendCancelMessage(ret);
		return false;
	}

	if(isHotkey)
		showHotkeyUseMessage(player, item);

	if(!player->canDoAction())
	{
		uint32_t delay = player->getNextActionTime();

		player->setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(delay), std::bind(&Game::playerUseBattleWindow, this,
				playerId, origin, creatureId)));
		return false;
	}

	player->setIdleTime(0);
	player->setNextActionTask(nullptr);
	return server.actions().useItemEx(player, origin, ExtendedPosition::forStackPosition(StackPosition(creature->getPosition(),
			creature->getParent()->__getIndexOfThing(creature))), item, creatureId);
}

bool Game::playerCloseContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->closeContainer(cid);
	player->sendCloseContainer(cid);
	return true;
}

bool Game::playerMoveUpContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Container* container = player->getContainer(cid);
	if(!container)
		return false;

	Container* parentContainer = dynamic_cast<Container*>(container->getParent());
	if(!parentContainer)
		return false;

	player->addContainer(cid, parentContainer);
	player->sendContainer(cid, parentContainer, (dynamic_cast<const Container*>(parentContainer->getParent()) != nullptr));
	return true;
}

bool Game::playerUpdateTile(uint32_t playerId, const Position& pos)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->canSee(pos))
		return false;

	if(Tile* tile = getTile(pos))
		player->sendUpdateTile(tile, pos);

	return true;
}

bool Game::playerUpdateContainer(uint32_t playerId, uint8_t cid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Container* container = player->getContainer(cid);
	if(!container)
		return false;

	player->sendContainer(cid, container, (dynamic_cast<const Container*>(container->getParent()) != nullptr));
	return true;
}

bool Game::playerRotateItem(uint32_t playerId, const ExtendedPosition& position)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Thing* thing = internalGetThing(player, position);
	if(!thing)
		return false;

	Item* item = thing->getItem();
	if(!item || (position.getType() == ExtendedPosition::Type::BACKPACK_SEARCH && item->getClientID() != position.getClientItemKindId()) || !item->isRoteable() || (item->isLoadedFromMap() &&
		(item->getUniqueId() != 0 || (item->getActionId() != 0 && item->getContainer()))))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(position.hasPosition(true) && !Position::areInRange<1,1,0>(position.getPosition(true), player->getPosition()))
	{
		std::list<Direction> listDir;
		if(getPathToEx(player, position.getPosition(true), listDir, 0, 1, true, true))
		{
			server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
				this, player->getID(), listDir)));

			player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(400), std::bind(&Game::playerRotateItem, this,
					playerId, position)));
			return true;
		}

		player->sendCancelMessage(RET_THEREISNOWAY);
		return false;
	}

	uint16_t newId = item->getKind()->rotateTo;
	if(newId != 0)
		transformItem(item, newId);

	player->setIdleTime(0);
	return true;
}

bool Game::playerWriteItem(uint32_t playerId, uint32_t windowTextId, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint16_t maxTextLength = 0;
	uint32_t internalWindowTextId = 0;

	Item* writeItem = player->getWriteItem(internalWindowTextId, maxTextLength);
	if(text.length() > maxTextLength || windowTextId != internalWindowTextId)
		return false;

	if(!writeItem || writeItem->isRemoved())
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Cylinder* topParent = writeItem->getTopParent();
	Player* owner = dynamic_cast<Player*>(topParent);
	if(owner && owner != player)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if(!Position::areInRange<1,1,0>(writeItem->getPosition(), player->getPosition()))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	bool deny = false;
	CreatureEventList textEditEvents = player->getCreatureEvents(CREATURE_EVENT_TEXTEDIT);
	for(CreatureEventList::iterator it = textEditEvents.begin(); it != textEditEvents.end(); ++it)
	{
		if(!(*it)->executeTextEdit(player, writeItem, text))
			deny = true;
	}

	if(deny)
		return false;

	if(!text.empty())
	{
		if(writeItem->getText() != text)
		{
			writeItem->setText(text);
			writeItem->setWriter(player->getName());
			writeItem->setDate(std::time(nullptr));
		}
	}
	else
	{
		writeItem->resetText();
		writeItem->resetWriter();
		writeItem->resetDate();
	}

	uint16_t newId = writeItem->getKind()->writeOnceItemId;
	if(newId != 0)
		transformItem(writeItem, newId);

	player->setWriteItem(nullptr);
	return true;
}

bool Game::playerUpdateHouseWindow(uint32_t playerId, uint8_t listId, uint32_t windowTextId, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t internalWindowTextId = 0;
	uint32_t internalListId = 0;

	House* house = player->getEditHouse(internalWindowTextId, internalListId);
	if(house && internalWindowTextId == windowTextId && listId == 0)
	{
		house->setAccessList(internalListId, text);
		player->setEditHouse(nullptr);
	}

	return true;
}

bool Game::playerRequestTrade(uint32_t playerId, const ExtendedPosition& position, uint32_t tradePlayerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* tradePartner = getPlayerByID(tradePlayerId);
	if(!tradePartner || tradePartner == player)
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Sorry, not possible.");
		return false;
	}

	if(!Position::areInRange<2,2,0>(tradePartner->getPosition(), player->getPosition()))
	{
		std::stringstream ss;
		ss << tradePartner->getName() << " tells you to move closer.";
		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
		return false;
	}

	Item* tradeItem = dynamic_cast<Item*>(internalGetThing(player, position, STACKPOS_USE));
	if(!tradeItem || (position.getType() == ExtendedPosition::Type::BACKPACK_SEARCH && tradeItem->getClientID() != position.getClientItemKindId()) || !tradeItem->isPickupable() || (tradeItem->isLoadedFromMap() &&
		(tradeItem->getUniqueId() != 0 || (tradeItem->getActionId() != 0 && tradeItem->getContainer()))))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	if (position.hasPosition(true)) {
		if(player->getPosition().z > tradeItem->getPosition().z)
		{
			player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
			return false;
		}

		if(player->getPosition().z < tradeItem->getPosition().z)
		{
			player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
			return false;
		}

		if(!Position::areInRange<1,1,0>(tradeItem->getPosition(), player->getPosition()))
		{
			std::list<Direction> listDir;
			if(getPathToEx(player, position.getPosition(true), listDir, 0, 1, true, true))
			{
				server.dispatcher().addTask(Task::create(std::bind(&Game::playerAutoWalk,
					this, player->getID(), listDir)));

				player->setNextWalkActionTask(SchedulerTask::create(std::chrono::milliseconds(400), std::bind(&Game::playerRequestTrade, this,
						playerId, position, tradePlayerId)));
				return true;
			}

			player->sendCancelMessage(RET_THEREISNOWAY);
			return false;
		}
	}

	const Container* container = nullptr;
	for(TradeItemMap::const_iterator it = tradeItems.begin(); it != tradeItems.end(); it++)
	{
		if(tradeItem == it->first ||
			((container = dynamic_cast<const Container*>(tradeItem)) && container->isHoldingItem(it->first.get())) ||
			((container = dynamic_cast<const Container*>(it->first.get())) && container->isHoldingItem(tradeItem)))
		{
			player->sendTextMessage(MSG_INFO_DESCR, "This item is already being traded.");
			return false;
		}
	}

	Container* tradeContainer = tradeItem->getContainer();
	if(tradeContainer && tradeContainer->getItemHoldingCount() + 1 > 100)
	{
		player->sendTextMessage(MSG_INFO_DESCR, "You cannot trade more than 100 items.");
		return false;
	}

	bool deny = false;
	CreatureEventList tradeEvents = player->getCreatureEvents(CREATURE_EVENT_TRADE_REQUEST);
	for(CreatureEventList::iterator it = tradeEvents.begin(); it != tradeEvents.end(); ++it)
	{
		if(!(*it)->executeTradeRequest(player, tradePartner, tradeItem))
			deny = true;
	}

	if(deny)
		return false;

	return internalStartTrade(player, tradePartner, tradeItem);
}

bool Game::internalStartTrade(Player* player, Player* tradePartner, Item* tradeItem)
{
	if(player->tradeState != TRADE_NONE && !(player->tradeState == TRADE_ACKNOWLEDGE && player->tradePartner == tradePartner))
	{
		player->sendCancelMessage(RET_YOUAREALREADYTRADING);
		return false;
	}
	else if(tradePartner->tradeState != TRADE_NONE && tradePartner->tradePartner != player)
	{
		player->sendCancelMessage(RET_THISPLAYERISALREADYTRADING);
		return false;
	}

	player->tradePartner = tradePartner;
	player->tradeItem = tradeItem;
	player->tradeState = TRADE_INITIATED;

	tradeItems[tradeItem] = player->getID();

	player->sendTradeItemRequest(player, tradeItem, true);
	if(tradePartner->tradeState == TRADE_NONE)
	{
		char buffer[100];
		sprintf(buffer, "%s wants to trade with you", player->getName().c_str());
		tradePartner->sendTextMessage(MSG_INFO_DESCR, buffer);
		tradePartner->tradeState = TRADE_ACKNOWLEDGE;
		tradePartner->tradePartner = player;
	}
	else
	{
		Item* counterOfferItem = tradePartner->tradeItem;
		player->sendTradeItemRequest(tradePartner, counterOfferItem, false);
		tradePartner->sendTradeItemRequest(player, tradeItem, false);
	}

	return true;
}

bool Game::playerAcceptTrade(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!(player->getTradeState() == TRADE_ACKNOWLEDGE || player->getTradeState() == TRADE_INITIATED))
		return false;

	player->setTradeState(TRADE_ACCEPT);
	Player* tradePartner = player->tradePartner;
	if(!tradePartner || tradePartner->getTradeState() != TRADE_ACCEPT)
		return false;

	Item* tradeItem1 = player->tradeItem;
	Item* tradeItem2 = tradePartner->tradeItem;

	bool deny = false;
	CreatureEventList tradeEvents = player->getCreatureEvents(CREATURE_EVENT_TRADE_ACCEPT);
	for(CreatureEventList::iterator it = tradeEvents.begin(); it != tradeEvents.end(); ++it)
	{
		if(!(*it)->executeTradeAccept(player, tradePartner, tradeItem1, tradeItem2))
			deny = true;
	}

	if(deny)
		return false;

	player->setTradeState(TRADE_TRANSFER);
	tradePartner->setTradeState(TRADE_TRANSFER);

	TradeItemMap::iterator it = tradeItems.find(tradeItem1);
	if(it != tradeItems.end())
	{
		autorelease(it->first);
		tradeItems.erase(it);
	}

	it = tradeItems.find(tradeItem2);
	if(it != tradeItems.end())
	{
		autorelease(it->first);
		tradeItems.erase(it);
	}

	ReturnValue ret1 = internalAddItem(player, tradePartner, tradeItem1, INDEX_WHEREEVER, 0, true);
	ReturnValue ret2 = internalAddItem(tradePartner, player, tradeItem2, INDEX_WHEREEVER, 0, true);

	bool isSuccess = false;
	if(ret1 == RET_NOERROR && ret2 == RET_NOERROR)
	{
		ret1 = internalRemoveItem(tradePartner, tradeItem1, tradeItem1->getItemCount(), true);
		ret2 = internalRemoveItem(player, tradeItem2, tradeItem2->getItemCount(), true);
		if(ret1 == RET_NOERROR && ret2 == RET_NOERROR)
		{
			Cylinder* cylinder1 = tradeItem1->getParent();
			Cylinder* cylinder2 = tradeItem2->getParent();

			internalMoveItem(player, cylinder1, tradePartner, INDEX_WHEREEVER, tradeItem1, tradeItem1->getItemCount(), nullptr);
			internalMoveItem(tradePartner, cylinder2, player, INDEX_WHEREEVER, tradeItem2, tradeItem2->getItemCount(), nullptr);

			tradeItem1->onTradeEvent(ON_TRADE_TRANSFER, tradePartner, player);
			tradeItem2->onTradeEvent(ON_TRADE_TRANSFER, player, tradePartner);

			isSuccess = true;
		}
	}

	if(!isSuccess)
	{
		std::string errorDescription = getTradeErrorDescription(ret1, tradeItem1);
		tradePartner->sendTextMessage(MSG_INFO_DESCR, errorDescription);
		tradeItem2->onTradeEvent(ON_TRADE_CANCEL, tradePartner, nullptr);

		errorDescription = getTradeErrorDescription(ret2, tradeItem2);
		player->sendTextMessage(MSG_INFO_DESCR, errorDescription);
		tradeItem1->onTradeEvent(ON_TRADE_CANCEL, player, nullptr);
	}

	player->setTradeState(TRADE_NONE);
	player->tradeItem = nullptr;
	player->tradePartner = nullptr;
	player->sendTradeClose();

	tradePartner->setTradeState(TRADE_NONE);
	tradePartner->tradeItem = nullptr;
	tradePartner->tradePartner = nullptr;
	tradePartner->sendTradeClose();
	return isSuccess;
}

std::string Game::getTradeErrorDescription(ReturnValue ret, Item* item)
{
	std::stringstream ss;
	if(ret == RET_NOTENOUGHCAPACITY)
	{
		ss << "You do not have enough capacity to carry";
		if(item->isStackable() && item->getItemCount() > 1)
			ss << " these objects.";
		else
			ss << " this object." ;

		ss << std::endl << " " << item->getWeightDescription();
	}
	else if(ret == RET_NOTENOUGHROOM || ret == RET_CONTAINERNOTENOUGHROOM)
	{
		ss << "You do not have enough room to carry";
		if(item->isStackable() && item->getItemCount() > 1)
			ss << " these objects.";
		else
			ss << " this object.";
	}
	else
		ss << "Trade could not be completed.";

	return ss.str().c_str();
}

bool Game::playerLookInTrade(uint32_t playerId, bool lookAtCounterOffer, int32_t index)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* tradePartner = player->tradePartner;
	if(!tradePartner)
		return false;

	Item* tradeItem = nullptr;
	if(lookAtCounterOffer)
		tradeItem = tradePartner->getTradeItem();
	else
		tradeItem = player->getTradeItem();

	if(!tradeItem)
		return false;

	std::stringstream ss;
	ss << "You see ";

	int32_t lookDistance = std::max(std::abs(player->getPosition().x - tradeItem->getPosition().x),
		std::abs(player->getPosition().y - tradeItem->getPosition().y));
	if(!index)
	{
		ss << tradeItem->getDescription(lookDistance);
		if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
		{
			ss << std::endl << "ItemID: [" << tradeItem->getId() << "]";
			if(tradeItem->getActionId() > 0)
				ss << ", ActionID: [" << tradeItem->getActionId() << "]";

			if(tradeItem->getUniqueId() > 0)
				ss << ", UniqueID: [" << tradeItem->getUniqueId() << "]";

			ss << ".";
			ItemKindPC kind = tradeItem->getKind();
			if(kind->transformEquipTo)
				ss << std::endl << "TransformTo (onEquip): [" << kind->transformEquipTo << "]";
			else if(kind->transformDeEquipTo)
				ss << std::endl << "TransformTo (onDeEquip): [" << kind->transformDeEquipTo << "]";
			else if(kind->decayTo != -1)
				ss << std::endl << "DecayTo: [" << kind->decayTo << "]";
		}

		player->sendTextMessage(MSG_INFO_DESCR, ss.str());
		return false;
	}

	Container* tradeContainer = tradeItem->getContainer();
	if(!tradeContainer || index > (int32_t)tradeContainer->getItemHoldingCount())
		return false;

	std::list<const Container*> listContainer;
	listContainer.push_back(tradeContainer);

	ItemList::const_iterator it;
	Container* tmpContainer = nullptr;
	while(listContainer.size() > 0)
	{
		const Container* container = listContainer.front();
		listContainer.pop_front();
		for(it = container->getItems(); it != container->getEnd(); ++it)
		{
			if((tmpContainer = (*it)->getContainer()))
				listContainer.push_back(tmpContainer);

			--index;
			if(index != 0)
				continue;

			ss << (*it)->getDescription(lookDistance);
			if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
			{
				ss << std::endl << "ItemID: [" << (*it)->getId() << "]";
				if((*it)->getActionId() > 0)
					ss << ", ActionID: [" << (*it)->getActionId() << "]";

				if((*it)->getUniqueId() > 0)
					ss << ", UniqueID: [" << (*it)->getUniqueId() << "]";

				ss << ".";
				ItemKindPC kind = (*it)->getKind();
				if(kind->transformEquipTo)
					ss << std::endl << "TransformTo: [" << kind->transformEquipTo << "] (onEquip).";
				else if(kind->transformDeEquipTo)
					ss << std::endl << "TransformTo: [" << kind->transformDeEquipTo << "] (onDeEquip).";
				else if(kind->decayTo != -1)
					ss << std::endl << "DecayTo: [" << kind->decayTo << "].";
			}

			player->sendTextMessage(MSG_INFO_DESCR, ss.str());
			return true;
		}
	}

	return false;
}

bool Game::playerCloseTrade(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	return internalCloseTrade(player);
}

bool Game::internalCloseTrade(Player* player)
{
	Player* tradePartner = player->tradePartner;
	if((tradePartner && tradePartner->getTradeState() == TRADE_TRANSFER) || player->getTradeState() == TRADE_TRANSFER)
	{
		LOGw("[Game::internalCloseTrade] TradeState == TRADE_TRANSFER, " <<
			player->getName() << " " << player->getTradeState() << ", " <<
			tradePartner->getName() << " " << tradePartner->getTradeState());
		return true;
	}

	std::vector<Item*>::iterator it;
	if(player->getTradeItem())
	{
		TradeItemMap::iterator it = tradeItems.find(player->getTradeItem());
		if(it != tradeItems.end())
		{
			autorelease(it->first);
			tradeItems.erase(it);
		}

		player->tradeItem->onTradeEvent(ON_TRADE_CANCEL, player, nullptr);
		player->tradeItem = nullptr;
	}

	player->setTradeState(TRADE_NONE);
	player->tradePartner = nullptr;

	player->sendTextMessage(MSG_STATUS_SMALL, "Trade cancelled.");
	player->sendTradeClose();
	if(tradePartner)
	{
		if(tradePartner->getTradeItem())
		{
			TradeItemMap::iterator it = tradeItems.find(tradePartner->getTradeItem());
			if(it != tradeItems.end())
			{
				autorelease(it->first);
				tradeItems.erase(it);
			}

			tradePartner->tradeItem->onTradeEvent(ON_TRADE_CANCEL, tradePartner, nullptr);
			tradePartner->tradeItem = nullptr;
		}

		tradePartner->setTradeState(TRADE_NONE);
		tradePartner->tradePartner = nullptr;

		tradePartner->sendTextMessage(MSG_STATUS_SMALL, "Trade cancelled.");
		tradePartner->sendTradeClose();
	}

	return true;
}

bool Game::playerPurchaseItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount,
	bool ignoreCap/* = false*/, bool inBackpacks/* = false*/)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	int32_t onBuy, onSell;
	Npc* merchant = player->getShopOwner(onBuy, onSell);
	if(!merchant)
		return false;

	ItemKindPC kind = server.items().getKindByClientId(spriteId);
	if(!kind)
		return false;

	uint8_t subType = count;
	if(kind->isFluidContainer() && count < uint8_t(sizeof(reverseFluidMap) / sizeof(int8_t)))
		subType = reverseFluidMap[count];

	if(!player->canShopItem(kind->id, subType, SHOPEVENT_BUY))
		return false;

	merchant->onPlayerTrade(player, SHOPEVENT_BUY, onBuy, kind->id, subType, amount, ignoreCap, inBackpacks);
	return true;
}

bool Game::playerSellItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	int32_t onBuy, onSell;
	Npc* merchant = player->getShopOwner(onBuy, onSell);
	if(!merchant)
		return false;

	ItemKindPC kind = server.items().getKindByClientId(spriteId);
	if(!kind)
		return false;

	uint8_t subType = count;
	if(kind->isFluidContainer() && count < uint8_t(sizeof(reverseFluidMap) / sizeof(int8_t)))
		subType = reverseFluidMap[count];

	if(!player->canShopItem(kind->id, subType, SHOPEVENT_SELL))
		return false;

	merchant->onPlayerTrade(player, SHOPEVENT_SELL, onSell, kind->id, subType, amount);
	return true;
}

bool Game::playerCloseShop(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(player == nullptr || player->isRemoved())
		return false;

	player->closeShopWindow();
	return true;
}

bool Game::playerLookInShop(uint32_t playerId, uint16_t spriteId, uint8_t count)
{
	Player* player = getPlayerByID(playerId);
	if(player == nullptr || player->isRemoved())
		return false;

	ItemKindPC kind = server.items().getKindByClientId(spriteId);
	if(!kind)
		return false;

	uint8_t subType = count;
	if(kind->isFluidContainer() && count < uint8_t(sizeof(reverseFluidMap) / sizeof(int8_t)))
		subType = reverseFluidMap[count];

	std::stringstream ss;
	ss << "You see " << Item::getDescription(kind, 1, nullptr, subType);
	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
		ss << std::endl << "ItemID: [" << kind->id << "].";

	player->sendTextMessage(MSG_INFO_DESCR, ss.str());
	return true;
}

bool Game::playerLookAt(uint32_t playerId, const ExtendedPosition& position)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Thing* thing = internalGetThing(player, position, STACKPOS_LOOK);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Position thingPos;
	if (position.hasPosition(true)) {
		thingPos = position.getPosition(true);
	}
	else {
		thingPos = thing->getPosition();
	}

	uint8_t stackpos;
	if (position.getType() == ExtendedPosition::Type::STACK_POSITION) {
		stackpos = position.getStackPosition().index;
	}
	else {
		stackpos = thing->getParent()->__getIndexOfThing(thing);
	}

	if(!player->canSee(thingPos))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		return false;
	}

	Position playerPos = player->getPosition();
	int32_t lookDistance = -1;
	if(thing != player)
	{
		lookDistance = std::max(std::abs(playerPos.x - thingPos.x), std::abs(playerPos.y - thingPos.y));
		if(playerPos.z != thingPos.z)
			lookDistance = lookDistance + 9 + 6;
	}

	bool deny = false;
	CreatureEventList lookEvents = player->getCreatureEvents(CREATURE_EVENT_LOOK);
	for(CreatureEventList::iterator it = lookEvents.begin(); it != lookEvents.end(); ++it)
	{
		if(!(*it)->executeLook(player, thing, thingPos, stackpos, lookDistance))
			deny = true;
	}

	if(deny)
		return false;

	std::stringstream ss;
	ss << "You see " << thing->getDescription(lookDistance);
	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeItemDetails))
	{
		if(Item* item = thing->getItem())
		{
			ss << std::endl << "ItemID: [" << item->getId() << "]";
			if(item->getActionId() > 0)
				ss << ", ActionID: [" << item->getActionId() << "]";

			if(item->getUniqueId() > 0)
				ss << ", UniqueID: [" << item->getUniqueId() << "]";

			ss << ".";

			ItemKindPC kind = item->getKind();
			if(kind->transformEquipTo)
				ss << std::endl << "TransformTo: [" << kind->transformEquipTo << "] (onEquip).";
			else if(kind->transformDeEquipTo)
				ss << std::endl << "TransformTo: [" << kind->transformDeEquipTo << "] (onDeEquip).";
			else if(kind->decayTo != -1)
				ss << std::endl << "DecayTo: [" << kind->decayTo << "].";
		}
	}

	if(player->hasCustomFlag(PlayerCustomFlag_CanSeeCreatureDetails))
	{
		if(const Creature* creature = thing->getCreature())
		{
			ss << std::endl << "Health: " << creature->getHealth() << " / " << creature->getMaxHealth();
			if (creature->getMaxMana() > 0)
				ss << std::endl << "Mana: " << creature->getMana() << " / " << creature->getMaxMana();

			ss << std::endl << "ID: " << creature->getID();
		}
	}

	if(player->hasCustomFlag(PlayerCustomFlag_CanSeePosition))
		ss << std::endl << "Position: [X: " << thingPos.x << "] [Y: " << thingPos.y << "] [Z: " << thingPos.z << "].";

	player->sendTextMessage(MSG_INFO_DESCR, ss.str());

	if (Item* item = thing->getItem()) {
		if (item->isReadable() && item->getKind()->allowDistRead && !item->getText().empty()) {
			player->setWriteItem(nullptr);
			player->sendTextWindow(item, 0, false);
		}
	}

	return true;
}

bool Game::playerQuests(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->sendQuests();
	return true;
}

bool Game::playerQuestInfo(uint32_t playerId, uint16_t questId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Quest* quest = Quests::getInstance()->getQuestById(questId);
	if(!quest)
		return false;

	player->sendQuestInfo(quest);
	return true;
}

bool Game::playerCancelAttackAndFollow(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	playerSetAttackedCreature(playerId, 0);
	playerFollowCreature(playerId, 0);

	player->stopWalk();
	return true;
}

bool Game::playerSetAttackedCreature(uint32_t playerId, uint32_t creatureId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(player->getAttackedCreature() && !creatureId)
	{
		player->setAttackedCreature(nullptr);
		player->sendCancelTarget();
		return true;
	}

	Creature* attackCreature = getCreatureByID(creatureId);
	if(!attackCreature)
	{
		player->setAttackedCreature(nullptr);
		player->sendCancelTarget();
		return false;
	}

	ReturnValue ret = Combat::canTargetCreature(player, attackCreature);
	if(ret != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		player->sendCancelTarget();
		player->setAttackedCreature(nullptr);
		return false;
	}

	player->setAttackedCreature(attackCreature);
	return true;
}

bool Game::playerFollowCreature(uint32_t playerId, uint32_t creatureId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Creature* followCreature = nullptr;
	if(creatureId)
		followCreature = getCreatureByID(creatureId);

	player->setAttackedCreature(nullptr);
	return player->setFollowCreature(followCreature);
}

bool Game::playerSetFightModes(uint32_t playerId, fightMode_t fightMode, chaseMode_t chaseMode, secureMode_t secureMode)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->setFightMode(fightMode);
	player->setChaseMode(chaseMode);

	player->setSecureMode(secureMode);
	return true;
}

bool Game::playerRequestAddVip(uint32_t playerId, const std::string& vipName)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t guid;
	bool specialVip;

	std::string name = vipName;
	if(!IOLoginData::getInstance()->getGuidByNameEx(guid, specialVip, name))
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with that name does not exist.");
		return false;
	}

	if(specialVip && !player->hasFlag(PlayerFlag_SpecialVIP))
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "You cannot add this player.");
		return false;
	}

	bool online = false;
	if(PlayerP target = getPlayerByName(name))
		online = player->canSeeCreature(target.get());

	return player->addVIP(guid, name, online);
}

bool Game::playerRequestRemoveVip(uint32_t playerId, uint32_t guid)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->removeVIP(guid);
	return true;
}

bool Game::playerTurn(uint32_t playerId, Direction dir)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(internalCreatureTurn(player, dir))
	{
		player->setIdleTime(0);
		return true;
	}

	if(player->getDirection() != dir || !player->hasCustomFlag(PlayerCustomFlag_CanTurnhop))
		return false;

	Position pos = getNextPosition(dir, player->getPosition());
	Tile* tile = map->getTile(pos);
	if(!tile || !tile->ground)
		return false;

	player->setIdleTime(0);
	ReturnValue ret = tile->__queryAdd(0, player, 1, FLAG_IGNOREBLOCKITEM);
	if(ret != RET_NOTENOUGHROOM && (ret != RET_NOTPOSSIBLE || player->hasCustomFlag(PlayerCustomFlag_CanMoveAnywhere))
		&& (ret != RET_PLAYERISNOTINVITED || player->hasFlag(PlayerFlag_CanEditHouses)))
		return internalTeleport(player, pos, true);

	player->sendCancelMessage(ret);
	return false;
}

bool Game::playerRequestOutfit(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	player->sendOutfitWindow();
	return true;
}

bool Game::playerChangeOutfit(uint32_t playerId, Outfit_t outfit)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->changeOutfit(outfit, true))
		return false;

	player->setIdleTime(0);
	if(!player->hasCondition(CONDITION_OUTFIT, -1))
		internalCreatureChangeOutfit(player, outfit);

	return true;
}

bool Game::playerSay(uint32_t playerId, uint16_t channelId, SpeakClasses type, const std::string& receiver, const std::string& text)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	uint32_t muteTime = 0;
	bool muted = player->isMuted(channelId, type, muteTime);
	if(muted)
	{
		char buffer[75];
		sprintf(buffer, "You are still muted for %d seconds.", muteTime);
		player->sendTextMessage(MSG_STATUS_SMALL, buffer);
		return false;
	}

	if(player->isAccountManager())
	{
		player->removeMessageBuffer();
		return internalCreatureSay(player, SPEAK_SAY, text, false);
	}

	if(server.talkActions().onPlayerSay(player, type == SPEAK_SAY ? static_cast<uint16_t>(CHANNEL_DEFAULT) : channelId, text, false))
		return true;

	if(!muted)
	{
		ReturnValue ret = RET_NOERROR;
		if(!muteTime)
		{
			ret = server.spells().onPlayerSay(player, text);
			if(ret == RET_NOERROR || (ret == RET_NEEDEXCHANGE && !server.configManager().getBool(ConfigManager::BUFFER_SPELL_FAILURE)))
				return true;
		}

		player->removeMessageBuffer();
		if(ret == RET_NEEDEXCHANGE)
			return true;
	}

	switch(type)
	{
		case SPEAK_SAY:
			return internalCreatureSay(player, SPEAK_SAY, text, false);
		case SPEAK_WHISPER:
			return playerWhisper(player, text);
		case SPEAK_YELL:
			return playerYell(player, text);
		case SPEAK_PRIVATE:
		case SPEAK_PRIVATE_RED:
		case SPEAK_RVR_ANSWER:
			return playerSpeakTo(player, type, receiver, text);
		case SPEAK_CHANNEL_O:
		case SPEAK_CHANNEL_Y:
		case SPEAK_CHANNEL_RN:
		case SPEAK_CHANNEL_RA:
		case SPEAK_CHANNEL_W:
		{
			if(playerTalkToChannel(player, type, text, channelId))
				return true;

			return playerSay(playerId, 0, SPEAK_SAY, receiver, text);
		}
		case SPEAK_PRIVATE_PN:
			return playerSpeakToNpc(player, text);
		case SPEAK_BROADCAST:
			return playerBroadcastMessage(player, SPEAK_BROADCAST, text);
		case SPEAK_RVR_CHANNEL:
			return playerReportRuleViolation(player, text);
		case SPEAK_RVR_CONTINUE:
			return playerContinueReport(player, text);

		default:
			break;
	}

	return false;
}

bool Game::playerWhisper(Player* player, const std::string& text)
{
	SpectatorList list;
	SpectatorList::const_iterator it;
	getSpectators(list, player->getPosition(), false, false,
		Map::maxClientViewportX, Map::maxClientViewportX,
		Map::maxClientViewportY, Map::maxClientViewportY);

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureSay(player, SPEAK_WHISPER, text);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureSay(player, SPEAK_WHISPER, text);

	return true;
}

bool Game::playerYell(Player* player, const std::string& text)
{
	if(player->getLevel() <= 1)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "You may not yell as long as you are on level 1.");
		return true;
	}

	if(player->hasCondition(CONDITION_MUTED, 1))
	{
		player->sendCancelMessage(RET_YOUAREEXHAUSTED);
		return true;
	}

	if(!player->hasFlag(PlayerFlag_CannotBeMuted))
	{
		if(Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, 30000, 0, false, 1))
			player->addCondition(condition);
	}

	internalCreatureSay(player, SPEAK_YELL, asUpperCaseString(text), false);
	return true;
}

bool Game::playerSpeakTo(Player* player, SpeakClasses type, const std::string& receiver,
	const std::string& text)
{
	PlayerP toPlayer = getPlayerByName(receiver);
	if(!toPlayer || toPlayer->isRemoved())
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with this name is not online.");
		return false;
	}

	bool canSee = player->canSeeCreature(toPlayer.get());
	if(toPlayer->hasCondition(CONDITION_GAMEMASTER, GAMEMASTER_IGNORE)
		&& !player->hasFlag(PlayerFlag_CannotBeMuted))
	{
		char buffer[70];
		if(!canSee)
			sprintf(buffer, "A player with this name is not online.");
		else
			sprintf(buffer, "Sorry, %s is currently ignoring private messages.", toPlayer->getName().c_str());

		player->sendTextMessage(MSG_STATUS_SMALL, buffer);
		return false;
	}

	if(type == SPEAK_PRIVATE_RED && !player->hasFlag(PlayerFlag_CanTalkRedPrivate))
		type = SPEAK_PRIVATE;

	toPlayer->sendCreatureSay(player, type, text);
	toPlayer->onCreatureSay(player, type, text);
	if(!canSee)
	{
		player->sendTextMessage(MSG_STATUS_SMALL, "A player with this name is not online.");
		return false;
	}

	char buffer[80];
	sprintf(buffer, "Message sent to %s.", toPlayer->getName().c_str());
	player->sendTextMessage(MSG_STATUS_SMALL, buffer);
	return true;
}

bool Game::playerTalkToChannel(Player* player, SpeakClasses type, const std::string& text, uint16_t channelId)
{
	switch(type)
	{
		case SPEAK_CHANNEL_Y:
		{
			if(channelId == CHANNEL_HELP && player->hasFlag(PlayerFlag_TalkOrangeHelpChannel))
				type = SPEAK_CHANNEL_O;
			break;
		}

		case SPEAK_CHANNEL_O:
		{
			if(channelId != CHANNEL_HELP || !player->hasFlag(PlayerFlag_TalkOrangeHelpChannel))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		case SPEAK_CHANNEL_RN:
		{
			if(!player->hasFlag(PlayerFlag_CanTalkRedChannel))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		case SPEAK_CHANNEL_RA:
		{
			if(!player->hasFlag(PlayerFlag_CanTalkRedChannelAnonymous))
				type = SPEAK_CHANNEL_Y;
			break;
		}

		default:
			break;
	}

	return server.chat().talkToChannel(player, type, text, channelId);
}

bool Game::playerSpeakToNpc(Player* player, const std::string& text)
{
	SpectatorList list;
	SpectatorList::iterator it;
	getSpectators(list, player->getPosition());

	//send to npcs only
	Npc* tmpNpc = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpNpc = (*it)->getNpc()))
			(*it)->onCreatureSay(player, SPEAK_PRIVATE_PN, text);
	}
	return true;
}

bool Game::playerReportRuleViolation(Player* player, const std::string& text)
{
	//Do not allow reports on multiclones worlds since reports are name-based
	if(server.configManager().getNumber(ConfigManager::ALLOW_CLONES))
	{
		player->sendTextMessage(MSG_INFO_DESCR, "Rule violation reports are disabled.");
		return false;
	}

	cancelRuleViolation(player);
	boost::shared_ptr<RuleViolation> rvr(new RuleViolation(player, text, time(nullptr)));
	ruleViolations[player->getID()] = rvr;

	ChatChannel* channel = server.chat().getChannelById(CHANNEL_RVR);
	if(!channel)
		return false;

	for(UsersMap::const_iterator it = channel->getUsers().begin(); it != channel->getUsers().end(); ++it)
		it->second->sendToChannel(player, SPEAK_RVR_CHANNEL, text, CHANNEL_RVR, rvr->time);

	return true;
}

bool Game::playerContinueReport(Player* player, const std::string& text)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	RuleViolation& rvr = *it->second;
	Player* toPlayer = rvr.gamemaster;
	if(!toPlayer)
		return false;

	toPlayer->sendCreatureSay(player, SPEAK_RVR_CONTINUE, text);
	player->sendTextMessage(MSG_STATUS_SMALL, "Message sent to Gamemaster.");
	return true;
}

//--
bool Game::canThrowObjectTo(const Position& fromPos, const Position& toPos, bool checkLineOfSight /*= true*/,
	int32_t rangex /*= Map::maxClientViewportX*/, int32_t rangey /*= Map::maxClientViewportY*/)
{
	return map->canThrowObjectTo(fromPos, toPos, checkLineOfSight, rangex, rangey);
}

bool Game::isSightClear(const Position& fromPos, const Position& toPos, bool floorCheck)
{
	return map->isSightClear(fromPos, toPos, floorCheck);
}

bool Game::internalCreatureTurn(Creature* creature, Direction dir)
{
	bool deny = false;
	CreatureEventList directionEvents = creature->getCreatureEvents(CREATURE_EVENT_DIRECTION);
	for(CreatureEventList::iterator it = directionEvents.begin(); it != directionEvents.end(); ++it)
	{
		if(!(*it)->executeDirection(creature, creature->getDirection(), dir) && !deny)
			deny = true;
	}

	if(deny || creature->getDirection() == dir)
		return false;

	creature->setDirection(dir);
	const SpectatorList& list = getSpectators(creature->getPosition());
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureTurn(creature);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureTurn(creature);

	return true;
}

bool Game::internalCreatureSay(Creature* creature, SpeakClasses type, const std::string& text,
	bool ghostMode, SpectatorList* spectators/* = nullptr*/, Position* pos/* = nullptr*/)
{
	Player* player = creature->getPlayer();
	if(player && player->isAccountManager())
	{
		player->manageAccount(text);
		return true;
	}

	Position destPos = creature->getPosition();
	if(pos)
		destPos = (*pos);

	SpectatorList list;
	SpectatorList::const_iterator it;
	if(!spectators || !spectators->size())
	{
		// This somewhat complex construct ensures that the cached SpectatorVec
		// is used if available and if it can be used, else a local vector is
		// used (hopefully the compiler will optimize away the construction of
		// the temporary when it's not used).
		if(type != SPEAK_YELL && type != SPEAK_MONSTER_YELL)
			getSpectators(list, destPos, false, false,
				Map::maxClientViewportX, Map::maxClientViewportX,
				Map::maxClientViewportY, Map::maxClientViewportY);
		else
			getSpectators(list, destPos, false, true, 18, 18, 14, 14);
	}
	else
		list = (*spectators);

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if(!(tmpPlayer = (*it)->getPlayer()))
			continue;

		if(!ghostMode || tmpPlayer->canSeeCreature(creature))
			tmpPlayer->sendCreatureSay(creature, type, text, &destPos);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureSay(creature, type, text, &destPos);

	return true;
}

bool Game::getPathTo(const Creature* creature, const Position& destPos,
	std::list<Direction>& listDir, int32_t maxSearchDist /*= -1*/)
{
	return map->getPathTo(creature, destPos, listDir, maxSearchDist);
}

bool Game::getPathToEx(const Creature* creature, const Position& targetPos,
	std::list<Direction>& dirList, const FindPathParams& fpp)
{
	return map->getPathMatching(creature, dirList, FrozenPathingConditionCall(targetPos), fpp);
}

bool Game::getPathToEx(const Creature* creature, const Position& targetPos, std::list<Direction>& dirList,
	uint32_t minTargetDist, uint32_t maxTargetDist, bool fullPathSearch /*= true*/,
	bool clearSight /*= true*/, int32_t maxSearchDist /*= -1*/)
{
	FindPathParams fpp;
	fpp.fullPathSearch = fullPathSearch;
	fpp.maxSearchDist = maxSearchDist;
	fpp.clearSight = clearSight;
	fpp.minTargetDist = minTargetDist;
	fpp.maxTargetDist = maxTargetDist;
	return getPathToEx(creature, targetPos, dirList, fpp);
}

void Game::checkCreatureWalk(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
	{
		creature->onWalk();
	}
}

void Game::updateCreatureWalk(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
		creature->getPathToFollowCreature();
}

void Game::checkCreatureAttack(uint32_t creatureId)
{
	Creature* creature = getCreatureByID(creatureId);
	if(creature && creature->getHealth() > 0)
		creature->onAttacking(0);
}



void Game::changeSpeed(Creature* creature, int32_t varSpeedDelta)
{
	int32_t varSpeed = creature->getSpeed() - creature->getBaseSpeed();
	varSpeed += varSpeedDelta;
	creature->setSpeed(varSpeed);

	const SpectatorList& list = getSpectators(creature->getPosition());
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendChangeSpeed(creature, creature->getStepSpeed());
	}
}

void Game::internalCreatureChangeOutfit(Creature* creature, const Outfit_t& outfit, bool forced/* = false*/)
{
	if(!forced)
	{
		bool deny = false;
		CreatureEventList outfitEvents = creature->getCreatureEvents(CREATURE_EVENT_OUTFIT);
		for(CreatureEventList::iterator it = outfitEvents.begin(); it != outfitEvents.end(); ++it)
		{
			if(!(*it)->executeOutfit(creature, creature->getCurrentOutfit(), outfit) && !deny)
				deny = true;
		}

		if(deny || creature->getCurrentOutfit() == outfit)
			return;
	}

	creature->setCurrentOutfit(outfit);
	const SpectatorList& list = getSpectators(creature->getPosition());
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureChangeOutfit(creature, outfit);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureChangeOutfit(creature, outfit);
}

void Game::internalCreatureChangeVisible(Creature* creature, Visible_t visible)
{
	if (creature->isRemoved()) {
		return;
	}

	if (logger->isTraceEnabled()) {
		std::string visibleString = "?";
		switch (visible) {
		case VISIBLE_NONE:
			visibleString = "none";
			break;
		case VISIBLE_APPEAR:
			visibleString = "appear";
			break;
		case VISIBLE_DISAPPEAR:
			visibleString = "disappear";
			break;
		case VISIBLE_GHOST_APPEAR:
			visibleString = "ghost-appear";
			break;
		case VISIBLE_GHOST_DISAPPEAR:
			visibleString = "ghost-disappear";
			break;
		}

		LOGt("Game::internalCreatureChangeVisible(" << creature << ", visible = " << visibleString << ")");
	}

	const SpectatorList& list = getSpectators(creature->getPosition());
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureChangeVisible(creature, visible, BOOST_CURRENT_FUNCTION);
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureChangeVisible(creature, visible);
}


void Game::changeLight(const Creature* creature)
{
	const SpectatorList& list = getSpectators(creature->getPosition());

	//send to client
	Player* tmpPlayer = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureLight(creature);
	}
}

bool Game::combatBlockHit(CombatType_t combatType, Creature* attacker, Creature* target,
	int32_t& healthChange, bool checkDefense, bool checkArmor)
{
	if(healthChange > 0)
		return false;

	const Position& targetPos = target->getPosition();
	const SpectatorList& list = getSpectators(targetPos);
	if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
	{
		addMagicEffect(list, targetPos, MAGIC_EFFECT_POFF, target->isGhost());
		return true;
	}

	int32_t damage = -healthChange;
	BlockType_t blockType = target->blockHit(attacker, combatType, damage, checkDefense, checkArmor);

	healthChange = -damage;
	if(blockType == BLOCK_DEFENSE)
	{
		addMagicEffect(list, targetPos, MAGIC_EFFECT_POFF);
		return true;
	}
	else if(blockType == BLOCK_ARMOR)
	{
		addMagicEffect(list, targetPos, MAGIC_EFFECT_BLOCKHIT);
		return true;
	}
	else if(blockType != BLOCK_IMMUNITY)
		return false;

	MagicEffect_t effect = MAGIC_EFFECT_NONE;
	switch(combatType)
	{
		case COMBAT_UNDEFINEDDAMAGE:
			break;

		case COMBAT_ENERGYDAMAGE:
		case COMBAT_FIREDAMAGE:
		case COMBAT_PHYSICALDAMAGE:
		case COMBAT_ICEDAMAGE:
		case COMBAT_DEATHDAMAGE:
		case COMBAT_EARTHDAMAGE:
		case COMBAT_HOLYDAMAGE:
		{
			effect = MAGIC_EFFECT_BLOCKHIT;
			break;
		}

		default:
		{
			effect = MAGIC_EFFECT_POFF;
			break;
		}
	}

	addMagicEffect(list, targetPos, effect);
	return true;
}

bool Game::combatChangeHealth(CombatType_t combatType, Creature* attacker, Creature* target, int32_t healthChange,
	MagicEffect_t hitEffect/* = MAGIC_EFFECT_UNKNOWN*/, TextColor_t hitColor/* = TEXTCOLOR_UNKNOWN*/, bool force/* = false*/)
{
	const Position& targetPos = target->getPosition();
	if(healthChange > 0)
	{
		if(!force && target->getHealth() <= 0)
			return false;

		bool deny = false;
		CreatureEventList statsChangeEvents = target->getCreatureEvents(CREATURE_EVENT_STATSCHANGE);
		for(CreatureEventList::iterator it = statsChangeEvents.begin(); it != statsChangeEvents.end(); ++it)
		{
			if(!(*it)->executeStatsChange(target, attacker, STATSCHANGE_HEALTHGAIN, combatType, healthChange))
				deny = true;
		}

		if(deny)
			return false;

		target->gainHealth(attacker, healthChange);
		if(server.configManager().getBool(ConfigManager::SHOW_HEALING_DAMAGE) && !target->isGhost() &&
			(server.configManager().getBool(ConfigManager::SHOW_HEALING_DAMAGE_MONSTER) || !target->getMonster()))
		{
			char buffer[20];
			sprintf(buffer, "+%d", healthChange);

			const SpectatorList& list = getSpectators(targetPos);
			if(combatType != COMBAT_HEALING)
				addMagicEffect(list, targetPos, MAGIC_EFFECT_WRAPS_BLUE);

			addAnimatedText(list, targetPos, TEXTCOLOR_GREEN, buffer);
		}
	}
	else
	{
		SpectatorList list = getSpectators(targetPos);
		if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
		{
			addMagicEffect(list, targetPos, MAGIC_EFFECT_POFF);
			return true;
		}

		int32_t damage = -healthChange;
		if(damage != 0)
		{
			if(target->hasCondition(CONDITION_MANASHIELD) && combatType != COMBAT_UNDEFINEDDAMAGE)
			{
				int32_t manaDamage = std::min(target->getMana(), damage);
				damage = std::max((int32_t)0, damage - manaDamage);
				if(manaDamage != 0)
				{
					bool deny = false;
					CreatureEventList statsChangeEvents = target->getCreatureEvents(CREATURE_EVENT_STATSCHANGE);
					for(CreatureEventList::iterator it = statsChangeEvents.begin(); it != statsChangeEvents.end(); ++it)
					{
						if(!(*it)->executeStatsChange(target, attacker, STATSCHANGE_MANALOSS, combatType, manaDamage))
							deny = true;
					}

					if(deny)
						return false;

					target->drainMana(attacker, combatType, manaDamage);
					char buffer[20];
					sprintf(buffer, "%d", manaDamage);

					addMagicEffect(list, targetPos, MAGIC_EFFECT_LOSE_ENERGY);
					addAnimatedText(list, targetPos, TEXTCOLOR_BLUE, buffer);
				}
			}

			damage = std::min(target->getHealth(), damage);
			if(damage > 0)
			{
				bool deny = false;
				CreatureEventList statsChangeEvents = target->getCreatureEvents(CREATURE_EVENT_STATSCHANGE);
				for(CreatureEventList::iterator it = statsChangeEvents.begin(); it != statsChangeEvents.end(); ++it)
				{
					if(!(*it)->executeStatsChange(target, attacker, STATSCHANGE_HEALTHLOSS, combatType, damage))
						deny = true;
				}

				if(deny)
					return false;

				target->drainHealth(attacker, combatType, damage);
				addCreatureHealth(list, target);

				TextColor_t textColor = TEXTCOLOR_NONE;
				MagicEffect_t magicEffect = MAGIC_EFFECT_NONE;
				switch(combatType)
				{
					case COMBAT_PHYSICALDAMAGE:
					{
						boost::intrusive_ptr<Item> splash;
						switch(target->getRace())
						{
							case RACE_VENOM:
								textColor = TEXTCOLOR_LIGHTGREEN;
								magicEffect = MAGIC_EFFECT_POISON;
								splash = Item::CreateItem(ITEM_SMALLSPLASH, FLUID_GREEN);
								break;

							case RACE_BLOOD:
								textColor = TEXTCOLOR_RED;
								magicEffect = MAGIC_EFFECT_DRAW_BLOOD;
								splash = Item::CreateItem(ITEM_SMALLSPLASH, FLUID_BLOOD);
								break;

							case RACE_UNDEAD:
								textColor = TEXTCOLOR_GREY;
								magicEffect = MAGIC_EFFECT_HIT_AREA;
								break;

							case RACE_FIRE:
								textColor = TEXTCOLOR_ORANGE;
								magicEffect = MAGIC_EFFECT_DRAW_BLOOD;
								break;

							case RACE_ENERGY:
								textColor = TEXTCOLOR_PURPLE;
								magicEffect = MAGIC_EFFECT_PURPLEENERGY;
								break;

							default:
								break;
						}

						if(splash)
						{
							internalAddItem(nullptr, target->getTile(), splash.get(), INDEX_WHEREEVER, FLAG_NOLIMIT);
							startDecay(splash.get());
						}
						break;
					}

					case COMBAT_ENERGYDAMAGE:
					{
						textColor = TEXTCOLOR_PURPLE;
						magicEffect = MAGIC_EFFECT_ENERGY_DAMAGE;
						break;
					}

					case COMBAT_EARTHDAMAGE:
					{
						textColor = TEXTCOLOR_LIGHTGREEN;
						magicEffect = MAGIC_EFFECT_POISON_RINGS;
						break;
					}

					case COMBAT_DROWNDAMAGE:
					{
						textColor = TEXTCOLOR_LIGHTBLUE;
						magicEffect = MAGIC_EFFECT_LOSE_ENERGY;
						break;
					}

					case COMBAT_FIREDAMAGE:
					{
						textColor = TEXTCOLOR_ORANGE;
						magicEffect = MAGIC_EFFECT_HITBY_FIRE;
						break;
					}

					case COMBAT_ICEDAMAGE:
					{
						textColor = TEXTCOLOR_TEAL;
						magicEffect = MAGIC_EFFECT_ICEATTACK;
						break;
					}

					case COMBAT_HOLYDAMAGE:
					{
						textColor = TEXTCOLOR_YELLOW;
						magicEffect = MAGIC_EFFECT_HOLYDAMAGE;
						break;
					}

					case COMBAT_DEATHDAMAGE:
					{
						textColor = TEXTCOLOR_DARKRED;
						magicEffect = MAGIC_EFFECT_SMALLCLOUDS;
						break;
					}

					case COMBAT_LIFEDRAIN:
					{
						textColor = TEXTCOLOR_RED;
						magicEffect = MAGIC_EFFECT_WRAPS_RED;
						break;
					}

					default:
						break;
				}

				if(hitEffect != MAGIC_EFFECT_UNKNOWN)
					magicEffect = hitEffect;

				if(hitColor != TEXTCOLOR_UNKNOWN)
					textColor = hitColor;

				if(textColor < TEXTCOLOR_NONE && magicEffect < MAGIC_EFFECT_NONE)
				{
					char buffer[20];
					sprintf(buffer, "%d", damage);

					addMagicEffect(list, targetPos, magicEffect);
					addAnimatedText(list, targetPos, textColor, buffer);
				}
			}
		}
	}

	return true;
}

bool Game::combatChangeMana(Creature* attacker, Creature* target, int32_t manaChange)
{
	const Position& targetPos = target->getPosition();
	if(manaChange > 0)
	{
		bool deny = false;
		CreatureEventList statsChangeEvents = target->getCreatureEvents(CREATURE_EVENT_STATSCHANGE);
		for(CreatureEventList::iterator it = statsChangeEvents.begin(); it != statsChangeEvents.end(); ++it)
		{
			if(!(*it)->executeStatsChange(target, attacker, STATSCHANGE_MANAGAIN, COMBAT_HEALING, manaChange))
				deny = true;
		}

		if(deny)
			return false;

		target->changeMana(manaChange);
		if(server.configManager().getBool(ConfigManager::SHOW_HEALING_DAMAGE) && !target->isGhost() &&
			(server.configManager().getBool(ConfigManager::SHOW_HEALING_DAMAGE_MONSTER) || !target->getMonster()))
		{
			char buffer[20];
			sprintf(buffer, "+%d", manaChange);

			const SpectatorList& list = getSpectators(targetPos);
			addAnimatedText(list, targetPos, TEXTCOLOR_DARKPURPLE, buffer);
		}
	}
	else
	{
		const SpectatorList& list = getSpectators(targetPos);
		if(!target->isAttackable() || Combat::canDoCombat(attacker, target) != RET_NOERROR)
		{
			addMagicEffect(list, targetPos, MAGIC_EFFECT_POFF);
			return false;
		}

		int32_t manaLoss = std::min(target->getMana(), -manaChange);
		BlockType_t blockType = target->blockHit(attacker, COMBAT_MANADRAIN, manaLoss);
		if(blockType != BLOCK_NONE)
		{
			addMagicEffect(list, targetPos, MAGIC_EFFECT_POFF);
			return false;
		}

		if(manaLoss > 0)
		{
			bool deny = false;
			CreatureEventList statsChangeEvents = target->getCreatureEvents(CREATURE_EVENT_STATSCHANGE);
			for(CreatureEventList::iterator it = statsChangeEvents.begin(); it != statsChangeEvents.end(); ++it)
			{
				if(!(*it)->executeStatsChange(target, attacker, STATSCHANGE_MANALOSS, COMBAT_UNDEFINEDDAMAGE, manaChange))
					deny = true;
			}

			if(deny)
				return false;

			target->drainMana(attacker, COMBAT_MANADRAIN, manaLoss);
			char buffer[20];
			sprintf(buffer, "%d", manaLoss);

			addAnimatedText(list, targetPos, TEXTCOLOR_BLUE, buffer);
		}
	}

	return true;
}

void Game::addCreatureHealth(const Creature* target)
{
	const SpectatorList& list = getSpectators(target->getPosition());
	addCreatureHealth(list, target);
}

void Game::addCreatureHealth(const SpectatorList& list, const Creature* target)
{
	Player* player = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendCreatureHealth(target);
	}
}

void Game::addAnimatedText(const Position& pos, uint8_t textColor,
	const std::string& text)
{
	const SpectatorList& list = getSpectators(pos);
	addAnimatedText(list, pos, textColor, text);
}

void Game::addAnimatedText(const SpectatorList& list, const Position& pos, uint8_t textColor,
	const std::string& text)
{
	Player* player = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendAnimatedText(pos, textColor, text);
	}
}

void Game::addMagicEffect(const Position& pos, uint8_t effect, bool ghostMode /* = false */)
{
	if(ghostMode)
		return;

	const SpectatorList& list = getSpectators(pos);
	addMagicEffect(list, pos, effect);
}

void Game::addMagicEffect(const SpectatorList& list, const Position& pos, uint8_t effect, bool ghostMode/* = false*/)
{
	if(ghostMode)
		return;

	Player* player = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendMagicEffect(pos, effect);
	}
}

void Game::addDistanceEffect(const Position& fromPos, const Position& toPos, uint8_t effect)
{
	SpectatorList list;
	getSpectators(list, fromPos, false);
	getSpectators(list, toPos, true);
	addDistanceEffect(list, fromPos, toPos, effect);
}

void Game::addDistanceEffect(const SpectatorList& list, const Position& fromPos, const Position& toPos, uint8_t effect)
{
	Player* player = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendDistanceShoot(fromPos, toPos, effect);
	}
}

void Game::startDecay(Item* item)
{
	if(!item || !item->canDecay() || item->getDecaying() == DECAYING_TRUE)
		return;

	if(item->getDuration() > 0)
	{
		item->setDecaying(DECAYING_TRUE);
		toDecayItems.push_back(item);
	}
	else
		internalDecayItem(item);
}

void Game::internalDecayItem(Item* item)
{
	if(item->getKind()->decayTo)
	{
		ItemP newItem = transformItem(item, item->getKind()->decayTo);
		startDecay(newItem.get());
	}
	else
	{
		ReturnValue ret = internalRemoveItem(nullptr, item);
		if(ret != RET_NOERROR)
			LOGd("internalDecayItem failed, error code: " << (int32_t)ret << ", item id: " << item->getId());
	}
}

void Game::checkDecay()
{
	int64_t startTime = OTSYS_TIME();

	server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(EVENT_DECAYINTERVAL),
		std::bind(&Game::checkDecay, this)));

	size_t bucket = (lastBucket + 1) % EVENT_DECAYBUCKETS;
	lastBucket = bucket;

	ItemList& items = decayItems[bucket];
	if (items.empty()) {
		cleanup();
		return;
	}

	for(ItemList::iterator it = items.begin(); it != items.end();)
	{
		Item* item = (*it).get();

		int32_t decreaseTime = EVENT_DECAYINTERVAL * EVENT_DECAYBUCKETS;
		if(item->getDuration() - decreaseTime < 0)
			decreaseTime = item->getDuration();

		item->decreaseDuration(decreaseTime);
		if(!item->canDecay())
		{
			item->setDecaying(DECAYING_FALSE);
			autorelease(item);
			it = decayItems[bucket].erase(it);
			continue;
		}

		int32_t dur = item->getDuration();
		if(dur <= 0)
		{
			autorelease(item);
			it = decayItems[bucket].erase(it);
			internalDecayItem(item);
		}
		else if(dur < EVENT_DECAYINTERVAL * EVENT_DECAYBUCKETS)
		{
			autorelease(item);
			it = decayItems[bucket].erase(it);
			size_t newBucket = (bucket + ((dur + EVENT_DECAYINTERVAL / 2) / 1000)) % EVENT_DECAYBUCKETS;
			if(newBucket == bucket)
			{
				internalDecayItem(item);
			}
			else
				decayItems[newBucket].push_back(item);
		}
		else
			++it;
	}

	cleanup();

	double duration = static_cast<double>(OTSYS_TIME() - startTime) / 1000.0;
	if (duration > 0.5) {
		LOGw("Decaying items took too long with " << duration << " seconds!");
	}
}

void Game::checkLight()
{
	server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(EVENT_LIGHTINTERVAL),
		std::bind(&Game::checkLight, this)));

	lightHour = lightHour + lightHourDelta;
	if(lightHour > 1440)
		lightHour = lightHour - 1440;

	if(std::abs(lightHour - SUNRISE) < 2 * lightHourDelta)
		lightState = LIGHT_STATE_SUNRISE;
	else if(std::abs(lightHour - SUNSET) < 2 * lightHourDelta)
		lightState = LIGHT_STATE_SUNSET;

	int32_t newLightLevel = lightLevel;
	bool lightChange = false;
	switch(lightState)
	{
		case LIGHT_STATE_SUNRISE:
		{
			newLightLevel += (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT) / 30;
			lightChange = true;
			break;
		}
		case LIGHT_STATE_SUNSET:
		{
			newLightLevel -= (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT) / 30;
			lightChange = true;
			break;
		}
		default:
			break;
	}

	if(newLightLevel <= LIGHT_LEVEL_NIGHT)
	{
		lightLevel = LIGHT_LEVEL_NIGHT;
		lightState = LIGHT_STATE_NIGHT;
	}
	else if(newLightLevel >= LIGHT_LEVEL_DAY)
	{
		lightLevel = LIGHT_LEVEL_DAY;
		lightState = LIGHT_STATE_DAY;
	}
	else
		lightLevel = newLightLevel;

	if(lightChange)
	{
		LightInfo lightInfo;
		getWorldLightInfo(lightInfo);
		for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
			it->second->sendWorldLight(lightInfo);
	}
}

void Game::getWorldLightInfo(LightInfo& lightInfo)
{
	lightInfo.level = lightLevel;
	lightInfo.color = 0xD7;
}

bool Game::cancelRuleViolation(Player* player)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	Player* gamemaster = it->second->gamemaster;
	if(!it->second->isOpen && gamemaster) //Send to the responser
		gamemaster->sendRuleViolationCancel(player->getName());
	else if(ChatChannel* channel = server.chat().getChannelById(CHANNEL_RVR))
	{
		UsersMap tmpMap = channel->getUsers();
		for(UsersMap::iterator tit = tmpMap.begin(); tit != tmpMap.end(); ++tit)
			tit->second->sendRemoveReport(player->getName());
	}

	//Now erase it
	ruleViolations.erase(it);
	return true;
}

bool Game::closeRuleViolation(Player* player)
{
	RuleViolationsMap::iterator it = ruleViolations.find(player->getID());
	if(it == ruleViolations.end())
		return false;

	ruleViolations.erase(it);
	player->sendLockRuleViolation();
	if(ChatChannel* channel = server.chat().getChannelById(CHANNEL_RVR))
	{
		UsersMap tmpMap = channel->getUsers();
		for(UsersMap::iterator tit = tmpMap.begin(); tit != tmpMap.end(); ++tit)
			tit->second->sendRemoveReport(player->getName());
	}

	return true;
}

void Game::updateCreatureSkull(Creature* creature)
{
	const SpectatorList& list = getSpectators(creature->getPosition());

	//send to client
	Player* tmpPlayer = nullptr;
	for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
	{
		 if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendCreatureSkull(creature);
	}
}

bool Game::playerInviteToParty(uint32_t playerId, uint32_t invitedId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* invitedPlayer = getPlayerByID(invitedId);
	if(!invitedPlayer || invitedPlayer->isRemoved() || invitedPlayer->isInviting(player))
		return false;

	if(invitedPlayer->getParty())
	{
		char buffer[90];
		sprintf(buffer, "%s is already in a party.", invitedPlayer->getName().c_str());
		player->sendTextMessage(MSG_INFO_DESCR, buffer);
		return false;
	}

	Party* party = player->getParty();
	if(!party)
		party = new Party(player);
	else if(party->getLeader() != player)
		return false;

	return party->invitePlayer(invitedPlayer);
}

bool Game::playerJoinParty(uint32_t playerId, uint32_t leaderId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Player* leader = getPlayerByID(leaderId);
	if(!leader || leader->isRemoved() || !leader->isInviting(player))
		return false;

	if(!player->getParty())
		return leader->getParty()->join(player);

	player->sendTextMessage(MSG_INFO_DESCR, "You are already in a party.");
	return false;
}

bool Game::playerRevokePartyInvitation(uint32_t playerId, uint32_t invitedId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->getParty() || player->getParty()->getLeader() != player)
		return false;

	Player* invitedPlayer = getPlayerByID(invitedId);
	if(!invitedPlayer || invitedPlayer->isRemoved() || !player->isInviting(invitedPlayer))
		return false;

	player->getParty()->revokeInvitation(invitedPlayer);
	return true;
}

bool Game::playerPassPartyLeadership(uint32_t playerId, uint32_t newLeaderId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved() || !player->getParty() || player->getParty()->getLeader() != player)
		return false;

	Player* newLeader = getPlayerByID(newLeaderId);
	if(!newLeader || newLeader->isRemoved() || !player->isPartner(newLeader))
		return false;

	return player->getParty()->passLeadership(newLeader);
}

bool Game::playerLeaveParty(uint32_t playerId)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->getParty() || player->hasCondition(CONDITION_INFIGHT))
		return false;

	return player->getParty()->leave(player);
}

bool Game::playerSharePartyExperience(uint32_t playerId, bool activate, uint8_t unknown)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->getParty() || (!player->hasFlag(PlayerFlag_NotGainInFight)
		&& player->hasCondition(CONDITION_INFIGHT)))
		return false;

	return player->getParty()->setSharedExperience(player, activate);
}

bool Game::playerReportBug(uint32_t playerId, std::string comment)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	if(!player->hasFlag(PlayerFlag_CanReportBugs))
		return false;

	CreatureEventList reportBugEvents = player->getCreatureEvents(CREATURE_EVENT_REPORTBUG);
	for(CreatureEventList::iterator it = reportBugEvents.begin(); it != reportBugEvents.end(); ++it)
		(*it)->executeReportBug(player, comment);

	return true;
}

bool Game::playerViolationWindow(uint32_t playerId, std::string name, uint8_t reason, ViolationAction_t action,
	std::string comment, std::string statement, uint32_t statementId, bool ipBanishment)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return false;

	Group* group = player->getGroup();
	if(!group)
		return false;

	time_t length[3] = {0, 0, 0};
	int32_t pos = 0, start = comment.find("{");
	while((start = comment.find("{")) > 0 && pos < 4)
	{
		std::string::size_type end = comment.find("}", start);
		if(end == std::string::npos)
			break;

		std::string data = comment.substr(start + 1, end - 1);
		comment = comment.substr(end + 1);

		++pos;
		if(data.empty())
			continue;

		if(data == "delete")
		{
			action = ACTION_DELETION;
			continue;
		}

		time_t banTime = time(nullptr);
		StringVector vec = explodeString(";", data);
		for(StringVector::iterator it = vec.begin(); it != vec.end(); ++it)
		{
			StringVector tmp = explodeString(",", *it);
			uint32_t count = 1;
			if(tmp.size() > 1)
			{
				count = atoi(tmp[1].c_str());
				if(!count)
					count = 1;
			}

			if(tmp[0][0] == 's')
				banTime += count;
			if(tmp[0][0] == 'm')
				banTime += count * 60;
			if(tmp[0][0] == 'h')
				banTime += count * 3600;
			if(tmp[0][0] == 'd')
				banTime += count * 86400;
			if(tmp[0][0] == 'w')
				banTime += count * 604800;
			if(tmp[0][0] == 'm')
				banTime += count * 2592000;
			if(tmp[0][0] == 'y')
				banTime += count * 31536000;
		}

		if(action == ACTION_DELETION)
			length[pos - 2] = banTime;
		else
			length[pos - 1] = banTime;
	}

	int16_t nameFlags = group->getNameViolationFlags(), statementFlags = group->getStatementViolationFlags();
	if((ipBanishment && ((nameFlags & IPBAN_FLAG) != IPBAN_FLAG || (statementFlags & IPBAN_FLAG) != IPBAN_FLAG)) ||
		!((nameFlags & (1 << action)) || (statementFlags & (1 << action))) || reason > group->getViolationReasons())
	{
		player->sendCancel("You do not have authorization for this action.");
		return false;
	}

	uint32_t commentSize = server.configManager().getNumber(ConfigManager::MAX_VIOLATIONCOMMENT_SIZE);
	if(comment.size() > commentSize)
	{
		char buffer[90];
		sprintf(buffer, "The comment may not exceed limit of %d characters.", commentSize);

		player->sendCancel(buffer);
		return false;
	}

	toLowerCaseString(name);
	PlayerP target = getPlayerByNameEx(name);
	if(!target || name == "account manager")
	{
		player->sendCancel("A player with this name does not exist.");
		return false;
	}

	if(target->hasFlag(PlayerFlag_CannotBeBanned))
	{
		player->sendCancel("You do not have authorization for this action.");
		return false;
	}

	AccountP account = IOLoginData::getInstance()->loadAccount(target->getAccount(), true);
	if (account == nullptr) {
		return false;
	}

	enum KickAction {
		NONE = 1,
		KICK = 2,
		FULL_KICK = 3,
	} kickAction = FULL_KICK;

	pos = 1;
	switch(action)
	{
		case ACTION_STATEMENT:
		{
			StatementMap::iterator it = server.chat().statementMap.find(statementId);
			if(it == server.chat().statementMap.end())
			{
				player->sendCancel("Statement has been already reported.");
				return false;
			}

			IOBan::getInstance()->addStatement(target->getGUID(), reason, comment,
				player->getGUID(), -1, statement);
			server.chat().statementMap.erase(it);

			kickAction = NONE;
			break;
		}

		case ACTION_NAMEREPORT:
		{
			int64_t banTime = -1;
			PlayerBan_t tmp = (PlayerBan_t)server.configManager().getNumber(ConfigManager::NAME_REPORT_TYPE);
			if(tmp == PLAYERBAN_BANISHMENT)
			{
				if(!length[0])
					banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::BAN_LENGTH);
				else
					banTime = length[0];
			}

			if(!IOBan::getInstance()->addPlayerBanishment(target->getGUID(), banTime, reason, action,
				comment, player->getGUID(), tmp))
			{
				player->sendCancel("Player has been already reported.");
				return false;
			}
			else if(tmp == PLAYERBAN_BANISHMENT)
				account->setWarnings(account->getWarnings() + 1);

			kickAction = (KickAction)tmp;
			break;
		}

		case ACTION_NOTATION:
		{
			if(!IOBan::getInstance()->addNotation(account->getId(), reason,
				comment, player->getGUID(), target->getGUID()))
			{
				player->sendCancel("Unable to perform action.");
				return false;
			}

			if(IOBan::getInstance()->getNotationsCount(account->getId()) < (uint32_t)
				server.configManager().getNumber(ConfigManager::NOTATIONS_TO_BAN))
			{
				kickAction = NONE;
				break;
			}

			action = ACTION_BANISHMENT;
		}
		/* no break */

		case ACTION_BANISHMENT:
		case ACTION_BANREPORT:
		{
			bool deny = action != ACTION_BANREPORT;
			int64_t banTime = -1;
			pos = 2;

			account->setWarnings(account->getWarnings() + 1);
			if(static_cast<signed>(account->getWarnings()) >= server.configManager().getNumber(ConfigManager::WARNINGS_TO_DELETION))
				action = ACTION_DELETION;
			else if(length[0])
				banTime = length[0];
			else if(static_cast<signed>(account->getWarnings()) >= server.configManager().getNumber(ConfigManager::WARNINGS_TO_FINALBAN))
				banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::FINALBAN_LENGTH);
			else
				banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::BAN_LENGTH);

			if(!IOBan::getInstance()->addAccountBanishment(account->getId(), banTime, reason, action,
				comment, player->getGUID(), target->getGUID()))
			{
				player->sendCancel("Account is already banned.");
				return false;
			}

			if(deny)
				break;

			banTime = -1;
			PlayerBan_t tmp = (PlayerBan_t)server.configManager().getNumber(ConfigManager::NAME_REPORT_TYPE);
			if(tmp == PLAYERBAN_BANISHMENT)
			{
				if(!length[1])
					banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::FINALBAN_LENGTH);
				else
					banTime = length[1];
			}

			IOBan::getInstance()->addPlayerBanishment(target->getGUID(), banTime, reason, action, comment,
				player->getGUID(), tmp);
			break;
		}

		case ACTION_BANFINAL:
		case ACTION_BANREPORTFINAL:
		{
			bool allow = action == ACTION_BANREPORTFINAL;
			int64_t banTime = -1;

			account->setWarnings(account->getWarnings() + 1);
			if(static_cast<signed>(account->getWarnings()) >= server.configManager().getNumber(ConfigManager::WARNINGS_TO_DELETION))
				action = ACTION_DELETION;
			else if(length[0])
				banTime = length[0];
			else
				banTime = time(nullptr) + server.configManager().getNumber(ConfigManager::FINALBAN_LENGTH);

			if(!IOBan::getInstance()->addAccountBanishment(account->getId(), banTime, reason, action,
				comment, player->getGUID(), target->getGUID()))
			{
				player->sendCancel("Account is already banned.");
				return false;
			}

			if(action != ACTION_DELETION)
				account->setWarnings(account->getWarnings() + (server.configManager().getNumber(ConfigManager::WARNINGS_TO_FINALBAN) - 1));

			if(allow)
				IOBan::getInstance()->addPlayerBanishment(target->getGUID(), -1, reason, action, comment,
					player->getGUID(), (PlayerBan_t)server.configManager().getNumber(
					ConfigManager::NAME_REPORT_TYPE));

			break;
		}

		case ACTION_DELETION:
		{
			//completely internal
			account->setWarnings(account->getWarnings() + 1);
			if(!IOBan::getInstance()->addAccountBanishment(account->getId(), -1, reason, ACTION_DELETION,
				comment, player->getGUID(), target->getGUID()))
			{
				player->sendCancel("Account is currently banned or already deleted.");
				return false;
			}

			break;
		}

		default:
			// these just shouldn't occur in rvw
			return false;
	}

	if(ipBanishment && target->getIP())
	{
		if(!length[pos])
			length[pos] = time(nullptr) + server.configManager().getNumber(ConfigManager::IPBANISHMENT_LENGTH);

		IOBan::getInstance()->addIpBanishment(target->getIP(), length[pos], reason, comment, player->getGUID(), 0xFFFFFFFF);
	}

	if(kickAction == FULL_KICK)
		IOBan::getInstance()->removeNotations(account->getId());

	std::stringstream ss;
	if(server.configManager().getBool(ConfigManager::BROADCAST_BANISHMENTS))
		ss << player->getName() << " has";
	else
		ss << "You have";

	ss << " taken the action \"" << getAction(action, ipBanishment) << "\"";
	switch(action)
	{
		case ACTION_NOTATION:
		{
			ss << " (" << (server.configManager().getNumber(ConfigManager::NOTATIONS_TO_BAN) - IOBan::getInstance()->getNotationsCount(
				account->getId())) << " left to banishment)";
			break;
		}
		case ACTION_STATEMENT:
		{
			ss << " for the statement: \"" << statement << "\"";
			break;
		}
		default:
			break;
	}

	ss << " against: " << name << " (Warnings: " << account->getWarnings() << "), with reason: \"" << getReason(
		reason) << "\", and comment: \"" << comment << "\".";
	if(server.configManager().getBool(ConfigManager::BROADCAST_BANISHMENTS))
		broadcastMessage(ss.str(), MSG_STATUS_WARNING);
	else
		player->sendTextMessage(MSG_STATUS_CONSOLE_RED, ss.str());

	if(!target->isVirtual() && kickAction > NONE)
	{
		char buffer[30];
		sprintf(buffer, "You have been %s.", (kickAction > KICK ? "banished" : "namelocked"));
		target->sendTextMessage(MSG_INFO_DESCR, buffer);

		addMagicEffect(target->getPosition(), MAGIC_EFFECT_WRAPS_GREEN);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(1000), std::bind(
			&Game::kickPlayer, this, target->getID(), false)));
	}

	IOLoginData::getInstance()->saveAccount(*account);
	return true;
}

void Game::kickPlayer(uint32_t playerId, bool displayEffect)
{
	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return;

	player->kickPlayer(displayEffect, true);
}

bool Game::broadcastMessage(const std::string& text, MessageClasses type)
{
	if(type < MSG_CLASS_FIRST || type > MSG_CLASS_LAST)
		return false;

	LOGi("Broadcasted message: \"" << text << "\".");
	for(AutoList<Player>::iterator it = Player::autoList.begin(); it != Player::autoList.end(); ++it)
		it->second->sendTextMessage(type, text);

	return true;
}

Position Game::getClosestFreeTile(Creature* creature, Position pos, bool extended/* = false*/, bool ignoreHouse/* = true*/)
{
	typedef std::pair<int32_t, int32_t> PositionPair;
	typedef std::vector<PositionPair> PairVector;

	PairVector relList;
	relList.push_back(PositionPair(0, 0));
	relList.push_back(PositionPair(-1, -1));
	relList.push_back(PositionPair(-1, 0));
	relList.push_back(PositionPair(-1, 1));
	relList.push_back(PositionPair(0, -1));
	relList.push_back(PositionPair(0, 1));
	relList.push_back(PositionPair(1, -1));
	relList.push_back(PositionPair(1, 0));
	relList.push_back(PositionPair(1, 1));

	if(extended)
	{
		relList.push_back(PositionPair(-2, 0));
		relList.push_back(PositionPair(0, -2));
		relList.push_back(PositionPair(0, 2));
		relList.push_back(PositionPair(2, 0));
	}

	std::random_shuffle(relList.begin() + 1, relList.end());
	if(Player* player = creature->getPlayer())
	{
		for(PairVector::iterator it = relList.begin(); it != relList.end(); ++it)
		{
			Tile* tile = map->getTile(Position((pos.x + it->first), (pos.y + it->second), pos.z));
			if(!tile || !tile->ground)
				continue;

			ReturnValue ret = tile->__queryAdd(0, player, 1, FLAG_IGNOREBLOCKITEM);
			if(ret == RET_NOTENOUGHROOM || (ret == RET_NOTPOSSIBLE && !player->hasCustomFlag(PlayerCustomFlag_CanMoveAnywhere))
				|| (ret == RET_PLAYERISNOTINVITED && !ignoreHouse && !player->hasFlag(PlayerFlag_CanEditHouses)))
				continue;

			return tile->getPosition();
		}
	}
	else
	{
		for(PairVector::iterator it = relList.begin(); it != relList.end(); ++it)
		{
			Tile* tile = nullptr;
			if((tile = map->getTile(Position((pos.x + it->first), (pos.y + it->second), pos.z)))
				&& tile->__queryAdd(0, creature, 1, FLAG_IGNOREBLOCKITEM) == RET_NOERROR)
				return tile->getPosition();
		}
	}

	return Position(0, 0, 0);
}

std::string Game::getSearchString(const Position fromPos, const Position toPos, bool fromIsCreature/* = false*/, bool toIsCreature/* = false*/)
{
	/*
	 * When the position is on same level and 0 to 4 squares away, they are "[toIsCreature: standing] next to you"
	 * When the position is on same level and 5 to 100 squares away they are "to the north/west/south/east."
	 * When the position is on any level and 101 to 274 squares away they are "far to the north/west/south/east."
	 * When the position is on any level and 275+ squares away they are "very far to the north/west/south/east."
	 * When the position is not directly north/west/south/east of you they are "((very) far) to the north-west/south-west/south-east/north-east."
	 * When the position is on a lower or higher level and 5 to 100 squares away they are "on a lower (or) higher level to the north/west/south/east."
	 * When the position is on a lower or higher level and 0 to 4 squares away they are "below (or) above you."
	 */

	enum distance_t
	{
		DISTANCE_BESIDE,
		DISTANCE_CLOSE,
		DISTANCE_FAR,
		DISTANCE_VERYFAR
	};

	enum direction_t
	{
		DIR_N, DIR_S, DIR_E, DIR_W,
		DIR_NE, DIR_NW, DIR_SE, DIR_SW
	};

	enum level_t
	{
		LEVEL_HIGHER,
		LEVEL_LOWER,
		LEVEL_SAME
	};

	distance_t distance;
	direction_t direction;
	level_t level;

	int32_t dx = fromPos.x - toPos.x, dy = fromPos.y - toPos.y, dz = fromPos.z - toPos.z;
	if(dz > 0)
		level = LEVEL_HIGHER;
	else if(dz < 0)
		level = LEVEL_LOWER;
	else
		level = LEVEL_SAME;

	if(std::abs(dx) < 5 && std::abs(dy) < 5)
		distance = DISTANCE_BESIDE;
	else
	{
		int32_t tmp = dx * dx + dy * dy;
		if(tmp < 10000)
			distance = DISTANCE_CLOSE;
		else if(tmp < 75625)
			distance = DISTANCE_FAR;
		else
			distance = DISTANCE_VERYFAR;
	}

	float tan;
	if(dx != 0)
		tan = (float)dy / (float)dx;
	else
		tan = 10.;

	if(std::abs(tan) < 0.4142)
	{
		if(dx > 0)
			direction = DIR_W;
		else
			direction = DIR_E;
	}
	else if(std::abs(tan) < 2.4142)
	{
		if(tan > 0)
		{
			if(dy > 0)
				direction = DIR_NW;
			else
				direction = DIR_SE;
		}
		else
		{
			if(dx > 0)
				direction = DIR_SW;
			else
				direction = DIR_NE;
		}
	}
	else
	{
		if(dy > 0)
			direction = DIR_N;
		else
			direction = DIR_S;
	}

	std::stringstream ss;
	switch(distance)
	{
		case DISTANCE_BESIDE:
		{
			switch(level)
			{
				case LEVEL_SAME:
				{
					ss << "is ";
					if(toIsCreature)
						ss << "standing ";

					ss << "next to you";
					break;
				}

				case LEVEL_HIGHER:
				{
					ss << "is above ";
					if(fromIsCreature)
						ss << "you";

					break;
				}

				case LEVEL_LOWER:
				{
					ss << "is below ";
					if(fromIsCreature)
						ss << "you";

					break;
				}

				default:
					break;
			}

			break;
		}

		case DISTANCE_CLOSE:
		{
			switch(level)
			{
				case LEVEL_SAME:
					ss << "is to the";
					break;
				case LEVEL_HIGHER:
					ss << "is on a higher level to the";
					break;
				case LEVEL_LOWER:
					ss << "is on a lower level to the";
					break;
				default:
					break;
			}

			break;
		}

		case DISTANCE_FAR:
			ss << "is far to the";
			break;

		case DISTANCE_VERYFAR:
			ss << "is very far to the";
			break;

		default:
			break;
	}

	if(distance != DISTANCE_BESIDE)
	{
		ss << " ";
		switch(direction)
		{
			case DIR_N:
				ss << "north";
				break;

			case DIR_S:
				ss << "south";
				break;

			case DIR_E:
				ss << "east";
				break;

			case DIR_W:
				ss << "west";
				break;

			case DIR_NE:
				ss << "north-east";
				break;

			case DIR_NW:
				ss << "north-west";
				break;

			case DIR_SE:
				ss << "south-east";
				break;

			case DIR_SW:
				ss << "south-west";
				break;

			default:
				break;
		}
	}

	return ss.str();
}

double Game::getExperienceStage(uint32_t level, double divider/* = 1.*/)
{
	if(!server.configManager().getBool(ConfigManager::EXPERIENCE_STAGES))
		return server.configManager().getDouble(ConfigManager::RATE_EXPERIENCE) * divider;

	if(lastStageLevel && level >= lastStageLevel)
		return stages[lastStageLevel] * divider;

	return stages[level] * divider;
}

bool Game::fetchBlacklist()
{
	xmlDocPtr doc = xmlParseFile("http://forgottenserver.otland.net/blacklist.xml");
	if(!doc)
		return false;

	xmlNodePtr p, root = xmlDocGetRootElement(doc);
	if(!xmlStrcmp(root->name, (const xmlChar*)"blacklist"))
	{
		p = root->children;
		while(p)
		{
			if(!xmlStrcmp(p->name, (const xmlChar*)"entry"))
			{
				std::string ip;
				if(readXMLString(p, "ip", ip))
					blacklist.push_back(ip);
			}

			p = p->next;
		}
	}

	xmlFreeDoc(doc);
	return true;
}

bool Game::loadExperienceStages()
{
	if(!server.configManager().getBool(ConfigManager::EXPERIENCE_STAGES))
		return true;

	xmlDocPtr doc = xmlParseFile(getFilePath(FileType::XML, "stages.xml").c_str());
	if(!doc)
	{
		LOGe("[Game::loadExperienceStages] Cannot load stages file: " << getLastXMLError());
		return false;
	}

	xmlNodePtr q, p, root = xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name, (const xmlChar*)"stages"))
	{
		LOGe("[Game::loadExperienceStages] Malformed stages file");
		xmlFreeDoc(doc);
		return false;
	}

	int32_t intValue, low = 0, high = 0;
	float floatValue, mul = 1.0f, defStageMultiplier;
	std::string strValue;

	lastStageLevel = 0;
	stages.clear();

	q = root->children;
	while(q)
	{
		if(!xmlStrcmp(q->name, (const xmlChar*)"world"))
		{
			if(readXMLString(q, "id", strValue))
			{
				IntegerVec intVector;
				if(!parseIntegerVec(strValue, intVector) || std::find(intVector.begin(),
					intVector.end(), server.configManager().getNumber(ConfigManager::WORLD_ID)) == intVector.end())
				{
					q = q->next;
					continue;
				}
			}

			defStageMultiplier = 1.0f;
			if(readXMLFloat(q, "multiplier", floatValue))
				defStageMultiplier = floatValue;

			p = q->children;
			while(p)
			{
				if(!xmlStrcmp(p->name, (const xmlChar*)"stage"))
				{
					low = 1;
					if(readXMLInteger(p, "minlevel", intValue) || readXMLInteger(p, "minLevel", intValue))
						low = intValue;

					high = 0;
					if(readXMLInteger(p, "maxlevel", intValue) || readXMLInteger(p, "maxLevel", intValue))
						high = intValue;
					else
						lastStageLevel = low;

					mul = 1.0f;
					if(readXMLFloat(p, "multiplier", floatValue))
						mul = floatValue;

					mul *= defStageMultiplier;
					if(lastStageLevel && lastStageLevel == (uint32_t)low)
						stages[lastStageLevel] = mul;
					else
					{
						for(int32_t i = low; i <= high; i++)
							stages[i] = mul;
					}
				}

				p = p->next;
			}
		}

		if(!xmlStrcmp(q->name, (const xmlChar*)"stage"))
		{
			low = 1;
			if(readXMLInteger(q, "minlevel", intValue))
				low = intValue;
			else

			high = 0;
			if(readXMLInteger(q, "maxlevel", intValue))
				high = intValue;
			else
				lastStageLevel = low;

			mul = 1.0f;
			if(readXMLFloat(q, "multiplier", floatValue))
				mul = floatValue;

			if(lastStageLevel && lastStageLevel == (uint32_t)low)
				stages[lastStageLevel] = mul;
			else
			{
				for(int32_t i = low; i <= high; i++)
					stages[i] = mul;
			}
		}

		q = q->next;
	}

	xmlFreeDoc(doc);
	return true;
}

bool Game::reloadHighscores()
{
	lastHighscoreCheck = time(nullptr);
	for(int16_t i = 0; i < 9; ++i)
		highscoreStorage[i] = getHighscore(i);

	return true;
}

void Game::checkHighscores()
{
	reloadHighscores();
	uint32_t tmp = server.configManager().getNumber(ConfigManager::HIGHSCORES_UPDATETIME) * 60 * 1000;
	if(tmp <= 0)
		return;

	server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(tmp), std::bind(&Game::checkHighscores, this)));
}

std::string Game::getHighscoreString(uint16_t skill)
{
	Highscore hs = highscoreStorage[skill];
	std::stringstream ss;
	ss << "Highscore for " << getSkillName(skill) << "\n\nRank Level - Player Name";
	for(uint32_t i = 0; i < hs.size(); i++)
		ss << "\n" << (i + 1) << ".  " << hs[i].second << "  -  " << hs[i].first;

	ss << "\n\nLast updated on:\n" << std::ctime(&lastHighscoreCheck);
	return ss.str();
}

Highscore Game::getHighscore(uint16_t skill)
{
	Highscore hs;

	Database& db = server.database();
	DBResultP result;

	DBQuery query;
	if(skill >= SKILL__MAGLEVEL)
	{
		if(skill == SKILL__MAGLEVEL)
			query << "SELECT `maglevel`, `name` FROM `players` ORDER BY `maglevel` DESC, `manaspent` DESC LIMIT " << server.configManager().getNumber(ConfigManager::HIGHSCORES_TOP);
		else
			query << "SELECT `level`, `name` FROM `players` ORDER BY `level` DESC, `experience` DESC LIMIT " << server.configManager().getNumber(ConfigManager::HIGHSCORES_TOP);

		if(!(result = db.storeQuery(query.str())))
			return hs;

		do
		{
			uint32_t level;
			if(skill == SKILL__MAGLEVEL)
				level = result->getDataInt("maglevel");
			else
				level = result->getDataInt("level");

			std::string name = result->getDataString("name");
			if(name.length() > 0)
				hs.push_back(std::make_pair(name, level));
		}
		while(result->next());
	}
	else
	{
		query << "SELECT `player_skills`.`value`, `players`.`name` FROM `player_skills`,`players` WHERE `player_skills`.`skillid`=" << skill << " AND `player_skills`.`player_id`=`players`.`id` ORDER BY `player_skills`.`value` DESC, `player_skills`.`count` DESC LIMIT " << server.configManager().getNumber(ConfigManager::HIGHSCORES_TOP);
		if(!(result = db.storeQuery(query.str())))
			return hs;

		do
		{
			std::string name = result->getDataString("name");
			if(name.length() > 0)
				hs.push_back(std::make_pair(name, result->getDataInt("value")));
		}
		while(result->next());
	}

	return hs;
}

int32_t Game::getMotdId()
{
	if(lastMotd == server.configManager().getString(ConfigManager::MOTD))
		return lastMotdId;

	lastMotd = server.configManager().getString(ConfigManager::MOTD);
	Database& db = server.database();

	DBQuery query;
	query << "INSERT INTO `server_motd` (`id`, `world_id`, `text`) VALUES (" << ++lastMotdId << ", " << server.configManager().getNumber(ConfigManager::WORLD_ID) << ", " << db.escapeString(lastMotd) << ")";
	if(db.executeQuery(query.str()))
		return lastMotdId;

	return --lastMotdId;
}

void Game::loadMotd()
{
	Database& db = server.database();
	DBQuery query;
	query << "SELECT `id`, `text` FROM `server_motd` WHERE `world_id` = " << server.configManager().getNumber(ConfigManager::WORLD_ID) << " ORDER BY `id` DESC LIMIT 1";

	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
	{
		LOGe("Failed to load motd!");
		lastMotdId = random_range(5, 500);
		return;
	}

	lastMotdId = result->getDataInt("id");
	lastMotd = result->getDataString("text");
}

void Game::checkPlayersRecord(Player* player)
{
	uint32_t count = getPlayersOnline();
	if(count <= playersRecord)
		return;

	GlobalEventMap recordEvents = server.globalEvents().getEventMap(GLOBAL_EVENT_RECORD);
	for(GlobalEventMap::iterator it = recordEvents.begin(); it != recordEvents.end(); ++it)
		it->second->executeRecord(count, playersRecord, player);

	playersRecord = count;
}

void Game::loadPlayersRecord()
{
	Database& db = server.database();
	DBQuery query;
	query << "SELECT `record` FROM `server_record` WHERE `world_id` = " << server.configManager().getNumber(ConfigManager::WORLD_ID) << " ORDER BY `timestamp` DESC LIMIT 1";

	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
	{
		LOGe("Failed to load players record!");
		return;
	}

	playersRecord = result->getDataInt("record");
}

bool Game::reloadInfo(ReloadInfo_t reload, uint32_t playerId/* = 0*/)
{
	bool done = false;
	switch(reload)
	{
		case RELOAD_ACTIONS:
		{
			if(server.actions().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload actions.");

			break;
		}

		case RELOAD_CHAT:
		{
			if(server.chat().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload chat.");

			break;
		}

		case RELOAD_CONFIG:
		{
			if(server.configManager().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload config.");

			break;
		}

		case RELOAD_CREATUREEVENTS:
		{
			if(server.creatureEvents().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload creature events.");

			break;
		}

		case RELOAD_GAMESERVERS:
		{
			#ifdef __LOGIN_SERVER__
			if(GameServers::getInstance()->reload())
				done = true;
			else
				LOGe("[Error - Game::reloadInfo] Failed to reload game servers.");

			#endif
			break;
		}

		case RELOAD_GLOBALEVENTS:
		{
			if(server.globalEvents().reload())
				done = true;
			else
				LOGe("[Error - Game::reloadInfo] Failed to reload global events.");

			break;
		}

		case RELOAD_GROUPS:
		{
			//if(Groups::getInstance()->reload())
				done = true;
			//else
			//	LOGe("[Game::reloadInfo] Failed to reload groups.");

			break;
		}

		case RELOAD_HIGHSCORES:
		{
			if(reloadHighscores())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload highscores.");

			break;
		}

		case RELOAD_HOUSEPRICES:
		{
			if(Houses::getInstance()->reloadPrices())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload house prices.");

			break;
		}

		case RELOAD_ITEMS:
		{
			//TODO
			LOGw("[Game::reloadInfo] Cannot reload items yet.");
			done = true;
			break;
		}

		case RELOAD_MODS:
		{
			LOGi("Reloading mods...");
			if(ScriptingManager::getInstance()->reloadMods())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload mods.");

			break;
		}

		case RELOAD_MONSTERS:
		{
			if(server.monsters().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload monsters.");

			break;
		}

		case RELOAD_MOVEEVENTS:
		{
			if(server.moveEvents().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload move events.");

			break;
		}

		case RELOAD_NPCS:
		{
			server.npcs().reload();
			done = true;
			break;
		}

		case RELOAD_OUTFITS:
		{
			//TODO
			LOGw("[Game::reloadInfo] Cannot reload outfits yet.");
			done = true;
			break;
		}

		case RELOAD_QUESTS:
		{
			if(Quests::getInstance()->reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload quests.");

			break;
		}

		case RELOAD_RAIDS:
		{
			if(!Raids::getInstance()->reload())
				LOGe("[Game::reloadInfo] Failed to reload raids.");
			else if(!Raids::getInstance()->startup())
				LOGe("[Game::reloadInfo] Failed to startup raids when reloading.");
			else
				done = true;

			break;
		}

		case RELOAD_SPELLS:
		{
			if(!server.spells().reload())
				LOGe("[Game::reloadInfo] Failed to reload spells.");
			else if(!server.monsters().reload())
				LOGe("[Game::reloadInfo] Failed to reload monsters when reloading spells.");
			else
				done = true;

			break;
		}

		case RELOAD_STAGES:
		{
			if(loadExperienceStages())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload stages.");

			break;
		}

		case RELOAD_TALKACTIONS:
		{
			if(server.talkActions().reload())
				done = true;
			else
				LOGe("[Game::reloadInfo] Failed to reload talk actions.");

			break;
		}

		case RELOAD_VOCATIONS:
		{
			//if(Vocations::getInstance()->reload())
				done = true;
			//else
			//	LOGe("[Game::reloadInfo] Reload type does not work.");

			break;
		}

		case RELOAD_WEAPONS:
		{
			//TODO
			LOGw("[Game::reloadInfo] Cannot reload weapons yet.");
			done = true;
			break;
		}

		case RELOAD_ALL:
		{
			done = true;
			for(uint8_t i = RELOAD_FIRST; i <= RELOAD_LAST; i++)
			{
				if(!reloadInfo((ReloadInfo_t)i) && done)
					done = false;
			}

			break;
		}

		default:
		{
			LOGe("[Game::reloadInfo] Reload type not found.");
			break;
		}
	}

	if(!playerId)
		return done;

	Player* player = getPlayerByID(playerId);
	if(!player || player->isRemoved())
		return done;

	if(done)
	{
		player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Reloaded successfully.");
		return true;
	}

	player->sendTextMessage(MSG_STATUS_CONSOLE_BLUE, "Failed to reload.");
	return false;
}

void Game::prepareGlobalSave()
{
	if(!globalSaveMessage[0])
	{
		setGameState(GAME_STATE_CLOSING);
		globalSaveMessage[0] = true;

		broadcastMessage("Server is going down for a global save within 5 minutes. Please logout.", MSG_STATUS_WARNING);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(120000), std::bind(&Game::prepareGlobalSave, this)));
	}
	else if(!globalSaveMessage[1])
	{
		globalSaveMessage[1] = true;
		broadcastMessage("Server is going down for a global save within 3 minutes. Please logout.", MSG_STATUS_WARNING);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(120000), std::bind(&Game::prepareGlobalSave, this)));
	}
	else if(!globalSaveMessage[2])
	{
		globalSaveMessage[2] = true;
		broadcastMessage("Server is going down for a global save in one minute, please logout!", MSG_STATUS_WARNING);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(60000), std::bind(&Game::prepareGlobalSave, this)));
	}
	else
		globalSave();
}

void Game::globalSave()
{
	if(server.configManager().getBool(ConfigManager::SHUTDOWN_AT_GLOBALSAVE))
	{
		//shutdown server
		server.dispatcher().addTask(Task::create(std::bind(&Game::setGameState, this, GAME_STATE_SHUTDOWN)));
		return;
	}

	//close server
	server.dispatcher().addTask(Task::create(std::bind(&Game::setGameState, this, GAME_STATE_CLOSED)));
	//clean map if configured to
	if(server.configManager().getBool(ConfigManager::CLEAN_MAP_AT_GLOBALSAVE))
	{
		uint32_t dummy;
		cleanMap(dummy);
	}

	//pay houses
	Houses::getInstance()->payHouses();
	//clear temporial and expired bans
	IOBan::getInstance()->clearTemporials();
	//remove premium days globally if configured to
	if(server.configManager().getBool(ConfigManager::REMOVE_PREMIUM_ON_INIT))
		IOLoginData::getInstance()->updatePremiumDays();

	//reload everything
	reloadInfo(RELOAD_ALL);
	//reset variables
	for(int16_t i = 0; i < 3; i++)
		setGlobalSaveMessage(i, false);

	//prepare for next global save after 24 hours
	server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(86100000), std::bind(&Game::prepareGlobalSave, this)));
	//open server
	server.dispatcher().addTask(Task::create(std::bind(&Game::setGameState, this, GAME_STATE_NORMAL)));
}

void Game::shutdown()
{
	LOGi("Preparing to shutdown the server...");
	server.scheduler().waitUntilStopped();
	server.dispatcher().waitUntilStopped();
	Spawns::getInstance()->clear();
	Raids::getInstance()->clear();
	clear();
	if(services)
		services->stop();
	LOGi("Server was shut down!");
}

void Game::cleanup() {
	autoreleasePool.clear();

	for(ItemList::iterator it = toDecayItems.begin(); it != toDecayItems.end(); ++it)
	{
		int32_t dur = (*it)->getDuration();
		if(dur >= EVENT_DECAYINTERVAL * EVENT_DECAYBUCKETS)
			decayItems[lastBucket].push_back(*it);
		else
			decayItems[(lastBucket + 1 + (*it)->getDuration() / 1000) % EVENT_DECAYBUCKETS].push_back(*it);
	}
	toDecayItems.clear();
}

void Game::autorelease(boost::intrusive_ptr<ReferenceCounted> object) {
	autorelease(object.get());
}

void Game::autorelease(ReferenceCounted* object) {
	if (object == nullptr) {
		return;
	}

	autoreleasePool.push_back(object);
}

void Game::showHotkeyUseMessage(Player* player, Item* item)
{
	ItemKindPC kind = item->getKind();
	uint32_t count = player->__getItemTypeCount(item->getId(), -1);

	char buffer[40 + kind->name.size()];
	if(count == 1)
		sprintf(buffer, "Using the last %s...", kind->name.c_str());
	else
		sprintf(buffer, "Using one of %d %s...", count, kind->pluralName.c_str());

	player->sendTextMessage(MSG_INFO_DESCR, buffer);
}


uint32_t Game::getPlayersOnline() {return (uint32_t)Player::autoList.size();}
uint32_t Game::getMonstersOnline() {return (uint32_t)Monster::autoList.size();}
uint32_t Game::getNpcsOnline() {return (uint32_t)Npc::autoList.size();}
uint32_t Game::getCreaturesOnline() {return (uint32_t)autoList.size();}
