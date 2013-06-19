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
#include "item.h"

#include "attributes/Attribute.hpp"
#include "items/Class.hpp"
#include "container.h"
#include "depot.h"

#include "teleport.h"
#include "trashholder.h"
#include "fileloader.h"
#include "mailbox.h"

#include "luascript.h"
#include "combat.h"

#include "house.h"
#include "beds.h"

#include "actions.h"
#include "configmanager.h"
#include "creature.h"
#include "game.h"
#include "items.h"
#include "movement.h"
#include "player.h"
#include "raids.h"
#include "server.h"


const std::string Item::ATTRIBUTE_AID("aid");
const std::string Item::ATTRIBUTE_ARMOR("armor");
const std::string Item::ATTRIBUTE_ARTICLE("article");
const std::string Item::ATTRIBUTE_ATTACK("attack");
const std::string Item::ATTRIBUTE_ATTACKSPEED("attackspeed");
const std::string Item::ATTRIBUTE_CHARGES("charges");
const std::string Item::ATTRIBUTE_CORPSEOWNER("corpseowner");
const std::string Item::ATTRIBUTE_DATE("date");
const std::string Item::ATTRIBUTE_DECAYING("decaying");
const std::string Item::ATTRIBUTE_DEFENSE("defense");
const std::string Item::ATTRIBUTE_DESCRIPTION("description");
const std::string Item::ATTRIBUTE_DURATION("duration");
const std::string Item::ATTRIBUTE_EXTRAATTACK("extraattack");
const std::string Item::ATTRIBUTE_EXTRADEFENSE("extradefense");
const std::string Item::ATTRIBUTE_FLUIDTYPE("fluidtype");
const std::string Item::ATTRIBUTE_HITCHANCE("hitchance");
const std::string Item::ATTRIBUTE_NAME("name");
const std::string Item::ATTRIBUTE_OWNER("owner");
const std::string Item::ATTRIBUTE_PLURALNAME("pluralname");
const std::string Item::ATTRIBUTE_SCRIPTPROTECTED("scriptprotected");
const std::string Item::ATTRIBUTE_SHOOTRANGE("shootrange");
const std::string Item::ATTRIBUTE_TEXT("text");
const std::string Item::ATTRIBUTE_UID("uid");
const std::string Item::ATTRIBUTE_WRITER("writer");

LOGGER_DEFINITION(Item);



Item::Attributes& Item::getAttributes() {
	return _attributes;
}


const Item::Attributes& Item::getAttributes() const {
	return _attributes;
}


Item::ClassAttributesP Item::getClassAttributes() {
	using attributes::Type;

	auto attributes = ClassAttributesP(new ClassAttributes);
	attributes->emplace(ATTRIBUTE_AID, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_ARMOR, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_ARTICLE, Type::STRING);
	attributes->emplace(ATTRIBUTE_ATTACK, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_ATTACKSPEED, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_CHARGES, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_CORPSEOWNER, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_DATE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_DECAYING, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_DEFENSE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_DESCRIPTION, Type::STRING);
	attributes->emplace(ATTRIBUTE_DURATION, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_EXTRAATTACK, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_EXTRADEFENSE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_FLUIDTYPE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_HITCHANCE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_NAME, Type::STRING);
	attributes->emplace(ATTRIBUTE_OWNER, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_PLURALNAME, Type::STRING);
	attributes->emplace(ATTRIBUTE_SCRIPTPROTECTED, Type::BOOLEAN);
	attributes->emplace(ATTRIBUTE_SHOOTRANGE, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_TEXT, Type::STRING);
	attributes->emplace(ATTRIBUTE_UID, Type::INTEGER);
	attributes->emplace(ATTRIBUTE_WRITER, Type::STRING);

	return attributes;
}


const std::string& Item::getClassName() {
	static const std::string name("Generic");
	return name;
}


uint16_t Item::getId() const {
	return kind->id;
}


boost::intrusive_ptr<Item> Item::CreateItem(uint16_t kindId, uint16_t amount/* = 1*/) {
	if (kindId == 0) {
		return nullptr;
	}

	switch (kindId) {
	case 2210:
	case 2211:
	case 2212:
		kindId -= 3;
		break;

	case 2215:
	case 2216:
		kindId -= 2;
		break;

	case 2202:
	case 2203:
	case 2204:
	case 2205:
	case 2206:
		kindId -= 37;
		break;

	case 2640:
		kindId = 6132;
		break;

	case 6301:
		kindId = 6300;
		break;
	}

	ItemKindPC kind = server.items()[kindId];
	if (kind == nullptr) {
		LOGe("Cannot create item with unknown id " << kind->id << ".");
		return nullptr;
	}
	if (kind->group == ITEM_GROUP_DEPRECATED) {
		LOGe("Cannot create deprecated item " << kind->id << ".");
		return nullptr;
	}

	boost::intrusive_ptr<Item> item;
	if(kind->isDepot())
		item = new Depot(kind);
	else if(kind->isContainer())
		item = new Container(kind);
	else if(kind->isTeleport())
		item = new Teleport(kind);
	else if(kind->isMagicField())
		item = new MagicField(kind);
	else if(kind->isDoor())
		item = new Door(kind);
	else if(kind->isTrashHolder())
		item = new TrashHolder(kind, kind->magicEffect);
	else if(kind->isMailbox())
		item = new Mailbox(kind);
	else if(kind->isBed())
		item = new BedItem(kind);
	else
		item = new Item(kind, amount);

	return item;
}

boost::intrusive_ptr<Item> Item::CreateItem(PropStream& propStream)
{
	uint16_t type;
	if(!propStream.GET_USHORT(type))
		return nullptr;

	return Item::CreateItem(server.items().getRandomizedKindId(type), 0);
}

