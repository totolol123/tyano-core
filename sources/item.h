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

#ifndef _ITEM_H
#define _ITEM_H

#include "attributes/Attribute.hpp"
#include "attributes/Scheme.hpp"
#include "attributes/Values.hpp"
#include "item/Kind.hpp"
#include "const.h"
#include "thing.h"

class  BedItem;
class  Container;
class  Creature;
class  Depot;
class  Door;
class  FileLoader;
class  ItemKind;
struct LightInfo;
class  MagicField;
class  Mailbox;
struct NodeStruct;
class  Player;
class  PropStream;
class  PropWriteStream;
class  Raid;
class  Teleport;
class  TrashHolder;


enum ITEMPROPERTY
{
	BLOCKSOLID = 0,
	HASHEIGHT,
	BLOCKPROJECTILE,
	BLOCKPATH,
	ISVERTICAL,
	ISHORIZONTAL,
	MOVEABLE,
	IMMOVABLEBLOCKSOLID,
	IMMOVABLEBLOCKPATH,
	IMMOVABLENOFIELDBLOCKPATH,
	NOFIELDBLOCKPATH,
	SUPPORTHANGABLE
};

enum TradeEvents_t
{
	ON_TRADE_TRANSFER,
	ON_TRADE_CANCEL,
};

enum ItemDecayState_t
{
	DECAYING_FALSE = 0,
	DECAYING_TRUE,
	DECAYING_PENDING
};

enum AttrTypes_t
{
	ATTR_END = 0,
	//ATTR_DESCRIPTION = 1,
	//ATTR_EXT_FILE = 2,
	ATTR_TILE_FLAGS = 3,
	ATTR_ACTION_ID = 4,
	ATTR_UNIQUE_ID = 5,
	ATTR_TEXT = 6,
	ATTR_DESC = 7,
	ATTR_TELE_DEST = 8,
	ATTR_ITEM = 9,
	ATTR_DEPOT_ID = 10,
	//ATTR_EXT_SPAWN_FILE = 11,
	ATTR_RUNE_CHARGES = 12,
	//ATTR_EXT_HOUSE_FILE = 13,
	ATTR_HOUSEDOORID = 14,
	ATTR_COUNT = 15,
	ATTR_DURATION = 16,
	ATTR_DECAYING_STATE = 17,
	ATTR_WRITTENDATE = 18,
	ATTR_WRITTENBY = 19,
	ATTR_SLEEPERGUID = 20,
	ATTR_SLEEPSTART = 21,
	ATTR_CHARGES = 22,
	ATTR_CONTAINER_ITEMS = 23,
	ATTR_NAME = 30,
	ATTR_PLURALNAME = 31,
	ATTR_ATTACK = 33,
	ATTR_EXTRAATTACK = 34,
	ATTR_DEFENSE = 35,
	ATTR_EXTRADEFENSE = 36,
	ATTR_ARMOR = 37,
	ATTR_ATTACKSPEED = 38,
	ATTR_HITCHANCE = 39,
	ATTR_SHOOTRANGE = 40,
	ATTR_ARTICLE = 41,
	ATTR_SCRIPTPROTECTED = 42,
	ATTR_ATTRIBUTE_MAP = 128
};

enum Attr_ReadValue
{
	ATTR_READ_CONTINUE,
	ATTR_READ_ERROR,
	ATTR_READ_END
};

// from iomap.h
#pragma pack(1)
struct TeleportDest
{
	uint16_t _x, _y;
	uint8_t _z;
};
#pragma pack()

class Item : public Thing {

	private:

		typedef ts::attributes::Values  DynamicAttributeValues;
		typedef ts::item::Kind          Kind;
		typedef ts::item::KindPC        KindPC;


	public:

		Item (const KindPC& kind);

		DynamicAttributeValues&       getDynamicAttributes ();
		const DynamicAttributeValues& getDynamicAttributes () const;
		const Kind&                   getKind              () const;
		KindPC                        getKindPointer       () const;


		LOGGER_DECLARATION;

		DynamicAttributeValues _dynamicAttributes;
		KindPC                 _kind;














		//Factory member to create item of right type based on type
		static boost::intrusive_ptr<Item> CreateItem(uint16_t type, uint16_t amount = 1);
		static boost::intrusive_ptr<Item> CreateItem(PropStream& propStream);

		static bool loadItem(xmlNodePtr node, Container* parent);
		static bool loadContainer(xmlNodePtr node, Container* parent);

		// Constructor for items
		virtual ~Item() {}

		virtual boost::intrusive_ptr<Item> clone() const;
		virtual void copyAttributes(Item* item);

		virtual Item* getItem() {return this;}
		virtual const Item* getItem() const {return this;}

		virtual Container* getContainer() {return nullptr;}
		virtual const Container* getContainer() const {return nullptr;}

		virtual Teleport* getTeleport() {return nullptr;}
		virtual const Teleport* getTeleport() const {return nullptr;}

		virtual TrashHolder* getTrashHolder() {return nullptr;}
		virtual const TrashHolder* getTrashHolder() const {return nullptr;}

		virtual Mailbox* getMailbox() {return nullptr;}
		virtual const Mailbox* getMailbox() const {return nullptr;}

		virtual Door* getDoor() {return nullptr;}
		virtual const Door* getDoor() const {return nullptr;}

