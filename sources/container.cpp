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

#include "container.h"
#include "game.h"

#include "fileloader.h"
#include "iomap.h"
#include "items.h"
#include "player.h"
#include "server.h"
#include "tools.h"


LOGGER_DEFINITION(Container);


Container::Container(const ItemKindPC& kind) : Item(kind)
{
	maxSize = kind->maxItems;
	serializationCount = 0;
	totalWeight = 0.0;
}


Container::~Container()
{
	for(ItemList::iterator cit = itemlist.begin(); cit != itemlist.end(); ++cit)
	{
		(*cit)->setParent(nullptr);
	}

	itemlist.clear();
}


Container::ClassAttributesP Container::getClassAttributes() {
	return Item::getClassAttributes();
}


const std::string& Container::getClassName() {
	static const std::string name("Container");
	return name;
}


boost::intrusive_ptr<Item> Container::clone() const
{
	boost::intrusive_ptr<Container> _item = boost::static_pointer_cast<Container>(Item::clone());
	for(ItemList::const_iterator it = itemlist.begin(); it != itemlist.end(); ++it)
		_item->addItem((*it)->clone().get());

	return _item;
}

Container* Container::getParentContainer()
{
	if(Cylinder* cylinder = getParent())
	{
		if(Item* item = cylinder->getItem())
			return item->getContainer();
	}

	return nullptr;
}

void Container::addItem(Item* item)
{
	itemlist.push_back(item);
	item->setParent(this);
}

Attr_ReadValue Container::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch(attr)
	{
		case ATTR_CONTAINER_ITEMS:
		{
			uint32_t count;
			if(!propStream.GET_ULONG(count))
				return ATTR_READ_ERROR;

			serializationCount = count;
			return ATTR_READ_END;
		}

		default:
			break;
	}

	return Item::readAttr(attr, propStream);
}

bool Container::unserializeItemNode(FileLoader& f, NODE node, PropStream& propStream)
{
	if(!Item::unserializeItemNode(f, node, propStream))
		return false;

	uint32_t type;
	for(const NodeStruct* nodeItem = f.getChildNode(node, type); nodeItem; nodeItem = f.getNextNode(nodeItem, type))
	{
		//load container items
		if(type != OTBM_ITEM)
			return false;

		PropStream itemPropStream;
		f.getProps(nodeItem, itemPropStream);

		boost::intrusive_ptr<Item> item = Item::CreateItem(itemPropStream);
		if(!item)
			return false;

		if(!item->unserializeItemNode(f, nodeItem, itemPropStream))
			return false;

		addItem(item.get());
		updateItemWeight(item->getWeight());
	}

	return true;
}

void Container::updateItemWeight(double diff)
{
	totalWeight += diff;
	if(Container* parent = getParentContainer())
		parent->updateItemWeight(diff);
}

double Container::getWeight() const
{
	return Item::getWeight() + totalWeight;
}

std::string Container::getContentDescription() const
{
	std::stringstream s;
	return getContentDescription(s).str();
}

std::stringstream& Container::getContentDescription(std::stringstream& s) const
{
	bool begin = true;
	Container* evil = const_cast<Container*>(this);
	for(ContainerIterator it = evil->begin(); it != evil->end(); ++it)
	{
		if(!begin)
			s << ", ";
		else
			begin = false;

		s << (*it)->getNameDescription();
	}

	if(begin)
		s << "nothing";

	return s;
}

Item* Container::getItem(uint32_t index)
{
	size_t n = 0;
	for(ItemList::const_iterator cit = getItems(); cit != getEnd(); ++cit)
	{
		if(n == index)
			return (*cit).get();
		else
			++n;
	}

	return nullptr;
}

uint32_t Container::getItemHoldingCount() const
{
	uint32_t counter = 0;
	for(ContainerIterator it = begin(); it != end(); ++it)
		++counter;

	return counter;
}

bool Container::isHoldingItem(const Item* item) const
{
	for(ContainerIterator it = begin(); it != end(); ++it)
	{
		if((*it) == item)
			return true;
	}

	return false;
}