bool Item::loadItem(xmlNodePtr node, Container* parent)
{
	if(xmlStrcmp(node->name, (const xmlChar*)"item"))
		return false;

	int32_t intValue;
	std::string strValue;

	boost::intrusive_ptr<Item> item;
	if(readXMLInteger(node, "id", intValue))
		item = Item::CreateItem(intValue);

	if(!item)
		return false;

	if(readXMLString(node, "attributes", strValue))
	{
		StringVector v, attr = explodeString(";", strValue);
		for(StringVector::iterator it = attr.begin(); it != attr.end(); ++it)
		{
			v = explodeString(",", (*it));
			if(v.size() < 2)
				continue;

			if(atoi(v[1].c_str()) || v[1] == "0")
				item->_attributes.set(v[0], atoi(v[1].c_str()));
			else
				item->_attributes.set(v[0], v[1]);
		}
	}

	//compatibility
	if(readXMLInteger(node, "subtype", intValue) || readXMLInteger(node, "subType", intValue))
		item->setSubType(intValue);

	if(readXMLInteger(node, "actionId", intValue) || readXMLInteger(node, "actionid", intValue)
		|| readXMLInteger(node, "aid", intValue))
		item->setActionId(intValue);

	if(readXMLInteger(node, "uniqueId", intValue) || readXMLInteger(node, "uniqueid", intValue)
		|| readXMLInteger(node, "uid", intValue))
		item->setUniqueId(intValue);

	if(readXMLString(node, "text", strValue))
		item->setText(strValue);

	if(item->getContainer())
		loadContainer(node, item->getContainer());

	if(parent)
		parent->addItem(item.get());

	return true;
}

bool Item::loadContainer(xmlNodePtr parentNode, Container* parent)
{
	xmlNodePtr node = parentNode->children;
	while(node)
	{
		if(node->type != XML_ELEMENT_NODE)
		{
			node = node->next;
			continue;
		}

		if(!xmlStrcmp(node->name, (const xmlChar*)"item") && !loadItem(node, parent))
			return false;

		node = node->next;
	}

	return true;
}

Item::Item(const ItemKindPC& kind, uint16_t amount/* = 0*/):
	_attributes(kind->_class->getAttributesScheme()), kind(kind)
{
	assert(kind);

	count = 1;
	raid = nullptr;
	loadedFromMap = false;

	if(kind->charges)
		setCharges(kind->charges);

	setDefaultDuration();
	if(kind->isFluidContainer() || kind->isSplash())
		setFluidType(amount);
	else if(kind->stackable && amount)
		count = amount;
	else if(kind->charges && amount)
		setCharges(amount);
}

boost::intrusive_ptr<Item> Item::clone() const
{
	boost::intrusive_ptr<Item> tmp = Item::CreateItem(kind->id, count);
	if(!tmp)
		return nullptr;

	tmp->_attributes = _attributes;

	return tmp;
}

void Item::copyAttributes(Item* item) {
	if (item == nullptr) {
		return;
	}

	_attributes = item->_attributes;

	_attributes.remove(ATTRIBUTE_DECAYING);
	_attributes.remove(ATTRIBUTE_DURATION);
}

void Item::onRemoved()
{
	if(raid)
	{
		raid->unRef();
		raid = nullptr;
	}

	ScriptEnviroment::removeTempItem(this);
	if(getUniqueId())
		ScriptEnviroment::removeUniqueThing(this);
}

void Item::setDefaultSubtype()
{
	count = 1;
	if(kind->charges)
		setCharges(kind->charges);
}


uint16_t Item::getClientID() const {
	return kind->clientId;
}


std::string Item::getDescription(int32_t lookDistance) const {
	return getDescription(kind, lookDistance, this);
}


std::string Item::getNameDescription() const {
	return getNameDescription(kind, this);
}


std::string Item::getWeightDescription() const {
	return getWeightDescription(getWeight(), kind->stackable, count);
}


Ammo_t Item::getAmmoType() const {return kind->ammoType;}
WeaponType_t Item::getWeaponType() const {return kind->weaponType;}
int32_t Item::getSlotPosition() const {return kind->slotPosition;}
slots_t Item::getWieldPosition() const {return kind->wieldPosition;}


int32_t Item::getMaxWriteLength() const {return kind->maxTextLen;}
uint64_t Item::getWorth() const {return static_cast<uint64_t>(getItemCount()) * kind->worth;}
int32_t Item::getThrowRange() const {return (isPickupable() ? 15 : 2);}

bool Item::forceSerialize() const {return kind->forceSerialize || canWriteText() || isContainer() || isBed() || isDoor();}

bool Item::hasSubType() const {return kind->hasSubType();}
bool Item::hasCharges() const {return kind->charges;}

bool Item::canRemove() const {return true;}
bool Item::canTransform() const {return true;}
bool Item::canWriteText() const {return kind->canWriteText;}

bool Item::isPushable() const {return !isNotMoveable();}
bool Item::isGroundTile() const {return kind->isGroundTile();}
bool Item::isContainer() const {return kind->isContainer();}
bool Item::isSplash() const {return kind->isSplash();}
bool Item::isFluidContainer() const {return (kind->isFluidContainer());}
bool Item::isDoor() const {return kind->isDoor();}
bool Item::isMagicField() const {return kind->isMagicField();}
bool Item::isTeleport() const {return kind->isTeleport();}
bool Item::isKey() const {return kind->isKey();}
bool Item::isDepot() const {return kind->isDepot();}
bool Item::isMailbox() const {return kind->isMailbox();}
bool Item::isTrashHolder() const {return kind->isTrashHolder();}
bool Item::isBed() const {return kind->isBed();}
bool Item::isRune() const {return kind->isRune();}
bool Item::isBlocking(const Creature* creature) const {return kind->blockSolid;}
bool Item::isStackable() const {return kind->stackable;}
bool Item::isAlwaysOnTop() const {return kind->alwaysOnTop;}
bool Item::isNotMoveable() const {return !kind->moveable;}
bool Item::isMoveable() const {return kind->moveable;}
bool Item::isPickupable() const {return kind->pickupable;}
bool Item::isUseable() const {return kind->useable;}
bool Item::isHangable() const {return kind->isHangable;}
bool Item::isRoteable() const {return kind->rotable && kind->rotateTo;}
bool Item::isWeapon() const {return (kind->weaponType != WEAPON_NONE);}
bool Item::isReadable() const {return kind->canReadText;}

