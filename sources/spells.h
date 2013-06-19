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

#ifndef _SPELLS_H
#define _SPELLS_H

#include "actions.h"
#include "talkaction.h"

class BaseSpell;
class CombatSpell;
class InstantSpell;
class ConjureSpell;
class RuneSpell;
class Spell;

typedef std::shared_ptr<BaseSpell>          BaseSpellP;
typedef std::shared_ptr<CombatSpell>        CombatSpellP;
typedef std::shared_ptr<InstantSpell>       InstantSpellP;
typedef std::map<std::string,InstantSpellP> InstantSpellMap;
typedef std::shared_ptr<RuneSpell>          RuneSpellP;
typedef std::map<uint32_t,RuneSpellP>       RuneSpellMap;
typedef std::shared_ptr<Spell>              SpellP;


class Spells : public BaseEvents<Spell> {
	public:
		Spells();

		SpellP getSpellByName(const std::string& name);

		RuneSpellP getRuneSpell(uint32_t id);
		RuneSpellP getRuneSpellByName(const std::string& name);

		InstantSpellP getInstantSpell(const std::string words);
		InstantSpellP getInstantSpellByName(const std::string& name);
		InstantSpellP getInstantSpellByIndex(const Player* player, uint32_t index);

		uint32_t getInstantSpellCount(const Player* player);
		ReturnValue onPlayerSay(Player* player, const std::string& words);
		virtual std::string getScriptBaseName() const {return "spells";}
		static Position getCasterPosition(const Creature* creature, Direction dir);

	private:
		virtual void clear();

		virtual SpellP getEvent(const std::string& nodeName);
		virtual bool registerEvent(const SpellP& event, xmlNodePtr p, bool override);

		virtual LuaScriptInterface& getInterface() {return m_interface;}


		LOGGER_DECLARATION;

		LuaScriptInterface m_interface;

		RuneSpellMap runes;
		InstantSpellMap instants;

		friend class CombatSpell;
};

typedef bool (InstantSpellFunction)(const InstantSpell* spell, Creature* creature, const std::string& param);
typedef bool (ConjureSpellFunction)(const ConjureSpell* spell, Creature* creature, const std::string& param);
typedef bool (RuneSpellFunction)(const RuneSpell* spell, Creature* creature, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination);

class BaseSpell
{
	public:
		BaseSpell() {}
		virtual ~BaseSpell() {}

		virtual bool castSpell(Creature* creature);
		virtual bool castSpell(Creature* creature, Creature* target);
};


class CombatSpell : public Event, public BaseSpell
{
	public:
		CombatSpell(Combat* _combat, bool _needTarget, bool _needDirection);
		virtual ~CombatSpell();

		virtual bool castSpell(Creature* creature);
		virtual bool castSpell(Creature* creature, Creature* target);
		virtual bool configureEvent(xmlNodePtr p) {return true;}

		//scripting
		bool executeCastSpell(Creature* creature, const LuaVariant& var);

		bool loadScriptCombat();
		Combat* getCombat() {return combat;}

	private:
		virtual std::string getScriptEventName() const {return "onCastSpell";}
		virtual std::string getScriptEventParams() const {return "cid, var";}


		LOGGER_DECLARATION;

		bool needDirection;
		bool needTarget;
		Combat* combat;
};

class Spell : public BaseSpell, virtual public EventBase
{
	protected:
		Spell();
		virtual ~Spell() {}

	public:
		bool configureSpell(xmlNodePtr xmlspell);
		const std::string& getName() const {return name;}

		void postCastSpell(Player* player, bool isFinished = true, bool payCost = true) const;
		void postCastSpell(Player* player, uint32_t manaCost, uint32_t soulCost) const;

		int32_t getManaCost(const Player* player) const;
		int32_t getSoulCost() const {return soul;}
		uint32_t getLevel() const {return level;}
		int32_t getMagicLevel() const {return magLevel;}
		int32_t getMana() const {return mana;}
		int32_t getManaPercent() const {return manaPercent;}
		uint32_t getExhaustion() const {return exhaustion;}
		bool isEnabled() const {return enabled;}
		bool isPremium() const {return premium;}

		virtual bool isInstant() const = 0;
		bool isLearnable() const {return learnable;}

		static ReturnValue CreateIllusion(Creature* creature, const Outfit_t outfit, int32_t time);
		static ReturnValue CreateIllusion(Creature* creature, const std::string& name, int32_t time);
		static ReturnValue CreateIllusion(Creature* creature, uint32_t itemId, int32_t time);

	protected:
		bool playerSpellCheck(Player* player) const;
		bool playerInstantSpellCheck(Player* player, Creature* creature);
		bool playerInstantSpellCheck(Player* player, const Position& toPos);
		bool playerRuneSpellCheck(Player* player, const ExtendedPosition& destination);

		int32_t level;
		int32_t magLevel;
		bool premium;
		bool learnable;
		bool enabled;

