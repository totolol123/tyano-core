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

#include "tile.h"
#include "housetile.h"

#include "player.h"
#include "creature.h"

#include "teleport.h"
#include "trashholder.h"
#include "mailbox.h"

#include "combat.h"
#include "items.h"
#include "monster.h"
#include "movement.h"

#include "game.h"
#include "configmanager.h"
#include "server.h"


LOGGER_DEFINITION(Tile);



Tile::~Tile() {
	if (ground != nullptr) {
		ground->setParent(nullptr);
	}
}


bool Tile::hasProperty(enum ITEMPROPERTY prop) const
{
	if(ground && ground->hasProperty(prop))
		return true;

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((*it)->hasProperty(prop))
				return true;
		}
	}

	return false;
}

bool Tile::hasProperty(Item* exclude, enum ITEMPROPERTY prop) const
{
	assert(exclude);
	if(ground && exclude != ground && ground->hasProperty(prop))
		return true;

	if(const TileItemVector* items = getItemList())
	{
		Item* item = nullptr;
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((item = (*it).get()) && item != exclude && item->hasProperty(prop))
				return true;
		}
	}

	return false;
}

HouseTile* Tile::getHouseTile()
{
	if(isHouseTile())
		return static_cast<HouseTile*>(this);

	return nullptr;
}

const HouseTile* Tile::getHouseTile() const
{
	if(isHouseTile())
		return static_cast<const HouseTile*>(this);

	return nullptr;
}

bool Tile::hasHeight(uint32_t n) const
{
	uint32_t height = 0;
	if(ground)
	{
		if(ground->hasProperty(HASHEIGHT))
			++height;

		if(n == height)
			return true;
	}

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
		{
			if((*it)->hasProperty(HASHEIGHT))
				++height;

			if(n == height)
				return true;
		}
	}

	return false;
}

bool Tile::isSwimmingPool(bool checkPz /*= true*/) const
{
	if(TrashHolder* trashHolder = getTrashHolder())
		return trashHolder->getEffect() == MAGIC_EFFECT_LOSE_ENERGY && (!checkPz ||
			getZone() == ZONE_PROTECTION || getZone() == ZONE_NOPVP);

	return false;
}

uint32_t Tile::getCreatureCount() const
{
	if(const CreatureVector* creatures = getCreatures())
		return creatures->size();

	return 0;
}

uint32_t Tile::getItemCount() const
{
	if(const TileItemVector* items = getItemList())
		return (uint32_t)items->size();

	return 0;
}

uint32_t Tile::getTopItemCount() const
{
	if(const TileItemVector* items = getItemList())
		return items->getTopItemCount();

	return 0;
}

uint32_t Tile::getDownItemCount() const
{
	if(const TileItemVector* items =getItemList())
		return items->getDownItemCount();

	return 0;
}

Teleport* Tile::getTeleportItem() const
{
	if(!hasFlag(TILESTATE_TELEPORT))
		return nullptr;

	if(ground && ground->getTeleport())
		return ground->getTeleport();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getTeleport())
				return (*it)->getTeleport();
		}
	}

	return nullptr;
}

MagicField* Tile::getFieldItem() const
{
	if(!hasFlag(TILESTATE_MAGICFIELD))
		return nullptr;

	if(ground && ground->getMagicField())
		return ground->getMagicField();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getMagicField())
				return (*it)->getMagicField();
		}
	}

	return nullptr;
}

TrashHolder* Tile::getTrashHolder() const
{
	if(!hasFlag(TILESTATE_TRASHHOLDER))
		return nullptr;

	if(ground && ground->getTrashHolder())
		return ground->getTrashHolder();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getTrashHolder())
				return (*it)->getTrashHolder();
		}
	}

	return nullptr;
}

Mailbox* Tile::getMailbox() const
{
	if(!hasFlag(TILESTATE_MAILBOX))
		return nullptr;

	if(ground && ground->getMailbox())
		return ground->getMailbox();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getMailbox())
				return (*it)->getMailbox();
		}
	}

	return nullptr;
}

BedItem* Tile::getBedItem() const
{
	if(!hasFlag(TILESTATE_BED))
		return nullptr;

	if(ground && ground->getBed())
		return ground->getBed();

	if(const TileItemVector* items = getItemList())
	{
		for(ItemVector::const_reverse_iterator it = items->rbegin(); it != items->rend(); ++it)
		{
			if((*it)->getBed())
				return (*it)->getBed();
		}
	}

	return nullptr;
}

Creature* Tile::getTopCreature()
{
	if(CreatureVector* creatures = getCreatures())
	{
		if(!creatures->empty())
			return (*creatures->begin()).get();
	}

	return nullptr;
}

Item* Tile::getTopDownItem()
{
	if(TileItemVector* items = getItemList())
	{
		if(items->getDownItemCount() > 0)
			return (*items->getBeginDownItem()).get();
	}

	return nullptr;
}

Item* Tile::getTopTopItem()
{
	if(TileItemVector* items = getItemList())
	{
		if(items->getTopItemCount() > 0)
			return (*(items->getEndTopItem() - 1)).get();
	}

	return nullptr;
}

Item* Tile::getItemByTopOrder(uint32_t topOrder)
{
	if(TileItemVector* items = getItemList())
	{
		ItemVector::reverse_iterator eit = ItemVector::reverse_iterator(items->getBeginTopItem());
		for(ItemVector::reverse_iterator it = ItemVector::reverse_iterator(items->getEndTopItem()); it != eit; ++it)
		{
			if((*it)->getKind()->alwaysOnTopOrder == (int32_t)topOrder)
				return (*it).get();
		}
	}

	return nullptr;
}

