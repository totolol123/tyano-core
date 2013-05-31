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

#ifndef _ITEMS_H
#define _ITEMS_H

#include "const.h"
#include "enums.h"
#include "itemloader.h"
#include "position.h"

#define SLOTP_WHEREEVER 0xFFFFFFFF
#define SLOTP_HEAD 1 << 0
#define	SLOTP_NECKLACE 1 << 1
#define	SLOTP_BACKPACK 1 << 2
#define	SLOTP_ARMOR 1 << 3
#define	SLOTP_RIGHT 1 << 4
#define	SLOTP_LEFT 1 << 5
#define	SLOTP_LEGS 1 << 6
#define	SLOTP_FEET 1 << 7
#define	SLOTP_RING 1 << 8
#define	SLOTP_AMMO 1 << 9
#define	SLOTP_DEPOT 1 << 10
#define	SLOTP_TWO_HAND 1 << 11
#define SLOTP_HAND SLOTP_LEFT | SLOTP_RIGHT

class ItemKind;
enum class ItemType : uint8_t;

typedef std::shared_ptr<ItemKind>        ItemKindP;
typedef std::shared_ptr<const ItemKind>  ItemKindPC;
typedef std::vector<ItemKindP>           ItemKindVector;

namespace items {

	class Class;

	typedef std::shared_ptr<Class>  ClassP;

} // namespace items



namespace std {

	template <>
	struct hash<ItemType> : function<size_t(const ItemType)> {
		typedef std::underlying_type<argument_type>::type  underlying_type;

		result_type operator()(const argument_type& type) const {
			return std::hash<underlying_type>()(static_cast<underlying_type>(type));
		}
	};

}



enum class ItemType : uint8_t {
	GENERIC = 0,
	DEPOT,
	MAILBOX,
	TRASHHOLDER,
	CONTAINER,
	DOOR,
	MAGICFIELD,
	TELEPORT,
	BED,
	KEY,
	LAST = KEY
};

enum FloorChange_t
{
	CHANGE_PRE_FIRST = 0,
	CHANGE_DOWN = CHANGE_PRE_FIRST,
	CHANGE_FIRST = 1,
	CHANGE_NORTH = CHANGE_FIRST,
	CHANGE_EAST = 2,
	CHANGE_SOUTH = 3,
	CHANGE_WEST = 4,
	CHANGE_FIRST_EX = 5,
	CHANGE_NORTH_EX = CHANGE_FIRST_EX,
	CHANGE_EAST_EX = 6,
	CHANGE_SOUTH_EX = 7,
	CHANGE_WEST_EX = 8,
	CHANGE_NONE = 9,
	CHANGE_LAST = CHANGE_NONE
};

struct Abilities
{
	Abilities()
	{
		memset(this, 0, sizeof(*this));
	};

	bool manaShield:1, invisible:1, regeneration:1, preventLoss:1, preventDrop:1;
	CombatType_t elementType:12;
	uint32_t conditionSuppressions:24;

	int16_t elementDamage, absorb[COMBAT_LAST + 1], increment[INCREMENT_LAST + 1], reflect[REFLECT_LAST + 1][COMBAT_LAST + 1];
	int32_t skills[SKILL_LAST + 1], skillsPercent[SKILL_LAST + 1], stats[STAT_LAST + 1], statsPercent[STAT_LAST + 1],
		speed, healthGain, healthTicks, manaGain, manaTicks;
};

class Condition;


class ItemKind {

	public:
		ItemKind();
		~ItemKind();

		bool isGroundTile() const {return (group == ITEM_GROUP_GROUND);}
		bool isContainer() const {return (group == ITEM_GROUP_CONTAINER);}
		bool isSplash() const {return (group == ITEM_GROUP_SPLASH);}
		bool isFluidContainer() const {return (group == ITEM_GROUP_FLUID);}

		bool isDoor() const {return (type == ItemType::DOOR);}
		bool isMagicField() const {return (type == ItemType::MAGICFIELD);}
		bool isTeleport() const {return (type == ItemType::TELEPORT);}
		bool isKey() const {return (type == ItemType::KEY);}
		bool isDepot() const {return (type == ItemType::DEPOT);}
		bool isMailbox() const {return (type == ItemType::MAILBOX);}
		bool isTrashHolder() const {return (type == ItemType::TRASHHOLDER);}
		bool isBed() const {return (type == ItemType::BED);}

		bool isRune() const {return clientCharges;}
		bool hasSubType() const {return (isFluidContainer() || isSplash() || stackable || charges);}

		inline bool hasFloorChange(FloorChange_t floorChange) const {
			switch (floorChange) {
			case CHANGE_DOWN:
				return floorChange0;

			case CHANGE_NORTH:
				return floorChange1;

			case CHANGE_EAST:
				return floorChange2;

			case CHANGE_SOUTH:
				return floorChange3;

			case CHANGE_WEST:
				return floorChange4;

			case CHANGE_NORTH_EX:
				return floorChange5;

			case CHANGE_EAST_EX:
				return floorChange6;

			case CHANGE_SOUTH_EX:
				return floorChange7;

			case CHANGE_WEST_EX:
				return floorChange8;

			case CHANGE_NONE:
				return floorChange9;
			}

			return false;
		}

