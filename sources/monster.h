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

#ifndef _MONSTER_H
#define _MONSTER_H

#include "creature.h"

class  Game;
class  MonsterType;
class  Raid;
struct spellBlock_t;
class  Spawn;


enum TargetSearchType_t
{
	TARGETSEARCH_DEFAULT,
	TARGETSEARCH_RANDOM,
	TARGETSEARCH_ATTACKRANGE,
	TARGETSEARCH_NEAREST
};


class Monster : public Creature {

public:

	using Creature::canMoveTo;


	static MonsterP create (MonsterType* type);
	static MonsterP create (MonsterType* type, Raid* raid);
	static MonsterP create (MonsterType* type, Spawn* spawn);
	static MonsterP create (const std::string& name);

	virtual ~Monster();

	virtual bool       canAttack          (const Creature& creature) const;
	        bool       canBeChallengedBy  (const CreatureP& convincer) const;
	        bool       canBeConvincedBy   (const CreatureP& convincer, bool forced = false) const;
	virtual bool       canMoveTo          (const Tile& tile) const;
	virtual bool       canSeeCreature     (const CreatureP& creature) const;
	        bool       challenge          (const CreatureP& challenger);
	virtual bool       convince           (const CreatureP& convincer, bool forced = false);
	virtual CreatureP  getDirectOwner     ();
	virtual CreaturePC getDirectOwner     () const;
	        CreatureP  getMaster          () const;
	        bool       hasMaster          () const;
	virtual uint32_t   getMoveFlags       () const;
	        bool       hasRaid            () const;
	        bool       hasSpawn           () const;
	virtual bool       isEnemy            (const Creature& creature) const;
	        void       release            ();
	        void       removeFromRaid     ();
	        void       removeFromSpawn    ();
	        bool       target             (const CreatureP& creature);
	        bool       targetClosestEnemy ();
	        bool       targetRandomEnemy  ();
	virtual void       willRemove         ();


protected:

	virtual uint32_t  getPreferredFollowDistance () const;
	virtual Direction getWanderingDirection      () const;
	virtual Duration  getWanderingInterval       () const;
	virtual bool      hasSomethingToThinkAbout   () const;
	virtual bool      hasToThinkAboutCreature    (const CreaturePC& creature) const;
	virtual void      onThink                    (Duration elapsedTime);
	virtual void      onThinkingStarted          ();


private:

	void babble                 ();
	bool isMasterInRange        () const;
	void notifyMasterChanged    (const CreatureP& previousMaster);
	void retarget               ();
	void setMaster              (const CreatureP& master);
	void setRetargetDelay       (Duration retargetDelay);
	bool shouldBeRemoved        () const;
	bool shouldTeleportToMaster () const;
	bool teleportToMaster       ();
	void updateBabbling         (Duration elapsedTime);
	void updateFollowing        ();
	void updateRelationship     ();
	void updateRetargeting      (Duration elapsedTime);
	void updateTarget           ();


	LOGGER_DECLARATION;

	Duration     _babbleDelay;
	CreatureP    _master;
	Raid*        _raid;
	Duration     _retargetDelay;
	Spawn*       _spawn;
	MonsterType* _type;







	private:
		Monster(MonsterType* _mType, Raid* raid, Spawn* spawn);

	public:


		virtual Monster* getMonster() {return this;}
		virtual const Monster* getMonster() const {return this;}

		virtual uint32_t rangeId() {return 0x40000000;}
		static AutoList<Monster> autoList;

		void addList() {autoList[id] = this;}
		void removeList() {autoList.erase(id);}

		virtual const std::string& getName() const;
		virtual const std::string& getNameDescription() const;
		virtual std::string getDescription(int32_t lookDistance) const;

		virtual RaceType_t getRace() const;
		virtual int32_t getArmor() const;
		virtual int32_t getDefense() const;
		virtual MonsterType* getMonsterType() const;
		virtual bool isPushable() const;
		virtual bool isAttackable() const;
		virtual bool isImmune(CombatType_t type) const;

		bool canPushItems() const;
		bool canPushCreatures() const;
		bool isHostile() const;
		virtual bool isWalkable() const;
		virtual bool canSeeInvisibility() const;
		uint32_t getManaCost() const;

		virtual void onAttackedCreature(Creature* target);
		virtual void onAttackedCreatureDisappear(bool isLogout);
		virtual void onAttackedCreatureDrain(Creature* target, int32_t points);

		virtual void drainHealth(const CreatureP& attacker, CombatType_t combatType, int32_t damage);

		virtual void setNormalCreatureLight();
		virtual bool getCombatValues(int32_t& min, int32_t& max);

		virtual void doAttacking(uint32_t interval);
		virtual bool hasExtraSwing() {return extraMeleeAttack;}

		bool isFleeing() const;

		virtual BlockType_t blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
			bool checkDefense = false, bool checkArmor = false);

		int32_t minCombatValue;
		int32_t maxCombatValue;
		uint32_t attackTicks;
		uint32_t targetTicks;
		uint32_t defenseTicks;
		uint32_t yellTicks;
		bool resetTicks;
		bool extraMeleeAttack;


		bool doTeleportToMaster();
		void updateLookDirection();

		virtual bool onDeath();
		virtual boost::intrusive_ptr<Item> createCorpse(DeathList deathList);
		bool despawn() const;
		bool inDespawnRange(const Position& pos) const;

		bool canUseAttack(const Position& pos, const Creature* target) const;
		bool canUseSpell(const Position& pos, const Position& targetPos,
			const spellBlock_t& sb, uint32_t interval, bool& inRange);
		Direction getDanceStep(bool keepAttack = true, bool keepDistance = true) const;
		bool isInSpawnRange(const Position& toPos) const;

		bool pushItem(Item* item, int32_t radius);
		void pushItems(Tile* tile);
		bool pushCreature(Creature* creature);
		void pushCreatures(Tile* tile);

		void onThinkYell(uint32_t interval);
		void onThinkDefense(uint32_t interval);

		virtual uint64_t getLostExperience() const;
		virtual void dropLoot(Container* corpse);
		virtual uint32_t getDamageImmunities() const;
		virtual uint32_t getConditionImmunities() const;
		virtual uint16_t getLookCorpse();
		virtual uint16_t getLookCorpse() const;
		virtual void getPathSearchParams(const Creature* creature, FindPathParams& fpp) const;
};

#endif // _MONSTER_H
