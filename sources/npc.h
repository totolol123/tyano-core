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

#ifndef _NPC_H
#define _NPC_H

#include "const.h"
#include "creature.h"
#include "luascript.h"

class  Npc;
class  NpcResponse;
struct NpcState;
class  Player;
class  Tile;

typedef std::list<Npc*> NpcList;


class Npcs
{
	public:
		void reload();


	private:

		LOGGER_DECLARATION;
};

class NpcScriptInterface : public LuaScriptInterface
{
	public:
		NpcScriptInterface();
		virtual ~NpcScriptInterface();

		bool loadNpcLib(std::string file);

		static void pushState(lua_State *L, NpcState* state);
		static void popState(lua_State *L, NpcState* &state);

	protected:
		virtual void registerFunctions();
		static int32_t luaActionFocus(lua_State* L);
		static int32_t luaActionSay(lua_State* L);

		static int32_t luaActionTurn(lua_State* L);
		static int32_t luaActionMove(lua_State* L);
		static int32_t luaActionMoveTo(lua_State* L);
		static int32_t luaActionFollow(lua_State* L);

		static int32_t luaGetNpcId(lua_State* L);
		static int32_t luaGetNpcDistanceTo(lua_State* L);
		static int32_t luaGetNpcParameter(lua_State* L);

		static int32_t luaGetNpcState(lua_State* L);
		static int32_t luaSetNpcState(lua_State* L);

		static int32_t luaOpenShopWindow(lua_State* L);
		static int32_t luaCloseShopWindow(lua_State* L);

	private:

		LOGGER_DECLARATION;

		bool m_libLoaded;

		virtual bool initState();
		virtual bool closeState();
};

class NpcEventsHandler
{
	public:
		NpcEventsHandler(Npc* npc);
		virtual ~NpcEventsHandler() {}

		virtual void onCreatureAppear(const Creature* creature) {}
		virtual void onCreatureDisappear(const Creature* creature) {}

		virtual void onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos) {}
		virtual void onCreatureSay(const Creature* creature, SpeakClasses, const std::string& text, Position* pos = nullptr) {}

		virtual void onPlayerTrade(const Player* player, int32_t callback, uint16_t itemid,
			uint8_t count, uint8_t amount, bool ignoreCap, bool inBackpacks) {}
		virtual void onPlayerEndTrade(const Player* player) {}
		virtual void onPlayerCloseChannel(const Player* player) {}

		virtual void onThink() {}
		bool isLoaded();

	protected:
		Npc* m_npc;
		bool m_loaded;
};

class NpcScript : public NpcEventsHandler
{
	public:
		NpcScript(std::string file, Npc* npc);
		virtual ~NpcScript() {}

		virtual void onCreatureAppear(const Creature* creature);
		virtual void onCreatureDisappear(const Creature* creature);

		virtual void onCreatureMove(const Creature* creature, const Position& oldPos, const Position& newPos);
		virtual void onCreatureSay(const Creature* creature, SpeakClasses, const std::string& text, Position* pos = nullptr);

		virtual void onPlayerTrade(const Player* player, int32_t callback, uint16_t itemid,
			uint8_t count, uint8_t amount, bool ignoreCap, bool inBackpacks);
		virtual void onPlayerEndTrade(const Player* player);
		virtual void onPlayerCloseChannel(const Player* player);

		virtual void onThink();

	private:

		LOGGER_DECLARATION;

		NpcScriptInterface* m_interface;
		int32_t m_onCreatureAppear, m_onCreatureDisappear, m_onCreatureMove, m_onCreatureSay,
			m_onPlayerCloseChannel, m_onPlayerEndTrade, m_onThink;
};


struct ResponseAction
{
	public:
		ResponseAction()
		{
			actionType = ACTION_NONE;
			key = intValue = 0;
			strValue = "";
			pos = Position();
		}

		ReponseActionParam_t actionType;
		int32_t key, intValue;
		std::string strValue;
		Position pos;
};

struct ListItem
{
	ListItem()
	{
		itemId = 0;
		subType = sellPrice = buyPrice = -1;
		keywords = name = pluralName = "";
	}

	int32_t sellPrice, buyPrice, itemId, subType;
	std::string keywords, name, pluralName;
};

struct ScriptVars
{
	ScriptVars()
	{
		b1 = b2 = b3 = false;
		n1 = n2 = n3 = -1;
		s1 = s2 = s3 = "";
	}

	bool b1, b2, b3;
	int32_t n1, n2, n3;
	std::string s1, s2, s3;
};