		virtual MagicField* getMagicField() {return nullptr;}
		virtual const MagicField* getMagicField() const {return nullptr;}

		virtual BedItem* getBed() {return nullptr;}
		virtual const BedItem* getBed() const {return nullptr;}

		uint16_t getId() const;
		uint16_t getClientID() const;

		static std::string getDescription(const Kind& kind, int32_t lookDistance, const Item* item = nullptr, uint16_t subType = 0, bool addArticle = true);
		static std::string getNameDescription(const Kind& kind, const Item* item = nullptr, uint16_t subType = 0, bool addArticle = true);
		static std::string getWeightDescription(double weight, bool stackable, uint32_t count = 1);

		virtual std::string getDescription(int32_t lookDistance) const;
		std::string getNameDescription() const;
		std::string getWeightDescription() const;

		Player* getHoldingPlayer();
		const Player* getHoldingPlayer() const;

		//serialization
		virtual Attr_ReadValue readAttr(AttrTypes_t attr, PropStream& propStream);
		virtual bool unserializeAttr(PropStream& propStream);
		virtual bool serializeAttr(PropWriteStream& propWriteStream) const;
		virtual bool unserializeItemNode(FileLoader& f, const NodeStruct* node, PropStream& propStream) {return unserializeAttr(propStream);}

		// Item attributes
		void decreaseDuration(Duration time);
		Duration getDuration() const;

		const std::string& getSpecialDescription() const;

		const std::string& getText() const;

		time_t getDate() const;

		const std::string& getWriter() const;

		void setActionId(int32_t aid);
		void resetActionId();
		int32_t getActionId() const;

		void setUniqueId(int32_t uid);
		int32_t getUniqueId() const;

		void setDuration(Duration time);
		void setSpecialDescription(const std::string& description);
		void resetSpecialDescription();
		void setText(const std::string& text);
		void resetText();
		void setDate(time_t date);
		void resetDate();
		void setWriter(std::string writer);
		void resetWriter();
		void setCharges(uint16_t charges);
		void setOwner(uint32_t owner);
		void setCorpseOwner(uint32_t corpseOwner);
		void setDecaying(ItemDecayState_t state);
		void setFluidType(uint16_t fluidType);

		uint16_t getCharges() const;

		uint16_t getFluidType() const;

		uint32_t getOwner() const;

		uint32_t getCorpseOwner();
		ItemDecayState_t getDecaying() const;

		std::string getName() const;
		std::string getPluralName() const;
		std::string getArticle() const;
		bool isScriptProtected() const;

		int32_t getAttack() const;
		int32_t getExtraAttack() const;
		int32_t getDefense() const;
		int32_t getExtraDefense() const;

		int32_t getArmor() const;
		int32_t getAttackSpeed() const;
		int32_t getHitChance() const;
		int32_t getShootRange() const;

		Ammo_t getAmmoType() const;
		WeaponType_t getWeaponType() const;
		int32_t getSlotPosition() const;
		int32_t getWieldPosition() const;

		virtual double getWeight() const;
		void getLight(LightInfo& lightInfo);

		int32_t getMaxWriteLength() const;
		uint64_t getWorth() const;
		virtual int32_t getThrowRange() const;

		bool floorChange(FloorChange_t change = CHANGE_NONE) const;
		bool forceSerialize() const;

		bool hasProperty(ITEMPROPERTY prop) const;
		bool hasSubType() const;
		bool hasCharges() const;

		bool canDecay(bool ignoreRemoved = false);
		virtual bool canRemove() const;
		virtual bool canTransform() const;
		bool canWriteText() const;

		virtual bool isPushable() const;
		bool isGroundTile() const;
		bool isContainer() const;
		bool isSplash() const;
		bool isFluidContainer() const;
		bool isDoor() const;
		bool isMagicField() const;
		bool isTeleport() const;
		bool isKey() const;
		bool isDepot() const;
		bool isMailbox() const;
		bool isTrashHolder() const;
		bool isBed() const;
		bool isRune() const;
		bool isBlocking(const Creature* creature) const;
		bool isStackable() const;
		bool isAlwaysOnTop() const;
		bool isNotMoveable() const;
		bool isMoveable() const;
		bool isPickupable() const;
		bool isUseable() const;
		bool isHangable() const;
		bool isRoteable() const;
		bool isWeapon() const;
		bool isReadable() const;

		bool isLoadedFromMap() const {return loadedFromMap;}
		void setLoadedFromMap(bool value) {loadedFromMap = value;}

		uint16_t getItemCount() const {return count;}
		void setItemCount(uint16_t n) {count = n;}

		uint16_t getSubType() const;
		void setSubType(uint16_t n);

		Duration getDefaultDuration() const;
		void setDefaultDuration()
		{
			Duration duration = getDefaultDuration();
			if(duration > Duration::zero())
				setDuration(duration);
		}

		Raid* getRaid() {return raid;}
		void setRaid(Raid* _raid) {raid = _raid;}

		virtual void onRemoved();
		virtual bool onTradeEvent(TradeEvents_t event, Player* owner, Player* seller) {return true;}

		void setDefaultSubtype();
		virtual void __startDecaying();
		static uint32_t countByType(const Item* item, int32_t checkType, bool multiCount);

	private:

		bool unserializeMap (PropStream& stream);

		uint8_t count;
		bool loadedFromMap;

		Raid* raid;
};

#endif // _ITEM_H