Thing* Tile::getTopVisibleThing(const Creature* creature)
{
	if(Creature* _creature = getTopVisibleCreature(creature))
		return _creature;

	if(TileItemVector* items = getItemList())
	{
		for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
		{
			if(!(*it)->getKind()->lookThrough)
				return (*it).get();
		}

		for(ItemVector::reverse_iterator it = ItemVector::reverse_iterator(items->getEndTopItem()),
			end = ItemVector::reverse_iterator(items->getBeginTopItem()); it != end; ++it)
		{
			if(!(*it)->getKind()->lookThrough)
				return (*it).get();
		}
	}

	return ground.get();
}

Creature* Tile::getTopVisibleCreature(const Creature* creature)
{
	if(CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if(creature->canSeeCreature((*cit).get()))
				return (*cit).get();
		}
	}

	return nullptr;
}

const Creature* Tile::getTopVisibleCreature(const Creature* creature) const
{
	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if(creature->canSeeCreature((*cit).get()))
				return (*cit).get();
		}
	}

	return nullptr;
}

void Tile::onAddTileItem(Item* item)
{
	updateTileFlags(item, false);
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendAddTileItem(this, cylinderMapPos, item, BOOST_CURRENT_FUNCTION);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onAddTileItem(this, cylinderMapPos, item);
}

void Tile::onUpdateTileItem(Item* oldItem, const ItemKindPC& oldType, Item* newItem, const ItemKindPC& newType)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTileItem(this, cylinderMapPos, oldItem, newItem);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTileItem(this, cylinderMapPos, oldItem, oldType, newItem, newType);
}

void Tile::onRemoveTileItem(const SpectatorList& list, std::vector<int32_t>& oldStackposVector, Item* item)
{
	updateTileFlags(item, true);
	const Position& cylinderMapPos = pos;

	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	uint32_t i = 0;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer())) {
			int32_t index = oldStackposVector[i++];
			if (index < 0) {
				continue;
			}

			tmpPlayer->sendRemoveTileItem(this, StackPosition(cylinderMapPos, index), item, BOOST_CURRENT_FUNCTION);
		}
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onRemoveTileItem(this, cylinderMapPos, item->getKind(), item);
}

void Tile::onUpdateTile()
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	//send to client
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->sendUpdateTile(this, cylinderMapPos);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onUpdateTile(this, cylinderMapPos);
}

void Tile::moveCreature(Creature* actor, Creature* creature, Cylinder* toCylinder, bool forceTeleport/* = false*/)
{
	Tile* newTile = toCylinder->getTile();
	SpectatorList list;
	SpectatorList::iterator it;

	server.game().getSpectators(list, pos, false, true);
	Position newPos = newTile->getPosition();
	server.game().getSpectators(list, newPos, true, true);

	bool teleport = false;
	if(forceTeleport || !newTile->ground || !Position::areInRange<1,1,0>(pos, newPos))
		teleport = true;

	std::vector<uint32_t> oldStackposVector;
	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, creature));
	}

	server.game().autorelease(creature);

	int32_t oldStackpos = __getIndexOfThing(creature);
	//remove the creature
	__removeThing(creature, 0);

	//add the creature
	newTile->__addThing(actor, creature);

	LOGt(creature << " moved from " << pos << " to " << newPos << ".");

	//switch the node ownership
	server.game().getMap()->onCreatureMoved(creature, this, newTile);

	int32_t newStackpos = newTile->__getIndexOfThing(creature);
	if(!teleport)
	{
		if(pos.y > newPos.y)
			creature->setDirection(Direction::NORTH);
		else if(pos.y < newPos.y)
			creature->setDirection(Direction::SOUTH);
		if(pos.x < newPos.x)
			creature->setDirection(Direction::EAST);
		else if(pos.x > newPos.x)
			creature->setDirection(Direction::WEST);
	}

	//send to client
	int32_t i = 0;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer())) {
			if (tmpPlayer->canSeeCreature(creature)) {
				tmpPlayer->sendCreatureMove(creature, newTile, newPos, this, pos, oldStackposVector[i], teleport, BOOST_CURRENT_FUNCTION);
			}
			++i;
		}
	}

	//event method
	for(it = list.begin(); it != list.end(); ++it)
		(*it)->onCreatureMove(creature, pos, this, newPos, newTile, teleport);

	postRemoveNotification(actor, creature, toCylinder, oldStackpos, true);
	newTile->postAddNotification(actor, creature, this, newStackpos);
}