void Container::onAddContainerItem(Item* item)
{
	const Position& cylinderMapPos = getPosition();
	SpectatorList list;

	SpectatorList::iterator it;
	server.game().getSpectators(list, cylinderMapPos, false, false, 2, 2, 2, 2);

	//send to client
	Player* player = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendAddContainerItem(this, item);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->onAddContainerItem(this, item);
	}
}

void Container::onUpdateContainerItem(uint32_t index, Item* oldItem, const ItemKindPC& oldType,
	Item* newItem, const ItemKindPC& newType)
{
	const Position& cylinderMapPos = getPosition();
	SpectatorList list;

	SpectatorList::iterator it;
	server.game().getSpectators(list, cylinderMapPos, false, false, 2, 2, 2, 2);

	//send to client
	Player* player = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendUpdateContainerItem(this, index, oldItem, newItem);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->onUpdateContainerItem(this, index, oldItem, oldType, newItem, newType);
	}
}

void Container::onRemoveContainerItem(uint32_t index, Item* item)
{
	const Position& cylinderMapPos = getPosition();
	SpectatorList list;

	SpectatorList::iterator it;
	server.game().getSpectators(list, cylinderMapPos, false, false, 2, 2, 2, 2);

	//send change to client
	Player* player = nullptr;
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->sendRemoveContainerItem(this, index, item);
	}

	//event methods
	for(it = list.begin(); it != list.end(); ++it)
	{
		if((player = (*it)->getPlayer()))
			player->onRemoveContainerItem(this, index, item);
	}
}


Cylinder* Container::getParent() {return Thing::getParent();}
const Cylinder* Container::getParent() const {return Thing::getParent();}
bool Container::isRemoved() const {return Thing::isRemoved();}
Position Container::getPosition() const {return Thing::getPosition();}
Tile* Container::getTile() {return Thing::getTile();}
const Tile* Container::getTile() const {return Thing::getTile();}
Item* Container::getItem() {return this;}
const Item* Container::getItem() const {return this;}
Creature* Container::getCreature() {return nullptr;}
const Creature* Container::getCreature() const {return nullptr;}


ReturnValue Container::__queryAdd(int32_t index, const Thing* thing, uint32_t count,
	uint32_t flags) const
{
	if(((flags & FLAG_CHILDISOWNER) == FLAG_CHILDISOWNER))
	{
		//a child container is querying, since we are the top container (not carried by a player)
		//just return with no error.
		return RET_NOERROR;
	}

	const Item* item = thing->getItem();
	if(!item)
		return RET_NOTPOSSIBLE;

	if(!item->isPickupable())
		return RET_CANNOTPICKUP;

	if(item == this)
		return RET_THISISIMPOSSIBLE;

	if(const Container* container = item->getContainer())
	{
		for(const Cylinder* cylinder = getParent(); cylinder; cylinder = cylinder->getParent())
		{
			if(cylinder == container)
				return RET_THISISIMPOSSIBLE;
		}
	}

	if(index == INDEX_WHEREEVER && !((flags & FLAG_NOLIMIT) == FLAG_NOLIMIT) && full())
		return RET_CONTAINERNOTENOUGHROOM;

	const Cylinder* topParent = getTopParent();
	if(topParent != this)
		return topParent->__queryAdd(INDEX_WHEREEVER, item, count, flags | FLAG_CHILDISOWNER);

	return RET_NOERROR;
}

ReturnValue Container::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count,
	uint32_t& maxQueryCount, uint32_t flags) const
{
	const Item* item = thing->getItem();
	if(!item)
	{
		maxQueryCount = 0;
		return RET_NOTPOSSIBLE;
	}

	if(((flags & FLAG_NOLIMIT) == FLAG_NOLIMIT))
	{
		maxQueryCount = std::max((uint32_t)1, count);
		return RET_NOERROR;
	}

	int32_t freeSlots = std::max((int32_t)(capacity() - size()), (int32_t)0);
	if(item->isStackable())
	{
		uint32_t n = 0;
		if(index != INDEX_WHEREEVER)
		{
			const Thing* destThing = __getThing(index);
			const Item* destItem = nullptr;
			if(destThing)
				destItem = destThing->getItem();

			if(destItem && destItem->getId() == item->getId())
				n = 100 - destItem->getItemCount();
		}

		maxQueryCount = freeSlots * 100 + n;
		if(maxQueryCount < count)
			return RET_CONTAINERNOTENOUGHROOM;
	}
	else
	{
		maxQueryCount = freeSlots;
		if(maxQueryCount == 0)
			return RET_CONTAINERNOTENOUGHROOM;
	}

	return RET_NOERROR;
}

