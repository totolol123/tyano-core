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

typedef std::shared_ptr<ItemKind>        ItemKindP;
typedef std::shared_ptr<const ItemKind>  ItemKindPC;


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

	public:

		typedef attributes::Values               Attributes;
		typedef attributes::Scheme::Attributes   ClassAttributes;
		typedef attributes::Scheme::AttributesP  ClassAttributesP;


		static const std::string ATTRIBUTE_AID;
		static const std::string ATTRIBUTE_ARMOR;
		static const std::string ATTRIBUTE_ARTICLE;
		static const std::string ATTRIBUTE_ATTACK;
		static const std::string ATTRIBUTE_ATTACKSPEED;
		static const std::string ATTRIBUTE_CHARGES;
		static const std::string ATTRIBUTE_CORPSEOWNER;
		static const std::string ATTRIBUTE_DATE;
		static const std::string ATTRIBUTE_DEFENSE;
		static const std::string ATTRIBUTE_DESCRIPTION;
		static const std::string ATTRIBUTE_DECAYING;
		static const std::string ATTRIBUTE_DURATION;
		static const std::string ATTRIBUTE_EXTRAATTACK;
		static const std::string ATTRIBUTE_EXTRADEFENSE;
		static const std::string ATTRIBUTE_FLUIDTYPE;
		static const std::string ATTRIBUTE_HITCHANCE;
		static const std::string ATTRIBUTE_NAME;
		static const std::string ATTRIBUTE_OWNER;
		static const std::string ATTRIBUTE_PLURALNAME;
		static const std::string ATTRIBUTE_SCRIPTPROTECTED;
		static const std::string ATTRIBUTE_SHOOTRANGE;
		static const std::string ATTRIBUTE_TEXT;
		static const std::string ATTRIBUTE_UID;
		static const std::string ATTRIBUTE_WRITER;


		static ClassAttributesP   getClassAttributes();
		static const std::string& getClassName();

		Attributes&       getAttributes ();
		const Attributes& getAttributes () const;

		//Factory member to create item of right type based on type
		static boost::intrusive_ptr<Item> CreateItem(uint16_t type, uint16_t amount = 1);
		static boost::intrusive_ptr<Item> CreateItem(PropStream& propStream);

		static bool loadItem(xmlNodePtr node, Container* parent);
		static bool loadContainer(xmlNodePtr node, Container* parent);

		// Constructor for items
		Item(const ItemKindPC& kind, uint16_t amount = 0);
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

		static std::string getDescription(const ItemKindPC& kind, int32_t lookDistance, const Item* item = nullptr, uint16_t subType = 0, bool addArticle = true);
		static std::string getNameDescription(const ItemKindPC& kind, const Item* item = nullptr, uint16_t subType = 0, bool addArticle = true);
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
		void setDuration(int32_t time) {_attributes.set(ATTRIBUTE_DURATION, time);}
		void decreaseDuration(int32_t time);
		int32_t getDuration() const;

		void setSpecialDescription(const std::string& description) {_attributes.set(ATTRIBUTE_DESCRIPTION, description);}
		void resetSpecialDescription() {_attributes.remove(ATTRIBUTE_DESCRIPTION);}
		const std::string& getSpecialDescription() const;

		void setText(const std::string& text) {_attributes.set(ATTRIBUTE_TEXT, text);}
		void resetText() {_attributes.remove(ATTRIBUTE_TEXT);}
		const std::string& getText() const;

		void setDate(time_t date) {_attributes.set(ATTRIBUTE_DATE, (int32_t)date);}
		void resetDate() {_attributes.remove(ATTRIBUTE_DATE);}
		time_t getDate() const;

		void setWriter(std::string writer) {_attributes.set(ATTRIBUTE_WRITER, writer);}
		void resetWriter() {_attributes.remove(ATTRIBUTE_WRITER);}
		const std::string& getWriter() const;

		void setActionId(int32_t aid);
		void resetActionId();
		int32_t getActionId() const;

		void setUniqueId(int32_t uid);
		int32_t getUniqueId() const;

		void setCharges(uint16_t charges) {_attributes.set(ATTRIBUTE_CHARGES, charges);}
		uint16_t getCharges() const;

		void setFluidType(uint16_t fluidType) {_attributes.set(ATTRIBUTE_FLUIDTYPE, fluidType);}
		uint16_t getFluidType() const;

		void setOwner(uint32_t owner) {_attributes.set(ATTRIBUTE_OWNER, (int32_t)owner);}
		uint32_t getOwner() const;

		void setCorpseOwner(uint32_t corpseOwner) {_attributes.set(ATTRIBUTE_CORPSEOWNER, (int32_t)corpseOwner);}
		uint32_t getCorpseOwner();

		void setDecaying(ItemDecayState_t state) {_attributes.set(ATTRIBUTE_DECAYING, (int32_t)state);}
		ItemDecayState_t getDecaying() const;

		ItemKindPC getKind() const;
		void setKind(const ItemKindPC& kind);

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

		uint32_t getDefaultDuration() const;
		void setDefaultDuration()
		{
			uint32_t duration = getDefaultDuration();
			if(duration)
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


		LOGGER_DECLARATION;


		attributes::Values _attributes;

		ItemKindPC kind;

		uint8_t count;
		bool loadedFromMap;

		Raid* raid;
};

#endif // _ITEM_H
