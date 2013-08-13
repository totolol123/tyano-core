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

#ifndef _WEAPONS_H
#define _WEAPONS_H

#include "combat.h"
#include "baseevents.h"

class Item;
class Tile;
class Weapon;
class WeaponDistance;
class WeaponMelee;
class WeaponWand;

typedef std::shared_ptr<Weapon>         WeaponP;
typedef std::shared_ptr<const Weapon>   WeaponPC;
typedef std::shared_ptr<WeaponDistance> WeaponDistanceP;
typedef std::shared_ptr<WeaponMelee>    WeaponMeleeP;
typedef std::shared_ptr<WeaponWand>     WeaponWandP;


class Weapons : public BaseEvents<Weapon> {
	public:
		Weapons();

		bool loadDefaults();
		WeaponPC getWeapon(const Item* item) const;

		static int32_t getMaxMeleeDamage(int32_t attackSkill, int32_t attackValue);
		static int32_t getMaxWeaponDamage(int32_t level, int32_t attackSkill, int32_t attackValue, float attackFactor);

	private:
		virtual std::string getScriptBaseName() const {return "weapons";}
		virtual void clear();

		virtual WeaponP getEvent(const std::string& nodeName);
		virtual bool registerEvent(const WeaponP& event, xmlNodePtr p, bool override);

		virtual LuaScriptInterface& getInterface() {return m_interface;}


		LOGGER_DECLARATION;

		LuaScriptInterface m_interface;

		typedef std::map<uint32_t, WeaponP> WeaponMap;
		WeaponMap weapons;
};

class Weapon : public Event
{
	public:
		Weapon(LuaScriptInterface* _interface);
		virtual ~Weapon() override;

		virtual bool configureEvent(xmlNodePtr p) override;
		virtual bool loadFunction(const std::string& functionName) override;
		virtual bool configureWeapon(const ItemKindPC& kind);

		virtual int32_t playerWeaponCheck(const PlayerP& player, const CreatureP& target) const;
		static bool useFist(const PlayerP& player, const CreatureP& target);
		virtual bool useWeapon(const PlayerP& player, const ItemP& item, const CreatureP& target) const;

		CombatParams getCombatParam() const {return params;}

		uint16_t getID() const {return id;}
		virtual int32_t getWeaponDamage(const Player& player, const Creature* target, const Item* item, bool maxDamage = false) const = 0;
		virtual bool interruptSwing() const {return !swing;}

		uint32_t getReqLevel() const {return level;}
		uint32_t getReqMagLv() const {return magLevel;}
		bool hasExhaustion() const {return exhaustion;}
		bool isWieldedUnproperly() const {return wieldUnproperly;}

	protected:
		virtual std::string getScriptEventName() const override {return "onUseWeapon";}
		virtual std::string getScriptEventParams() const override {return "cid, var";}

		bool executeUseWeapon(const PlayerP& player, const LuaVariant& var) const;

		bool internalUseWeapon(const PlayerP& player, const ItemP& item, const CreatureP& target, int32_t damageModifier) const;
		bool internalUseWeapon(const PlayerP& player, const ItemP& item, Tile* tile) const;

		virtual void onUsedWeapon(const PlayerP& player, const ItemP& item, Tile* destTile) const;
		virtual void onUsedAmmo(const PlayerP& player, const ItemP& item, Tile* destTile) const;
		virtual bool getSkillType(const Player& player, const Item* item, skills_t& skill, uint32_t& skillpoint) const {return false;}

		int32_t getManaCost(const Player& player) const;

		uint16_t id;
		bool enabled;
		uint32_t exhaustion;
		bool wieldUnproperly;
		int32_t level;
		int32_t magLevel;
		int32_t mana;
		int32_t manaPercent;
		int32_t soul;
		AmmoAction_t ammoAction;
		bool swing;
		CombatParams params;

	private:

		LOGGER_DECLARATION;

		VocationMap vocWeaponMap;
};

class WeaponMelee : public Weapon
{
	public:
		WeaponMelee(LuaScriptInterface* _interface);
		virtual ~WeaponMelee() override {}

		virtual bool configureEvent(xmlNodePtr p) override;
		virtual bool configureWeapon(const ItemKindPC& kind) override;

		virtual bool useWeapon(const PlayerP& player, const ItemP& item, const CreatureP& target) const override;
		virtual int32_t getWeaponDamage(const Player& player, const Creature* target, const Item* item, bool maxDamage = false) const override;
		int32_t getElementDamage(const Player& player, const Item* item) const;

	protected:
		virtual void onUsedWeapon(const PlayerP& player, const ItemP& item, Tile* destTile) const override;
		virtual void onUsedAmmo(const PlayerP& player, const ItemP& item, Tile* destTile) const override;
		virtual bool getSkillType(const Player& player, const Item* item, skills_t& skill, uint32_t& skillpoint) const override;

		CombatType_t elementType;
		int16_t elementDamage;
};

class WeaponDistance : public Weapon
{
	public:
		WeaponDistance(LuaScriptInterface* _interface);
		virtual ~WeaponDistance() override {}

		virtual bool configureEvent(xmlNodePtr p) override;
		virtual bool configureWeapon(const ItemKindPC& kind) override;

		virtual int32_t playerWeaponCheck(const PlayerP& player, const CreatureP& target) const override;
		virtual bool useWeapon(const PlayerP& player, const ItemP& item, const CreatureP& target) const override;
		virtual int32_t getWeaponDamage(const Player& player, const Creature* target, const Item* item, bool maxDamage = false) const override;

	protected:
		virtual void onUsedWeapon(const PlayerP& player, const ItemP& item, Tile* destTile) const override;
		virtual void onUsedAmmo(const PlayerP& player, const ItemP& item, Tile* destTile) const override;
		virtual bool getSkillType(const Player& player, const Item* item, skills_t& skill, uint32_t& skillpoint) const override;

		int32_t hitChance, maxHitChance, breakChance, ammoAttackValue;
};

class WeaponWand : public Weapon
{
	public:
		WeaponWand(LuaScriptInterface* _interface);
		virtual ~WeaponWand() override {}

		virtual bool configureEvent(xmlNodePtr p) override;
		virtual bool configureWeapon(const ItemKindPC& kind) override;

		virtual int32_t getWeaponDamage(const Player& player, const Creature* target, const Item* item, bool maxDamage = false) const override;

	protected:
		virtual bool getSkillType(const Player& player, const Item* item, skills_t& skill, uint32_t& skillpoint) const override {return false;}

		int32_t minChange, maxChange;
};

#endif // _WEAPONS_H