ReturnValue Container::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags) const
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
		return RET_NOTPOSSIBLE;

	const Item* item = thing->getItem();
	if(item == nullptr)
		return RET_NOTPOSSIBLE;

	if(count == 0 || (item->isStackable() && count > item->getItemCount()))
		return RET_NOTPOSSIBLE;

	if(item->isNotMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags))
		return RET_NOTMOVEABLE;

	return RET_NOERROR;
}

Cylinder* Container::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	if(index == 254 /*move up*/)
	{
		index = INDEX_WHEREEVER;
		*destItem = nullptr;

		Container* parentContainer = dynamic_cast<Container*>(getParent());
		if(parentContainer)
			return parentContainer;
		else
			return this;
	}
	else if(index == 255 /*add wherever*/)
	{
		index = INDEX_WHEREEVER;
		*destItem = nullptr;
		return this;
	}
	else
	{
		if(index >= (int32_t)capacity())
		{
			/*
			if you have a container, maximize it to show all 20 slots
			then you open a bag that is inside the container you will have a bag with 8 slots
			and a "grey" area where the other 12 slots where from the container
			if you drop the item on that grey area
			the client calculates the slot position as if the bag has 20 slots
			*/
			index = INDEX_WHEREEVER;
		}

		if(index != INDEX_WHEREEVER)
		{
			Thing* destThing = __getThing(index);
			if(destThing)
				*destItem = destThing->getItem();

			if(Cylinder* subCylinder = dynamic_cast<Cylinder*>(*destItem))
			{
				index = INDEX_WHEREEVER;
				*destItem = nullptr;
				return subCylinder;
			}
		}
	}

	return this;
}

void Container::__addThing(Creature* actor, Thing* thing)
{
	return __addThing(actor, 0, thing);
}

void Container::__addThing(Creature* actor, int32_t index, Thing* thing)
{
	if(index >= (int32_t)capacity())
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__addThing], index:" << index << ", index >= capacity()");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(item == nullptr)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__addThing] item == nullptr");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

#ifdef __DEBUG_MOVESYS__
	if(index != INDEX_WHEREEVER)
	{
		if(size() >= capacity())
		{
			LOGt("Failure: [Container::__addThing] size() >= capacity()");
			DEBUG_REPORT
			return /*RET_CONTAINERNOTENOUGHROOM*/;
		}
	}
#endif

	item->setParent(this);

	/*
	// needs netcode update!
	if (index > 0 && static_cast<uint32_t>(index) > itemlist.size()) {
		itemlist.push_back(item);
	}
	else {
		itemlist.insert(std::next(itemlist.begin(), index), item);
	}
	*/

	itemlist.push_front(item);

	totalWeight += item->getWeight();
	if(Container* parentContainer = getParentContainer())
		parentContainer->updateItemWeight(item->getWeight());

	//send change to client
	if(getParent() && getParent() != &VirtualCylinder::virtualCylinder)
		onAddContainerItem(item);
}

void Container::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__updateThing] index == -1");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(item == nullptr)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__updateThing] item == nullptr");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	ItemKindPC oldType = item->getKind();
	ItemKindPC newType = server.items()[itemId];
	if (!newType) {
		return;
	}

	const double oldWeight = item->getWeight();
	item->setKind(newType);
	item->setSubType(count);

	const double diffWeight = -oldWeight + item->getWeight();
	totalWeight += diffWeight;
	if(Container* parentContainer = getParentContainer())
		parentContainer->updateItemWeight(diffWeight);

	//send change to client
	if(getParent())
		onUpdateContainerItem(index, item, oldType, item, newType);
}

