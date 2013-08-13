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

#ifndef _GAME_H
#define _GAME_H

#include "map.h"
#include "tile.h"

class  CombatInfo;
class  Creature;
struct FindPathParams;
class  Monster;
class  Npc;
class  ReferenceCounted;
class  ServiceManager;

typedef boost::intrusive_ptr<Creature>                  CreatureP;
typedef std::vector<boost::intrusive_ptr<Creature>>     CreatureVector;
typedef std::vector<Player*>                            PlayerVector;
typedef boost::intrusive_ptr<Item>                      ItemP;
typedef std::list<boost::intrusive_ptr<Item>>           ItemList;
typedef boost::intrusive_ptr<Player>                    PlayerP;
typedef std::map<boost::intrusive_ptr<Item>, uint32_t>  TradeItemMap;


enum stackposType_t
{
	STACKPOS_NORMAL,
	STACKPOS_MOVE,
	STACKPOS_LOOK,
	STACKPOS_USE,
	STACKPOS_USEITEM
};

enum WorldType_t
{
	WORLD_TYPE_FIRST = 1,
	WORLD_TYPE_NO_PVP = WORLD_TYPE_FIRST,
	WORLD_TYPE_PVP = 2,
	WORLD_TYPE_PVP_ENFORCED = 3,
	WORLD_TYPE_LAST = WORLD_TYPE_PVP_ENFORCED
};

enum GameState_t
{
	GAME_STATE_FIRST = 1,
	GAME_STATE_STARTUP = GAME_STATE_FIRST,
	GAME_STATE_INIT = 2,
	GAME_STATE_NORMAL = 3,
	GAME_STATE_MAINTAIN = 4,
	GAME_STATE_CLOSED = 5,
	GAME_STATE_CLOSING = 6,
	GAME_STATE_SHUTDOWN = 7,
	GAME_STATE_LAST = GAME_STATE_SHUTDOWN
};

enum LightState_t
{
	LIGHT_STATE_DAY,
	LIGHT_STATE_NIGHT,
	LIGHT_STATE_SUNSET,
	LIGHT_STATE_SUNRISE
};

enum ReloadInfo_t
{
	RELOAD_FIRST = 1,
	RELOAD_ACTIONS = RELOAD_FIRST,
	RELOAD_CHAT = 2,
	RELOAD_CONFIG = 3,
	RELOAD_CREATUREEVENTS = 4,
	RELOAD_GAMESERVERS = 5,
	RELOAD_GLOBALEVENTS = 6,
	RELOAD_GROUPS = 7,
	RELOAD_HIGHSCORES = 8,
	RELOAD_HOUSEPRICES = 9,
	RELOAD_ITEMS = 10,
	RELOAD_MONSTERS = 11,
	RELOAD_MOVEEVENTS = 12,
	RELOAD_NPCS = 13,
	RELOAD_OUTFITS = 14,
	RELOAD_QUESTS = 15,
	RELOAD_RAIDS = 16,
	RELOAD_SPELLS = 17,
	RELOAD_STAGES = 18,
	RELOAD_TALKACTIONS = 19,
	RELOAD_VOCATIONS = 20,
	RELOAD_WEAPONS = 21,
	RELOAD_MODS = 22,
	RELOAD_ALL = 23,
	RELOAD_LAST = RELOAD_MODS
};

struct RuleViolation
{
	RuleViolation(Player* _reporter, const std::string& _text, uint32_t _time):
		reporter(_reporter), gamemaster(nullptr), text(_text), time(_time), isOpen(true) {}

	Player* reporter;
	Player* gamemaster;
	std::string text;
	uint32_t time;
	bool isOpen;

	private:
		RuleViolation(const RuleViolation&);
};

struct RefreshBlock_t
{
	TileItemVector list;
	uint64_t lastRefresh;
};

typedef std::map<uint32_t, boost::shared_ptr<RuleViolation> > RuleViolationsMap;
typedef std::map<Tile*, RefreshBlock_t> RefreshTiles;
typedef std::vector< std::pair<std::string, uint32_t> > Highscore;
typedef std::map<int32_t, float> StageList;

#define EVENT_LIGHTINTERVAL 10000
#define EVENT_DECAYINTERVAL 1000
#define EVENT_DECAYBUCKETS 16
#define STATE_DELAY 1000

