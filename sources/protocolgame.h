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

#ifndef _PROTOCOLGAME_H
#define _PROTOCOLGAME_H

#include "const.h"
#include "position.h"
#include "protocol.h"

class Connection;
class Container;
class Creature;
class Game;
class House;
class Item;
class NetworkMessage;
class Player;
class Quest;
class Tile;

typedef std::shared_ptr<NetworkMessage> NetworkMessage_ptr;


class ProtocolGame : public Protocol
{
	public:

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
		static uint32_t protocolGameCount;
#endif
		ProtocolGame(const Connection_ptr& connection);
		virtual ~ProtocolGame();

		enum {protocolId = 0x0A};
		enum {isSingleSocket = true};
		enum {hasChecksum = true};
		static const char* protocolName() {return "game protocol";}

		bool login(const std::string& name, uint32_t id, const std::string& password,
			OperatingSystem_t operatingSystem, uint16_t version, bool gamemaster);
		bool logout(bool displayEffect, bool forceLogout);

		void setPlayer(Player* p);

	private:

		struct CreatureValidationResult {

			CreatureValidationResult();
			CreatureValidationResult(bool registered, const StackPosition& expectedPosition, bool expectedPositionIsWrong);

			StackPosition expectedPosition;
			bool       expectedPositionIsWrong;
			bool       registered;

		};


		typedef boost::intrusive_ptr<const Creature>       CreatureP;
		typedef boost::intrusive_ptr<Player>               PlayerP;
		typedef std::unordered_map<CreatureP,StackPosition>  RegisteredCreatures;


		void disconnectClient(uint8_t error, const char* message);

		bool connect(uint32_t playerId, OperatingSystem_t operatingSystem, uint16_t version);
		void disconnect();

		virtual void releaseProtocol();
		virtual void deleteProtocolTask();

		bool canSee(uint16_t x, uint16_t y, uint16_t z) const;
		bool canSee(const Creature*) const;
		bool canSee(const Position& pos) const;

		virtual void onConnect();
		virtual void onRecvFirstMessage(NetworkMessage& msg);

		bool parseFirstPacket(NetworkMessage& msg);
		virtual void parsePacket(NetworkMessage& msg);

		//Parse methods
		void parseLogout(NetworkMessage& msg);
		void parseCancelMove(NetworkMessage& msg);

		void parseReceivePing(NetworkMessage& msg);
		void parseAutoWalk(NetworkMessage& msg);
		void parseMove(NetworkMessage& msg, Direction dir);
		void parseTurn(NetworkMessage& msg, Direction dir);

		void parseRequestOutfit(NetworkMessage& msg);
		void parseSetOutfit(NetworkMessage& msg);
		void parseSay(NetworkMessage& msg);
		void parseLookAt(NetworkMessage& msg);
		void parseFightModes(NetworkMessage& msg);
		void parseAttack(NetworkMessage& msg);
		void parseFollow(NetworkMessage& msg);

		void parseBugReport(NetworkMessage& msg);
		void parseDebugAssert(NetworkMessage& msg);

		void parseThrow(NetworkMessage& msg);
		void parseUseItemEx(NetworkMessage& msg);
		void parseBattleWindow(NetworkMessage& msg);
		void parseUseItem(NetworkMessage& msg);
		void parseCloseContainer(NetworkMessage& msg);
		void parseUpArrowContainer(NetworkMessage& msg);
		void parseUpdateTile(NetworkMessage& msg);
		void parseUpdateContainer(NetworkMessage& msg);
		void parseTextWindow(NetworkMessage& msg);
		void parseHouseWindow(NetworkMessage& msg);

		void parseLookInShop(NetworkMessage& msg);
		void parsePlayerPurchase(NetworkMessage& msg);
		void parsePlayerSale(NetworkMessage& msg);
		void parseCloseShop(NetworkMessage& msg);

		void parseQuests(NetworkMessage& msg);
		void parseQuestInfo(NetworkMessage& msg);

		void parseInviteToParty(NetworkMessage& msg);
		void parseJoinParty(NetworkMessage& msg);
		void parseRevokePartyInvite(NetworkMessage& msg);
		void parsePassPartyLeadership(NetworkMessage& msg);
		void parseLeaveParty(NetworkMessage& msg);
		void parseSharePartyExperience(NetworkMessage& msg);

		//trade methods
		void parseRequestTrade(NetworkMessage& msg);
		void parseLookInTrade(NetworkMessage& msg);
		void parseAcceptTrade(NetworkMessage& msg);
		void parseCloseTrade();

		//VIP methods
		void parseAddVip(NetworkMessage& msg);
		void parseRemoveVip(NetworkMessage& msg);

		void parseRotateItem(NetworkMessage& msg);