void Container::__replaceThing(uint32_t index, Thing* thing)
{
	Item* item = thing->getItem();
	if(item == nullptr)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__replaceThing] item == nullptr");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	uint32_t count = 0;
	ItemList::iterator cit = itemlist.end();
	for(cit = itemlist.begin(); cit != itemlist.end(); ++cit)
	{
		if(count == index)
			break;
		else
			++count;
	}

	if(cit == itemlist.end())
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__updateThing] item not found");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	boost::intrusive_ptr<Item> existingItem = *cit;

	totalWeight -= existingItem->getWeight();
	totalWeight += item->getWeight();
	if(Container* parentContainer = getParentContainer())
		parentContainer->updateItemWeight(-existingItem->getWeight() + item->getWeight());

	itemlist.insert(cit, item);
	item->setParent(this);
	//send change to client
	if(getParent())
	{
		onUpdateContainerItem(index, (*cit).get(), existingItem->getKind(), item, item->getKind());
	}

	(*cit)->setParent(nullptr);

	server.game().autorelease(*cit);
	itemlist.erase(cit);
}

void Container::__removeThing(Thing* thing, uint32_t count)
{
	Item* item = thing->getItem();
	if(item == nullptr)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__removeThing] item == nullptr");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__removeThing] index == -1");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	ItemList::iterator cit = std::find(itemlist.begin(), itemlist.end(), thing);
	if(cit == itemlist.end())
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__removeThing] item not found");
		DEBUG_REPORT
#endif
		return /*RET_NOTPOSSIBLE*/;
	}

	if(item->isStackable() && count != item->getItemCount())
	{
		const double oldWeight = -item->getWeight();
		item->setItemCount(std::max(0, (int32_t)(item->getItemCount() - count)));

		const double diffWeight = oldWeight + item->getWeight();
		totalWeight += diffWeight;
		//send change to client
		if(getParent())
		{
			if(Container* parentContainer = getParentContainer())
				parentContainer->updateItemWeight(diffWeight);

			onUpdateContainerItem(index, item, item->getKind(), item, item->getKind());
		}
	}
	else
	{
		//send change to client
		if(getParent())
		{
			if(Container* parentContainer = getParentContainer())
				parentContainer->updateItemWeight(-item->getWeight());

			onRemoveContainerItem(index, item);
		}

		totalWeight -= item->getWeight();
		item->setParent(nullptr);

		server.game().autorelease(*cit);
		itemlist.erase(cit);
	}
}

Thing* Container::__getThing(uint32_t index) const
{
	if(index > size())
		return nullptr;

	uint32_t count = 0;
	for(ItemList::const_iterator cit = itemlist.begin(); cit != itemlist.end(); ++cit)
	{
		if(count == index)
			return (*cit).get();
		else
			++count;
	}

	return nullptr;
}

int32_t Container::__getIndexOfThing(const Thing* thing) const
{
	uint32_t index = 0;
	for(ItemList::const_iterator cit = getItems(); cit != getEnd(); ++cit)
	{
		if(*cit == thing)
			return index;
		else
			++index;
	}

	return -1;
}

int32_t Container::__getFirstIndex() const
{
	return 0;
}

int32_t Container::__getLastIndex() const
{
	return size();
}

uint32_t Container::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/, bool itemCount /*= true*/, bool) const
{
	uint32_t count = 0;

	Item* item = nullptr;
	for(ItemList::const_iterator it = itemlist.begin(); it != itemlist.end(); ++it)
	{
		item = (*it).get();
		if(item && item->getId() == itemId && (subType == -1 || subType == item->getSubType()))
		{
			if(!itemCount)
			{
				if(item->isRune())
					count += item->getCharges();
				else
					count += item->getItemCount();
			}
			else
				count += item->getItemCount();
		}
	}

	return count;
}

std::map<uint32_t, uint32_t>& Container::__getAllItemTypeCount(std::map<uint32_t,
	uint32_t>& countMap, bool itemCount /*= true*/) const
{
	Item* item = nullptr;
	for(ItemList::const_iterator it = itemlist.begin(); it != itemlist.end(); ++it)
	{
		item = (*it).get();
		if(!itemCount)
		{
			if(item->isRune())
				countMap[item->getId()] += item->getCharges();
			else
				countMap[item->getId()] += item->getItemCount();
		}
		else
			countMap[item->getId()] += item->getItemCount();
	}

	return countMap;
}