/**
  * Main Game class.
  * This class is responsible to control everything that happens
  */

class Game {

public:

	Tile* getNextTile (const Tile& tile, Direction direction) const;


private:




	public:
		Game();
		~Game();

		void start(ServiceManager* servicer);

		Highscore getHighscore(uint16_t skill);
		std::string getHighscoreString(uint16_t skill);
		void checkHighscores();
		bool reloadHighscores();

		void prepareGlobalSave();
		void globalSave();

		/**
		  * Load a map.
		  * \param filename Mapfile to load
		  * \returns int32_t 0 built-in spawns, 1 needs xml spawns, 2 needs sql spawns, -1 if got error
		  */
		int32_t loadMap(std::string filename);

		/**
		  * Get the map size - info purpose only
		  * \param width width of the map
		  * \param height height of the map
		  */
		void getMapDimensions(uint32_t& width, uint32_t& height)
		{
			width = map->getWidth();
			height = map->getHeight();
			return;
		}

		void setWorldType(WorldType_t type) {worldType = type;}
		WorldType_t getWorldType() const {return worldType;}

		Cylinder* internalGetCylinder(Player* player, const ExtendedPosition& position);
		Thing* internalGetThing(Player* player, const ExtendedPosition& position, stackposType_t type = STACKPOS_NORMAL);
		ExtendedPosition internalGetPosition(Item* item, const ExtendedPosition& position);

		std::string getTradeErrorDescription(ReturnValue ret, Item* item);

		/**
		  * Get a single tile of the map.
		  * \returns A pointer to the tile
		  */
		Tile* getTile(int32_t x, int32_t y, int32_t z) const {return map->getTile(x, y, z);}
		Tile* getTile(const Position& pos) const {return map->getTile(pos);}


		/**
		  * Returns a creature based on a string name identifier
		  * \param s is the name identifier
		  * \returns A Pointer to the creature
		  */
		Creature* getCreatureByName(const std::string& name);

		/**
		  * Returns a player based on a string name identifier
		  * \param s is the name identifier
		  * \returns A Pointer to the player
		  */
		PlayerP getPlayerByName(const std::string& name);

		/**
		  * Returns a player based on a string name identifier
		  * this function returns a pointer even if the player is offline,
		  * it is up to the caller of the function to delete the pointer - if the player is offline
		  * use isOffline() to determine if the player was offline
		  * \param s is the name identifier
		  * \return A Pointer to the player
		  */
		PlayerP getPlayerByNameEx(const std::string& s);

		/**
		  * Returns a player based on a guid identifier
		  * this function returns a pointer even if the player is offline,
		  * it is up to the caller of the function to delete the pointer - if the player is offline
		  * use isOffline() to determine if the player was offline
		  * \param guid is the identifier
		  * \return A Pointer to the player
		  */
		PlayerP getPlayerByGuid(uint32_t guid);

		/**
		  * Returns a player based on a guid identifier
		  * this function returns a pointer even if the player is offline,
		  * it is up to the caller of the function to delete the pointer - if the player is offline
		  * use isOffline() to determine if the player was offline
		  * \param guid is the identifier
		  */
		PlayerP getPlayerByGuidEx(uint32_t guid);

		/**
		  * Returns a player based on a string name identifier, with support for the "~" wildcard.
		  * \param s is the name identifier, with or without wildcard
		  * \param player will point to the found player (if any)
		  * \return "RET_PLAYERWITHTHISNAMEISNOTONLINE" or "RET_NAMEISTOOAMBIGUOUS"
		  */
		ReturnValue getPlayerByNameWildcard(std::string s, Player*& player);

		/**
		  * Returns a player based on an account number identifier
		  * \param acc is the account identifier
		  * \returns A Pointer to the player
		  */
		Player* getPlayerByAccount(uint32_t accountId);

		/**
		  * Returns all players based on their name
		  * \param s is the player name
		  * \return A vector of all players with the selected name
		  */
		PlayerVector getPlayersByName(const std::string& name);

		/**
		  * Returns all players based on their account number identifier
		  * \param acc is the account identifier
		  * \return A vector of all players with the selected account number
		  */
		PlayerVector getPlayersByAccount(uint32_t accountId);