uint32_t Item::getDefaultDuration() const {return kind->decayTime * 1000;}


ItemKindPC Item::getKind() const {
	return kind;
}


void Item::setKind(const ItemKindPC& kind) {
	assert(kind);

	bool previousStopTime = this->kind->stopTime;

	this->kind = kind;

	uint32_t newDuration = kind->decayTime * 1000;
	if(!newDuration && !kind->stopTime && kind->decayTo == -1)
	{
		_attributes.remove(ATTRIBUTE_DECAYING);
		_attributes.remove(ATTRIBUTE_DURATION);
	}

	_attributes.remove(ATTRIBUTE_CORPSEOWNER);

	if(newDuration > 0 && (!previousStopTime || !_attributes.contains(ATTRIBUTE_DURATION)))
	{
		setDecaying(DECAYING_FALSE);
		setDuration(newDuration);
	}
}

bool Item::floorChange(FloorChange_t change/* = CHANGE_NONE*/) const
{
	if(change < CHANGE_NONE)
		return kind->hasFloorChange(change);

	for(int32_t i = CHANGE_PRE_FIRST; i < CHANGE_LAST; ++i)
	{
		if(kind->hasFloorChange(static_cast<FloorChange_t>(i)))
			return true;
	}

	return false;
}

Player* Item::getHoldingPlayer()
{
	Cylinder* p = getParent();
	while(p)
	{
		if(p->getCreature())
			return p->getCreature()->getPlayer();

		p = p->getParent();
	}

	return nullptr;
}

const Player* Item::getHoldingPlayer() const
{
	return const_cast<Item*>(this)->getHoldingPlayer();
}

uint16_t Item::getSubType() const
{
	if(kind->isFluidContainer() || kind->isSplash())
		return getFluidType();

	if(kind->charges)
		return getCharges();

	return count;
}

void Item::setSubType(uint16_t n)
{
	if(kind->isFluidContainer() || kind->isSplash())
		setFluidType(n);
	else if(kind->charges)
		setCharges(n);
	else
		count = n;
}