ReturnValue Tile::__queryAdd(int32_t index, const Thing* thing, uint32_t count,
	uint32_t flags) const
{
	if (!hasRoomForThing(*thing)) {
		return RET_TILEISFULL;
	}

	const CreatureVector* creatures = getCreatures();
	const TileItemVector* items = getItemList();
	if(const Creature* creature = thing->getCreature())
	{
		if(hasBitSet(FLAG_NOLIMIT, flags))
			return RET_NOERROR;

		if(hasBitSet(FLAG_PATHFINDING, flags))
		{
			if(floorChange() || positionChange())
				return RET_NOTPOSSIBLE;
		}

		if(!ground)
			return RET_NOTPOSSIBLE;

		if(const Monster* monster = creature->getMonster())
		{
			if(hasFlag(TILESTATE_PROTECTIONZONE))
				return RET_NOTPOSSIBLE;

			if(floorChange() || positionChange())
				return RET_NOTPOSSIBLE;

			if(monster->canPushCreatures() && !monster->hasMaster())
			{
				if(creatures)
				{
					Creature* tmp = nullptr;
					for(uint32_t i = 0; i < creatures->size(); ++i)
					{
						tmp = creatures->at(i).get();
						if(creature->canWalkthrough(tmp))
							continue;

						if(!tmp->getMonster() || !tmp->isPushable() ||
							(tmp->getMonster()->hasController()))
							return RET_NOTPOSSIBLE;
					}
				}
			}
			else if(creatures && !creatures->empty())
			{
				for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
				{
					if(!creature->canWalkthrough((*cit).get()))
						return RET_NOTENOUGHROOM;
				}
			}

			if(hasFlag(TILESTATE_IMMOVABLEBLOCKSOLID))
				return RET_NOTPOSSIBLE;

			if(hasBitSet(FLAG_PATHFINDING, flags) && hasFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH))
				return RET_NOTPOSSIBLE;

			if((hasFlag(TILESTATE_BLOCKSOLID) || (hasBitSet(FLAG_PATHFINDING, flags) && hasFlag(TILESTATE_NOFIELDBLOCKPATH)))
				&& (!(monster->canPushItems() || hasBitSet(FLAG_IGNOREBLOCKITEM, flags))))
				return RET_NOTPOSSIBLE;

			MagicField* field = getFieldItem();
			if(field && !field->isBlocking(monster))
			{
				CombatType_t combatType = field->getCombatType();
				//There is 3 options for a monster to enter a magic field
				//1) Monster is immune
				if(!monster->isImmune(combatType))
				{
					//1) Monster is "strong" enough to handle the damage
					//2) Monster is already afflicated by this type of condition
					if(!hasBitSet(FLAG_IGNOREFIELDDAMAGE, flags))
						return RET_NOTPOSSIBLE;

					if(!monster->canPushItems() && !monster->hasCondition(
						Combat::DamageToConditionType(combatType), false))
						return RET_NOTPOSSIBLE;
				}
			}

			return RET_NOERROR;
		}
		else if(const Player* player = creature->getPlayer())
		{
			if(creatures && !creatures->empty() && !hasBitSet(FLAG_IGNOREBLOCKCREATURE, flags))
			{
				for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
				{
					if(!creature->canWalkthrough((*cit).get()))
						return RET_NOTENOUGHROOM; //RET_NOTPOSSIBLE
				}
			}

			if(!player->getParent() && hasFlag(TILESTATE_NOLOGOUT)) //player is trying to login to a "no logout" tile
				return RET_NOTPOSSIBLE;

			if(player->isPzLocked() && !player->getTile()->hasFlag(TILESTATE_PVPZONE) && hasFlag(TILESTATE_PVPZONE)) //player is trying to enter a pvp zone while being pz-locked
				return RET_PLAYERISPZLOCKEDENTERPVPZONE;

			if(player->isPzLocked() && player->getTile()->hasFlag(TILESTATE_PVPZONE) && !hasFlag(TILESTATE_PVPZONE)) //player is trying to leave a pvp zone while being pz-locked
				return RET_PLAYERISPZLOCKEDLEAVEPVPZONE;

			if(hasFlag(TILESTATE_NOPVPZONE) && player->isPzLocked())
				return RET_PLAYERISPZLOCKED;

			if(hasFlag(TILESTATE_PROTECTIONZONE) && player->isPzLocked())
				return RET_PLAYERISPZLOCKED;
		}
		else if(creatures && !creatures->empty() && !hasBitSet(FLAG_IGNOREBLOCKCREATURE, flags))
		{
			for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
			{
				if(!creature->canWalkthrough((*cit).get()))
					return RET_NOTENOUGHROOM;
			}
		}

		if(items)
		{
			MagicField* field = getFieldItem();
			if(field && field->isBlocking(creature))
				return RET_NOTPOSSIBLE;

			if(!hasBitSet(FLAG_IGNOREBLOCKITEM, flags))
			{
				//If the FLAG_IGNOREBLOCKITEM bit isn't set we dont have to iterate every single item
				if(hasFlag(TILESTATE_BLOCKSOLID))
					return RET_NOTENOUGHROOM;
			}
			else
			{
				//FLAG_IGNOREBLOCKITEM is set
				if(ground)
				{
					if(ground->isBlocking(creature) && (!ground->getKind()->moveable || (ground->isLoadedFromMap() &&
						(ground->getUniqueId() || (ground->getActionId()
						&& ground->getContainer())))))
						return RET_NOTPOSSIBLE;
				}

				if(const TileItemVector* items = getItemList())
				{
					Item* iItem = nullptr;
					for(ItemVector::const_iterator it = items->begin(); it != items->end(); ++it)
					{
						iItem = (*it).get();
						if(iItem->isBlocking(creature) && (!iItem->getKind()->moveable || (iItem->isLoadedFromMap() &&
							(iItem->getUniqueId() || (iItem->getActionId()
							&& iItem->getContainer())))))
							return RET_NOTPOSSIBLE;
					}
				}
			}
		}
	}
	else if(const Item* item = thing->getItem())
	{
		if(items && items->size() >= 0xFFFF)
			return RET_NOTPOSSIBLE;

		if(hasBitSet(FLAG_NOLIMIT, flags))
			return RET_NOERROR;

		bool itemIsHangable = item->isHangable();
		if(!ground && !itemIsHangable)
			return RET_NOTPOSSIBLE;

		if(creatures && !creatures->empty() && !hasBitSet(FLAG_IGNOREBLOCKCREATURE, flags))
		{
			for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
			{
				if(!(*cit)->isGhost() && item->isBlocking((*cit).get()))
					return RET_NOTENOUGHROOM;
			}
		}

		if(hasFlag(TILESTATE_PROTECTIONZONE))
		{
			const uint32_t itemLimit = server.configManager().getNumber(ConfigManager::ITEMLIMIT_PROTECTIONZONE);
			if(itemLimit && getThingCount() > itemLimit)
				return RET_TILEISFULL;
		}

		bool hasHangable = false, supportHangable = false;
		if(items)
		{
			Thing* iThing = nullptr;
			for(uint32_t i = 0; i < getThingCount(); ++i)
			{
				iThing = __getThing(i);
				if(const Item* iItem = iThing->getItem())
				{
					ItemKindPC kind = iItem->getKind();
					if(kind->isHangable)
						hasHangable = true;

					if(kind->isHorizontal || kind->isVertical)
						supportHangable = true;

					if(itemIsHangable && (kind->isHorizontal || kind->isVertical))
						continue;
					else if(kind->blockSolid)
					{
						if(!item->isPickupable())
							return RET_NOTENOUGHROOM;

						if(kind->allowPickupable)
							continue;

						if(!kind->hasHeight || kind->pickupable || kind->isBed())
							return RET_NOTENOUGHROOM;
					}
				}
			}
		}

		if(itemIsHangable && hasHangable && supportHangable)
			return RET_NEEDEXCHANGE;
	}

	return RET_NOERROR;
}