		//Channel tabs
		void parseCreatePrivateChannel(NetworkMessage& msg);
		void parseChannelInvite(NetworkMessage& msg);
		void parseChannelExclude(NetworkMessage& msg);
		void parseGetChannels(NetworkMessage& msg);
		void parseOpenChannel(NetworkMessage& msg);
		void parseOpenPriv(NetworkMessage& msg);
		void parseCloseChannel(NetworkMessage& msg);
		void parseCloseNpc(NetworkMessage& msg);
		void parseProcessRuleViolation(NetworkMessage& msg);
		void parseCloseRuleViolation(NetworkMessage& msg);
		void parseCancelRuleViolation(NetworkMessage& msg);

		//Send functions
		void sendChannelMessage(std::string author, std::string text, SpeakClasses type, uint8_t channel);
		void sendClosePrivate(uint16_t channelId);
		void sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName);
		void sendChannelsDialog();
		void sendChannel(uint16_t channelId, const std::string& channelName);
		void sendRuleViolationsChannel(uint16_t channelId);
		void sendOpenPrivateChannel(const std::string& receiver);
		void sendToChannel(const Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId, uint32_t time = 0);
		void sendRemoveReport(const std::string& name);
		void sendLockRuleViolation();
		void sendRuleViolationCancel(const std::string& name);
		void sendIcons(int32_t icons);
		void sendFYIBox(const std::string& message);

		void sendDistanceShoot(const Position& from, const Position& to, uint8_t type);
		void sendMagicEffect(const Position& pos, uint8_t type);
		void sendAnimatedText(const Position& pos, uint8_t color, std::string text);
		void sendCreatureHealth(const CreatureP& creature);
		void sendSkills();
		void sendPing();
		void sendCreatureTurn(const CreatureP& creature, const StackPosition& position);
		void sendCreatureSay(const CreatureP& creature, SpeakClasses type, const std::string& text, Position* pos = nullptr);

		void sendCancel(const std::string& message);
		void sendCancelWalk();
		void sendChangeSpeed(const CreatureP& creature, uint32_t speed);
		void sendCancelTarget();
		void sendCreatureOutfit(const CreatureP& creature, const Outfit_t& outfit);
		void sendStats();
		void sendTextMessage(MessageClasses mclass, const std::string& message);
		void sendReLoginWindow();

		void sendTutorial(uint8_t tutorialId);
		void sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc);

		void sendCreatureSkull(const CreatureP& creature);
		void sendCreatureShield(const CreatureP& creature);

		void sendShop(const ShopInfoList& shop);
		void sendCloseShop();
		void sendGoods(const ShopInfoList& shop);
		void sendTradeItemRequest(const Player* player, const Item* item, bool ack);
		void sendCloseTrade();

		void sendTextWindow(uint32_t windowTextId, Item* item, uint16_t maxLen, bool canWrite);
		void sendTextWindow(uint32_t windowTextId, uint32_t itemId, const std::string& text);
		void sendHouseWindow(uint32_t windowTextId, House* house, uint32_t listId, const std::string& text);

		void sendOutfitWindow();
		void sendQuests();
		void sendQuestInfo(Quest* quest);

		void sendVIPLogIn(uint32_t guid);
		void sendVIPLogOut(uint32_t guid);
		void sendVIP(uint32_t guid, const std::string& name, bool isOnline);

		void sendCreatureLight(const CreatureP& creature);
		void sendWorldLight(const LightInfo& lightInfo);

		void sendCreatureSquare(const CreatureP& creature, SquareColor_t color);

		//tiles
		void sendAddTileItem(const Tile* tile, const StackPosition& position, const Item* item, const char* callSource);
		void sendUpdateTileItem(const Tile* tile, const StackPosition& position, const Item* item);
		void sendRemoveTileItem(const Tile* tile, const StackPosition& position, const Item* item, const char* callSource);
		void sendUpdateTile(const Tile* tile, const Position& pos);

		void sendAddCreature(const CreatureP& creature, const StackPosition& position, const char* callSource);
		void sendRemoveCreature(const CreatureP& creature, StackPosition position, const char* callSource);
		void sendMoveCreature(const CreatureP& creature, const Tile* newTile, const StackPosition& newPosition, const Tile* oldTile, StackPosition oldPosition, bool teleport, const char* callSource);

		//containers
		void sendAddContainerItem(uint8_t cid, const Item* item);
		void sendUpdateContainerItem(uint8_t cid, uint8_t slot, const Item* item);
		void sendRemoveContainerItem(uint8_t cid, uint8_t slot);

		void sendContainer(uint32_t cid, const Container* container, bool hasParent);
		void sendCloseContainer(uint32_t cid);

		//inventory
		void sendAddInventoryItem(slots_t slot, const Item* item);
		void sendUpdateInventoryItem(slots_t slot, const Item* item);
		void sendRemoveInventoryItem(slots_t slot);

		//Help functions

		// translate a tile to clientreadable format
		void GetTileDescription(const Tile* tile, NetworkMessage_ptr msg);

		// translate a floor to clientreadable format
		void GetFloorDescription(NetworkMessage_ptr msg, int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, int32_t offset, int32_t& skip);

		// translate a map area to clientreadable format
		void GetMapDescription(int32_t x, int32_t y, int32_t z,
			int32_t width, int32_t height, NetworkMessage_ptr msg);

		void AddMapDescription(NetworkMessage_ptr msg, const Position& pos);
		void AddTextMessage(NetworkMessage_ptr msg, MessageClasses mclass, const std::string& message);
		void AddAnimatedText(NetworkMessage_ptr msg, const Position& pos, uint8_t color, const std::string& text);
		void AddMagicEffect(NetworkMessage_ptr msg, const Position& pos, uint8_t type);
		void AddDistanceShoot(NetworkMessage_ptr msg, const Position& from, const Position& to, uint8_t type);
		void AddCreature(NetworkMessage_ptr msg, const CreatureP& creature, const StackPosition& position);
		void AddPlayerStats(NetworkMessage_ptr msg);
		void AddCreatureSpeak(NetworkMessage_ptr msg, const Creature* creature, SpeakClasses type,
			std::string text, uint16_t channelId, uint32_t time = 0, Position* pos = nullptr);
		void AddCreatureHealth(NetworkMessage_ptr msg, const Creature* creature);
		void AddCreatureOutfit(NetworkMessage_ptr msg, const Creature* creature, const Outfit_t& outfit, bool outfitWindow = false);
		void AddPlayerSkills(NetworkMessage_ptr msg);
		void AddWorldLight(NetworkMessage_ptr msg, const LightInfo& lightInfo);
		void AddCreatureLight(NetworkMessage_ptr msg, const Creature* creature);

		//tiles
		void AddTileItem(NetworkMessage_ptr msg, const StackPosition& position, const Item* item);
		void AddTileCreature(NetworkMessage_ptr msg, const StackPosition& position, const CreatureP& creature);
		void UpdateTileItem(NetworkMessage_ptr msg, const StackPosition& position, const Item* item);
		void RemoveTileItem(NetworkMessage_ptr msg, const StackPosition& position);
		void RemoveTileCreature(NetworkMessage_ptr msg, const StackPosition& position);

		void MoveUpCreature(NetworkMessage_ptr msg, const Creature* creature,
			const Position& newPos, const Position& oldPos, uint32_t oldStackpos);
		void MoveDownCreature(NetworkMessage_ptr msg, const Creature* creature,
			const Position& newPos, const Position& oldPos, uint32_t oldStackpos);

		//container
		void AddContainerItem(NetworkMessage_ptr msg, uint8_t cid, const Item* item);
		void UpdateContainerItem(NetworkMessage_ptr msg, uint8_t cid, uint8_t slot, const Item* item);
		void RemoveContainerItem(NetworkMessage_ptr msg, uint8_t cid, uint8_t slot);

		//inventory
		void AddInventoryItem(NetworkMessage_ptr msg, slots_t slot, const Item* item);
		void UpdateInventoryItem(NetworkMessage_ptr msg, slots_t slot, const Item* item);
		void RemoveInventoryItem(NetworkMessage_ptr msg, slots_t slot);

		//rule violation window
		void parseViolationWindow(NetworkMessage& msg);

		//shop
		void AddShopItem(NetworkMessage_ptr msg, const ShopInfo item);

		#define addGameTask(f, ...) addGameTaskInternal(0, std::bind(f, &server.game(), __VA_ARGS__))
		#define addGameTaskTimed(delay, f, ...) addGameTaskInternal(delay, std::bind(f, &server.game(), __VA_ARGS__))
		template<class FunctionType>
		void addGameTaskInternal(uint32_t delay, const FunctionType&);

		void                     correctRegisteredCreature     (const CreatureP& creature, const StackPosition& previousPosition, const StackPosition& newPosition, const CreatureValidationResult& validationResult);
		bool                     invalidateCreature            (const CreatureP& creature);
		void                     invalidateCreaturesAtPosition (const Position& position);
		void                     invalidateDistantCreatures    ();
		bool                     registerCreature              (const CreatureP& creature, const StackPosition& position, uint32_t& removedCreatureId);
		void                     updateCreaturesAtPosition     (const Position& position);
		bool                     updateRegisteredCreature      (const CreatureP& creature, const StackPosition& position);
		CreatureValidationResult validateRegisteredCreature    (const CreatureP& creature, const StackPosition& previousPosition, const StackPosition& newPosition, bool mustExist, const char* callSource) const;


		LOGGER_DECLARATION;

		PlayerP             player;
		RegisteredCreatures _registeredCreatures;

		uint32_t m_eventConnect;
		bool m_debugAssertSent, m_acceptPackets;

		friend class Player;
};

#endif // _PROTOCOLGAME_H