Attr_ReadValue Item::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch(attr)
	{
		case ATTR_COUNT:
		{
			uint8_t _count;
			if(!propStream.GET_UCHAR(_count))
				return ATTR_READ_ERROR;

			setSubType((uint16_t)_count);
			break;
		}

		case ATTR_ACTION_ID:
		{
			uint16_t aid;
			if(!propStream.GET_USHORT(aid))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_AID, aid);
			break;
		}

		case ATTR_UNIQUE_ID:
		{
			uint16_t uid;
			if(!propStream.GET_USHORT(uid))
				return ATTR_READ_ERROR;

			setUniqueId(uid);
			break;
		}

		case ATTR_NAME:
		{
			std::string name;
			if(!propStream.GET_STRING(name))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_NAME, name);
			break;
		}

		case ATTR_PLURALNAME:
		{
			std::string name;
			if(!propStream.GET_STRING(name))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_PLURALNAME, name);
			break;
		}

		case ATTR_ARTICLE:
		{
			std::string article;
			if(!propStream.GET_STRING(article))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_ARTICLE, article);
			break;
		}

		case ATTR_ATTACK:
		{
			int32_t attack;
			if(!propStream.GET_ULONG((uint32_t&)attack))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_ATTACK, attack);
			break;
		}

		case ATTR_EXTRAATTACK:
		{
			int32_t attack;
			if(!propStream.GET_ULONG((uint32_t&)attack))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_EXTRAATTACK, attack);
			break;
		}

		case ATTR_DEFENSE:
		{
			int32_t defense;
			if(!propStream.GET_ULONG((uint32_t&)defense))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_DEFENSE, defense);
			break;
		}

		case ATTR_EXTRADEFENSE:
		{
			int32_t defense;
			if(!propStream.GET_ULONG((uint32_t&)defense))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_EXTRADEFENSE, defense);
			break;
		}

		case ATTR_ARMOR:
		{
			int32_t armor;
			if(!propStream.GET_ULONG((uint32_t&)armor))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_ARMOR, armor);
			break;
		}

		case ATTR_ATTACKSPEED:
		{
			int32_t attackSpeed;
			if(!propStream.GET_ULONG((uint32_t&)attackSpeed))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_ATTACKSPEED, attackSpeed);
			break;
		}

		case ATTR_HITCHANCE:
		{
			int32_t hitChance;
			if(!propStream.GET_ULONG((uint32_t&)hitChance))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_HITCHANCE, hitChance);
			break;
		}

		case ATTR_SCRIPTPROTECTED:
		{
			uint8_t protection;
			if(!propStream.GET_UCHAR(protection))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_SCRIPTPROTECTED, protection != 0);
			break;
		}

		case ATTR_TEXT:
		{
			std::string text;
			if(!propStream.GET_STRING(text))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_TEXT, text);
			break;
		}

		case ATTR_WRITTENDATE:
		{
			int32_t date;
			if(!propStream.GET_ULONG((uint32_t&)date))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_DATE, date);
			break;
		}

		case ATTR_WRITTENBY:
		{
			std::string writer;
			if(!propStream.GET_STRING(writer))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_WRITER, writer);
			break;
		}

		case ATTR_DESC:
		{
			std::string text;
			if(!propStream.GET_STRING(text))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_DESCRIPTION, text);
			break;
		}

		case ATTR_RUNE_CHARGES:
		{
			uint8_t charges;
			if(!propStream.GET_UCHAR(charges))
				return ATTR_READ_ERROR;

			setSubType((uint16_t)charges);
			break;
		}

		case ATTR_CHARGES:
		{
			uint16_t charges;
			if(!propStream.GET_USHORT(charges))
				return ATTR_READ_ERROR;

			setSubType(charges);
			break;
		}

		case ATTR_DURATION:
		{
			int32_t duration;
			if(!propStream.GET_ULONG((uint32_t&)duration))
				return ATTR_READ_ERROR;

			_attributes.set(ATTRIBUTE_DURATION, duration);
			break;
		}

		case ATTR_DECAYING_STATE:
		{
			uint8_t state;
			if(!propStream.GET_UCHAR(state))
				return ATTR_READ_ERROR;

			if((ItemDecayState_t)state != DECAYING_FALSE)
				_attributes.set(ATTRIBUTE_DECAYING, (int32_t)DECAYING_PENDING);

			break;
		}

		//these should be handled through derived classes
		//if these are called then something has changed in the items.otb since the map was saved
		//just read the values

		//Depot class
		case ATTR_DEPOT_ID:
		{
			uint16_t depot;
			if(!propStream.GET_USHORT(depot))
				return ATTR_READ_ERROR;

			break;
		}

		//Door class
		case ATTR_HOUSEDOORID:
		{
			uint8_t door;
			if(!propStream.GET_UCHAR(door))
				return ATTR_READ_ERROR;

			break;
		}

		//Teleport class
		case ATTR_TELE_DEST:
		{
			TeleportDest* dest;
			if(!propStream.GET_STRUCT(dest))
				return ATTR_READ_ERROR;

			break;
		}

		//Bed class
		case ATTR_SLEEPERGUID:
		{
			uint32_t sleeper;
			if(!propStream.GET_ULONG(sleeper))
				return ATTR_READ_ERROR;

			break;
		}

		case ATTR_SLEEPSTART:
		{
			uint32_t sleepStart;
			if(!propStream.GET_ULONG(sleepStart))
				return ATTR_READ_ERROR;

			break;
		}

		//Container class
		case ATTR_CONTAINER_ITEMS:
		{
			uint32_t _count;
			propStream.GET_ULONG(_count);
			return ATTR_READ_ERROR;
		}

		//ItemAttributes class
		case ATTR_ATTRIBUTE_MAP:
		{
			bool unique = _attributes.contains(ATTRIBUTE_UID);
			bool ret = unserializeMap(propStream);
			if(!unique && _attributes.contains(ATTRIBUTE_UID)) // unfortunately we have to do this
				ScriptEnviroment::addUniqueThing(this);

			// this attribute has a custom behavior as well
			if(getDecaying() != DECAYING_FALSE)
				setDecaying(DECAYING_PENDING);

			if(!ret)
				return ATTR_READ_ERROR;

			break;
		}

		default:
			return ATTR_READ_ERROR;
	}

	return ATTR_READ_CONTINUE;
}

bool Item::unserializeAttr(PropStream& propStream)
{
	uint8_t attrType = ATTR_END;
	while(propStream.GET_UCHAR(attrType) && attrType != ATTR_END)
	{
		switch(readAttr((AttrTypes_t)attrType, propStream))
		{
			case ATTR_READ_ERROR:
				return false;

			case ATTR_READ_END:
				return true;

			default:
				break;
		}
	}

	return true;
}