ReturnValue Tile::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	maxQueryCount = std::max((uint32_t)1, count);
	return RET_NOERROR;
}

ReturnValue Tile::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags) const
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
		return RET_NOTPOSSIBLE;

	const Item* item = thing->getItem();
	if(!item || !count || (item->isStackable() && count > item->getItemCount())
		|| (item->isNotMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags)))
		return RET_NOTPOSSIBLE;

	return RET_NOERROR;
}

Cylinder* Tile::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	Tile* destTile = nullptr;
	*destItem = nullptr;

	Position _pos = pos;
	if(floorChange(CHANGE_DOWN))
	{
		_pos.z++;
		for(int32_t i = CHANGE_FIRST_EX; i < CHANGE_LAST; ++i)
		{
			Position __pos = _pos;
			Tile* tmpTile = nullptr;
			switch(i)
			{
				case CHANGE_NORTH_EX:
					__pos.y++;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.y++;

					break;
				case CHANGE_SOUTH_EX:
					__pos.y--;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.y--;

					break;
				case CHANGE_EAST_EX:
					__pos.x--;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.x--;

					break;
				case CHANGE_WEST_EX:
					__pos.x++;
					if((tmpTile = server.game().getTile(__pos)))
						__pos.x++;

					break;
				default:
					break;
			}

			if(!tmpTile || !tmpTile->floorChange((FloorChange_t)i))
				continue;

			destTile = server.game().getTile(__pos);
			break;
		}

		if(!destTile)
		{
			if(Tile* downTile = server.game().getTile(_pos))
			{
				if(downTile->floorChange(CHANGE_NORTH) || downTile->floorChange(CHANGE_NORTH_EX))
					_pos.y++;

				if(downTile->floorChange(CHANGE_SOUTH) || downTile->floorChange(CHANGE_SOUTH_EX))
					_pos.y--;

				if(downTile->floorChange(CHANGE_EAST) || downTile->floorChange(CHANGE_EAST_EX))
					_pos.x--;

				if(downTile->floorChange(CHANGE_WEST) || downTile->floorChange(CHANGE_WEST_EX))
					_pos.x++;

				destTile = server.game().getTile(_pos);
			}
		}
	}
	else if(floorChange())
	{
		_pos.z--;
		if(floorChange(CHANGE_NORTH))
			_pos.y--;

		if(floorChange(CHANGE_SOUTH))
			_pos.y++;

		if(floorChange(CHANGE_EAST))
			_pos.x++;

		if(floorChange(CHANGE_WEST))
			_pos.x--;

		if(floorChange(CHANGE_NORTH_EX))
			_pos.y -= 2;

		if(floorChange(CHANGE_SOUTH_EX))
			_pos.y += 2;

		if(floorChange(CHANGE_EAST_EX))
			_pos.x += 2;

		if(floorChange(CHANGE_WEST_EX))
			_pos.x -= 2;

		destTile = server.game().getTile(_pos);
	}

	if(!destTile)
		destTile = this;
	else
		flags |= FLAG_NOLIMIT; //will ignore that there is blocking items/creatures

	if(destTile)
	{
		Thing* destThing = destTile->getTopDownItem();
		if(destThing && !destThing->isRemoved())
			*destItem = destThing->getItem();
	}

	return destTile;
}

