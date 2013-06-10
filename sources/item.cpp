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
#include "item/Class.hpp"
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

using namespace ts;
using namespace ts::item;

LOGGER_DEFINITION(Item);



Item::DynamicAttributeValues& Item::getDynamicAttributes() {
	return _dynamicAttributes;
}


const Item::DynamicAttributeValues& Item::getDynamicAttributes() const {
	return _dynamicAttributes;
}


KindId Item::getId() const {
	return _kind->getId();
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

	auto _kind = server.items()[kindId];
	if (_kind == nullptr) {
		LOGe("Cannot create item with unknown id " << kindId << ".");
		return nullptr;
	}

	auto item = _kind->newItem();
	if (amount > 0) {
		item->setSubType(amount);
	}

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
				item->_dynamicAttributes.set(v[0], atoi(v[1].c_str()));
			else
				item->_dynamicAttributes.set(v[0], v[1]);
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

Item::Item(const KindPC& _kind):
	_dynamicAttributes(_kind->getClass().getDynamicAttributeScheme()), _kind(_kind)
{
	assert(_kind);

	count = 1;
	raid = nullptr;
	loadedFromMap = false;

	if (_kind->needsCharges()) {
		setCharges(_kind->getCharges());
	}

	setDefaultDuration();
	setDefaultSubtype();
}


boost::intrusive_ptr<Item> Item::clone() const {
	boost::intrusive_ptr<Item> item = _kind->newItem();
	if (item == nullptr) {
		return nullptr;
	}

	item->setSubType(getSubType());
	item->_dynamicAttributes = _dynamicAttributes;

	return item;
}


void Item::copyAttributes(Item* item) {
	if (item == nullptr) {
		return;
	}

	_dynamicAttributes = item->_dynamicAttributes;

	_dynamicAttributes.remove(Kind::ATTRIBUTE_DECAYING);
	_dynamicAttributes.remove(Kind::ATTRIBUTE_DURATION);
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
	if(_kind->needsCharges())
		setCharges(_kind->getCharges());
}


uint16_t Item::getClientID() const {
	return _kind->getClientId();
}


std::string Item::getDescription(int32_t lookDistance) const {
	return getDescription(*_kind, lookDistance, this);
}


std::string Item::getNameDescription() const {
	return getNameDescription(*_kind, this);
}


std::string Item::getWeightDescription() const {
	return getWeightDescription(getWeight(), _kind->isStackable(), count);
}


int32_t Item::getSlotPosition() const {return _kind->getEquipmentSlot();}


int32_t Item::getMaxWriteLength() const {return _kind->getMaximumTextLength();}
uint64_t Item::getWorth() const {return static_cast<uint64_t>(getItemCount()) * _kind->getWorth();}
int32_t Item::getThrowRange() const {return (isPickupable() ? 15 : 2);}

bool Item::forceSerialize() const {return _kind->isPersistent() || canWriteText() || isContainer() || isBed() || isDoor();}

bool Item::hasSubType() const {return isFluidContainer() || isSplash() || isStackable() || hasCharges();}

bool Item::hasCharges() const {return _kind->needsCharges();}

bool Item::canRemove() const {return true;}
bool Item::canTransform() const {return true;}
bool Item::canWriteText() const {return _kind->isWritable();}

bool Item::isPushable() const {return !isNotMoveable();}
#warning isGroundTile from item group
bool Item::isGroundTile() const {return false;}
#warning isContainer from item group
bool Item::isContainer() const {return false;}
#warning isSplash from item group
bool Item::isSplash() const {return false;}
bool Item::isFluidContainer() const {return (_kind->containsFluid());}
#warning rune to own class - what was ItemKind::clientCharges?
bool Item::isRune() const {return false;}
bool Item::isBlocking(const Creature* creature) const {return _kind->blocksSolids();}
bool Item::isStackable() const {return _kind->isStackable();}
bool Item::isAlwaysOnTop() const {return _kind->isAlwaysOnTop();}
bool Item::isNotMoveable() const {return !_kind->isMovable();}
bool Item::isMoveable() const {return _kind->isMovable();}
bool Item::isPickupable() const {return _kind->isPocketable();}
bool Item::isUseable() const {return _kind->isUsable();}
bool Item::isHangable() const {return _kind->getHangableType() == HangableType::HANGING;}
bool Item::isRoteable() const {return _kind->isRotatable();}
bool Item::isReadable() const {return _kind->isReadable();}

Duration Item::getDefaultDuration() const {return _kind->getLifetime();}


const Kind& Item::getKind() const {
	return *_kind;
}


#warning FIXME
//void Item::setKind(const ItemKindPC& _kind) {
//	assert(_kind);
//
//	bool previousStopTime = this->_kind->stopTime;
//
//	this->_kind = _kind;
//
//	uint32_t newDuration = _kind->decayTime * 1000;
//	if(!newDuration && !_kind->stopTime && _kind->decayTo == -1)
//	{
//		_dynamicAttributes.remove(Kind::ATTRIBUTE_DECAYING);
//		_dynamicAttributes.remove(Kind::ATTRIBUTE_DURATION);
//	}
//
//	_dynamicAttributes.remove(Kind::ATTRIBUTE_CORPSEOWNER);
//
//	if(newDuration > 0 && (!previousStopTime || !_dynamicAttributes.contains(Kind::ATTRIBUTE_DURATION)))
//	{
//		setDecaying(DECAYING_FALSE);
//		setDuration(newDuration);
//	}
//}

bool Item::floorChange(FloorChange_t change/* = CHANGE_NONE*/) const
{
	if(change < CHANGE_NONE)
		return _kind->hasFloorChanges();

	for(int32_t i = CHANGE_PRE_FIRST; i < CHANGE_LAST; ++i)
	{
		if(_kind->hasFloorChange(getDeprecatedFloorChange(static_cast<FloorChange_t>(i))))
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
	if(isFluidContainer() || isSplash())
		return getFluidType();

	if(hasCharges())
		return getCharges();

	return count;
}

void Item::setSubType(uint16_t n)
{
	if(isFluidContainer() || isSplash())
		setFluidType(n);
	else if(hasCharges())
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

			_dynamicAttributes.set(Kind::ATTRIBUTE_AID, aid);
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

			_dynamicAttributes.set(Kind::ATTRIBUTE_NAME, name);
			break;
		}

		case ATTR_PLURALNAME:
		{
			std::string name;
			if(!propStream.GET_STRING(name))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_PLURALNAME, name);
			break;
		}

		case ATTR_ARTICLE:
		{
			std::string article;
			if(!propStream.GET_STRING(article))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_ARTICLE, article);
			break;
		}

		case ATTR_ATTACK:
		{
			int32_t attack;
			if(!propStream.GET_ULONG((uint32_t&)attack))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_ATTACK, attack);
			break;
		}

		case ATTR_EXTRAATTACK:
		{
			int32_t attack;
			if(!propStream.GET_ULONG((uint32_t&)attack))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_EXTRAATTACK, attack);
			break;
		}

		case ATTR_DEFENSE:
		{
			int32_t defense;
			if(!propStream.GET_ULONG((uint32_t&)defense))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_DEFENSE, defense);
			break;
		}

		case ATTR_EXTRADEFENSE:
		{
			int32_t defense;
			if(!propStream.GET_ULONG((uint32_t&)defense))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_EXTRADEFENSE, defense);
			break;
		}

		case ATTR_ARMOR:
		{
			int32_t armor;
			if(!propStream.GET_ULONG((uint32_t&)armor))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_ARMOR, armor);
			break;
		}

		case ATTR_ATTACKSPEED:
		{
			int32_t attackSpeed;
			if(!propStream.GET_ULONG((uint32_t&)attackSpeed))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_ATTACKSPEED, attackSpeed);
			break;
		}

		case ATTR_HITCHANCE:
		{
			int32_t hitChance;
			if(!propStream.GET_ULONG((uint32_t&)hitChance))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_HITCHANCE, hitChance);
			break;
		}

		case ATTR_SCRIPTPROTECTED:
		{
			uint8_t protection;
			if(!propStream.GET_UCHAR(protection))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_SCRIPTPROTECTED, protection != 0);
			break;
		}

		case ATTR_TEXT:
		{
			std::string text;
			if(!propStream.GET_STRING(text))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_TEXT, text);
			break;
		}

		case ATTR_WRITTENDATE:
		{
			int32_t date;
			if(!propStream.GET_ULONG((uint32_t&)date))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_DATE, date);
			break;
		}

		case ATTR_WRITTENBY:
		{
			std::string writer;
			if(!propStream.GET_STRING(writer))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_WRITER, writer);
			break;
		}

		case ATTR_DESC:
		{
			std::string text;
			if(!propStream.GET_STRING(text))
				return ATTR_READ_ERROR;

			_dynamicAttributes.set(Kind::ATTRIBUTE_DESCRIPTION, text);
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

			_dynamicAttributes.set(Kind::ATTRIBUTE_DURATION, duration);
			break;
		}

		case ATTR_DECAYING_STATE:
		{
			uint8_t state;
			if(!propStream.GET_UCHAR(state))
				return ATTR_READ_ERROR;

			if((ItemDecayState_t)state != DECAYING_FALSE)
				_dynamicAttributes.set(Kind::ATTRIBUTE_DECAYING, (int32_t)DECAYING_PENDING);

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
			bool unique = _dynamicAttributes.contains(Kind::ATTRIBUTE_UID);
			bool ret = unserializeMap(propStream);
			if(!unique && _dynamicAttributes.contains(Kind::ATTRIBUTE_UID)) // unfortunately we have to do this
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
			if(_kind->blocksSolids())
				return true;

			break;

		case MOVEABLE:
			if(_kind->isMovable() && (!isLoadedFromMap() || (!getUniqueId()
				&& (!getActionId() || getContainer()))))
				return true;

			break;

		case HASHEIGHT:
			if(_kind->isElevated())
				return true;

			break;

		case BLOCKPROJECTILE:
			if(_kind->blocksProjectiles())
				return true;

			break;

		case BLOCKPATH:
			if(_kind->blocksPathfinding())
				return true;

			break;

		case ISVERTICAL:
			if(_kind->getHangableType() == HangableType::VERTICAL_WALL)
				return true;

			break;

		case ISHORIZONTAL:
			if(_kind->getHangableType() == HangableType::HORIZONTAL_WALL)
				return true;

			break;

		case IMMOVABLEBLOCKSOLID:
			if(_kind->blocksSolids() && (!_kind->isMovable() || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case IMMOVABLEBLOCKPATH:
			if(_kind->blocksPathfinding() && (!_kind->isMovable() || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case SUPPORTHANGABLE:
			if(_kind->getHangableType() == HangableType::HORIZONTAL_WALL || _kind->getHangableType() == HangableType::VERTICAL_WALL)
				return true;

			break;

		case IMMOVABLENOFIELDBLOCKPATH:
			if(!_kind->isMagicField() && _kind->blocksPathfinding() && (!_kind->isMovable() || (isLoadedFromMap() &&
				(getUniqueId() || (getActionId() && getContainer())))))
				return true;

			break;

		case NOFIELDBLOCKPATH:
			if(!_kind->isMagicField() && _kind->blocksPathfinding())
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
		return _kind->getWeight() * std::max((int32_t)1, (int32_t)count);

	return _kind->getWeight();
}

std::string Item::getDescription(const Kind& _kind, int32_t lookDistance, const Item* item/* = nullptr*/,
		uint16_t subType/* = 0*/, bool addArticle/* = true*/)
{
	std::stringstream s;
	s << getNameDescription(_kind, item, subType, addArticle);
	if(item)
		subType = item->getSubType();

	bool dot = true;
	if(_kind.isRune())
	{
		s << "(";
		if(!_kind.runeSpellName.empty())
			s << "\"" << _kind.runeSpellName << "\", ";

		s << "Charges:" << subType << ")";
		if(_kind.runeLevel > 0 || _kind.runeMagLevel > 0 || (_kind.vocationString != "" && _kind.wieldInfo == 0))
		{
			s << "." << std::endl << "It can only be used";
			if(_kind.vocationString != "" && _kind.wieldInfo == 0)
				s << " by " << _kind.vocationString;

			bool begin = true;
			if(_kind.runeLevel > 0)
			{
				begin = false;
				s << " with level " << _kind.runeLevel;
			}

			if(_kind.runeMagLevel > 0)
			{
				begin = false;
				s << " " << (begin ? "with" : "and") << " magic level " << _kind.runeMagLevel;
			}

			if(!begin)
				s << " or higher";
		}
	}
	else if(_kind.weaponType != WEAPON_NONE)
	{
		bool begin = true;
		if(_kind.weaponType == WEAPON_DIST && _kind.ammoType != AMMO_NONE)
		{
			begin = false;
			s << " (Range:" << int32_t(item ? item->getShootRange() : _kind.shootRange);
			if(_kind.attack || _kind.extraAttack || (item && (item->getAttack() || item->getExtraAttack())))
			{
				s << ", Atk " << std::showpos << int32_t(item ? item->getAttack() : _kind.attack);
				if(_kind.extraAttack || (item && item->getExtraAttack()))
					s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : _kind.extraAttack) << std::noshowpos;
			}

			if(_kind.hitChance != -1 || (item && item->getHitChance() != -1))
				s << ", Hit% " << std::showpos << (item ? item->getHitChance() : _kind.hitChance) << std::noshowpos;
		}
		else if(_kind.weaponType != WEAPON_AMMO && _kind.weaponType != WEAPON_WAND)
		{
			if(_kind.attack || _kind.extraAttack || (item && (item->getAttack() || item->getExtraAttack())))
			{
				begin = false;
				s << " (Atk:";
				if(_kind.abilities.elementType != COMBAT_NONE && _kind.decayTo < 1)
				{
					s << std::max((int32_t)0, int32_t((item ? item->getAttack() : _kind.attack) - _kind.abilities.elementDamage));
					if(_kind.extraAttack || (item && item->getExtraAttack()))
						s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : _kind.extraAttack) << std::noshowpos;

					s << " physical + " << _kind.abilities.elementDamage << " " << getCombatName(_kind.abilities.elementType);
				}
				else
				{
					s << int32_t(item ? item->getAttack() : _kind.attack);
					if(_kind.extraAttack || (item && item->getExtraAttack()))
						s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : _kind.extraAttack) << std::noshowpos;
				}
			}

			if(_kind.defense || _kind.extraDefense || (item && (item->getDefense() || item->getExtraDefense())))
			{
				if(begin)
				{
					begin = false;
					s << " (";
				}
				else
					s << ", ";

				s << "Def:" << int32_t(item ? item->getDefense() : _kind.defense);
				if(_kind.extraDefense || (item && item->getExtraDefense()))
					s << " " << std::showpos << int32_t(item ? item->getExtraDefense() : _kind.extraDefense) << std::noshowpos;
			}
		}
		else if (_kind.weaponType == WEAPON_WAND) {
			if (_kind.attack > 0 || (item && item->getAttack())) {
				begin = false;
				s << " (Bonus Dmg:";
				s << int32_t(item ? item->getAttack() : _kind.attack);
			}
		}

		for(uint16_t i = SKILL_FIRST; i <= SKILL_LAST; i++)
		{
			if(!_kind.abilities.skills[i])
				continue;

			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << getSkillName(i) << " " << std::showpos << (int32_t)_kind.abilities.skills[i] << std::noshowpos;
		}

		if(_kind.abilities.stats[STAT_MAGICLEVEL])
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "magic level " << std::showpos << (int32_t)_kind.abilities.stats[STAT_MAGICLEVEL] << std::noshowpos;
		}

		int32_t show = _kind.abilities.getAbsorb(COMBAT_PHYSICALDAMAGE);
		for(uint32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(_kind.abilities.getAbsorb((CombatType_t)i) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(uint32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t absorb = _kind.abilities.getAbsorb((CombatType_t)i);
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

		if(_kind.abilities.speed)
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "speed " << std::showpos << (int32_t)(_kind.abilities.speed / 2) << std::noshowpos;
		}

		if(!begin)
			s << ")";
	}
	else if(_kind.armor || (item && item->getArmor()) || _kind.showAttributes)
	{
		int32_t tmp = _kind.armor;
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
			if(!_kind.abilities.skills[i])
				continue;

			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << getSkillName(i) << " " << std::showpos << (int32_t)_kind.abilities.skills[i] << std::noshowpos;
		}

		if(_kind.abilities.stats[STAT_MAGICLEVEL])
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "magic level " << std::showpos << (int32_t)_kind.abilities.stats[STAT_MAGICLEVEL] << std::noshowpos;
		}

		int32_t show = _kind.abilities.getAbsorb(COMBAT_PHYSICALDAMAGE);
		for(int32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(_kind.abilities.getAbsorb((CombatType_t)i) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t absorb = _kind.abilities.getAbsorb((CombatType_t)i);
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

		show = _kind.abilities.getReflect(COMBAT_PHYSICALDAMAGE, REFLECT_CHANCE);
		for(int32_t i = (COMBAT_PHYSICALDAMAGE + 1); i <= COMBAT_LAST; i++)
		{
			if(_kind.abilities.getReflect((CombatType_t)i, REFLECT_CHANCE) == show)
				continue;

			show = 0;
			break;
		}

		if(!show)
		{
			bool tmp = true;
			for(int32_t i = COMBAT_PHYSICALDAMAGE; i <= COMBAT_LAST; i++)
			{
				int16_t chance = _kind.abilities.getReflect((CombatType_t)i, REFLECT_CHANCE);
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

				int16_t percent = _kind.abilities.getReflect((CombatType_t)i, REFLECT_PERCENT);
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

		if(_kind.abilities.speed)
		{
			if(begin)
			{
				begin = false;
				s << " (";
			}
			else
				s << ", ";

			s << "speed " << std::showpos << (int32_t)(_kind.abilities.speed / 2) << std::noshowpos;
		}

		if(!begin)
			s << ")";
	}
	else if(_kind.isContainer())
		s << " (Vol:" << (int32_t)_kind.maxItems << ")";
	else if(_kind.isKey())
		s << " (Key:" << (item ? (int32_t)item->getActionId() : 0) << ")";
	else if(_kind.isFluidContainer())
	{
		ItemKindPC subKind = server.items()[subType];
		if(subKind)
			s << " of " << (subkind.name.empty() ? "unknown" : subkind.name);
		else
			s << ". It is empty";
	}
	else if(_kind.isSplash())
	{
		s << " of ";

		ItemKindPC subKind = server.items()[subType];
		if(subKind && !subkind.name.empty())
			s << subkind.name;
		else
			s << "unknown";
	}
	else if(_kind.allowDistRead)
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
	else if(_kind.levelDoor && item && item->getActionId() >= (int32_t)_kind.levelDoor && item->getActionId()
		<= ((int32_t)_kind.levelDoor + server.configManager().getNumber(ConfigManager::MAXIMUM_DOOR_LEVEL)))
		s << " for level " << item->getActionId() - _kind.levelDoor;

	if(_kind.showCharges)
		s << " that has " << subType << " charge" << (subType != 1 ? "s" : "") << " left";

	if(_kind.showDuration)
	{
		if(item && item->_dynamicAttributes.contains(Kind::ATTRIBUTE_DURATION))
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

	if(_kind.wieldInfo)
	{
		s << std::endl << "It can only be wielded properly by ";
		if(_kind.wieldInfo & WIELDINFO_PREMIUM)
			s << "premium ";

		if(_kind.wieldInfo & WIELDINFO_VOCREQ)
			s << _kind.vocationString;
		else
			s << "players";

		if(_kind.wieldInfo & WIELDINFO_LEVEL)
			s << " of level " << (int32_t)_kind.minReqLevel << " or higher";

		if(_kind.wieldInfo & WIELDINFO_MAGLV)
		{
			if(_kind.wieldInfo & WIELDINFO_LEVEL)
				s << " and";
			else
				s << " of";

			s << " magic level " << (int32_t)_kind.minReqMagicLevel << " or higher";
		}

		s << ".";
	}

	if(lookDistance <= 1 && _kind.pickupable)
	{
		std::string tmp;
		if(!item)
			tmp = getWeightDescription(_kind.weight, _kind.stackable, subType);
		else
			tmp = item->getWeightDescription();

		if(!tmp.empty())
			s << std::endl << tmp;
	}

	if(_kind.abilities.elementType != COMBAT_NONE && _kind.decayTo > 0)
	{
		s << std::endl << "It is temporarily enchanted with " << getCombatName(_kind.abilities.elementType) << " (";
		s << std::max((int32_t)0, int32_t((item ? item->getAttack() : _kind.attack) - _kind.abilities.elementDamage));
		if(_kind.extraAttack || (item && item->getExtraAttack()))
			s << " " << std::showpos << int32_t(item ? item->getExtraAttack() : _kind.extraAttack) << std::noshowpos;

		s << " physical + " << _kind.abilities.elementDamage << " " << getCombatName(_kind.abilities.elementType) << " damage).";
	}

	std::string str;
	if(item && !item->getSpecialDescription().empty())
		str = item->getSpecialDescription();
	else if(!_kind.description.empty() && lookDistance <= 1)
		str = _kind.description;

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

std::string Item::getNameDescription(const ItemKindPC& _kind, const Item* item/* = nullptr*/, uint16_t subType/* = 0*/, bool addArticle/* = true*/)
{
	if(item)
		subType = item->getSubType();

	std::stringstream s;
	if(!_kind->name.empty() || (item && !item->getName().empty()))
	{
		if(subType > 1 && _kind->stackable && _kind->showCount)
			s << subType << " " << (item ? item->getPluralName() : _kind->pluralName);
		else
		{
			if(addArticle)
			{
				if(item && !item->getArticle().empty())
					s << item->getArticle() << " ";
				else if(!_kind->article.empty())
					s << _kind->article << " ";
			}

			s << (item ? item->getName() : _kind->name);
		}
	}
	else
		s << "an item of type " << _kind->id << ", please report it to gamemaster";

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

	_dynamicAttributes.set(Kind::ATTRIBUTE_AID, aid);
	server.moveEvents().onAddTileItem(getTile(), this);
}

void Item::resetActionId()
{
	if(!getActionId())
		return;

	_dynamicAttributes.remove(Kind::ATTRIBUTE_AID);
	server.moveEvents().onAddTileItem(getTile(), this);
}

void Item::setUniqueId(int32_t uid)
{
	if(getUniqueId())
		return;

	_dynamicAttributes.set(Kind::ATTRIBUTE_UID, uid);
	ScriptEnviroment::addUniqueThing(this);
}

bool Item::canDecay(bool ignoreRemoved)
{
	if(!ignoreRemoved && isRemoved())
		return false;

	if(isLoadedFromMap() && (getUniqueId() || (getActionId() && getContainer())))
		return false;

	return _kind->decayTo >= 0 && _kind->decayTime;
}

void Item::getLight(LightInfo& lightInfo)
{
	lightInfo.color = _kind->lightColor;
	lightInfo.level = _kind->lightLevel;
}

void Item::__startDecaying()
{
	server.game().startDecay(this);
}

std::string Item::getName() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_NAME);
	if(v)
		return *v;

	return _kind->name;
}

std::string Item::getPluralName() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_PLURALNAME);
	if(v)
		return *v;

	return _kind->pluralName;
}

std::string Item::getArticle() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_ARTICLE);
	if(v)
		return *v;

	return _kind->article;
}