bool Item::hasProperty(enum ITEMPROPERTY prop) const
{
	switch(prop)
	{
		case BLOCKSOLID:
			if(kind->blockSolid)
				return true;

			break;

		case MOVEABLE:
			if(kind->moveable && (!isLoadedFromMap() || (!getUniqueId()
				&& (!getActionId() || getContainer()))))
				return true;

			break;

		case HASHEIGHT:
			if(kind->hasHeight)
				return true;

			break;

		case BLOCKPROJECTILE:
			if(kind->blockProjectile)
				return true;

			break;

		case BLOCKPATH:
			if(kind->blockPathFind)
				return true;

			break;

		case ISVERTICAL:
			if(kind->isVertical)
				return true;

			break;

		case ISHORIZONTAL:
			if(kind->isHorizontal)
				return true;

			break;

		case IMMOVABLEBLOCKSOLID:
			if(kind->blockSolid && (!kind->moveable || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case IMMOVABLEBLOCKPATH:
			if(kind->blockPathFind && (!kind->moveable || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case SUPPORTHANGABLE:
			if(kind->isHorizontal || kind->isVertical)
				return true;

			break;

		case IMMOVABLENOFIELDBLOCKPATH:
			if(!kind->isMagicField() && kind->blockPathFind && (!kind->moveable || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case NOFIELDBLOCKPATH:
			if(!kind->isMagicField() && kind->blockPathFind)
				return true;

			break;

		default:
			break;
	}

	return false;
}

double Item::getWeight() const
{
	if(isStackable())
		return kind->weight * std::max((int32_t)1, (int32_t)count);

	return kind->weight;
}

std::string Item::getDescription(const ItemKindPC& kind, int32_t lookDistance, const Item* item/* = nullptr*/,
		uint16_t subType/* = 0*/, bool addArticle/* = true*/)
{
	std::stringstream s;
	s << getNameDescription(kind, item, subType, addArticle);
	if(item)
		subType = item->getSubType();

	bool dot = true;
	if(kind->isRune())
	{
		s << "(";
		if(!kind->runeSpellName.empty())
			s << "\"" << kind->runeSpellName << "\", ";

		s << "Charges:" << subType << ")";
		if(kind->runeLevel > 0 || kind->runeMagLevel > 0 || (kind->vocationString != "" && kind->wieldInfo == 0))
		{
			s << "." << std::endl << "It can only be used";
			if(kind->vocationString != "" && kind->wieldInfo == 0)
				s << " by " << kind->vocationString;

			bool begin = true;
			if(kind->runeLevel > 0)
			{
				begin = false;
				s << " with level " << kind->runeLevel;
			}

			if(kind->runeMagLevel > 0)
			{
				begin = false;
				s << " " << (begin ? "with" : "and") << " magic level " << kind->runeMagLevel;
			}

			if(!begin)
				s << " or higher";
		}
	}
	else if(kind->weaponType != WEAPON_NONE)
	{
		bool begin = true;
		if(kind->weaponType == WEAPON_DIST && kind->ammoType != AMMO_NONE)
		{
			begin = false;
			s << " (Range:" << int32_t(item ? item->getShootRange() : kind->shootRange);
			if(kind->attack || kind->extraAttack || (item && (item->getAttack() || item->getExtraAttack())))
			{
				s << ", Atk " << std::showpos << int32_t(item ? item->getAttack() : kind->attack);
				if(kind->extraAttack || (item && item->getExtraAttack()))
					s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : kind->extraAttack) << std::noshowpos;
			}

			if(kind->hitChance != -1 || (item && item->getHitChance() != -1))
				s << ", Hit% " << std::showpos << (item ? item->getHitChance() : kind->hitChance) << std::noshowpos;
		}
		else if(kind->weaponType != WEAPON_AMMO && kind->weaponType != WEAPON_WAND)
		{
			if(kind->attack || kind->extraAttack || (item && (item->getAttack() || item->getExtraAttack())))
			{
				begin = false;
				s << " (Atk:";
				if(kind->abilities.elementType != COMBAT_NONE && kind->decayTo < 1)
				{
					s << std::max((int32_t)0, int32_t((item ? item->getAttack() : kind->attack) - kind->abilities.elementDamage));
					if(kind->extraAttack || (item && item->getExtraAttack()))
						s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : kind->extraAttack) << std::noshowpos;

					s << " physical + " << kind->abilities.elementDamage << " " << getCombatName(kind->abilities.elementType);
				}
				else
				{
					s << int32_t(item ? item->getAttack() : kind->attack);
					if(kind->extraAttack || (item && item->getExtraAttack()))
						s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : kind->extraAttack) << std::noshowpos;
				}
			}

			if(kind->defense || kind->extraDefense || (item && (item->getDefense() || item->getExtraDefense())))
			{
				if(begin)
				{
					begin = false;
					s << " (";
				}
				else
					s << ", ";

				s << "Def:" << int32_t(item ? item->getDefense() : kind->defense);
				if(kind->extraDefense || (item && item->getExtraDefense()))
					s << " " << std::showpos << int32_t(item ? item->getExtraDefense() : kind->extraDefense) << std::noshowpos;
			}
		}
		else if (kind->weaponType == WEAPON_WAND) {
			if (kind->attack > 0 || (item && item->getAttack())) {
				begin = false;
				s << " (Bonus Dmg:";
				s << int32_t(item ? item->getAttack() : kind->attack);
			}
		}

		for(uint16_t i = SKILL_FIRST; i <= SKILL_LAST; i++)
		{
			if(!kind->abilities.skills[i])
				continue;

			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << getSkillName(i) << " " << std::showpos << (int32_t)kind->abilities.skills[i] << std::noshowpos;
		}

		if(kind->abilities.stats[STAT_MAGICLEVEL])
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "magic level " << std::showpos << (int32_t)kind->abilities.stats[STAT_MAGICLEVEL] << std::noshowpos;
		}

		int32_t show = kind->abilities.getAbsorb(COMBAT_PHYSICALDAMAGE);
		for(uint32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(kind->abilities.getAbsorb((CombatType_t)i) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(uint32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t absorb = kind->abilities.getAbsorb((CombatType_t)i);
				if(absorb <= 0)
					continue;

				if(tmp)
				{
					if(begin)
					{
						begin = false;
						s << " (";
					}
					else
						s << ", ";

					tmp = false;
					s << "protection ";
				}
				else
					s << ", ";

				s << getCombatName((CombatType_t)i) << " " << absorb << "%";
			}
		}
		else
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "protection all " << show << "%";
		}

		if(kind->abilities.speed)
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "speed " << std::showpos << (int32_t)(kind->abilities.speed / 2) << std::noshowpos;
		}

		if(!begin)
			s << ")";
	}
	else if(kind->armor || (item && item->getArmor()) || kind->showAttributes)
	{
		int32_t tmp = kind->armor;
		if(item)
			tmp = item->getArmor();

		bool begin = true;
		if(tmp)
		{
			s << " (Arm:" << tmp;
			begin = false;
		}

		for(uint16_t i = SKILL_FIRST; i <= SKILL_LAST; i++)
		{
			if(!kind->abilities.skills[i])
				continue;

			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << getSkillName(i) << " " << std::showpos << (int32_t)kind->abilities.skills[i] << std::noshowpos;
		}

		if(kind->abilities.stats[STAT_MAGICLEVEL])
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "magic level " << std::showpos << (int32_t)kind->abilities.stats[STAT_MAGICLEVEL] << std::noshowpos;
		}

		int32_t show = kind->abilities.getAbsorb(COMBAT_PHYSICALDAMAGE);
		for(int32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(kind->abilities.getAbsorb((CombatType_t)i) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t absorb = kind->abilities.getAbsorb((CombatType_t)i);
				if(absorb <= 0)
					continue;

				if(tmp)
				{
					tmp = false;
					if(begin)
					{
						begin = false;
						s << " (";
					}
					else
						s << ", ";

					s << "protection ";
				}
				else
					s << ", ";

				s << getCombatName((CombatType_t)i) << " " << absorb << "%";
			}
		}
		else
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "protection all " << show << "%";
		}

		show = kind->abilities.getReflect(COMBAT_PHYSICALDAMAGE, REFLECT_CHANCE);
		for(int32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(kind->abilities.getReflect((CombatType_t)i, REFLECT_CHANCE) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t chance = kind->abilities.getReflect((CombatType_t)i, REFLECT_CHANCE);
				if(chance <= 0)
					continue;

				if(tmp)
				{
					tmp = false;
					if(begin)
					{
						begin = false;
						s << " (";
					}
					else
						s << ", ";

					s << "reflect ";
				}
				else
					s << ", ";

				std::string ss = "no";

				int16_t percent = kind->abilities.getReflect((CombatType_t)i, REFLECT_PERCENT);
				if(percent > 99)
					ss = "all";
				else if(percent >= 75)
					ss = "huge";
				else if(percent >= 50)
					ss = "medium";
				else if(percent >= 25)
					ss = "small";
				else if(percent > 0)
					ss = "tiny";

				s << getCombatName((CombatType_t)i) << " " << chance << "% for " << ss;
			}

			if(!tmp)
				s << " damage";
		}
		else
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "reflect all " << show << "% for mixed damage";
		}

		if(kind->abilities.speed)
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "speed " << std::showpos << (int32_t)(kind->abilities.speed / 2) << std::noshowpos;
		}

		if(!begin)
			s << ")";
	}
	else if(kind->isContainer())
		s << " (Vol:" << (int32_t)kind->maxItems << ")";
	else if(kind->isKey())
		s << " (Key:" << (item ? (int32_t)item->getActionId() : 0) << ")";
	else if(kind->isFluidContainer())
	{
		ItemKindPC subKind = server.items()[subType];
		if(subKind)
			s << " of " << (subKind->name.empty() ? "unknown" : subKind->name);
		else
			s << ". It is empty";
	}
	else if(kind->isSplash())
	{
		s << " of ";

		ItemKindPC subKind = server.items()[subType];
		if(subKind && !subKind->name.empty())
			s << subKind->name;
		else
			s << "unknown";
	}
	else if(kind->allowDistRead)
	{
		s << std::endl;
		if(item && !item->getText().empty())
		{
			if(lookDistance <= 4)
			{
				if(!item->getWriter().empty())
				{
					s << item->getWriter() << " wrote";
					time_t date = item->getDate();
					if(date > 0)
						s << " on " << formatDate(date);

					s << ": ";
				}
				else
					s << "You read: ";

				std::string text = item->getText();
				s << text;
				if(!text.empty())
				{
					char end = *text.rbegin();
					if(end == '?' || end == '!' || end == '.')
						dot = false;
				}
			}
			else
				s << "You are too far away to read it";
		}
		else
			s << "Nothing is written on it";
	}
	else if(kind->levelDoor && item && item->getActionId() >= (int32_t)kind->levelDoor && item->getActionId()
		<= ((int32_t)kind->levelDoor + server.configManager().getNumber(ConfigManager::MAXIMUM_DOOR_LEVEL)))
		s << " for level " << item->getActionId() - kind->levelDoor;

	if(kind->showCharges)
		s << " that has " << subType << " charge" << (subType != 1 ? "s" : "") << " left";

	if(kind->showDuration)
	{
		if(item && item->_attributes.contains(ATTRIBUTE_DURATION))
		{
			int32_t duration = item->getDuration() / 1000;
			s << " that has energy for ";

			if(duration >= 120)
				s << duration / 60 << " minutes left";
			else if(duration > 60)
				s << "1 minute left";
			else
				s << " less than a minute left";
		}
		else
			s << " that is brand-new";
	}

	if(dot)
		s << ".";

	if(kind->wieldInfo)
	{
		s << std::endl << "It can only be wielded properly by ";

		if(kind->wieldInfo & WIELDINFO_VOCREQ)
			s << kind->vocationString;
		else
			s << "players";

		if(kind->wieldInfo & WIELDINFO_LEVEL)
			s << " of level " << (int32_t)kind->minReqLevel << " or higher";

		if(kind->wieldInfo & WIELDINFO_MAGLV)
		{
			if(kind->wieldInfo & WIELDINFO_LEVEL)
				s << " and";
			else
				s << " of";

			s << " magic level " << (int32_t)kind->minReqMagicLevel << " or higher";
		}

		s << ".";
	}

	if(lookDistance <= 1 && kind->pickupable)
	{
		std::string tmp;
		if(!item)
			tmp = getWeightDescription(kind->weight, kind->stackable, subType);
		else
			tmp = item->getWeightDescription();

		if(!tmp.empty())
			s << std::endl << tmp;
	}

	if(kind->abilities.elementType != COMBAT_NONE && kind->decayTo > 0)
	{
		s << std::endl << "It is temporarily enchanted with " << getCombatName(kind->abilities.elementType) << " (";
		s << std::max((int32_t)0, int32_t((item ? item->getAttack() : kind->attack) - kind->abilities.elementDamage));
		if(kind->extraAttack || (item && item->getExtraAttack()))
			s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : kind->extraAttack) << std::noshowpos;

		s << " physical + " << kind->abilities.elementDamage << " " << getCombatName(kind->abilities.elementType) << " damage).";
	}

	std::string str;
	if(item && !item->getSpecialDescription().empty())
		str = item->getSpecialDescription();
	else if(!kind->description.empty() && lookDistance <= 1)
		str = kind->description;

	if(str.empty())
		return s.str();

	if(str.find("|PLAYERNAME|") != std::string::npos)
	{
		std::string tmp = "You";
		if(item)
		{
			if(const Player* player = item->getHoldingPlayer())
				tmp = player->getName();
		}

		replaceString(str, "|PLAYERNAME|", tmp);
	}

	if(str.find("|TIME|") != std::string::npos || str.find("|DATE|") != std::string::npos || str.find(
		"|DAY|") != std::string::npos || str.find("|MONTH|") != std::string::npos || str.find(
		"|YEAR|") != std::string::npos || str.find("|HOUR|") != std::string::npos || str.find(
		"|MINUTES|") != std::string::npos || str.find("|SECONDS|") != std::string::npos ||
		str.find("|WEEKDAY|") != std::string::npos || str.find("|YEARDAY|") != std::string::npos)
	{
		time_t now = time(nullptr);
		tm* ts = localtime(&now);

		std::stringstream ss;
		ss << ts->tm_sec;
		replaceString(str, "|SECONDS|", ss.str());

		ss.str("");
		ss << ts->tm_min;
		replaceString(str, "|MINUTES|", ss.str());

		ss.str("");
		ss << ts->tm_hour;
		replaceString(str, "|HOUR|", ss.str());

		ss.str("");
		ss << ts->tm_mday;
		replaceString(str, "|DAY|", ss.str());

		ss.str("");
		ss << (ts->tm_mon + 1);
		replaceString(str, "|MONTH|", ss.str());

		ss.str("");
		ss << (ts->tm_year + 1900);
		replaceString(str, "|YEAR|", ss.str());

		ss.str("");
		ss << ts->tm_wday;
		replaceString(str, "|WEEKDAY|", ss.str());

		ss.str("");
		ss << ts->tm_yday;
		replaceString(str, "|YEARDAY|", ss.str());

		ss.str("");
		ss << ts->tm_hour << ":" << ts->tm_min << ":" << ts->tm_sec;
		replaceString(str, "|TIME|", ss.str());

		ss.str("");
		replaceString(str, "|DATE|", formatDateShort(now, true));
	}

	s << std::endl << str;
	return s.str();
}