typedef std::list<ResponseAction> ActionList;
typedef std::list<NpcResponse*> ResponseList;
typedef std::map<std::string, int32_t> ResponseScriptMap;

class NpcResponse
{
	public:
		struct ResponseProperties
		{
			ResponseProperties()
			{
				topic = amount = focusStatus = -1;
				output = "";
				interactType = INTERACT_TEXT;
				responseType = RESPONSE_DEFAULT;
				params = 0;
				storageId = -1;
				storageComp = STORAGE_EQUAL;
				knowSpell = "";
				publicize = true;
			}

			bool publicize;
			InteractType_t interactType;
			ResponseType_t responseType;
			StorageComparision_t storageComp;
			int32_t topic, amount, focusStatus, storageId;
			uint32_t params;
			std::string output, knowSpell, storageValue;
			ActionList actionList;
			std::list<std::string> inputList;
			std::list<ListItem> itemList;
		};

		NpcResponse(const ResponseProperties& _prop,
			ResponseList _subResponseList,
			ScriptVars _scriptVars)
		{
			prop = _prop;
			subResponseList = _subResponseList;
			scriptVars = _scriptVars;
		}

		NpcResponse(NpcResponse& rhs)
		{
			prop = rhs.prop;
			scriptVars = rhs.scriptVars;
			for(ResponseList::iterator it = rhs.subResponseList.begin(); it != rhs.subResponseList.end(); ++it)
			{
				NpcResponse* response = new NpcResponse(*(*it));
				subResponseList.push_back(response);
			}
		}

		~NpcResponse()
		{
			for(ResponseList::iterator it = subResponseList.begin(); it != subResponseList.end(); ++it)
				delete *it;

			subResponseList.clear();
		}

		uint32_t getParams() const {return prop.params;}
		std::string getInputText() const {return (prop.inputList.empty() ? "" : *prop.inputList.begin());}
		int32_t getTopic() const {return prop.topic;}
		int32_t getFocusState() const {return prop.focusStatus;}
		int32_t getStorageId() const {return prop.storageId;}
		std::string getStorage() const {return prop.storageValue;}
		ResponseType_t getResponseType() const {return prop.responseType;}
		InteractType_t getInteractType() const {return prop.interactType;}
		StorageComparision_t getStorageComp() const {return prop.storageComp;}
		const std::string& getKnowSpell() const {return prop.knowSpell;}
		const std::string& getText() const {return prop.output;}
		int32_t getAmount() const {return prop.amount;}
		void setAmount(int32_t _amount) {prop.amount = _amount;}
		bool publicize() const {return prop.publicize;}

		std::string formatResponseString(Creature* creature) const;
		void addAction(ResponseAction action) {prop.actionList.push_back(action);}
		const std::list<std::string>& getInputList() const {return prop.inputList;}

		void setResponseList(ResponseList _list) {subResponseList.insert(subResponseList.end(),_list.begin(),_list.end());}
		const ResponseList& getResponseList() const {return subResponseList;}

		ActionList::const_iterator getFirstAction() const {return prop.actionList.begin();}
		ActionList::const_iterator getEndAction() const {return prop.actionList.end();}

		ResponseProperties prop;
		ResponseList subResponseList;
		ScriptVars scriptVars;
};

struct NpcState
{
	bool isIdle, isQueued, ignoreCap, inBackpacks;
	int32_t topic, price, sellPrice, buyPrice, amount, itemId, subType, level;
	uint32_t respondToCreature;
	uint64_t prevInteraction;
	std::string spellName, listName, listPluralName, respondToText, prevRespondToText;
	const NpcResponse* lastResponse;
	ScriptVars scriptVars;
	//Do not forget to update pushState/popState if you add more variables
};

struct Voice
{
	bool randomSpectator;
	SpeakClasses type;
	uint32_t interval, margin;
	std::string text;
};

#define MAX_RAND_RANGE 10000000



class Npc : public Creature {

public:

	virtual bool       canAttack      (const Creature& creature) const;
	virtual bool       canMoveTo      (const Tile& tile) const;
	virtual CreatureP  getDirectOwner ();
	virtual CreaturePC getDirectOwner () const;
	virtual uint32_t   getMoveFlags   () const;
	virtual bool       isEnemy        (const Creature& creature) const;


protected:

	virtual Duration  getWanderingInterval    () const;
	virtual bool      hasToThinkAboutCreature (const CreaturePC& creature) const;
	virtual void      onThinkingStarted       ();


private:

	friend class Npcs;
	friend class NpcScriptInterface;




	public:
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
		static uint32_t npcCount;
#endif
		virtual ~Npc();
		static Npc* createNpc(const std::string& name);