bool Item::isScriptProtected() const
{
	const bool* v = _dynamicAttributes.getBoolean(Kind::ATTRIBUTE_SCRIPTPROTECTED);
	if(v)
		return *v;

	return false;
}

int32_t Item::getAttack() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_ATTACK);
	if(v)
		return *v;

	return _kind->attack;
}

int32_t Item::getExtraAttack() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_EXTRAATTACK);
	if(v)
		return *v;

	return _kind->extraAttack;
}

int32_t Item::getDefense() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_DEFENSE);
	if(v)
		return *v;

	return _kind->defense;
}

int32_t Item::getExtraDefense() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_EXTRADEFENSE);
	if(v)
		return *v;

	return _kind->extraDefense;
}

int32_t Item::getArmor() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_ARMOR);
	if(v)
		return *v;

	return _kind->armor;
}

int32_t Item::getAttackSpeed() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_ATTACKSPEED);
	if(v)
		return *v;

	return _kind->attackSpeed;
}

int32_t Item::getHitChance() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_HITCHANCE);
	if(v)
		return *v;

	return _kind->hitChance;
}

int32_t Item::getShootRange() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_SHOOTRANGE);
	if(v)
		return *v;

	return _kind->shootRange;
}

void Item::decreaseDuration(int32_t time)
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_DURATION);
	if(v)
		_dynamicAttributes.set(Kind::ATTRIBUTE_DURATION, *v - time);
}