std::string Item::getNameDescription(const ItemKindPC& kind, const Item* item/* = nullptr*/, uint16_t subType/* = 0*/, bool addArticle/* = true*/)
{
	if(item)
		subType = item->getSubType();

	std::stringstream s;
	if(!kind->name.empty() || (item && !item->getName().empty()))
	{
		if(subType > 1 && kind->stackable && kind->showCount)
			s << subType << " " << (item ? item->getPluralName() : kind->pluralName);
		else
		{
			if(addArticle)
			{
				if(item && !item->getArticle().empty())
					s << item->getArticle() << " ";
				else if(!kind->article.empty())
					s << kind->article << " ";
			}

			s << (item ? item->getName() : kind->name);
		}
	}
	else
		s << "an item of type " << kind->id << ", please report it to gamemaster";

	return s.str();
}

std::string Item::getWeightDescription(double weight, bool stackable, uint32_t count/* = 1*/)
{
	if(weight <= 0)
		return "";

	std::stringstream s;
	if(stackable && count > 1)
		s << "They weigh " << std::fixed << std::setprecision(2) << weight << " oz.";
	else
		s << "It weighs " << std::fixed << std::setprecision(2) << weight << " oz.";

	return s.str();
}

void Item::setActionId(int32_t aid)
{
	if(getActionId())
		server.moveEvents().onRemoveTileItem(getTile(), this);

	_attributes.set(ATTRIBUTE_AID, aid);
	server.moveEvents().onAddTileItem(getTile(), this);
}