void Tile::__addThing(Creature* actor, int32_t index, Thing* thing)
{
	assert(thing->getParent() == nullptr || thing->getParent() == &VirtualCylinder::virtualCylinder);

	if (!hasRoomForThing(*thing)) {
		return;
	}

	if(Creature* creature = thing->getCreature())
	{
		server.game().clearSpectatorCache();
		creature->setParent(this);

		CreatureVector* creatures = makeCreatures();
		creatures->push_back(creature);

		++thingCount;
		return;
	}

	Item* item = thing->getItem();
	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	TileItemVector* items = getItemList();
	if(items && items->size() >= 0xFFFF)
		return/* RET_NOTPOSSIBLE*/;

	if(server.configManager().getBool(ConfigManager::STORE_TRASH) && !hasFlag(TILESTATE_TRASHED))
	{
		server.game().addTrash(pos);
		setFlag(TILESTATE_TRASHED);
	}

	item->setParent(this);
	if(item->isGroundTile())
	{
		if(ground)
		{
			ItemKindPC oldKind = ground->getKind();
			int32_t oldGroundIndex = __getIndexOfThing(ground.get());
			boost::intrusive_ptr<Item> oldGround = ground;

			ground->setParent(nullptr);
			ground = item;

			updateTileFlags(oldGround.get(), true);
			updateTileFlags(item, false);

			onUpdateTileItem(oldGround.get(), oldKind, item, item->getKind());
			postRemoveNotification(actor, oldGround.get(), nullptr, oldGroundIndex, true);
		}
		else
		{
			ground = item;
			++thingCount;
			onAddTileItem(item);
		}
	}
	else if(item->isAlwaysOnTop())
	{
		if(item->isSplash())
		{
			//remove old splash if exists
			if(items)
			{
				for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
				{
					if(!(*it)->isSplash())
						continue;

					int32_t oldSplashIndex = __getIndexOfThing((*it).get());
					boost::intrusive_ptr<Item> oldSplash = *it;

					__removeThing(oldSplash.get(), 1);
					oldSplash->setParent(nullptr);

					postRemoveNotification(actor, oldSplash.get(), nullptr, oldSplashIndex, true);
					break;
				}
			}
		}

		bool isInserted = false;
		if(items)
		{
			for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				//Note: this is different from internalAddThing
				if(item->getKind()->alwaysOnTopOrder > (*it)->getKind()->alwaysOnTopOrder)
					continue;

				items->insert(it, item);
				++thingCount;

				isInserted = true;
				break;
			}
		}
		else
			items = makeItemList();

		if(!isInserted)
		{
			items->push_back(item);
			++thingCount;
		}

		onAddTileItem(item);
	}
	else
	{
		if(item->isMagicField())
		{
			//remove old field item if exists
			if(items)
			{
				boost::intrusive_ptr<MagicField> oldField;
				for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
				{
					if(!(oldField = (*it)->getMagicField()))
						continue;

					if(oldField->isReplaceable())
					{
						int32_t oldFieldIndex = __getIndexOfThing((*it).get());
						__removeThing(oldField.get(), 1);

						oldField->setParent(nullptr);

						postRemoveNotification(actor, oldField.get(), nullptr, oldFieldIndex, true);
						break;
					}

					//This magic field cannot be replaced.
					item->setParent(nullptr);
					return;
				}
			}
		}

		if(item->getId() == ITEM_WATERBALL_SPLASH && !hasFlag(TILESTATE_TRASHHOLDER))
			item->setKind(server.items().getKind(ITEM_WATERBALL));

		items = makeItemList();
		items->insert(items->getBeginDownItem(), item);

		++items->downItemCount;
		++thingCount;
		onAddTileItem(item);
	}
}

void Tile::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	ItemKindPC newKind = server.items().getKind(itemId);
	if (!newKind) {
		return;
	}

	ItemKindPC oldKind = item->getKind();

	//Need to update it here too since the old and new item is the same
	updateTileFlags(item, true);

	item->setKind(newKind);
	item->setSubType(count);

	updateTileFlags(item, false);
	onUpdateTileItem(item, oldKind, item, newKind);
}

void Tile::__replaceThing(uint32_t index, Thing* thing)
{
	Item* item = thing->getItem();
	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	int32_t pos = index;
	boost::intrusive_ptr<Item> oldItem;
	if(ground)
	{
		if(!pos)
		{
			oldItem = ground.get();
			ground = item;
		}

		--pos;
	}

	TileItemVector* items = getItemList();
	if(!oldItem && items)
	{
		int32_t topItemSize = getTopItemCount();
		if(pos < topItemSize)
		{
			ItemVector::iterator it = items->getBeginTopItem();
			it += pos;

			oldItem = (*it);
			it = items->erase(it);
			items->insert(it, item);
		}

		pos -= topItemSize;
	}

	if(!oldItem)
	{
		if(CreatureVector* creatures = getCreatures())
		{
			if(pos < (int32_t)creatures->size())
			{
				return/* RET_NOTPOSSIBLE*/;
			}

			pos -= (uint32_t)creatures->size();
		}
	}

	if(!oldItem && items)
	{
		int32_t downItemSize = getDownItemCount();
		if(pos < downItemSize)
		{
			ItemVector::iterator it = items->begin();
			it += pos;
			pos = 0;

			oldItem = (*it);
			it = items->erase(it);
			items->insert(it, item);
		}
	}

	if(oldItem)
	{
		item->setParent(this);
		updateTileFlags(oldItem.get(), true);
		updateTileFlags(item, false);

		onUpdateTileItem(oldItem.get(), oldItem->getKind(), item, item->getKind());
		oldItem->setParent(nullptr);
		return/* RET_NOERROR*/;
	}
}