int32_t Item::getDuration() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_DURATION);
	if(v)
		return *v;

	return 0;
}

const std::string& Item::getSpecialDescription() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_DESCRIPTION);
	if(v)
		return *v;

	return EMPTY_STRING;
}

const std::string& Item::getText() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_TEXT);
	if(v)
		return *v;

	return EMPTY_STRING;
}

time_t Item::getDate() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_DATE);
	if(v)
		return (time_t)*v;

	return 0;
}

const std::string& Item::getWriter() const
{
	const std::string* v = _dynamicAttributes.getString(Kind::ATTRIBUTE_WRITER);
	if(v)
		return *v;

	return EMPTY_STRING;
}

int32_t Item::getActionId() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_AID);
	if(v)
		return *v;

	return 0;
}

int32_t Item::getUniqueId() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_UID);
	if(v)
		return *v;

	return 0;
}

uint16_t Item::getCharges() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_CHARGES);
	if(v && *v >= 0)
		return (uint16_t)*v;

	return 0;
}

uint16_t Item::getFluidType() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_FLUIDTYPE);
	if(v && *v >= 0)
		return (uint16_t)*v;

	return 0;
}

uint32_t Item::getOwner() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_OWNER);
	if(v)
		return (uint32_t)*v;

	return 0;
}