void Item::resetActionId()
{
	if(!getActionId())
		return;

	_attributes.remove(ATTRIBUTE_AID);
	server.moveEvents().onAddTileItem(getTile(), this);
}

void Item::setUniqueId(int32_t uid)
{
	if(getUniqueId())
		return;

	_attributes.set(ATTRIBUTE_UID, uid);
	ScriptEnviroment::addUniqueThing(this);
}

bool Item::canDecay(bool ignoreRemoved)
{
	if(!ignoreRemoved && isRemoved())
		return false;

	if(isLoadedFromMap() && (getUniqueId() || (getActionId() && getContainer())))
		return false;

	return kind->decayTo >= 0 && kind->decayTime;
}

void Item::getLight(LightInfo& lightInfo)
{
	lightInfo.color = kind->lightColor;
	lightInfo.level = kind->lightLevel;
}

void Item::__startDecaying()
{
	server.game().startDecay(this);
}

std::string Item::getName() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_NAME);
	if(v)
		return *v;

	return kind->name;
}

std::string Item::getPluralName() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_PLURALNAME);
	if(v)
		return *v;

	return kind->pluralName;
}

std::string Item::getArticle() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_ARTICLE);
	if(v)
		return *v;

	return kind->article;
}

bool Item::isScriptProtected() const
{
	const bool* v = _attributes.getBoolean(ATTRIBUTE_SCRIPTPROTECTED);
	if(v)
		return *v;

	return false;
}

int32_t Item::getAttack() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_ATTACK);
	if(v)
		return *v;

	return kind->attack;
}

int32_t Item::getExtraAttack() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_EXTRAATTACK);
	if(v)
		return *v;

	return kind->extraAttack;
}

int32_t Item::getDefense() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_DEFENSE);
	if(v)
		return *v;

	return kind->defense;
}

