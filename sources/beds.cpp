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

#include "beds.h"
#include "house.h"
#include "player.h"
#include "fileloader.h"
#include "iologindata.h"

#include "game.h"
#include "configmanager.h"
#include "server.h"
#include "scheduler.h"
#include "schedulertask.h"



const std::string BedItem::ATTRIBUTE_SLEEPSTART("sleepstart");



BedItem::ClassAttributesP BedItem::getClassAttributes() {
	using attributes::Type;

	auto attributes = Container::getClassAttributes();
	attributes->emplace(ATTRIBUTE_SLEEPSTART, Type::INTEGER);

	return attributes;
}


const std::string& BedItem::getClassName() {
	static const std::string name("Bed");
	return name;
}


Attr_ReadValue BedItem::readAttr(AttrTypes_t attr, PropStream& propStream)
{
	switch(attr)
	{
		case ATTR_SLEEPERGUID:
		{
			uint32_t _sleeper;
			if(!propStream.GET_ULONG(_sleeper))
				return ATTR_READ_ERROR;

			if(_sleeper)
			{
				std::string name;
				if(IOLoginData::getInstance()->getNameByGuid(_sleeper, name))
				{
					setSpecialDescription(name + " is sleeping there.");
					Beds::getInstance()->setBedSleeper(this, _sleeper);
				}
			}

			sleeper = _sleeper;
			return ATTR_READ_CONTINUE;
		}

		case ATTR_SLEEPSTART:
		{
			uint32_t sleepStart;
			if(!propStream.GET_ULONG(sleepStart))
				return ATTR_READ_ERROR;

			getAttributes().set(ATTRIBUTE_SLEEPSTART, (int32_t)sleepStart);
			return ATTR_READ_CONTINUE;
		}

		default:
			break;
	}

	return Item::readAttr(attr, propStream);
}

bool BedItem::serializeAttr(PropWriteStream& propWriteStream) const
{
	bool ret = Item::serializeAttr(propWriteStream);
	if(!sleeper)
		return ret;

	propWriteStream.ADD_UCHAR(ATTR_SLEEPERGUID);
	propWriteStream.ADD_ULONG(sleeper);
	return ret;
}

BedItem* BedItem::getNextBedItem()
{
	if(Tile* tile = server.game().getTile(getNextPosition(getKind()->bedPartnerDir, getPosition())))
		return tile->getBedItem();

	return nullptr;
}

bool BedItem::canUse(Player* player)
{
	if(!house || !player || player->isRemoved() || (!player->isPremium() && server.configManager().getBool(
		ConfigManager::BED_REQUIRE_PREMIUM)) || player->hasCondition(CONDITION_INFIGHT))
		return false;

	if(!sleeper || house->getHouseAccessLevel(player) == HOUSE_OWNER)
		return isBed();

	Player* _player = server.game().getPlayerByGuidEx(sleeper);
	if(!_player)
		return isBed();

	bool ret = house->getHouseAccessLevel(_player) <= house->getHouseAccessLevel(player);
	if(_player->isVirtual())
		delete _player;

	return ret;
}

void BedItem::sleep(Player* player)
{
	if(!house || !player || player->isRemoved())
		return;

	if(!sleeper)
	{
		Beds::getInstance()->setBedSleeper(this, player->getGUID());
		internalSetSleeper(player);

		BedItem* nextBedItem = getNextBedItem();
		if(nextBedItem)
			nextBedItem->internalSetSleeper(player);

		updateAppearance(player);
		if(nextBedItem)
			nextBedItem->updateAppearance(player);

		player->getTile()->moveCreature(nullptr, player, getTile());
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_SLEEP);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(SCHEDULER_MINTICKS), std::bind(&Game::kickPlayer, &server.game(), player->getID(), false)));
		return;
	}

	if(getKind()->transformToFree)
	{
		wakeUp();
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return;
	}

	player->sendCancelMessage(RET_NOTPOSSIBLE);
}

void BedItem::wakeUp()
{
	if(!house)
		return;

	if(sleeper)
	{
		if(Player* player = server.game().getPlayerByGuidEx(sleeper))
		{
			regeneratePlayer(player);
			if(player->isVirtual())
			{
				IOLoginData::getInstance()->savePlayer(player);
				delete player;
			}
			else
				server.game().addCreatureHealth(player);
		}
	}

	Beds::getInstance()->setBedSleeper(nullptr, sleeper);
	internalRemoveSleeper();

	BedItem* nextBedItem = getNextBedItem();
	if(nextBedItem)
		nextBedItem->internalRemoveSleeper();

	updateAppearance(nullptr);
	if(nextBedItem)
		nextBedItem->updateAppearance(nullptr);
}

void BedItem::regeneratePlayer(Player* player) const
{
	const int32_t* sleepStart = getAttributes().getInteger(ATTRIBUTE_SLEEPSTART);
	int32_t sleptTime = (int32_t)time(nullptr) - (sleepStart ? *sleepStart : 0);
	if(Condition* condition = player->getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT))
	{
		int32_t amount = sleptTime / 30;
		if(condition->getTicks() != -1)
		{
			amount = std::min((condition->getTicks() / 1000), sleptTime) / 30;
			int32_t tmp = condition->getTicks() - (amount * 30000);
			if(tmp <= 0)
				player->removeCondition(condition);
			else
				condition->setTicks(tmp);
		}

		player->changeHealth(amount);
		player->changeMana(amount);
	}

	player->changeSoul((int32_t)std::max((float)0, (float)sleptTime / (60 * 15)));
}

void BedItem::updateAppearance(const Player* player)
{
	if(!getKind()->isBed())
		return;

	if(player && getKind()->transformUseTo[player->getSex(false)])
	{
		ItemKindPC newType = server.items()[getKind()->transformUseTo[player->getSex(false)]];
		if(newType && newType->isBed())
			server.game().transformItem(this, getKind()->transformUseTo[player->getSex(false)]);
	}
	else if(getKind()->transformToFree)
	{
		ItemKindPC newType = server.items()[getKind()->transformToFree];
		if(newType && newType->isBed())
			server.game().transformItem(this, getKind()->transformToFree);
	}
}

void BedItem::internalSetSleeper(const Player* player)
{
	sleeper = player->getGUID();
	getAttributes().set(ATTRIBUTE_SLEEPSTART, (int32_t)time(nullptr));
	setSpecialDescription(player->getName() + " is sleeping there.");
}

void BedItem::internalRemoveSleeper()
{
	sleeper = 0;
	getAttributes().remove(ATTRIBUTE_SLEEPSTART);
	setSpecialDescription("Nobody is sleeping there.");
}

BedItem* Beds::getBedBySleeper(uint32_t guid)
{
	std::map<uint32_t, BedItem*>::iterator it = BedSleepersMap.find(guid);
	if(it != BedSleepersMap.end())
		return it->second;

	return nullptr;
}