uint32_t Item::getCorpseOwner()
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_CORPSEOWNER);
	if(v)
		return (uint32_t)*v;

	return 0;
}

ItemDecayState_t Item::getDecaying() const
{
	const int32_t* v = _dynamicAttributes.getInteger(Kind::ATTRIBUTE_DECAYING);
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

	auto entries = _dynamicAttributes.getEntries();
	if (entries != nullptr && !entries->empty()) {
		stream.ADD_UCHAR(ATTR_Kind::ATTRIBUTE_MAP);
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

				_dynamicAttributes.set(key, v);
				break;
			}
			case SerializedTypeInt:
			{
				int32_t v;
				if(!stream.GET_VALUE(v))
					return false;

				_dynamicAttributes.set(key, v);
				break;
			}
			case SerializedTypeFloat:
			{
				float v;
				if(!stream.GET_VALUE(v))
					return false;

				_dynamicAttributes.set(key, v);
				break;
			}
			case SerializedTypeBool:
			{
				uint8_t v;
				if(!stream.GET_UCHAR(v))
					return false;

				_dynamicAttributes.set(key, v != 0);
				break;
			}
		}
	}

	return true;
}

void Item::setDuration(Duration time) {_dynamicAttributes.set(Kind::ATTRIBUTE_DURATION, time.count());}
void Item::setSpecialDescription(const std::string& description) {_dynamicAttributes.set(Kind::ATTRIBUTE_DESCRIPTION, description);}
void Item::resetSpecialDescription() {_dynamicAttributes.remove(Kind::ATTRIBUTE_DESCRIPTION);}
void Item::setText(const std::string& text) {_dynamicAttributes.set(Kind::ATTRIBUTE_TEXT, text);}
void Item::resetText() {_dynamicAttributes.remove(Kind::ATTRIBUTE_TEXT);}
void Item::setDate(time_t date) {_dynamicAttributes.set(Kind::ATTRIBUTE_DATE, (int32_t)date);}
void Item::resetDate() {_dynamicAttributes.remove(Kind::ATTRIBUTE_DATE);}
void Item::setWriter(std::string writer) {_dynamicAttributes.set(Kind::ATTRIBUTE_WRITER, writer);}
void Item::resetWriter() {_dynamicAttributes.remove(Kind::ATTRIBUTE_WRITER);}
void Item::setCharges(uint16_t charges) {_dynamicAttributes.set(Kind::ATTRIBUTE_CHARGES, charges);}
void Item::setOwner(uint32_t owner) {_dynamicAttributes.set(Kind::ATTRIBUTE_OWNER, (int32_t)owner);}
void Item::setCorpseOwner(uint32_t corpseOwner) {_dynamicAttributes.set(Kind::ATTRIBUTE_CORPSEOWNER, (int32_t)corpseOwner);}
void Item::setDecaying(ItemDecayState_t state) {_dynamicAttributes.set(Kind::ATTRIBUTE_DECAYING, (int32_t)state);}
void Item::setFluidType(uint16_t fluidType) {_dynamicAttributes.set(Kind::ATTRIBUTE_FLUIDTYPE, fluidType);}