void Container::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, cylinderlink_t link /*= LINK_OWNER*/)
{
	Cylinder* topParent = getTopParent();
	if(!topParent->getCreature())
	{
		if(topParent == this)
		{
			//let the tile class notify surrounding players
			if(topParent->getParent())
				topParent->getParent()->postAddNotification(actor, thing, oldParent, index, LINK_NEAR);
		}
		else
			topParent->postAddNotification(actor, thing, oldParent, index, LINK_PARENT);
	}
	else
		topParent->postAddNotification(actor, thing, oldParent, index, LINK_TOPPARENT);
}

void Container::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, cylinderlink_t link /*= LINK_OWNER*/)
{
	Cylinder* topParent = getTopParent();
	if(!topParent->getCreature())
	{
		if(topParent == this)
		{
			//let the tile class notify surrounding players
			if(topParent->getParent())
				topParent->getParent()->postRemoveNotification(actor, thing,
					newParent, index, isCompleteRemoval, LINK_NEAR);
		}
		else
			topParent->postRemoveNotification(actor, thing, newParent,
				index, isCompleteRemoval, LINK_PARENT);
	}
	else
		topParent->postRemoveNotification(actor, thing, newParent,
			index, isCompleteRemoval, LINK_TOPPARENT);
}

void Container::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Container::__internalAddThing(uint32_t index, Thing* thing)
{
#ifdef __DEBUG_MOVESYS__
	LOGt("[Container::__internalAddThing] index: " << index);
#endif
	if(!thing)
		return;

	Item* item = thing->getItem();
	if(item == nullptr)
	{
#ifdef __DEBUG_MOVESYS__
		LOGt("Failure: [Container::__internalAddThing] item == nullptr");
#endif
		return;
	}

	itemlist.push_front(item);
	item->setParent(this);

	totalWeight += item->getWeight();
	if(Container* parentContainer = getParentContainer())
		parentContainer->updateItemWeight(item->getWeight());
}

void Container::__startDecaying()
{
	for(ItemList::const_iterator it = itemlist.begin(); it != itemlist.end(); ++it)
		(*it)->__startDecaying();
}

ContainerIterator Container::begin()
{
	ContainerIterator cit(this);
	if(!itemlist.empty())
	{
		cit.over.push(this);
		cit.current = itemlist.begin();
	}

	return cit;
}

ContainerIterator Container::end()
{
	ContainerIterator cit(this);
	return cit;
}

ContainerIterator Container::begin() const
{
	Container* evil = const_cast<Container*>(this);
	return evil->begin();
}

ContainerIterator Container::end() const
{
	Container* evil = const_cast<Container*>(this);
	return evil->end();
}

ContainerIterator::ContainerIterator():
base(nullptr) {}

ContainerIterator::ContainerIterator(Container* _base):
base(_base) {}

ContainerIterator::ContainerIterator(const ContainerIterator& rhs):
base(rhs.base), over(rhs.over), current(rhs.current) {}

bool ContainerIterator::operator==(const ContainerIterator& rhs)
{
	return !(*this != rhs);
}

bool ContainerIterator::operator!=(const ContainerIterator& rhs)
{
	assert(base);
	if(base != rhs.base)
		return true;

	if(over.empty() && rhs.over.empty())
		return false;

	if(over.empty())
		return true;

	if(rhs.over.empty())
		return true;

	if(over.front() != rhs.over.front())
		return true;

	return current != rhs.current;
}

ContainerIterator& ContainerIterator::operator=(const ContainerIterator& rhs)
{
	this->base = rhs.base;
	this->current = rhs.current;
	this->over = rhs.over;
	return *this;
}

Item* ContainerIterator::operator*()
{
	assert(base);
	return (*current).get();
}

Item* ContainerIterator::operator->()
{
	return *(*this);
}

ContainerIterator& ContainerIterator::operator++()
{
	assert(base);
	if(Item* item = (*current).get())
	{
		Container* container = item->getContainer();
		if(container && !container->empty())
			over.push(container);
	}

	++current;
	if(current == over.front()->itemlist.end())
	{
		over.pop();
		if(over.empty())
			return *this;

		current = over.front()->itemlist.begin();
	}

	return *this;
}

ContainerIterator ContainerIterator::operator++(int32_t)
{
	ContainerIterator tmp(*this);
	++*this;
	return tmp;
}