		/**
		  * Returns all players with a certain IP address
		  * \param ip is the IP address of the clients, as an unsigned long
		  * \param mask An IP mask, default 255.255.255.255
		  * \return A vector of all players with the selected IP
		  */
		PlayerVector getPlayersByIP(uint32_t ip, uint32_t mask = 0xFFFFFFFF);

		ReturnValue placeSummon(Creature* creature, const std::string& name);

		uint32_t getPlayersOnline();
		uint32_t getMonstersOnline();
		uint32_t getNpcsOnline();
		uint32_t getCreaturesOnline();

		uint32_t getPlayersRecord() {return playersRecord;}
		void getWorldLightInfo(LightInfo& lightInfo);

		void getSpectators(SpectatorList& list, const Position& centerPos, bool checkforduplicate = false, bool multifloor = false,
			int32_t minRangeX = 0, int32_t maxRangeX = 0,
			int32_t minRangeY = 0, int32_t maxRangeY = 0)
			{map->getSpectators(list, centerPos, checkforduplicate, multifloor, minRangeX, maxRangeX, minRangeY, maxRangeY);}
		const SpectatorList& getSpectators(const Position& centerPos) {return map->getSpectators(centerPos);}
		void clearSpectatorCache() {if(map) map->clearSpectatorCache();}

		ReturnValue internalMoveCreature(Creature* creature, Direction direction, uint32_t flags = 0);

		ReturnValue internalMoveItem(const CreatureP& actor, Cylinder* fromCylinder, Cylinder* toCylinder, int32_t index,
			const ItemP& item, uint32_t count, ItemP* _moveItem, uint32_t flags = 0);

		ReturnValue internalAddItem(Creature* actor, Cylinder* toCylinder, Item* item, int32_t index = INDEX_WHEREEVER,
			uint32_t flags = 0, bool test = false);
		ReturnValue internalRemoveItem(Creature* actor, Item* item, int32_t count = -1,  bool test = false, uint32_t flags = 0);

		ReturnValue internalPlayerAddItem(Creature* actor, Player* player, Item* item, bool dropOnMap = true);

		/**
		  * Find an item of a certain type
		  * \param cylinder to search the item
		  * \param itemId is the item to remove
		  * \param subType is the extra type an item can have such as charges/fluidtype, default is -1
			* meaning it's not used
		  * \param depthSearch if true it will check child containers aswell
		  * \returns A pointer to the item to an item and nullptr if not found
		  */
		Item* findItemOfType(Cylinder* cylinder, uint16_t itemId,
			bool depthSearch = true, int32_t subType = -1);

		/**
		  * Remove item(s) of a certain type
		  * \param cylinder to remove the item(s) from
		  * \param itemId is the item to remove
		  * \param count is the amount to remove
		  * \param subType is the extra type an item can have such as charges/fluidtype, default is -1
			* meaning it's not used
		  * \returns true if the removal was successful
		  */
		bool removeItemOfType(Cylinder* cylinder, uint16_t itemId, int32_t count, int32_t subType = -1, bool includeSlots = true);

		/**
		  * Get the amount of money in a a cylinder
		  * \returns the amount of money found
		  */
		uint64_t getMoney(const Cylinder* cylinder);

		/**
		  * Remove/Add item(s) with a monetary value
		  * \param cylinder to remove the money from
		  * \param money is the amount to remove
		  * \param flags optional flags to modifiy the default behaviour
		  * \returns true if the removal was successful
		  */
		bool removeMoney(Cylinder* cylinder, uint64_t money, uint32_t flags = 0);

		/**
		  * Add item(s) with monetary value
		  * \param cylinder which will receive money
		  * \param money the amount to give
		  * \param flags optional flags to modify default behavior
		  */
		void addMoney(Cylinder* cylinder, uint64_t money, uint32_t flags = 0);

		/**
		  * Transform one item to another type/count
		  * \param item is the item to transform
		  * \param newId is the new itemid
		  * \param newCount is the new count value, use default value (-1) to not change it
		  * \returns true if the tranformation was successful
		  */
		ItemP transformItem(const ItemP& item, uint16_t newId, int32_t newCount = -1);