int32_t Item::getExtraDefense() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_EXTRADEFENSE);
	if(v)
		return *v;

	return kind->extraDefense;
}

int32_t Item::getArmor() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_ARMOR);
	if(v)
		return *v;

	return kind->armor;
}

int32_t Item::getAttackSpeed() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_ATTACKSPEED);
	if(v)
		return *v;

	return kind->attackSpeed;
}

int32_t Item::getHitChance() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_HITCHANCE);
	if(v)
		return *v;

	return kind->hitChance;
}

int32_t Item::getShootRange() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_SHOOTRANGE);
	if(v)
		return *v;

	return kind->shootRange;
}

void Item::decreaseDuration(int32_t time)
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_DURATION);
	if(v)
		_attributes.set(ATTRIBUTE_DURATION, *v - time);
}

int32_t Item::getDuration() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_DURATION);
	if(v)
		return *v;

	return 0;
}

const std::string& Item::getSpecialDescription() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_DESCRIPTION);
	if(v)
		return *v;

	return EMPTY_STRING;
}

const std::string& Item::getText() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_TEXT);
	if(v)
		return *v;

	return EMPTY_STRING;
}

time_t Item::getDate() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_DATE);
	if(v)
		return (time_t)*v;

	return 0;
}

const std::string& Item::getWriter() const
{
	const std::string* v = _attributes.getString(ATTRIBUTE_WRITER);
	if(v)
		return *v;

	return EMPTY_STRING;
}

int32_t Item::getActionId() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_AID);
	if(v)
		return *v;

	return 0;
}

int32_t Item::getUniqueId() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_UID);
	if(v)
		return *v;

	return 0;
}

uint16_t Item::getCharges() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_CHARGES);
	if(v && *v >= 0)
		return (uint16_t)*v;

	return 0;
}

uint16_t Item::getFluidType() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_FLUIDTYPE);
	if(v && *v >= 0)
		return (uint16_t)*v;

	return 0;
}

uint32_t Item::getOwner() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_OWNER);
	if(v)
		return (uint32_t)*v;

	return 0;
}

uint32_t Item::getCorpseOwner()
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_CORPSEOWNER);
	if(v)
		return (uint32_t)*v;

	return 0;
}

ItemDecayState_t Item::getDecaying() const
{
	const int32_t* v = _attributes.getInteger(ATTRIBUTE_DECAYING);
	if(v)
		return (ItemDecayState_t)*v;

	return DECAYING_FALSE;
}

uint32_t Item::countByType(const Item* item, int32_t checkType, bool multiCount)
{
	if(checkType != -1 && checkType != (int32_t)item->getSubType())
		return 0;

	if(multiCount)
		return item->getItemCount();

	if(item->isRune())
		return item->getCharges();

	return item->getItemCount();
}


typedef enum {
	SerializedTypeNone = 0,
	SerializedTypeString = 1,
	SerializedTypeInt = 2,
	SerializedTypeFloat = 3,
	SerializedTypeBool = 4
} SerializedType;



bool Item::serializeAttr(PropWriteStream& stream) const
{
	if(isStackable() || isFluidContainer() || isSplash())
	{
		stream.ADD_UCHAR(ATTR_COUNT);
		stream.ADD_UCHAR((uint8_t)getSubType());
	}

	auto entries = _attributes.getEntries();
	if (entries != nullptr && !entries->empty()) {
		stream.ADD_UCHAR(ATTR_ATTRIBUTE_MAP);
		stream.ADD_USHORT((uint16_t)std::min((size_t)0xFFFF, entries->size()));

		for (auto& it : *entries) {
			auto& attribute = *it.first;

			stream.ADD_STRING(attribute.getName());

			switch (attribute.getType()) {
			case attributes::Type::BOOLEAN:
				stream.ADD_UCHAR((uint8_t)SerializedTypeBool);
				stream.ADD_UCHAR(boost::any_cast<bool>(it.second) ? 1 : 0);
				break;

			case attributes::Type::FLOAT:
				stream.ADD_UCHAR((uint8_t)SerializedTypeFloat);
				stream.ADD_VALUE(boost::any_cast<float>(it.second));
				break;

			case attributes::Type::INTEGER:
				stream.ADD_UCHAR((uint8_t)SerializedTypeInt);
				stream.ADD_VALUE(boost::any_cast<int32_t>(it.second));
				break;

			case attributes::Type::STRING:
				stream.ADD_UCHAR((uint8_t)SerializedTypeString);
				stream.ADD_LSTRING(boost::any_cast<const std::string&>(it.second));
				break;

			default:
				stream.ADD_UCHAR((uint8_t)SerializedTypeNone);
			}
		}
	}

	return true;
}


bool Item::unserializeMap(PropStream& stream) {
	uint16_t n;
	if(!stream.GET_USHORT(n))
		return true;

	while(n--)
	{
		std::string key;
		if(!stream.GET_STRING(key))
			return false;


		uint8_t type = 0;
		stream.GET_UCHAR(type);

		switch (type)
		{
			case SerializedTypeString:
			{
				std::string v;
				if(!stream.GET_LSTRING(v))
					return false;

				_attributes.set(key, v);
				break;
			}
			case SerializedTypeInt:
			{
				int32_t v;
				if(!stream.GET_VALUE(v))
					return false;

				_attributes.set(key, v);
				break;
			}
			case SerializedTypeFloat:
			{
				float v;
				if(!stream.GET_VALUE(v))
					return false;

				_attributes.set(key, v);
				break;
			}
			case SerializedTypeBool:
			{
				uint8_t v;
				if(!stream.GET_UCHAR(v))
					return false;

				_attributes.set(key, v != 0);
				break;
			}
		}
	}

	return true;
}