		int32_t mana;
		int32_t manaPercent;
		int32_t soul;
		int32_t range;
		uint32_t exhaustion;

		bool needTarget;
		bool needWeapon;
		bool blockingSolid;
		bool blockingCreature;
		bool selfTarget;
		bool isAggressive;

		VocationMap vocSpellMap;
		typedef std::vector<std::string> VocStringVec;
		VocStringVec vocStringVec;

	private:

		LOGGER_DECLARATION;

		std::string name;
};

class InstantSpell : public Spell, public TalkAction
{
	public:

		InstantSpell(LuaScriptInterface* _interface);
		virtual ~InstantSpell() {}

		virtual bool configureEvent(xmlNodePtr p);
		virtual bool loadFunction(const std::string& functionName);

		virtual bool playerCastInstant(Player* player, const std::string& param);

		virtual bool castSpell(Creature* creature);
		virtual bool castSpell(Creature* creature, Creature* target);

		//scripting
		bool executeCastSpell(Creature* creature, const LuaVariant& var);

		virtual bool isInstant() const {return true;}
		bool getHasParam() const {return hasParam;}
		bool canCast(const Player* player) const;
		bool canThrowSpell(const Creature* creature, const Creature* target) const;

	private:
		virtual std::string getScriptEventName() const {return "onCastSpell";}
		virtual std::string getScriptEventParams() const {return "cid, var";}

		static InstantSpellFunction SearchPlayer;
		static InstantSpellFunction SummonMonster;
		static InstantSpellFunction Levitate;
		static InstantSpellFunction Illusion;

		bool internalCastSpell(Creature* creature, const LuaVariant& var);


		LOGGER_DECLARATION;

		bool needDirection;
		bool hasParam;
		bool checkLineOfSight;
		bool casterTargetOrDirection;
		uint8_t limitRange;

		InstantSpellFunction* function;
};

class ConjureSpell : public InstantSpell
{
	public:
		ConjureSpell(LuaScriptInterface* _interface);
		virtual ~ConjureSpell() {}

		virtual bool configureEvent(xmlNodePtr p);
		virtual bool loadFunction(const std::string& functionName);

		virtual bool playerCastInstant(Player* player, const std::string& param);

		virtual bool castSpell(Creature* creature) {return false;}
		virtual bool castSpell(Creature* creature, Creature* target) {return false;}

		uint32_t getConjureId() const {return conjureId;}
		uint32_t getConjureCount() const {return conjureCount;}
		uint32_t getReagentId() const {return conjureReagentId;}

	private:
		virtual std::string getScriptEventName() const {return "onCastSpell";}
		virtual std::string getScriptEventParams() const {return "cid, var";}

		static ReturnValue internalConjureItem(Player* player, uint32_t conjureId, uint32_t conjureCount,
			bool transform = false, uint32_t reagentId = 0, slots_t slot = slots_t::WHEREEVER, bool test = false);

		static ConjureSpellFunction ConjureItem;
		static ConjureSpellFunction ConjureFood;

		bool internalCastSpell(Creature* creature, const LuaVariant& var);
		Position getCasterPosition(Creature* creature);


		LOGGER_DECLARATION;

		ConjureSpellFunction* function;

		uint32_t conjureId;
		uint32_t conjureCount;
		uint32_t conjureReagentId;

};

class RuneSpell : public Spell, public Action
{
	public:

		RuneSpell(LuaScriptInterface* _interface);
		virtual ~RuneSpell() {}

		virtual bool configureEvent(xmlNodePtr p);
		virtual bool loadFunction(const std::string& functionName);

		virtual ReturnValue canExecuteAction(const Player* player, const ExtendedPosition& toPos);
		virtual bool hasOwnErrorHandler() {return true;}

		virtual bool executeUse(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, bool extendedUse, uint32_t creatureId);

		virtual bool castSpell(Creature* creature);
		virtual bool castSpell(Creature* creature, Creature* target);

		//scripting
		bool executeCastSpell(Creature* creature, const LuaVariant& var);

		virtual bool isInstant() const {return false;}
		uint32_t getRuneItemId(){return runeId;}

	private:
		virtual std::string getScriptEventName() const {return "onCastSpell";}
		virtual std::string getScriptEventParams() const {return "cid, var";}

		static RuneSpellFunction Illusion;
		static RuneSpellFunction Convince;

		bool internalCastSpell(Creature* creature, const LuaVariant& var);


		LOGGER_DECLARATION;

		bool hasCharges;
		uint32_t runeId;

		RuneSpellFunction* function;
		typedef std::map<uint32_t, uint32_t> AttackersMap;
        AttackersMap comboAttackersList;
        AttackersMap comboAttackersCount;
        AttackersMap comboList;

        typedef std::map<uint32_t, AttackersMap> ComboFriendsList;
        ComboFriendsList comboFriendsList;
        
        uint32_t comboLastCheck;
        uint32_t comboFriendsNumber;
        uint32_t comboFriendsListLastClear; 
};

#endif // _SPELLS_H