		inline void setFloorChange(FloorChange_t floorChange, bool value) {
			switch (floorChange) {
			case CHANGE_DOWN:
				floorChange0 = value;
				break;

			case CHANGE_NORTH:
				floorChange1 = value;
				break;

			case CHANGE_EAST:
				floorChange2 = value;
				break;

			case CHANGE_SOUTH:
				floorChange3 = value;
				break;

			case CHANGE_WEST:
				floorChange4 = value;
				break;

			case CHANGE_NORTH_EX:
				floorChange5 = value;
				break;

			case CHANGE_EAST_EX:
				floorChange6 = value;
				break;

			case CHANGE_SOUTH_EX:
				floorChange7 = value;
				break;

			case CHANGE_WEST_EX:
				floorChange8 = value;
				break;

			case CHANGE_NONE:
				floorChange9 = value;
				break;
			}
		}

		bool stopTime:1;
		bool showCount:1;
		bool clientCharges:1;
		bool stackable:1;
		bool showDuration:1;
		bool showCharges:1;
		bool showAttributes:1;
		bool allowDistRead:1;
		bool canReadText:1;
		bool canWriteText:1;
		bool forceSerialize:1;
		bool isVertical:1;
		bool isHorizontal:1;
		bool isHangable:1;
		bool useable:1;
		bool moveable:1;
		bool pickupable:1;
		bool rotable:1;
		bool replaceable:1;
		bool lookThrough:1;
		bool hasHeight:1;
		bool blockSolid:1;
		bool blockPickupable:1;
		bool blockProjectile:1;
		bool blockPathFind:1;
		bool allowPickupable:1;
		bool alwaysOnTop:1;
		bool floorChange0:1;
		bool floorChange1:1;
		bool floorChange2:1;
		bool floorChange3:1;
		bool floorChange4:1;
		bool floorChange5:1;
		bool floorChange6:1;
		bool floorChange7:1;
		bool floorChange8:1;
		bool floorChange9:1;

		MagicEffect_t magicEffect:16;
		FluidTypes_t fluidSource:5;
		WeaponType_t weaponType:4;
		Direction bedPartnerDir:3;
		AmmoAction_t ammoAction:3;
		CombatType_t combatType:12;
		RaceType_t corpseType:3;
		ShootEffect_t shootType:16;
		Ammo_t ammoType:3;
		itemgroup_t group:4;
		uint16_t slotPosition:12;
		uint8_t wieldPosition:4;

		items::ClassP _class;
		ItemType type;

		uint16_t charges, transformUseTo[2], transformToFree, transformEquipTo, transformDeEquipTo,
			id, clientId, maxItems, speed, maxTextLen, writeOnceItemId;

		int32_t attack, extraAttack, defense, extraDefense, armor, breakChance, hitChance, maxHitChance,
			runeLevel, runeMagLevel, lightLevel, lightColor, decayTo, rotateTo, alwaysOnTopOrder;
		uint32_t shootRange, decayTime, attackSpeed, wieldInfo, minReqLevel, minReqMagicLevel,
			worth, levelDoor;

		std::string name, pluralName, article, description, runeSpellName, vocationString;

		Condition* condition;
		Abilities abilities;
		float weight;
};



struct RandomizationBlock
{
	int32_t fromRange, toRange, chance;
};


class Items {

private:

	typedef std::map<uint16_t,RandomizationBlock> RandomizationMap;     // <kindId,randomization>
	typedef std::map<int32_t,uint16_t>            KindIdByClientIdMap;  // <clientId,kindId>


public:

	typedef ItemKindVector::const_iterator  const_iterator;
	typedef std::map<int32_t,uint16_t>      WorthMap;  // <worth,kindId>


	Items();

	const_iterator  begin               () const;
	const_iterator  end                 () const;
	ItemKindPC      getKind             (uint16_t kindId) const;
	ItemKindPC      getKindByClientId   (int32_t clientId) const;
	ItemKindPC      getKindByName       (const std::string& name) const;
	uint16_t        getKindIdByName     (const std::string& name) const;
	ItemKindP       getMutableKind      (uint16_t kindId) const;
	uint16_t        getRandomizedKindId (uint16_t kindId) const;
	uint32_t        getVersionBuild     () const;
	uint32_t        getVersionMajor     () const;
	uint32_t        getVersionMinor     () const;
	const WorthMap& getWorth            () const;
	void            loadKindFromXmlNode (xmlNodePtr root, uint16_t kindId, const std::string& filePath);
	bool            reload              ();

	ItemKindPC operator[](uint16_t kindId) const;


private:

	typedef std::unordered_map<ItemType,items::ClassP>  Classes;


	void addKind                  (ItemKindP kind, const std::string& fileName);
	void addRandomization         (uint16_t kindId, int32_t fromId, int32_t toId, int32_t chance, const std::string& fileName);
	void clear                    ();
	bool loadKindsFromOtb         ();
	bool loadKindsFromXml         ();
	bool loadRandomizationFromXml ();
	void setupClasses             ();


	LOGGER_DECLARATION;

	Classes             _classes;
	uint8_t             _defaultRandomizationChance;
	KindIdByClientIdMap _kindIdsByClientId;
	ItemKindVector      _kinds;
	RandomizationMap    _randomizations;
	uint32_t            _versionBuild;
	uint32_t            _versionMajor;
	uint32_t            _versionMinor;
	WorthMap            _worth;

};

#endif // _ITEMS_H