void Tile::__removeThing(Thing* thing, uint32_t count)
{
	assert(thing->getParent() == this);

	Creature* creature = thing->getCreature();
	if(creature)
	{
		if(CreatureVector* creatures = getCreatures())
		{
			CreatureVector::iterator it = std::find(creatures->begin(), creatures->end(), thing);
			if(it == creatures->end())
			{
				return/* RET_NOTPOSSIBLE*/;
			}

			server.game().clearSpectatorCache();

			creature->setLastPosition(pos);
			creature->setParent(nullptr);
			creatures->erase(it);

			--thingCount;
		}

		return;
	}

	Item* item = thing->getItem();
	if(!item)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(item);
	if(index == -1)
	{
		return/* RET_NOTPOSSIBLE*/;
	}

	if(item == ground)
	{
		const SpectatorList& list = server.game().getSpectators(pos);
		std::vector<int32_t> oldStackposVector;

		Player* tmpPlayer = nullptr;
		for(SpectatorList::const_iterator it = list.begin(); it != list.end(); ++it)
		{
			if((tmpPlayer = (*it)->getPlayer()))
				oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, ground.get()));
		}

		ground->setParent(nullptr);
		ground = nullptr;

		--thingCount;
		onRemoveTileItem(list, oldStackposVector, item);
		return/* RET_NOERROR*/;
	}

	TileItemVector* items = getItemList();
	if(!items)
		return;

	if(item->isAlwaysOnTop())
	{
		for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
		{
			if(*it != item)
				continue;

			const SpectatorList& list = server.game().getSpectators(pos);
			std::vector<int32_t> oldStackposVector;

			Player* tmpPlayer = nullptr;
			for(SpectatorList::const_iterator iit = list.begin(); iit != list.end(); ++iit)
			{
				if((tmpPlayer = (*iit)->getPlayer()))
					oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, (*it).get()));
			}

			server.game().autorelease(*it);

			(*it)->setParent(nullptr);
			items->erase(it);

			--thingCount;
			onRemoveTileItem(list, oldStackposVector, item);
			return/* RET_NOERROR*/;
		}
	}
	else
	{
		for(ItemVector::iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
		{
			if((*it) != item)
				continue;

			if(item->isStackable() && count != item->getItemCount())
			{
				uint8_t newCount = (uint8_t)std::max((int32_t)0, (int32_t)(item->getItemCount() - count));
				updateTileFlags(item, true);

				item->setItemCount(newCount);
				updateTileFlags(item, false);

				onUpdateTileItem(item, item->getKind(), item, item->getKind());
			}
			else
			{
				const SpectatorList& list = server.game().getSpectators(pos);
				std::vector<int32_t> oldStackposVector;

				Player* tmpPlayer = nullptr;
				for(SpectatorList::const_iterator iit = list.begin(); iit != list.end(); ++iit)
				{
					if((tmpPlayer = (*iit)->getPlayer()))
						oldStackposVector.push_back(getClientIndexOfThing(tmpPlayer, (*it).get()));
				}

				server.game().autorelease(*it);

				(*it)->setParent(nullptr);
				items->erase(it);

				--items->downItemCount;
				--thingCount;
				onRemoveTileItem(list, oldStackposVector, item);
			}

			return/* RET_NOERROR*/;
		}
	}
}

int32_t Tile::getClientIndexOfThing(const Player* player, const Thing* thing) const
{
	if(ground && ground == thing)
		return 0;

	if(const Item* item = thing->getItem())
	{
		if(item->isGroundTile())
			return 0;
	}

	int32_t n = 0;
	if(!ground)
		n--;

	const TileItemVector* items = getItemList();
	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getTopItemCount();
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			if((*cit) == thing)
				return ++n;

			if(player->canSeeCreature((*cit).get()))
				++n;
		}
	}

	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getDownItemCount();
	}

	return -1;
}

int32_t Tile::__getIndexOfThing(const Thing* thing) const
{
	if(ground && ground == thing)
		return 0;

	int32_t n = 0;
	const TileItemVector* items = getItemList();
	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getTopItemCount();
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		for(CreatureVector::const_iterator cit = creatures->begin(); cit != creatures->end(); ++cit)
		{
			++n;
			if((*cit) == thing)
				return n;
		}
	}

	if(items)
	{
		if(thing && thing->getItem())
		{
			for(ItemVector::const_iterator it = items->getBeginDownItem(); it != items->getEndDownItem(); ++it)
			{
				++n;
				if((*it) == thing)
					return n;
			}
		}
		else
			n += items->getDownItemCount();
	}

	return -1;
}

uint32_t Tile::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/, bool itemCount /*= true*/, bool) const
{
	uint32_t count = 0;
	Thing* thing = nullptr;
	for(uint32_t i = 0; i < getThingCount(); ++i)
	{
		if(!(thing = __getThing(i)))
			continue;

		if(const Item* item = thing->getItem())
		{
			if(item->getId() != itemId || (subType != -1 && subType != item->getSubType()))
				continue;

			if(!itemCount)
			{
				if(item->isRune())
					count+= item->getCharges();
				else
					count+= item->getItemCount();
			}
			else
				count+= item->getItemCount();
		}
	}

	return count;
}