		virtual Npc* getNpc() {return this;}
		virtual const Npc* getNpc() const {return this;}

		virtual uint32_t rangeId() {return 0x80000000;}
		static AutoList<Npc> autoList;

		void addList() {autoList[id] = this;}
		void removeList() {autoList.erase(id);}

		static void clear();

		virtual bool isPushable() const {return false;}
		virtual bool isAttackable() const {return attackable;}
		virtual bool isWalkable() const {return walkable;}

		virtual bool canSee(const Position& pos) const;

		bool isLoaded() {return loaded;}
		bool load();
		void reload();

		virtual const std::string& getName() const {return name;}
		virtual const std::string& getNameDescription() const {return nameDescription;}

		void doSay(const std::string& text, SpeakClasses type, Player* player);
		void doTurn(Direction dir);
		void doMove(Direction dir);
		void doMoveTo(Position pos);

		void onPlayerTrade(Player* player, ShopEvent_t type, int32_t callback, uint16_t itemId, uint8_t count,
			uint8_t amount, bool ignoreCap = false, bool inBackpacks = false);
		void onPlayerEndTrade(Player* player, int32_t buyCallback,
			int32_t sellCallback);
		void onPlayerCloseChannel(const Player* player);

		void setCreatureFocus(Creature* creature);
		NpcScriptInterface* getInterface();

	protected:
		Npc(const std::string& _name);
		bool loaded;

		virtual void onCreatureAppear(const CreatureP& creature);
		virtual void onCreatureDisappear(const Creature* creature);
		virtual void onCreatureMove(const CreatureP& creature, const Position& origin, Tile* originTile, const Position& destination, Tile* destinationTile, bool teleport);
		virtual void onCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text, Position* pos = nullptr);
		virtual void onThink(Duration interval);

		bool isImmune(CombatType_t type) const {return true;}
		bool isImmune(ConditionType_t type) const {return true;}

		virtual std::string getDescription(int32_t lookDistance) const {return nameDescription + ".";}

		void reset();
		bool loadFromXml(const std::string& name);
		bool canWalkTo(const Position& fromPos, Direction dir);

		const NpcResponse* getResponse(const ResponseList& list, const Player* player,
			NpcState* npcState, const std::string& text, bool exactMatch = false);
		const NpcResponse* getResponse(const Player* player, NpcState* npcState, const std::string& text);
		const NpcResponse* getResponse(const Player* player, NpcEvent_t eventType);
		const NpcResponse* getResponse(const Player* player, NpcState* npcState, NpcEvent_t eventType);
		std::string getEventResponseName(NpcEvent_t eventType);

		NpcState* getState(const Player* player, bool makeNew = true);
		uint32_t getMatchCount(NpcResponse* response, StringVector wordList,
			bool exactMatch, int32_t& matchAllCount, int32_t& totalKeywordCount);
		uint32_t getListItemPrice(uint16_t itemId, ShopEvent_t type);

		std::string formatResponse(Creature* creature, const NpcState* npcState, const NpcResponse* response) const;
		void executeResponse(Player* player, NpcState* npcState, const NpcResponse* response);

		void onPlayerEnter(Player* player, NpcState* state);
		void onPlayerLeave(Player* player, NpcState* state);

		typedef std::map<std::string, std::string> ParametersMap;
		ParametersMap m_parameters;

		uint32_t loadParams(xmlNodePtr node);
		ResponseList loadInteraction(xmlNodePtr node);

		void addShopPlayer(Player* player);
		void removeShopPlayer(const Player* player);
		void closeAllShopWindows();


		LOGGER_DECLARATION;

		uint32_t walkTicks;
		std::string name, nameDescription, m_filename;
		int32_t talkRadius, idleTime, idleInterval, focusCreature;
		bool floorChange, attackable, walkable, isIdle, hasBusyReply, hasScriptedFocus, defaultPublic;
		int64_t lastVoice;

		typedef std::list<Player*> ShopPlayerList;
		ShopPlayerList shopPlayerList;

		typedef std::list<NpcState*> StateList;
		StateList stateList;

		typedef std::list<uint32_t> QueueList;
		QueueList queueList;

		typedef std::list<Voice> VoiceList;
		VoiceList voiceList;

		typedef std::map<std::string, std::list<ListItem> > ItemListMap;
		ItemListMap itemListMap;

		ResponseScriptMap responseScriptMap;
		ResponseList responseList;

		NpcEventsHandler* m_npcEventHandler;
		static NpcScriptInterface* m_interface;

};

#endif // _NPC_H