		/**
		  * Teleports an object to another position
		  * \param thing is the object to teleport
		  * \param newPos is the new position
		  * \param flags optional flags to modify default behavior
		  * \returns true if the teleportation was successful
		  */
		ReturnValue internalTeleport(Thing* thing, const Position& newPos, bool pushMove, uint32_t flags = 0);

		/**
			* Turn a creature to a different direction.
			* \param creature Creature to change the direction
			* \param dir Direction to turn to
			*/
		bool internalCreatureTurn(Creature* creature, Direction dir);

		/**
		  * Creature wants to say something.
		  * \param creature Creature pointer
		  * \param type Type of message
		  * \param text The text to say
		  * \param ghostMode Is creature on ghost mode
		  * \param spectators Send message only to creatures pointed in vector
		  * \param pos Appear as sent from different position
		  */
		bool internalCreatureSay(Creature* creature, SpeakClasses type, const std::string& text,
			bool ghostMode, SpectatorList* spectators = nullptr, Position* pos = nullptr);

		bool internalStartTrade(Player* player, Player* partner, Item* tradeItem);
		bool internalCloseTrade(Player* player);

		//Implementation of player invoked events
		bool playerBroadcastMessage(Player* player, SpeakClasses type, const std::string& text);
		bool playerReportBug(uint32_t playerId, std::string bug);
		bool playerViolationWindow(uint32_t playerId, std::string name, uint8_t reason,
			ViolationAction_t action, std::string comment, std::string statement,
			uint32_t statementId, bool ipBanishment);
		bool playerMoveThing(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination, uint8_t count);
		bool playerMoveCreature(uint32_t playerId, uint32_t movingCreatureId,
			const Position& movingCreatureOrigPos, const Position& toPos);
		bool playerMoveItem(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination, uint8_t count);
		bool playerMove(uint32_t playerId, Direction dir);
		bool playerCreatePrivateChannel(uint32_t playerId);
		bool playerChannelInvite(uint32_t playerId, const std::string& name);
		bool playerChannelExclude(uint32_t playerId, const std::string& name);
		bool playerRequestChannels(uint32_t playerId);
		bool playerOpenChannel(uint32_t playerId, uint16_t channelId);
		bool playerCloseChannel(uint32_t playerId, uint16_t channelId);
		bool playerOpenPrivateChannel(uint32_t playerId, std::string& receiver);
		bool playerCloseNpcChannel(uint32_t playerId);
		bool playerProcessRuleViolation(uint32_t playerId, const std::string& name);
		bool playerCloseRuleViolation(uint32_t playerId, const std::string& name);
		bool playerCancelRuleViolation(uint32_t playerId);
		bool playerReceivePing(uint32_t playerId);
		bool playerAutoWalk(uint32_t playerId, std::deque<Direction>& route);
		bool playerStopAutoWalk(uint32_t playerId);
		bool playerUseItemEx(uint32_t playerId, const ExtendedPosition& origin, const ExtendedPosition& destination);
		bool playerUseItem(uint32_t playerId, const ExtendedPosition& origin, uint8_t openContainerId);
		bool playerUseBattleWindow(uint32_t playerId, const ExtendedPosition& origin, uint32_t creatureId);
		bool playerCloseContainer(uint32_t playerId, uint8_t cid);
		bool playerMoveUpContainer(uint32_t playerId, uint8_t cid);
		bool playerUpdateContainer(uint32_t playerId, uint8_t cid);
		bool playerUpdateTile(uint32_t playerId, const Position& pos);
		bool playerRotateItem(uint32_t playerId, const ExtendedPosition& position);
		bool playerWriteItem(uint32_t playerId, uint32_t windowTextId, const std::string& text);
		bool playerUpdateHouseWindow(uint32_t playerId, uint8_t listId, uint32_t windowTextId, const std::string& text);
		bool playerRequestTrade(uint32_t playerId, const ExtendedPosition& position, uint32_t tradePlayerId);
		bool playerAcceptTrade(uint32_t playerId);
		bool playerLookInTrade(uint32_t playerId, bool lookAtCounterOffer, int index);
		bool playerPurchaseItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount,
			bool ignoreCap = false, bool inBackpacks = false);
		bool playerSellItem(uint32_t playerId, uint16_t spriteId, uint8_t count, uint8_t amount);
		bool playerCloseShop(uint32_t playerId);
		bool playerLookInShop(uint32_t playerId, uint16_t spriteId, uint8_t count);
		bool playerCloseTrade(uint32_t playerId);
		bool playerSetAttackedCreature(uint32_t playerId, uint32_t creatureId);
		bool playerFollowCreature(uint32_t playerId, uint32_t creatureId);
		bool playerCancelAttackAndFollow(uint32_t playerId);
		bool playerSetFightModes(uint32_t playerId, fightMode_t fightMode, chaseMode_t chaseMode, secureMode_t secureMode);
		bool playerLookAt(uint32_t playerId, const ExtendedPosition& position);
		bool playerQuests(uint32_t playerId);
		bool playerQuestInfo(uint32_t playerId, uint16_t questId);
		bool playerRequestAddVip(uint32_t playerId, const std::string& name);
		bool playerRequestRemoveVip(uint32_t playerId, uint32_t guid);
		bool playerTurn(uint32_t playerId, Direction dir);
		bool playerRequestOutfit(uint32_t playerId);
		bool playerSay(uint32_t playerId, uint16_t channelId, SpeakClasses type,
			const std::string& receiver, const std::string& text);
		bool playerChangeOutfit(uint32_t playerId, Outfit_t outfit);
		bool playerInviteToParty(uint32_t playerId, uint32_t invitedId);
		bool playerJoinParty(uint32_t playerId, uint32_t leaderId);
		bool playerRevokePartyInvitation(uint32_t playerId, uint32_t invitedId);
		bool playerPassPartyLeadership(uint32_t playerId, uint32_t newLeaderId);
		bool playerLeaveParty(uint32_t playerId);
		bool playerSharePartyExperience(uint32_t playerId, bool activate, uint8_t unknown);

		void kickPlayer(uint32_t playerId, bool displayEffect);
		bool broadcastMessage(const std::string& text, MessageClasses type);
		void showHotkeyUseMessage(Player* player, Item* item);

		int32_t getMotdId();
		void loadMotd();
		void loadPlayersRecord();
		void checkPlayersRecord(Player* player);

		bool reloadInfo(ReloadInfo_t reload, uint32_t playerId = 0);
		void cleanup();
		void shutdown();
		void autorelease(boost::intrusive_ptr<ReferenceCounted> object);
		void autorelease(ReferenceCounted* object);

		bool canThrowObjectTo(const Position& fromPos, const Position& toPos, bool checkLineOfSight = true,
			int32_t rangex = Map::maxClientViewportX, int32_t rangey = Map::maxClientViewportY);
		bool isSightClear(const Position& fromPos, const Position& toPos, bool sameFloor);

		bool getPathTo(const Creature* creature, const Position& destPos,
			std::deque<Direction>& route, int32_t maxSearchDist /*= -1*/);

		bool getPathToEx(const Creature* creature, const Position& targetPos, std::deque<Direction>& route,
			const FindPathParams& fpp);

		bool getPathToEx(const Creature* creature, const Position& targetPos, std::deque<Direction>& route,
			uint32_t minTargetDist, uint32_t maxTargetDist, bool fullPathSearch = true,
			bool clearSight = true, int32_t maxSearchDist = -1);

		Position getClosestFreeTile(Creature* creature, Position pos, bool extended = false, bool ignoreHouse = true);
		std::string getSearchString(const Position fromPos, const Position toPos, bool fromIsCreature = false, bool toIsCreature = false);

		void changeLight(const CreatureP& creature);
		void changeSpeed(Creature* creature, int32_t varSpeedDelta);
		void internalCreatureChangeOutfit(Creature* creature, const Outfit_t& oufit, bool forced = false);
		void internalCreatureChangeVisible(Creature* creature, Visible_t visible);
		void updateCreatureSkull(Creature* creature);
		void sendPublicSquare(Player* sender, SquareColor_t color);

		GameState_t getGameState() const {return gameState;}
		void setGameState(GameState_t newState);

		void saveGameState(bool shallow);
		void loadGameState();

		void refreshMap(RefreshTiles::iterator* it = nullptr, uint32_t limit = 0);
		void proceduralRefresh(RefreshTiles::iterator* it = nullptr);

		void addRefreshTile(Tile* tile, RefreshBlock_t rb) {refreshTiles[tile] = rb;}

		//Events
		void checkCreatureAttack(uint32_t creatureId);
		void checkLight();

		bool combatBlockHit(CombatType_t combatType, const CreatureP& attacker, const CreatureP& target,
			int32_t& healthChange, bool checkDefense, bool checkArmor);

		bool combatChangeHealth(CombatType_t combatType, const CreatureP& attacker, const CreatureP& target, int32_t healthChange,
			MagicEffect_t hitEffect = MAGIC_EFFECT_UNKNOWN, TextColor_t hitColor = TEXTCOLOR_UNKNOWN, bool force = false);
		bool combatChangeMana(const CreatureP& attacker, const CreatureP& target, int32_t manaChange);

		//animation help functions
		void addCreatureHealth(const CreatureP& target);
		void addCreatureHealth(const SpectatorList& list, const CreatureP& target);
		void addAnimatedText(const Position& pos, uint8_t textColor, const std::string& text);
		void addAnimatedText(const SpectatorList& list, const Position& pos, uint8_t textColor, const std::string& text);
		void addMagicEffect(const Position& pos, uint8_t effect, bool ghostMode = false);
		void addMagicEffect(const SpectatorList& list, const Position& pos, uint8_t effect, bool ghostMode = false);
		void addDistanceEffect(const SpectatorList& list, const Position& fromPos, const Position& toPos, uint8_t effect);
		void addDistanceEffect(const Position& fromPos, const Position& toPos, uint8_t effect);

		const RuleViolationsMap& getRuleViolations() const {return ruleViolations;}
		bool cancelRuleViolation(Player* player);
		bool closeRuleViolation(Player* player);

		std::vector<std::string> blacklist;
		bool fetchBlacklist();

		bool loadExperienceStages();
		double getExperienceStage(uint32_t level, double divider = 1.);

		inline StageList::const_iterator getFirstStage() const {return stages.begin();}
		inline StageList::const_iterator getLastStage() const {return stages.end();}
		size_t getStagesCount() const {return stages.size();}

		void setGlobalSaveMessage(int16_t key, bool value) {globalSaveMessage[key] = value;}
		bool getGlobalSaveMessage(int16_t key) const {return globalSaveMessage[key];}

		Map* getMap();
		const Map* getMap() const;

		int32_t getLightHour() {return lightHour;}
		void startDecay(Item* item);

	private:
		bool playerWhisper(Player* player, const std::string& text);
		bool playerYell(Player* player, const std::string& text);
		bool playerSpeakTo(Player* player, SpeakClasses type, const std::string& receiver, const std::string& text);
		bool playerTalkToChannel(Player* player, SpeakClasses type, const std::string& text, uint16_t channelId);
		bool playerSpeakToNpc(Player* player, const std::string& text);
		bool playerReportRuleViolation(Player* player, const std::string& text);
		bool playerContinueReport(Player* player, const std::string& text);

		struct GameEvent
		{
			int64_t tick;
			int32_t type;
			void* data;
		};

		LOGGER_DECLARATION;

		std::vector<boost::intrusive_ptr<ReferenceCounted>> autoreleasePool;
		TradeItemMap tradeItems;
		RuleViolationsMap ruleViolations;

		void clear();
		void checkDecay();
		void internalDecayItem(Item* item);

		ItemList decayItems[EVENT_DECAYBUCKETS];
		ItemList toDecayItems;
		size_t lastBucket;

		static const int32_t LIGHT_LEVEL_DAY = 250;
		static const int32_t LIGHT_LEVEL_NIGHT = 40;
		static const int32_t SUNSET = 1305;
		static const int32_t SUNRISE = 430;
		int32_t lightLevel, lightHour, lightHourDelta;
		LightState_t lightState;

		GameState_t gameState;
		WorldType_t worldType;

		ServiceManager* services;
		Map* map;

		std::string lastMotd;
		int32_t lastMotdId;
		uint32_t playersRecord;
		uint32_t checkLightEvent, checkDecayEvent, saveEvent;
		bool globalSaveMessage[3];

		RefreshTiles refreshTiles;

		StageList stages;
		uint32_t lastStageLevel;

		Highscore highscoreStorage[9];
		time_t lastHighscoreCheck;
};

#endif // _GAME_H