Thing* Tile::__getThing(uint32_t index) const
{
	if(ground)
	{
		if(!index)
			return ground.get();

		--index;
	}

	const TileItemVector* items = getItemList();
	if(items)
	{
		uint32_t topItemSize = items->getTopItemCount();
		if(index < topItemSize)
		{
			Item* item = items->at(items->downItemCount + index);
			if(item && !item->isRemoved())
				return item;
		}

		index -= topItemSize;
	}

	if(const CreatureVector* creatures = getCreatures())
	{
		if(index < (uint32_t)creatures->size())
			return creatures->at(index).get();

		index -= (uint32_t)creatures->size();
	}

	if(items && index < items->getDownItemCount())
	{
		Item* item = items->at(index);
		if(item && !item->isRemoved())
			return item;
	}

	return nullptr;
}

void Tile::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, cylinderlink_t link/* = LINK_OWNER*/)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;

	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->postAddNotification(actor, thing, oldParent, index, LINK_NEAR);
	}

	//add a reference to this item, it may be deleted after being added (mailbox for example)
	if(link == LINK_OWNER)
	{
		//calling movement scripts
		if(Creature* creature = thing->getCreature())
		{
			const Tile* fromTile = nullptr;
			if(oldParent)
				fromTile = oldParent->getTile();

			server.moveEvents().onCreatureMove(actor, creature, fromTile, this, true);
		}
		else if(Item* item = thing->getItem())
		{
			server.moveEvents().onAddTileItem(this, item);
			server.moveEvents().onItemMove(actor, item, this, true);
		}

		if(hasFlag(TILESTATE_TELEPORT))
		{
			if(Teleport* teleport = getTeleportItem())
				teleport->__addThing(actor, thing);
		}
		else if(hasFlag(TILESTATE_TRASHHOLDER))
		{
			if(TrashHolder* trashHolder = getTrashHolder())
				trashHolder->__addThing(actor, thing);
		}
		else if(hasFlag(TILESTATE_MAILBOX))
		{
			if(Mailbox* mailbox = getMailbox())
				mailbox->__addThing(actor, thing);
		}
	}
}

void Tile::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, cylinderlink_t link/* = LINK_OWNER*/)
{
	const Position& cylinderMapPos = pos;

	const SpectatorList& list = server.game().getSpectators(cylinderMapPos);
	SpectatorList::const_iterator it;
	if(/*isCompleteRemoval && */getThingCount() > 8)
		onUpdateTile();

	Player* tmpPlayer = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((tmpPlayer = (*it)->getPlayer()))
			tmpPlayer->postRemoveNotification(actor, thing, newParent, index, isCompleteRemoval, LINK_NEAR);
	}

	//calling movement scripts
	if(Creature* creature = thing->getCreature())
	{
		const Tile* toTile = nullptr;
		if(newParent)
			toTile = newParent->getTile();

		server.moveEvents().onCreatureMove(actor, creature, this, toTile, false);
	}
	else if(Item* item = thing->getItem())
	{
		server.moveEvents().onRemoveTileItem(this, item);
		server.moveEvents().onItemMove(actor, item, this, false);
	}
}


bool Tile::hasRoomForThing(const Thing& thing) const {
	if (thingCount >= StackPosition::MAX_INDEX) {
		// we are full
		return false;
	}

	if (thing.getCreature() != nullptr) {
		// add creatures until we are full
		return true;
	}

	if (thing.getItem() != nullptr) {
		auto creatures = getCreatures();
		if (creatures != nullptr && !creatures->empty()) {
			// we have at least one creature - add items until we are full
			return true;
		}

		if (thingCount + 1 >= StackPosition::MAX_INDEX) {
			// we have no creatures - leave at least one spot free so that creatures can still walk over this tile
			return false;
		}
	}

	return true;
}


void Tile::__internalAddThing(uint32_t index, Thing* thing)
{
	assert(thing->getParent() == nullptr);

	if (!hasRoomForThing(*thing)) {
		return;
	}

	if(Creature* creature = thing->getCreature())
	{
		server.game().clearSpectatorCache();
		CreatureVector* creatures = makeCreatures();
		creatures->push_back(creature);

		thing->setParent(this);
		++thingCount;
		return;
	}

	Item* item = thing->getItem();
	if(!item)
		return;

	TileItemVector* items = makeItemList();
	if(items && items->size() >= 0xFFFF)
		return/* RET_NOTPOSSIBLE*/;

	if(item->isGroundTile())
	{
		if (ground != nullptr) {
			return;
		}

		ground = item;
	}
	else if(item->isAlwaysOnTop())
	{
		bool isInserted = false;
		for(ItemVector::iterator it = items->getBeginTopItem(); it != items->getEndTopItem(); ++it)
		{
			if((*it)->getKind()->alwaysOnTopOrder <= item->getKind()->alwaysOnTopOrder)
				continue;

			items->insert(it, item);

			isInserted = true;
			break;
		}

		if(!isInserted)
		{
			items->push_back(item);
		}
	}
	else
	{
		items->insert(items->getBeginDownItem(), item);
		++items->downItemCount;
	}

	thing->setParent(this);
	++thingCount;

	updateTileFlags(item, false);
}

void Tile::updateTileFlags(Item* item, bool removed)
{
	if(!removed)
	{
		if(!hasFlag(TILESTATE_FLOORCHANGE))
		{
			if(item->floorChange(CHANGE_DOWN))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_DOWN);
			}

			if(item->floorChange(CHANGE_NORTH))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_NORTH);
			}

			if(item->floorChange(CHANGE_SOUTH))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_SOUTH);
			}

			if(item->floorChange(CHANGE_EAST))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_EAST);
			}

			if(item->floorChange(CHANGE_WEST))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_WEST);
			}

			if(item->floorChange(CHANGE_NORTH_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_NORTH_EX);
			}

			if(item->floorChange(CHANGE_SOUTH_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_SOUTH_EX);
			}

			if(item->floorChange(CHANGE_EAST_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_EAST_EX);
			}

			if(item->floorChange(CHANGE_WEST_EX))
			{
				setFlag(TILESTATE_FLOORCHANGE);
				setFlag(TILESTATE_FLOORCHANGE_WEST_EX);
			}
		}

		if(item->getTeleport())
			setFlag(TILESTATE_TELEPORT);

		if(item->getMagicField())
			setFlag(TILESTATE_MAGICFIELD);

		if(item->getMailbox())
			setFlag(TILESTATE_MAILBOX);

		if(item->getTrashHolder())
			setFlag(TILESTATE_TRASHHOLDER);

		if(item->getBed())
			setFlag(TILESTATE_BED);

		if(item->getContainer() && item->getContainer()->getDepot())
			setFlag(TILESTATE_DEPOT);

		if(item->hasProperty(BLOCKSOLID))
			setFlag(TILESTATE_BLOCKSOLID);

		if(item->hasProperty(IMMOVABLEBLOCKSOLID))
			setFlag(TILESTATE_IMMOVABLEBLOCKSOLID);

		if(item->hasProperty(BLOCKPATH))
			setFlag(TILESTATE_BLOCKPATH);

		if(item->hasProperty(NOFIELDBLOCKPATH))
			setFlag(TILESTATE_NOFIELDBLOCKPATH);

		if(item->hasProperty(IMMOVABLENOFIELDBLOCKPATH))
			setFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH);
	}
	else
	{
		if(item->floorChange(CHANGE_DOWN))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_DOWN);
		}

		if(item->floorChange(CHANGE_NORTH))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_NORTH);
		}

		if(item->floorChange(CHANGE_SOUTH))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_SOUTH);
		}

		if(item->floorChange(CHANGE_EAST))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_EAST);
		}

		if(item->floorChange(CHANGE_WEST))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_WEST);
		}

		if(item->floorChange(CHANGE_NORTH_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_NORTH_EX);
		}

		if(item->floorChange(CHANGE_SOUTH_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_SOUTH_EX);
		}

		if(item->floorChange(CHANGE_EAST_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_EAST_EX);
		}

		if(item->floorChange(CHANGE_WEST_EX))
		{
			resetFlag(TILESTATE_FLOORCHANGE);
			resetFlag(TILESTATE_FLOORCHANGE_WEST_EX);
		}

		if(item->getTeleport())
			resetFlag(TILESTATE_TELEPORT);

		if(item->getMagicField())
			resetFlag(TILESTATE_MAGICFIELD);

		if(item->getMailbox())
			resetFlag(TILESTATE_MAILBOX);

		if(item->getTrashHolder())
			resetFlag(TILESTATE_TRASHHOLDER);

		if(item->getBed())
			resetFlag(TILESTATE_BED);

		if(item->getContainer() && item->getContainer()->getDepot())
			resetFlag(TILESTATE_DEPOT);

		if(item->hasProperty(BLOCKSOLID) && !hasProperty(item, BLOCKSOLID))
			resetFlag(TILESTATE_BLOCKSOLID);

		if(item->hasProperty(IMMOVABLEBLOCKSOLID) && !hasProperty(item, IMMOVABLEBLOCKSOLID))
			resetFlag(TILESTATE_IMMOVABLEBLOCKSOLID);

		if(item->hasProperty(BLOCKPATH) && !hasProperty(item, BLOCKPATH))
			resetFlag(TILESTATE_BLOCKPATH);

		if(item->hasProperty(NOFIELDBLOCKPATH) && !hasProperty(item, NOFIELDBLOCKPATH))
			resetFlag(TILESTATE_NOFIELDBLOCKPATH);

		if(item->hasProperty(IMMOVABLEBLOCKPATH) && !hasProperty(item, IMMOVABLEBLOCKPATH))
			resetFlag(TILESTATE_IMMOVABLEBLOCKPATH);

		if(item->hasProperty(IMMOVABLENOFIELDBLOCKPATH) && !hasProperty(item, IMMOVABLENOFIELDBLOCKPATH))
			resetFlag(TILESTATE_IMMOVABLENOFIELDBLOCKPATH);
	}
}


DynamicTile::DynamicTile(uint16_t x, uint16_t y, uint16_t z):
	Tile(x, y, z)
{
	m_flags |= TILESTATE_DYNAMIC_TILE;
}


DynamicTile::~DynamicTile() {
	for (CreatureVector::iterator it = creatures.begin(); it != creatures.end(); ++it) {
		(*it)->setParent(nullptr);
	}

	for (ItemVector::iterator it = items.begin(); it != items.end(); ++it) {
		(*it)->setParent(nullptr);
	}
}


StaticTile::StaticTile(uint16_t x, uint16_t y, uint16_t z):
	Tile(x, y, z), items(nullptr), creatures(nullptr) {}


StaticTile::~StaticTile() {
	if (creatures != nullptr) {
		for (CreatureVector::iterator it = creatures->begin(); it != creatures->end(); ++it) {
			(*it)->setParent(nullptr);
		}

		delete creatures;
	}

	if (items != nullptr) {
		for (ItemVector::iterator it = items->begin(); it != items->end(); ++it) {
			(*it)->setParent(nullptr);
		}

		delete items;
	}
}
