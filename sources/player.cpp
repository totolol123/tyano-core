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

#include "player.h"
#include "iologindata.h"
#include "ioban.h"

#include "depot.h"
#include "town.h"
#include "house.h"
#include "beds.h"

#include "combat.h"

#include "protocolgame.h"
#include "movement.h"
#include "weapons.h"
#include "creatureevent.h"

#include "account.h"
#include "dispatcher.h"
#include "ioguild.h"
#include "condition.h"
#include "configmanager.h"
#include "group.h"
#include "game.h"
#include "monster.h"
#include "chat.h"
#include "items.h"
#include "npc.h"
#include "outfit.h"
#include "party.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"
#include "vocation.h"


LOGGER_DEFINITION(Player);



CreatureP Player::getDirectOwner() {
	return nullptr;
}


CreaturePC Player::getDirectOwner() const {
	return nullptr;
}


bool Player::isEnemy(const CreaturePC& creature) const {
	if (creature == nullptr) {
		assert(creature != nullptr);
		return false;
	}

	return creature->isEnemy(this);
}












AutoList<Player> Player::autoList;
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t Player::playerCount = 0;
#endif
MuteCountMap Player::muteCountMap;

Player::Player(const std::string& _name, ProtocolGame* p):
	Creature(), transferContainer(server.items()[ITEM_LOCKER]), name(_name), nameDescription(_name), client(p)
{
	if(client)
		client->setPlayer(this);

	pzLocked = isConnecting = addAttackSkillPoint = requestedOutfit = false;
	saving = true;

	lastAttackBlockType = BLOCK_NONE;
	chaseMode = CHASEMODE_STANDSTILL;
	fightMode = FIGHTMODE_ATTACK;
	tradeState = TRADE_NONE;
	accountManager = MANAGER_NONE;
	guildLevel = GUILDLEVEL_NONE;

	promotionLevel = walkTaskEvent = actionTaskEvent = nextStepEvent = bloodHitCount = shieldBlockCount = 0;
	lastAttack = idleTime = marriage = blessings = balance = premiumDays = mana = manaMax = manaSpent = 0;
	soul = guildId = levelPercent = magLevelPercent = magLevel = experience = damageImmunities = 0;
	conditionImmunities = conditionSuppressions = groupId = vocation_id = managerNumber2 = town = skullEnd = 0;
	lastLogin = lastLogout = lastIP = messageTicks = messageBuffer = nextAction = 0;
	editListId = maxWriteLen = windowTextId = rankId = 0;

	purchaseCallback = saleCallback = -1;
	level = shootRange = 1;
	rates[SKILL__MAGLEVEL] = rates[SKILL__LEVEL] = 1.0f;
	soulMax = 100;
	capacity = 400.00;
	stamina = STAMINA_MAX;
	lastLoad = lastPing = lastPong = OTSYS_TIME();

	writeItem = nullptr;
	group = nullptr;
	editHouse = nullptr;
	shopOwner = nullptr;
	tradeItem = nullptr;
	tradePartner = nullptr;
	walkTask = nullptr;

	setVocation(0);
	setParty(nullptr);

	transferContainer.setParent(nullptr);
	for(int32_t i = 0; i < 11; i++)
	{
		inventory[i] = nullptr;
		inventoryAbilities[i] = false;
	}

	for(int32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i)
	{
		skills[i][SKILL_LEVEL] = 10;
		skills[i][SKILL_TRIES] = skills[i][SKILL_PERCENT] = 0;
		rates[i] = 1.0f;
	}

	for(int32_t i = SKILL_FIRST; i <= SKILL_LAST; ++i)
		varSkills[i] = 0;

	for(int32_t i = STAT_FIRST; i <= STAT_LAST; ++i)
		varStats[i] = 0;

	for(int32_t i = LOSS_FIRST; i <= LOSS_LAST; ++i)
		lossPercent[i] = 100;

	for(int8_t i = 0; i < 13; i++)
		talkState[i] = false;
#ifdef __ENABLE_SERVER_DIAGNOSTIC__

	playerCount++;
#endif
}

Player::~Player()
{
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
	playerCount--;
#endif
	setWriteItem(nullptr);
	for(int32_t i = 0; i < 11; i++)
	{
		if(inventory[i])
		{
			inventory[i]->setParent(nullptr);
			inventory[i].reset();

			inventoryAbilities[i] = false;
		}
	}

	setNextWalkActionTask(nullptr);
	transferContainer.setParent(nullptr);
}

void Player::setVocation(uint32_t vocId)
{
	vocation_id = vocId;
	vocation = Vocations::getInstance()->getVocation(vocId);

	soulMax = vocation->getGain(GAIN_SOUL);
	if(Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT))
	{
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->getGainAmount(GAIN_HEALTH));
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, (vocation->getGainTicks(GAIN_HEALTH) * 1000));
		condition->setParam(CONDITIONPARAM_MANAGAIN, vocation->getGainAmount(GAIN_MANA));
		condition->setParam(CONDITIONPARAM_MANATICKS, (vocation->getGainTicks(GAIN_MANA) * 1000));
	}
}

bool Player::isPushable() const
{
	return accountManager == MANAGER_NONE && !hasFlag(PlayerFlag_CannotBePushed) && Creature::isPushable();
}

std::string Player::getDescription(int32_t lookDistance) const
{
	std::stringstream s;
	if(lookDistance == -1)
	{
		s << "yourself.";
		if(hasFlag(PlayerFlag_ShowGroupNameInsteadOfVocation))
			s << " You are " << group->getName();
		else if(vocation_id != 0)
			s << " You are " << vocation->getDescription();
		else
			s << " You have no vocation";
	}
	else
	{
		s << nameDescription;
		if(!hasCustomFlag(PlayerCustomFlag_HideLevel))
			s << " (Level " << level << ")";

		s << ". " << (sex % 2 ? "He" : "She");
		if(hasFlag(PlayerFlag_ShowGroupNameInsteadOfVocation))
			s << " is " << group->getName();
		else if(vocation_id != 0)
			s << " is " << vocation->getDescription();
		else
			s << " has no vocation";

		s << getSpecialDescription();
	}

	std::string tmp;
	if(marriage && IOLoginData::getInstance()->getNameByGuid(marriage, tmp))
	{
		s << ", ";
		if(vocation_id == 0)
		{
			if(lookDistance == -1)
				s << "and you are";
			else
				s << "and is";

			s << " ";
		}

		s << (sex % 2 ? "husband" : "wife") << " of " << tmp;
	}

	s << ".";
	if(guildId)
	{
		if(lookDistance == -1)
			s << " You are ";
		else
			s << " " << (sex % 2 ? "He" : "She") << " is ";

		s << (rankName.empty() ? "a member" : rankName)<< " of the " << guildName;
		if(!guildNick.empty())
			s << " (" << guildNick << ")";

		s << ".";
	}

	return s.str();
}

Item* Player::getInventoryItem(slots_t slot) const
{
	if(slot > slots_t::PRE_FIRST && slot < slots_t::LAST)
		return inventory[+slot].get();

	if(slot == slots_t::HAND)
		return inventory[+slots_t::LEFT]
		                 ? inventory[+slots_t::LEFT].get()
		                		 : inventory[+slots_t::RIGHT].get();

	return nullptr;
}

Item* Player::getEquippedItem(slots_t slot) const
{
	Item* item = getInventoryItem(slot);
	if(!item)
		return nullptr;

	switch(slot)
	{
		case slots_t::LEFT:
		case slots_t::RIGHT:
			return item->getWieldPosition() == slots_t::HAND ? item : nullptr;

		default:
			break;
	}

	return item->getWieldPosition() == slot ? item : nullptr;
}

void Player::setConditionSuppressions(uint32_t conditions, bool remove)
{
	if(!remove)
		conditionSuppressions |= conditions;
	else
		conditionSuppressions &= ~conditions;
}

Item* Player::getWeapon(bool ignoreAmmo /*= false*/)
{
	Item* item;
	for(uint32_t slot = +slots_t::RIGHT; slot <= +slots_t::LEFT; slot++)
	{
		item = getEquippedItem((slots_t)slot);
		if(!item)
			continue;

		switch(item->getWeaponType())
		{
			case WEAPON_SWORD:
			case WEAPON_AXE:
			case WEAPON_CLUB:
			case WEAPON_WAND:
			case WEAPON_FIST:
			{
				WeaponPC weapon = server.weapons().getWeapon(item);
				if(weapon)
					return item;
				break;
			}

			case WEAPON_DIST:
			{
				if(!ignoreAmmo && item->getAmmoType() != AMMO_NONE)
				{
					Item* ammoItem = getInventoryItem(slots_t::AMMO);
					if(ammoItem && ammoItem->getAmmoType() == item->getAmmoType())
					{
						WeaponPC weapon = server.weapons().getWeapon(ammoItem);
						if(weapon)
						{
							shootRange = item->getShootRange();
							return ammoItem;
						}
					}
				}
				else
				{
					WeaponPC weapon = server.weapons().getWeapon(item);
					if(weapon)
					{
						shootRange = item->getShootRange();
						return item;
					}
				}
				break;
			}

			default:
				break;
		}
	}

	return nullptr;
}

WeaponType_t Player::getWeaponType()
{
	if(Item* item = getWeapon())
		return item->getWeaponType();

	return WEAPON_NONE;
}

int32_t Player::getWeaponSkill(const Item* item) const
{
	if(!item)
		return getSkill(SKILL_FIST, SKILL_LEVEL);

	switch(item->getWeaponType())
	{
		case WEAPON_SWORD:
			return getSkill(SKILL_SWORD, SKILL_LEVEL);

		case WEAPON_CLUB:
			return getSkill(SKILL_CLUB, SKILL_LEVEL);

		case WEAPON_AXE:
			return getSkill(SKILL_AXE, SKILL_LEVEL);

		case WEAPON_FIST:
			return getSkill(SKILL_FIST, SKILL_LEVEL);

		case WEAPON_DIST:
			return getSkill(SKILL_DIST, SKILL_LEVEL);

		default:
			break;
	}

	return 0;
}

int32_t Player::getArmor() const
{
	int32_t armor = 0;
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(Item* item = getInventoryItem((slots_t)i))
			armor += item->getArmor();
	}

	if(vocation->getMultiplier(MULTIPLIER_ARMOR) != 1.0)
		return int32_t(armor * vocation->getMultiplier(MULTIPLIER_ARMOR));

	return armor;
}

void Player::getShieldAndWeapon(const Item* &shield, const Item* &weapon) const
{
	shield = weapon = nullptr;

	Item* item = nullptr;
	for(uint32_t slot = +slots_t::RIGHT; slot <= +slots_t::LEFT; slot++)
	{
		item = getInventoryItem((slots_t)slot);
		if(!item)
			continue;

		switch(item->getWeaponType())
		{
			case WEAPON_NONE:
				break;

			case WEAPON_SHIELD:
			{
				if(!shield || (shield && item->getDefense() > shield->getDefense()))
					shield = item;

				break;
			}

			default: //weapons that are not shields
			{
				weapon = item;
				break;
			}
		}
	}
}

int32_t Player::getDefense() const
{
	int32_t baseDefense = 5, defenseValue = 0, defenseSkill = 0, extraDefense = 0;
	float defenseFactor = getDefenseFactor();

	const Item* weapon = nullptr;
	const Item* shield = nullptr;

	getShieldAndWeapon(shield, weapon);
	if(weapon)
	{
		extraDefense = weapon->getExtraDefense();
		defenseValue = baseDefense + weapon->getDefense();
		defenseSkill = getWeaponSkill(weapon);
	}

	if(shield && shield->getDefense() > defenseValue)
	{
		if(shield->getExtraDefense() > extraDefense)
			extraDefense = shield->getExtraDefense();

		defenseValue = baseDefense + shield->getDefense();
		defenseSkill = getSkill(SKILL_SHIELD, SKILL_LEVEL);
	}

	if(!defenseSkill)
		return 0;

	defenseValue += extraDefense;
	if(vocation->getMultiplier(MULTIPLIER_DEFENSE) != 1.0)
		defenseValue = int32_t(defenseValue * vocation->getMultiplier(MULTIPLIER_DEFENSE));

	return ((int32_t)std::ceil(((float)(defenseSkill * (defenseValue * 0.015)) + (defenseValue * 0.1)) * defenseFactor));
}

float Player::getAttackFactor() const
{
	switch(fightMode)
	{
		case FIGHTMODE_BALANCED:
			return 1.2f;

		case FIGHTMODE_DEFENSE:
			return 2.0f;

		case FIGHTMODE_ATTACK:
		default:
			break;
	}

	return 1.0f;
}

float Player::getDefenseFactor() const
{
	switch(fightMode)
	{
		case FIGHTMODE_BALANCED:
			return 1.2f;

		case FIGHTMODE_DEFENSE:
		{
			if((OTSYS_TIME() - lastAttack) < const_cast<Player*>(this)->getAttackSpeed()) //attacking will cause us to get into normal defense
				return 1.0f;

			return 2.0f;
		}

		case FIGHTMODE_ATTACK:
		default:
			break;
	}

	return 1.0f;
}

void Player::sendIcons() const
{
	if(!client)
		return;

	uint32_t icons = 0;
	for(ConditionList::const_iterator it = conditions.begin(); it != conditions.end(); ++it)
	{
		if(!isSuppress((*it)->getType()))
			icons |= (*it)->getIcons();
	}

	if(getZone() == ZONE_PROTECTION)
		icons |= ICON_PROTECTIONZONE;

	if(pzLocked)
		icons |= ICON_PZ;

	client->sendIcons(icons);
}

void Player::updateInventoryWeight()
{
	inventoryWeight = 0.00;
	if(hasFlag(PlayerFlag_HasInfiniteCapacity))
		return;

	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(Item* item = getInventoryItem((slots_t)i))
			inventoryWeight += item->getWeight();
	}
}

void Player::updateInventoryGoods(const ItemKindPC& kind)
{
	if(kind->worth)
	{
		sendGoods();
		return;
	}

	for(ShopInfoList::iterator it = shopOffer.begin(); it != shopOffer.end(); ++it)
	{
		if(it->itemId != kind->id)
			continue;

		sendGoods();
		break;
	}
}

int32_t Player::getPlayerInfo(playerinfo_t playerinfo) const
{
	switch(playerinfo)
	{
		case PLAYERINFO_LEVEL:
			return level;
		case PLAYERINFO_LEVELPERCENT:
			return levelPercent;
		case PLAYERINFO_MAGICLEVEL:
			return std::max((int32_t)0, ((int32_t)magLevel + varStats[STAT_MAGICLEVEL]));
		case PLAYERINFO_MAGICLEVELPERCENT:
			return magLevelPercent;
		case PLAYERINFO_HEALTH:
			return health;
		case PLAYERINFO_MAXHEALTH:
			return std::max((int32_t)1, ((int32_t)healthMax + varStats[STAT_MAXHEALTH]));
		case PLAYERINFO_MANA:
			return mana;
		case PLAYERINFO_MAXMANA:
			return std::max((int32_t)0, ((int32_t)manaMax + varStats[STAT_MAXMANA]));
		case PLAYERINFO_SOUL:
			return std::max((int32_t)0, ((int32_t)soul + varStats[STAT_SOUL]));
		default:
			break;
	}

	return 0;
}

int32_t Player::getSkill(skills_t skilltype, skillsid_t skillinfo) const
{
	int32_t ret = skills[skilltype][skillinfo];
	if(skillinfo == SKILL_LEVEL)
		ret += varSkills[skilltype];

	return std::max((int32_t)0, ret);
}

void Player::addSkillAdvance(skills_t skill, uint32_t count, bool useMultiplier/* = true*/)
{
	if(!count)
		return;

	//player has reached max skill
	uint32_t currReqTries = vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL]),
		nextReqTries = vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL] + 1);
	if(currReqTries > nextReqTries)
		return;

	if(useMultiplier)
		count = uint32_t((double)count * rates[skill] * server.configManager().getDouble(ConfigManager::RATE_SKILL));

	std::stringstream s;
	while(skills[skill][SKILL_TRIES] + count >= nextReqTries)
	{
		count -= nextReqTries - skills[skill][SKILL_TRIES];
	 	skills[skill][SKILL_TRIES] = skills[skill][SKILL_PERCENT] = 0;
		skills[skill][SKILL_LEVEL]++;

		s.str("");
		s << "You advanced in " << getSkillName(skill);
		if(server.configManager().getBool(ConfigManager::ADVANCING_SKILL_LEVEL))
			s << " [" << skills[skill][SKILL_LEVEL] << "]";

		s << ".";
		sendTextMessage(MSG_EVENT_ADVANCE, s.str().c_str());

		CreatureEventList advanceEvents = getCreatureEvents(CREATURE_EVENT_ADVANCE);
		for(CreatureEventList::iterator it = advanceEvents.begin(); it != advanceEvents.end(); ++it)
			(*it)->executeAdvance(this, skill, (skills[skill][SKILL_LEVEL] - 1), skills[skill][SKILL_LEVEL]);

		currReqTries = nextReqTries;
		nextReqTries = vocation->getReqSkillTries(skill, skills[skill][SKILL_LEVEL] + 1);
		if(currReqTries > nextReqTries)
		{
			count = 0;
			break;
		}
	}

	if(count)
		skills[skill][SKILL_TRIES] += count;

	//update percent
	uint32_t newPercent = Player::getPercentLevel(skills[skill][SKILL_TRIES], nextReqTries);
 	if(skills[skill][SKILL_PERCENT] != newPercent)
	{
		skills[skill][SKILL_PERCENT] = newPercent;
		sendSkills();
 	}
	else if(!s.str().empty())
		sendSkills();
}

void Player::setVarStats(stats_t stat, int32_t modifier)
{
	varStats[stat] += modifier;
	switch(stat)
	{
		case STAT_MAXHEALTH:
		{
			if(getHealth() > getMaxHealth())
				Creature::changeHealth(getMaxHealth() - getHealth());
			else
				server.game().addCreatureHealth(this);

			break;
		}

		case STAT_MAXMANA:
		{
			if(getMana() > getMaxMana())
				Creature::changeMana(getMaxMana() - getMana());

			break;
		}

		default:
			break;
	}
}

int32_t Player::getDefaultStats(stats_t stat)
{
	switch(stat)
	{
		case STAT_MAGICLEVEL:
			return getMagicLevel() - getVarStats(STAT_MAGICLEVEL);
		case STAT_MAXHEALTH:
			return getMaxHealth() - getVarStats(STAT_MAXHEALTH);
		case STAT_MAXMANA:
			return getMaxMana() - getVarStats(STAT_MAXMANA);
		case STAT_SOUL:
			return getSoul() - getVarStats(STAT_SOUL);
		default:
			break;
	}

	return 0;
}

Container* Player::getContainer(uint32_t cid)
{
	for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it)
	{
		if(it->first == cid)
			return it->second;
	}

	return nullptr;
}

int32_t Player::getContainerID(const Container* container) const
{
	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			return cl->first;
	}

	return -1;
}

void Player::addContainer(uint32_t cid, Container* container)
{
	if(cid > 0xF)
		return;

	for(ContainerVector::iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->first == cid)
		{
			cl->second = container;
			return;
		}
	}

	containerVec.push_back(std::make_pair(cid, container));
}

void Player::closeContainer(uint32_t cid)
{
	for(ContainerVector::iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->first == cid)
		{
			containerVec.erase(cl);
			break;
		}
	}
}

bool Player::canOpenCorpse(uint32_t ownerId)
{
	return getID() == ownerId || (party && party->canOpenCorpse(ownerId)) || hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges);
}

uint16_t Player::getLookCorpse() const
{
    if(sex % 2)
        return ITEM_MALE_CORPSE;

    return ITEM_FEMALE_CORPSE;
}   

void Player::dropLoot(Container* corpse)
{
	if(!corpse || lootDrop != LOOT_DROP_FULL)
		return;

	uint32_t start = server.configManager().getNumber(ConfigManager::BLESS_REDUCTION_BASE), loss = lossPercent[LOSS_CONTAINERS], bless = getBlessings();
	while(bless > 0 && loss > 0)
	{
		loss -= start;
		start -= server.configManager().getNumber(ConfigManager::BLESS_REDUCTION_DECREAMENT);
		bless--;
	}

	uint32_t itemLoss = (uint32_t)std::floor((5. + loss) * lossPercent[LOSS_ITEMS] / 1000.);
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		Item* item = inventory[i].get();
		if(!item)
			continue;

		uint32_t rand = random_range(1, 100);
		if(skull > SKULL_WHITE || (item->getContainer() && rand < loss) || (!item->getContainer() && rand < itemLoss))
		{
			server.game().internalMoveItem(nullptr, this, corpse, INDEX_WHEREEVER, item, item->getItemCount(), 0);
			sendRemoveInventoryItem((slots_t)i, inventory[i].get());
		}
	}
}

bool Player::setStorage(const uint32_t key, const std::string& value)
{
	if(!IS_IN_KEYRANGE(key, RESERVED_RANGE))
		return Creature::setStorage(key, value);

	if(IS_IN_KEYRANGE(key, OUTFITS_RANGE))
	{
		uint32_t lookType = atoi(value.c_str()) >> 16;
		uint32_t addons = atoi(value.c_str()) & 0xFF;
		if(addons < 4)
		{
			Outfit outfit;
			if(Outfits::getInstance()->getOutfit(lookType, outfit))
				return addOutfit(outfit.outfitId, addons);
		}
		else
			LOGw("[Player::setStorage] Invalid addons value key: " << key
				<< ", value: " << value << " for player: " << getName());
	}
	else if(IS_IN_KEYRANGE(key, OUTFITSID_RANGE))
	{
		uint32_t outfitId = atoi(value.c_str()) >> 16;
		uint32_t addons = atoi(value.c_str()) & 0xFF;
		if(addons < 4)
			return addOutfit(outfitId, addons);
		else
			LOGw("[Player::setStorage] Invalid addons value key: " << key
				<< ", value: " << value << " for player: " << getName());
	}
	else
		LOGw("[Player::setStorage] Unknown reserved key: " << key << " for player: " << getName());

	return false;
}

void Player::eraseStorage(const uint32_t key)
{
	Creature::eraseStorage(key);
	if(IS_IN_KEYRANGE(key, RESERVED_RANGE))
		LOGw("[Player::eraseStorage] Unknown reserved key: " << key << " for player: " << name);
}

bool Player::canSee(const Position& pos) const
{
	if(client)
		return client->canSee(pos);

	return false;
}

bool Player::canSeeCreature(const Creature* creature) const
{
	if(creature == this)
		return true;

	if(const Player* player = creature->getPlayer())
		return !player->isGhost() || getGhostAccess() >= player->getGhostAccess();

	return !creature->isInvisible() || canSeeInvisibility();
}

bool Player::canWalkthrough(const Creature* creature) const
{
	if(!creature)
		return true;

	if(creature == this)
		return false;

	const Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(server.game().getWorldType() == WORLD_TYPE_NO_PVP && player->getTile()->ground
		&& player->getTile()->ground->getId() != ITEM_GLOWING_SWITCH)
		return true;

	return (this->isGhost() || player->isGhost());
}

Depot* Player::getDepot(uint32_t depotId, bool autoCreateDepot)
{
	DepotMap::iterator it = depots.find(depotId);
	if(it != depots.end())
		return it->second.first.get();

	//create a new depot?
	if(autoCreateDepot)
	{
		boost::intrusive_ptr<Item> locker = Item::CreateItem(ITEM_LOCKER);
		if(Container* container = locker->getContainer())
		{
			if(Depot* depot = container->getDepot())
			{
				boost::intrusive_ptr<Item> newDepot = Item::CreateItem(ITEM_DEPOT);
				container->__internalAddThing(newDepot.get());
				addDepot(depot, depotId);
				return depot;
			}
		}

		LOGe("Cannot create a new depot with id: " << depotId <<
			", for player: " << getName());
	}

	return nullptr;
}

bool Player::addDepot(Depot* depot, uint32_t depotId)
{
	if(getDepot(depotId, false))
		return false;

	depots[depotId] = std::make_pair(depot, false);
	depot->setMaxDepotLimit((group != nullptr ? group->getDepotLimit(isPremium()) : 1000));
	return true;
}

void Player::useDepot(uint32_t depotId, bool value)
{
	DepotMap::iterator it = depots.find(depotId);
	if(it != depots.end())
		depots[depotId] = std::make_pair(it->second.first, value);
}

void Player::sendCancelMessage(ReturnValue message) const
{
	switch(message)
	{
		case RET_DESTINATIONOUTOFREACH:
			sendCancel("Destination is out of reach.");
			break;

		case RET_NOTMOVEABLE:
			sendCancel("You cannot move this object.");
			break;

		case RET_DROPTWOHANDEDITEM:
			sendCancel("Drop the double-handed object first.");
			break;

		case RET_BOTHHANDSNEEDTOBEFREE:
			sendCancel("Both hands needs to be free.");
			break;

		case RET_CANNOTBEDRESSED:
			sendCancel("You cannot dress this object there.");
			break;

		case RET_PUTTHISOBJECTINYOURHAND:
			sendCancel("Put this object in your hand.");
			break;

		case RET_PUTTHISOBJECTINBOTHHANDS:
			sendCancel("Put this object in both hands.");
			break;

		case RET_CANONLYUSEONEWEAPON:
			sendCancel("You may use only one weapon.");
			break;

		case RET_TOOFARAWAY:
			sendCancel("Too far away.");
			break;

		case RET_FIRSTGODOWNSTAIRS:
			sendCancel("First go downstairs.");
			break;

		case RET_FIRSTGOUPSTAIRS:
			sendCancel("First go upstairs.");
			break;

		case RET_NOTENOUGHCAPACITY:
			sendCancel("This object is too heavy.");
			break;

		case RET_CONTAINERNOTENOUGHROOM:
			sendCancel("You cannot put more objects in this container.");
			break;

		case RET_NEEDEXCHANGE:
		case RET_NOTENOUGHROOM:
			sendCancel("There is not enough room.");
			break;

		case RET_CANNOTPICKUP:
			sendCancel("You cannot pickup this object.");
			break;

		case RET_CANNOTTHROW:
			sendCancel("You cannot throw there.");
			break;

		case RET_THEREISNOWAY:
			sendCancel("There is no way.");
			break;

		case RET_THISISIMPOSSIBLE:
			sendCancel("This is impossible.");
			break;

		case RET_PLAYERISPZLOCKED:
			sendCancel("You cannot enter a protection zone after attacking another player.");
			break;

		case RET_PLAYERISNOTINVITED:
			sendCancel("You are not invited.");
			break;

		case RET_CREATUREDOESNOTEXIST:
			sendCancel("Creature does not exist.");
			break;

		case RET_DEPOTISFULL:
			sendCancel("You cannot put more items in this depot.");
			break;

		case RET_CANNOTUSETHISOBJECT:
			sendCancel("You cannot use this object.");
			break;

		case RET_PLAYERWITHTHISNAMEISNOTONLINE:
			sendCancel("A player with this name is not online.");
			break;

		case RET_NOTREQUIREDLEVELTOUSERUNE:
			sendCancel("You do not have the required magic level to use this rune.");
			break;

		case RET_YOUAREALREADYTRADING:
			sendCancel("You are already trading.");
			break;

		case RET_THISPLAYERISALREADYTRADING:
			sendCancel("This player is already trading.");
			break;

		case RET_YOUMAYNOTLOGOUTDURINGAFIGHT:
			sendCancel("You may not logout during or immediately after a fight!");
			break;

		case RET_DIRECTPLAYERSHOOT:
			sendCancel("You are not allowed to shoot directly on players.");
			break;

		case RET_NOTENOUGHLEVEL:
			sendCancel("You do not have enough level.");
			break;

		case RET_NOTENOUGHMAGICLEVEL:
			sendCancel("You do not have enough magic level.");
			break;

		case RET_NOTENOUGHMANA:
			sendCancel("You do not have enough energy.");
			break;

		case RET_NOTENOUGHSOUL:
			sendCancel("You do not have enough soul.");
			break;

		case RET_YOUAREEXHAUSTED:
			sendCancel("You are exhausted.");
			break;

		case RET_CANONLYUSETHISRUNEONCREATURES:
			sendCancel("You can only use this rune on creatures.");
			break;

		case RET_PLAYERISNOTREACHABLE:
			sendCancel("Player is not reachable.");
			break;

		case RET_CREATUREISNOTREACHABLE:
			sendCancel("Creature is not reachable.");
			break;

		case RET_ACTIONNOTPERMITTEDINPROTECTIONZONE:
			sendCancel("This action is not permitted in a protection zone.");
			break;

		case RET_YOUMAYNOTATTACKTHISPLAYER:
			sendCancel("You may not attack this player.");
			break;

		case RET_YOUMAYNOTATTACKTHISCREATURE:
			sendCancel("You may not attack this creature.");
			break;

		case RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE:
			sendCancel("You may not attack a person in a protection zone.");
			break;

		case RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE:
			sendCancel("You may not attack a person while you are in a protection zone.");
			break;

		case RET_YOUCANONLYUSEITONCREATURES:
			sendCancel("You can only use it on creatures.");
			break;

		case RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS:
			sendCancel("Turn secure mode off if you really want to attack unmarked players.");
			break;

		case RET_YOUNEEDPREMIUMACCOUNT:
			sendCancel("You need a premium account.");
			break;

		case RET_YOUNEEDTOLEARNTHISSPELL:
			sendCancel("You need to learn this move first.");
			break;

		case RET_YOURVOCATIONCANNOTUSETHISSPELL:
			sendCancel("Your vocation cannot use this move.");
			break;

		case RET_YOUNEEDAWEAPONTOUSETHISSPELL:
			sendCancel("You need to equip a weapon to use this move.");
			break;

		case RET_PLAYERISPZLOCKEDLEAVEPVPZONE:
			sendCancel("You cannot leave a pvp zone after attacking another player.");
			break;

		case RET_PLAYERISPZLOCKEDENTERPVPZONE:
			sendCancel("You cannot enter a pvp zone after attacking another player.");
			break;

		case RET_ACTIONNOTPERMITTEDINANOPVPZONE:
			sendCancel("This action is not permitted in a non-pvp zone.");
			break;

		case RET_YOUCANNOTLOGOUTHERE:
			sendCancel("You cannot logout here.");
			break;

		case RET_YOUNEEDAMAGICITEMTOCASTSPELL:
			sendCancel("You need a magic item to cast this move.");
			break;

		case RET_CANNOTCONJUREITEMHERE:
			sendCancel("You cannot conjure items here.");
			break;

		case RET_YOUNEEDTOSPLITYOURSPEARS:
			sendCancel("You need to split your spears first.");
			break;

		case RET_NAMEISTOOAMBIGUOUS:
			sendCancel("Name is too ambiguous.");
			break;

		case RET_CANONLYUSEONESHIELD:
			sendCancel("You may use only one shield.");
			break;

		case RET_YOUARENOTTHEOWNER:
			sendCancel("You are not the owner.");
			break;

		case RET_YOUMAYNOTCASTAREAONBLACKSKULL:
			sendCancel("You may not cast area spells while you have a black skull.");
			break;

		case RET_TILEISFULL:
			sendCancel("You cannot add more items on this tile.");
			break;

		case RET_DONTSHOWMESSAGE:
			break;

		case RET_NOTPOSSIBLE:
		default:
			sendCancel("Sorry, not possible.");
			break;
	}
}

void Player::sendStats()
{
	if(client)
		client->sendStats();
}

Item* Player::getWriteItem(uint32_t& _windowTextId, uint16_t& _maxWriteLen)
{
	_windowTextId = windowTextId;
	_maxWriteLen = maxWriteLen;
	return writeItem.get();
}

void Player::setWriteItem(Item* item, uint16_t _maxWriteLen/* = 0*/)
{
	windowTextId++;

	if(item)
	{
		writeItem = item;
		maxWriteLen = _maxWriteLen;
	}
	else
	{
		writeItem = nullptr;
		maxWriteLen = 0;
	}
}

House* Player::getEditHouse(uint32_t& _windowTextId, uint32_t& _listId)
{
	_windowTextId = windowTextId;
	_listId = editListId;
	return editHouse;
}

void Player::setEditHouse(House* house, uint32_t listId/* = 0*/)
{
	windowTextId++;
	editHouse = house;
	editListId = listId;
}

void Player::sendHouseWindow(House* house, uint32_t listId) const
{
	if(!client)
		return;

	std::string text;
	if(house->getAccessList(listId, text))
		client->sendHouseWindow(windowTextId, house, listId, text);
}

void Player::sendCreatureChangeVisible(const Creature* creature, Visible_t visible, const char* callSource)
{
	if(!client)
		return;

	const Player* player = creature->getPlayer();
	if(player == this || (player && (visible < VISIBLE_GHOST_APPEAR || getGhostAccess() >= player->getGhostAccess()))
		|| (!player && canSeeInvisibility()))
		sendCreatureChangeOutfit(creature, creature->getCurrentOutfit());
	else if(visible == VISIBLE_DISAPPEAR || visible == VISIBLE_GHOST_DISAPPEAR)
		sendCreatureDisappear(creature, creature->getTile()->getClientIndexOfThing(this, creature), callSource);
	else
		sendCreatureAppear(creature, callSource);
}

void Player::sendAddContainerItem(const Container* container, const Item* item)
{
	if(!client)
		return;

	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			client->sendAddContainerItem(cl->first, item);
	}
}

void Player::sendUpdateContainerItem(const Container* container, uint8_t slot, const Item* oldItem, const Item* newItem)
{
	if(!client)
		return;

	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			client->sendUpdateContainerItem(cl->first, slot, newItem);
	}
}

void Player::sendRemoveContainerItem(const Container* container, uint8_t slot, const Item* item)
{
	if(!client)
		return;

	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			client->sendRemoveContainerItem(cl->first, slot);
	}
}

void Player::onUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem,
	const ItemKindPC& oldType, const Item* newItem, const ItemKindPC& newType)
{
	Creature::onUpdateTileItem(tile, pos, oldItem, oldType, newItem, newType);
	if(oldItem != newItem)
		onRemoveTileItem(tile, pos, oldType, oldItem);

	if(tradeState != TRADE_TRANSFER && tradeItem && oldItem == tradeItem)
		server.game().internalCloseTrade(this);
}

void Player::onRemoveTileItem(const Tile* tile, const Position& pos, const ItemKindPC& iType, const Item* item)
{
	Creature::onRemoveTileItem(tile, pos, iType, item);
	if(tradeState == TRADE_TRANSFER)
		return;

	checkTradeState(item);
	if(tradeItem)
	{
		const Container* container = item->getContainer();
		if(container && container->isHoldingItem(tradeItem))
			server.game().internalCloseTrade(this);
	}
}

void Player::onCreatureAppear(const CreatureP& creature)
{
	Creature::onCreatureAppear(creature);
	if(creature != this)
		return;

	Item* item = nullptr;
	for(int32_t slot = +slots_t::FIRST; slot < +slots_t::LAST; ++slot)
	{
		if(!(item = getInventoryItem((slots_t)slot)))
			continue;

		item->__startDecaying();
		server.moveEvents().onPlayerEquip(this, item, (slots_t)slot, false);
	}

	if(BedItem* bed = Beds::getInstance()->getBedBySleeper(guid))
		bed->wakeUp();

	Outfit outfit;
	if(Outfits::getInstance()->getOutfit(defaultOutfit.lookType, outfit))
		outfitAttributes = Outfits::getInstance()->addAttributes(getID(), outfit.outfitId, sex, defaultOutfit.lookAddons);

	if(lastLogout && stamina < STAMINA_MAX)
	{
		int64_t ticks = (int64_t)time(nullptr) - lastLogout - 600;
		if(ticks > 0)
		{
			ticks = (int64_t)((double)(ticks * 1000) / server.configManager().getDouble(ConfigManager::RATE_STAMINA_GAIN));
			int64_t premium = server.configManager().getNumber(ConfigManager::STAMINA_LIMIT_TOP) * STAMINA_MULTIPLIER, period = ticks;
			if((int64_t)stamina <= premium)
			{
				period += stamina;
				if(period > premium)
					period -= premium;
				else
					period = 0;

				useStamina(ticks - period);
			}

			if(period > 0)
			{
				ticks = (int64_t)((server.configManager().getDouble(ConfigManager::RATE_STAMINA_GAIN) * period)
					/ server.configManager().getDouble(ConfigManager::RATE_STAMINA_THRESHOLD));
				if(stamina + ticks > STAMINA_MAX)
					ticks = STAMINA_MAX - stamina;

				useStamina(ticks);
			}

			sendStats();
		}
	}

	server.game().checkPlayersRecord(this);
	if(!isGhost())
		IOLoginData::getInstance()->updateOnlineStatus(guid, true);

	if(server.configManager().getBool(ConfigManager::DISPLAY_LOGGING))
		LOGi(name << " has logged in.");
}

void Player::onAttackedCreatureDisappear(bool isLogout)
{
	sendCancelTarget();
	if(!isLogout)
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
}

void Player::onFollowCreatureDisappear(bool isLogout)
{
	sendCancelTarget();
	if(!isLogout)
		sendTextMessage(MSG_STATUS_SMALL, "Target lost.");
}

void Player::onChangeZone(ZoneType_t zone)
{
	if(attackedCreature && zone == ZONE_PROTECTION && !hasFlag(PlayerFlag_IgnoreProtectionZone))
	{
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(false);
	}
	sendIcons();
}

void Player::onAttackedCreatureChangeZone(ZoneType_t zone)
{
	if(zone == ZONE_PROTECTION && !hasFlag(PlayerFlag_IgnoreProtectionZone))
	{
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(false);
	}
	else if(zone == ZONE_NOPVP && attackedCreature->getPlayer() && !hasFlag(PlayerFlag_IgnoreProtectionZone))
	{
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(false);
	}
	else if(zone == ZONE_NORMAL && server.game().getWorldType() == WORLD_TYPE_NO_PVP && attackedCreature->getPlayer())
	{
		//attackedCreature can leave a pvp zone if not pzlocked
		setAttackedCreature(nullptr);
		onAttackedCreatureDisappear(false);
	}
}

void Player::onCreatureDisappear(const Creature* creature, bool isLogout)
{
	Creature::onCreatureDisappear(creature, isLogout);
	if(creature != this)
		return;

	if(isLogout)
	{
		loginPosition = getPosition();
		lastLogout = time(nullptr);
	}

	if(eventWalk)
		setFollowCreature(nullptr);

	closeShopWindow();
	if(tradePartner)
		server.game().internalCloseTrade(this);

	clearPartyInvitations();
	if(party)
		party->leave(this);

	server.game().cancelRuleViolation(this);
	if(hasFlag(PlayerFlag_CanAnswerRuleViolations))
	{
		PlayerVector closeReportList;
		for(RuleViolationsMap::const_iterator it = server.game().getRuleViolations().begin(); it != server.game().getRuleViolations().end(); ++it)
		{
			if(it->second->gamemaster == this)
				closeReportList.push_back(it->second->reporter);
		}

		for(PlayerVector::iterator it = closeReportList.begin(); it != closeReportList.end(); ++it)
			server.game().closeRuleViolation(*it);
	}

	server.chat().removeUserFromAllChannels(this);
	if(!isGhost())
		IOLoginData::getInstance()->updateOnlineStatus(guid, false);

	if(server.configManager().getBool(ConfigManager::DISPLAY_LOGGING))
		LOGi(name << " has logged out.");

	bool saved = false;
	for(uint32_t tries = 0; !saved && tries < 3; ++tries)
	{
		if(IOLoginData::getInstance()->savePlayer(this))
			saved = true;
	}

	if(!saved)
		LOGe("Player " << getName() << " couldn't be saved.");
}

void Player::openShopWindow()
{
	sendShop();
	sendGoods();
}

void Player::closeShopWindow(Npc* npc/* = nullptr*/, int32_t onBuy/* = -1*/, int32_t onSell/* = -1*/)
{
	if(npc || (npc = getShopOwner(onBuy, onSell)))
		npc->onPlayerEndTrade(this, onBuy, onSell);

	if(shopOwner)
		sendCloseShop();

	shopOwner = nullptr;
	purchaseCallback = saleCallback = -1;
	shopOffer.clear();
}

bool Player::canShopItem(uint16_t itemId, uint8_t subType, ShopEvent_t event)
{
	for(ShopInfoList::iterator sit = shopOffer.begin(); sit != shopOffer.end(); ++sit)
	{
		if(sit->itemId != itemId || ((event != SHOPEVENT_BUY || sit->buyPrice < 0)
			&& (event != SHOPEVENT_SELL || sit->sellPrice < 0)))
			continue;

		if(event == SHOPEVENT_SELL)
			return true;

		ItemKindPC kind = server.items()[itemId];
		if (!kind) {
			continue;
		}

		if(kind->isFluidContainer() || kind->isSplash() || kind->isRune())
			return sit->subType == subType;

		return true;
	}

	return false;
}

void Player::onWalk(Direction& dir)
{
	Creature::onWalk(dir);
	setNextActionTask(nullptr);
	setNextAction(OTSYS_TIME() + getStepDuration(dir));
}

void Player::onCreatureMove(const CreatureP& creature, const Position& origin, Tile* originTile, const Position& destination, Tile* destinationTile, bool teleport)
{
	Creature::onCreatureMove(creature, origin, originTile, destination, destinationTile, teleport);
	if(creature != this)
		return;

	if(getParty())
		getParty()->updateSharedExperience();

	//check if we should close trade
	if(tradeState != TRADE_TRANSFER && ((tradeItem && !Position::areInRange<1,1,0>(tradeItem->getPosition(), getPosition()))
		|| (tradePartner && !Position::areInRange<2,2,0>(tradePartner->getPosition(), getPosition()))))
		server.game().internalCloseTrade(this);

	if((teleport || origin.z != destination.z) && !hasCustomFlag(PlayerCustomFlag_CanStairhop))
	{
		int32_t ticks = server.configManager().getNumber(ConfigManager::STAIRHOP_DELAY);
		if(ticks > 0)
		{
			addExhaust(ticks, EXHAUST_COMBAT);
			if(Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_PACIFIED, ticks))
				addCondition(condition);
		}
	}
}

void Player::onAddContainerItem(const Container* container, const Item* item)
{
	checkTradeState(item);
}

void Player::onUpdateContainerItem(const Container* container, uint8_t slot,
	const Item* oldItem, const ItemKindPC& oldType, const Item* newItem, const ItemKindPC& newType)
{
	if(oldItem != newItem)
		onRemoveContainerItem(container, slot, oldItem);

	if(tradeState != TRADE_TRANSFER)
		checkTradeState(oldItem);
}

void Player::onRemoveContainerItem(const Container* container, uint8_t slot, const Item* item)
{
	if(tradeState == TRADE_TRANSFER)
		return;

	checkTradeState(item);
	if(tradeItem)
	{
		if(tradeItem->getParent() != container && container->isHoldingItem(tradeItem))
			server.game().internalCloseTrade(this);
	}
}

void Player::onCloseContainer(const Container* container)
{
	if(!client)
		return;

	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			client->sendCloseContainer(cl->first);
	}
}

void Player::onSendContainer(const Container* container)
{
	if(!client)
		return;

	bool hasParent = dynamic_cast<const Container*>(container->getParent()) != nullptr;
	for(ContainerVector::const_iterator cl = containerVec.begin(); cl != containerVec.end(); ++cl)
	{
		if(cl->second == container)
			client->sendContainer(cl->first, container, hasParent);
	}
}

void Player::onUpdateInventoryItem(slots_t slot, Item* oldItem, const ItemKindPC& oldType,
	Item* newItem, const ItemKindPC& newType)
{
	if(oldItem != newItem)
		onRemoveInventoryItem(slot, oldItem);

	if(tradeState != TRADE_TRANSFER)
		checkTradeState(oldItem);
}

void Player::onRemoveInventoryItem(slots_t slot, Item* item)
{
	if(tradeState == TRADE_TRANSFER)
		return;

	checkTradeState(item);
	if(tradeItem)
	{
		const Container* container = item->getContainer();
		if(container && container->isHoldingItem(tradeItem))
			server.game().internalCloseTrade(this);
	}
}

void Player::checkTradeState(const Item* item)
{
	if(!tradeItem || tradeState == TRADE_TRANSFER)
		return;

	if(tradeItem != item)
	{
		const Container* container = dynamic_cast<const Container*>(item->getParent());
		while(container != nullptr)
		{
			if(container == tradeItem)
			{
				server.game().internalCloseTrade(this);
				break;
			}

			container = dynamic_cast<const Container*>(container->getParent());
		}
	}
	else
		server.game().internalCloseTrade(this);
}

void Player::setNextWalkActionTask(std::unique_ptr<SchedulerTask> task)
{
	if(walkTaskEvent)
	{
		server.scheduler().cancelTask(walkTaskEvent);
		walkTaskEvent = 0;
	}

	walkTask = std::move(task);
	setIdleTime(0);
}

void Player::setNextWalkTask(std::unique_ptr<SchedulerTask> task)
{
	if(nextStepEvent)
	{
		server.scheduler().cancelTask(nextStepEvent);
		nextStepEvent = 0;
	}

	if(task)
	{
		nextStepEvent = server.scheduler().addTask(std::move(task));
		setIdleTime(0);
	}
}

void Player::setNextActionTask(std::unique_ptr<SchedulerTask> task)
{
	if(actionTaskEvent)
	{
		server.scheduler().cancelTask(actionTaskEvent);
		actionTaskEvent = 0;
	}

	if(task)
	{
		actionTaskEvent = server.scheduler().addTask(std::move(task));
		setIdleTime(0);
	}
}

uint32_t Player::getNextActionTime() const
{
	int64_t time = nextAction - OTSYS_TIME();
	if(time < SCHEDULER_MINTICKS)
		return SCHEDULER_MINTICKS;

	return time;
}

void Player::onThink(Duration elapsedTime)
{
	Creature::onThink(elapsedTime);

	if (!isAlive()) {
		return;
	}

	int64_t timeNow = OTSYS_TIME();
	if(timeNow - lastPing >= 5000)
	{
		lastPing = timeNow;
		if(client)
			client->sendPing();
		else if(server.configManager().getBool(ConfigManager::STOP_ATTACK_AT_EXIT))
			setAttackedCreature(nullptr);
	}

	if((timeNow - lastPong) >= 60000 && canLogout(true))
	{
		if(client)
			client->logout(true, true);
		else if(server.creatureEvents().playerLogout(this, true))
			server.game().removeCreature(this, true);
	}

	messageTicks += std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count();
	if(messageTicks >= 1500)
	{
		messageTicks = 0;
		addMessageBuffer();
	}
}

bool Player::isMuted(uint16_t channelId, SpeakClasses type, uint32_t& time)
{
	time = 0;
	if(hasFlag(PlayerFlag_CannotBeMuted))
		return false;

	int32_t muteTicks = 0;
	for(ConditionList::iterator it = conditions.begin(); it != conditions.end(); ++it)
	{
		if((*it)->getType() == CONDITION_MUTED && (*it)->getSubId() == 0 && (*it)->getTicks() > muteTicks)
			muteTicks = (*it)->getTicks();
	}

	time = (uint32_t)muteTicks / 1000;
	return time > 0 && type != SPEAK_PRIVATE_PN && (type != SPEAK_CHANNEL_Y || (channelId != CHANNEL_GUILD && !server.chat().isPrivateChannel(channelId)));
}

void Player::addMessageBuffer()
{
	if(!hasFlag(PlayerFlag_CannotBeMuted) && server.configManager().getNumber(
		ConfigManager::MAX_MESSAGEBUFFER) != 0 && messageBuffer > 0)
		messageBuffer--;
}

void Player::removeMessageBuffer()
{
	int32_t maxBuffer = server.configManager().getNumber(ConfigManager::MAX_MESSAGEBUFFER);
	if(!hasFlag(PlayerFlag_CannotBeMuted) && maxBuffer != 0 && messageBuffer <= maxBuffer + 1)
	{
		if(++messageBuffer > maxBuffer)
		{
			uint32_t muteCount = 1;
			MuteCountMap::iterator it = muteCountMap.find(guid);
			if(it != muteCountMap.end())
				muteCount = it->second;

			uint32_t muteTime = 5 * muteCount * muteCount;
			muteCountMap[guid] = muteCount + 1;
			if(Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_MUTED, muteTime * 1000))
				addCondition(condition);

			char buffer[50];
			sprintf(buffer, "You are muted for %d seconds.", muteTime);
			sendTextMessage(MSG_STATUS_SMALL, buffer);
		}
	}
}

void Player::drainHealth(const CreatureP& attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainHealth(attacker, combatType, damage);
	char buffer[150];
	if(attacker)
		sprintf(buffer, "You lose %d hitpoint%s due to an attack by %s.", damage, (damage != 1 ? "s" : ""), attacker->getNameDescription().c_str());
	else
		sprintf(buffer, "You lose %d hitpoint%s.", damage, (damage != 1 ? "s" : ""));

	sendStats();
	sendTextMessage(MSG_EVENT_DEFAULT, buffer);
}

void Player::drainMana(const CreatureP& attacker, CombatType_t combatType, int32_t damage)
{
	Creature::drainMana(attacker, combatType, damage);
	char buffer[150];
	if(attacker)
		sprintf(buffer, "You lose %d mana blocking an attack by %s.", damage, attacker->getNameDescription().c_str());
	else
		sprintf(buffer, "You lose %d mana.", damage);

	sendStats();
	sendTextMessage(MSG_EVENT_DEFAULT, buffer);
}

void Player::addManaSpent(uint64_t amount, bool useMultiplier/* = true*/)
{
	if(!amount)
		return;

	uint64_t currReqMana = vocation->getReqMana(magLevel), nextReqMana = vocation->getReqMana(magLevel + 1);
	if(currReqMana > nextReqMana) //player has reached max magic level
		return;

	if(useMultiplier)
		amount = uint64_t((double)amount * rates[SKILL__MAGLEVEL] * server.configManager().getDouble(ConfigManager::RATE_MAGIC));

	bool advance = false;
	while(manaSpent + amount >= nextReqMana)
	{
		amount -= nextReqMana - manaSpent;
		manaSpent = 0;
		magLevel++;

		char advMsg[50];
		sprintf(advMsg, "You advanced to magic level %d.", magLevel);
		sendTextMessage(MSG_EVENT_ADVANCE, advMsg);

		advance = true;
		CreatureEventList advanceEvents = getCreatureEvents(CREATURE_EVENT_ADVANCE);
		for(CreatureEventList::iterator it = advanceEvents.begin(); it != advanceEvents.end(); ++it)
			(*it)->executeAdvance(this, SKILL__MAGLEVEL, (magLevel - 1), magLevel);

		currReqMana = nextReqMana;
		nextReqMana = vocation->getReqMana(magLevel + 1);
		if(currReqMana > nextReqMana)
		{
			amount = 0;
			break;
		}
	}

	if(amount)
		manaSpent += amount;

	uint32_t newPercent = Player::getPercentLevel(manaSpent, nextReqMana);
	if(magLevelPercent != newPercent)
	{
		magLevelPercent = newPercent;
		sendStats();
	}
	else if(advance)
		sendStats();
}

void Player::addExperience(uint64_t exp)
{
	uint32_t prevLevel = level;
	uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
	if(Player::getExpForLevel(level) > nextLevelExp)
	{
		//player has reached max level
		levelPercent = 0;
		sendStats();
		return;
	}

	experience += exp;
	while(experience >= nextLevelExp)
	{
		healthMax += vocation->getGain(GAIN_HEALTH);
		health += vocation->getGain(GAIN_HEALTH);
		manaMax += vocation->getGain(GAIN_MANA);
		mana += vocation->getGain(GAIN_MANA);
		capacity += vocation->getGainCap();

		++level;
		nextLevelExp = Player::getExpForLevel(level + 1);
		if(Player::getExpForLevel(level) > nextLevelExp) //player has reached max level
			break;
	}

	if(prevLevel != level)
	{
		updateBaseSpeed();
		setBaseSpeed(getBaseSpeed());

		server.game().changeSpeed(this, 0);
		server.game().addCreatureHealth(this);
		if(getParty())
			getParty()->updateSharedExperience();

		char advMsg[60];
		sprintf(advMsg, "You advanced from Level %d to Level %d.", prevLevel, level);
		sendTextMessage(MSG_EVENT_ADVANCE, advMsg);

		CreatureEventList advanceEvents = getCreatureEvents(CREATURE_EVENT_ADVANCE);
		for(CreatureEventList::iterator it = advanceEvents.begin(); it != advanceEvents.end(); ++it)
			(*it)->executeAdvance(this, SKILL__LEVEL, prevLevel, level);
	}

	uint64_t currLevelExp = Player::getExpForLevel(level);
	nextLevelExp = Player::getExpForLevel(level + 1);
	levelPercent = 0;
	if(nextLevelExp > currLevelExp)
		levelPercent = Player::getPercentLevel(experience - currLevelExp, nextLevelExp - currLevelExp);

	sendStats();
}

void Player::removeExperience(uint64_t exp, bool updateStats/* = true*/)
{
	uint32_t prevLevel = level;
	experience -= std::min(exp, experience);
	while(level > 1 && experience < Player::getExpForLevel(level))
	{
		level--;
		healthMax = std::max((int32_t)0, (healthMax - (int32_t)vocation->getGain(GAIN_HEALTH)));
		manaMax = std::max((int32_t)0, (manaMax - (int32_t)vocation->getGain(GAIN_MANA)));
		capacity = std::max((double)0, (capacity - (double)vocation->getGainCap()));
	}

	if (health > healthMax) {
		health = healthMax;
	}
	if (mana > manaMax) {
		mana = manaMax;
	}

	if(prevLevel != level)
	{
		if(updateStats)
		{
			updateBaseSpeed();
			setBaseSpeed(getBaseSpeed());

			server.game().changeSpeed(this, 0);
			server.game().addCreatureHealth(this);
		}

		char advMsg[90];
		sprintf(advMsg, "You were downgraded from Level %d to Level %d.", prevLevel, level);
		sendTextMessage(MSG_EVENT_ADVANCE, advMsg);
	}

	uint64_t currLevelExp = Player::getExpForLevel(level);
	uint64_t nextLevelExp = Player::getExpForLevel(level + 1);
	if(nextLevelExp > currLevelExp)
		levelPercent = Player::getPercentLevel(experience - currLevelExp, nextLevelExp - currLevelExp);
	else
		levelPercent = 0;

	if(updateStats)
		sendStats();
}

uint32_t Player::getPercentLevel(uint64_t count, uint64_t nextLevelCount)
{
	if(nextLevelCount > 0)
		return std::min((uint32_t)100, std::max((uint32_t)0, uint32_t(count * 100 / nextLevelCount)));

	return 0;
}

void Player::onBlockHit(BlockType_t blockType)
{
	if(shieldBlockCount > 0)
	{
		--shieldBlockCount;
		if(hasShield())
			addSkillAdvance(SKILL_SHIELD, 1);
	}
}

void Player::onAttackedCreatureBlockHit(Creature* target, BlockType_t blockType)
{
	Creature::onAttackedCreatureBlockHit(target, blockType);
	lastAttackBlockType = blockType;
	switch(blockType)
	{
		case BLOCK_NONE:
		{
			addAttackSkillPoint = true;
			bloodHitCount = 30;
			shieldBlockCount = 30;
			break;
		}

		case BLOCK_DEFENSE:
		case BLOCK_ARMOR:
		{
			//need to draw blood every 30 hits
			if(bloodHitCount > 0)
			{
				addAttackSkillPoint = true;
				--bloodHitCount;
			}
			else
				addAttackSkillPoint = false;

			break;
		}

		default:
		{
			addAttackSkillPoint = false;
			break;
		}
	}
}

bool Player::hasShield() const
{
	bool result = false;
	Item* item = getInventoryItem(slots_t::LEFT);
	if(item && item->getWeaponType() == WEAPON_SHIELD)
		result = true;

	item = getInventoryItem(slots_t::RIGHT);
	if(item && item->getWeaponType() == WEAPON_SHIELD)
		result = true;

	return result;
}

BlockType_t Player::blockHit(Creature* attacker, CombatType_t combatType, int32_t& damage,
	bool checkDefense/* = false*/, bool checkArmor/* = false*/)
{
	BlockType_t blockType = Creature::blockHit(attacker, combatType, damage, checkDefense, checkArmor);
	if(attacker)
	{
		int16_t color = server.configManager().getNumber(ConfigManager::SQUARE_COLOR);
		if(color < 0)
			color = random_range(0, 255);

		sendCreatureSquare(attacker, (SquareColor_t)color);
	}

	if(blockType != BLOCK_NONE)
		return blockType;

	if(vocation->getMultiplier(MULTIPLIER_MAGICDEFENSE) != 1.0 && combatType != COMBAT_NONE &&
		combatType != COMBAT_PHYSICALDAMAGE && combatType != COMBAT_UNDEFINEDDAMAGE &&
		combatType != COMBAT_DROWNDAMAGE)
		damage -= (int32_t)std::ceil((double)(damage * vocation->getMultiplier(MULTIPLIER_MAGICDEFENSE)) / 100.);

	if(damage > 0)
	{
		Item* item = nullptr;
		double blocked = 0, reflected = 0;
		for(int32_t slot = +slots_t::FIRST; slot < +slots_t::LAST; ++slot)
		{
			if(!(item = getInventoryItem((slots_t)slot)) || (server.moveEvents().hasEquipEvent(item)
				&& !isItemAbilityEnabled((slots_t)slot)))
				continue;

			ItemKindPC kind = item->getKind();

			int16_t absorb = kind->abilities.getAbsorb(combatType);
			if(absorb > 0)
			{
				blocked += damage * absorb / 100.;
				if(item->hasCharges())
					server.game().transformItem(item, item->getId(), std::max((int32_t)0, (int32_t)item->getCharges() - 1));
			}

			int16_t reflectPercent = kind->abilities.getReflect(combatType, REFLECT_PERCENT);
			if (reflectPercent > 0) {
				int16_t reflectChance = kind->abilities.getReflect(combatType, REFLECT_CHANCE);
				if (reflectChance > random_range(0, 100)) {
					reflected += damage * reflectPercent / 100.;
					if(item->hasCharges() && absorb <= 0)
						server.game().transformItem(item, item->getId(), std::max((int32_t)0, (int32_t)item->getCharges() - 1));
				}
			}
		}

		if(outfitAttributes)
		{
			uint32_t tmp = Outfits::getInstance()->getOutfitAbsorb(defaultOutfit.lookType, sex, combatType);
			if(tmp)
				blocked += damage * tmp / 100.;

			tmp = Outfits::getInstance()->getOutfitReflect(defaultOutfit.lookType, sex, combatType);
			if(tmp)
				reflected += damage * tmp / 100.;
		}

		if(vocation->getAbsorb(combatType))
			blocked += damage * vocation->getAbsorb(combatType) / 100.;

		if(vocation->getReflect(combatType))
			reflected += damage * vocation->getReflect(combatType) / 100.;

		damage -= static_cast<int32_t>(std::round(blocked));
		if(damage <= 0)
		{
			damage = 0;
			blockType = BLOCK_DEFENSE;
		}

		if (reflected != 0) {
			CombatType_t reflectType = combatType;
			if(reflected <= 0)
				reflectType = COMBAT_HEALING;

			server.game().combatChangeHealth(reflectType, nullptr, attacker, -static_cast<int32_t>(std::round(reflected)));
		}
	}

	return blockType;
}

uint32_t Player::getIP() const
{
	if(client)
		return client->getIP();

	return lastIP;
}

bool Player::onDeath()
{
	Item* preventLoss = nullptr;
	Item* preventDrop = nullptr;

	/*
	if(getZone() == ZONE_PVP)
	{
		setDropLoot(LOOT_DROP_NONE);
		setLossSkill(false);
	}
	else*/ if(skull < SKULL_RED && server.game().getWorldType() != WORLD_TYPE_PVP_ENFORCED)
	{
		Item* item = nullptr;
		for(int32_t i = +slots_t::FIRST; ((skillLoss || lootDrop == LOOT_DROP_FULL) && i < +slots_t::LAST); ++i)
		{
			if(!(item = getInventoryItem((slots_t)i)) || (server.moveEvents().hasEquipEvent(item)
				&& !isItemAbilityEnabled((slots_t)i)))
				continue;

			ItemKindPC kind = item->getKind();
			if(lootDrop == LOOT_DROP_FULL && kind->abilities.preventDrop)
			{
				setDropLoot(LOOT_DROP_PREVENT);
				preventDrop = item;
			}

			if(skillLoss && !preventLoss && kind->abilities.preventLoss)
				preventLoss = item;
		}
	}

	if(!Creature::onDeath())
	{
		if(preventDrop)
			setDropLoot(LOOT_DROP_FULL);

		return false;
	}

	if(preventLoss)
	{
		setLossSkill(false);

		if (preventLoss->hasCharges()) {
			if(preventLoss->getCharges() > 1) //weird, but transform failed to remove for some hosters
				server.game().transformItem(preventLoss, preventLoss->getId(), std::max(0, ((int32_t)preventLoss->getCharges() - 1)));
			else
				server.game().internalRemoveItem(nullptr, preventDrop);
		}
	}

	if(preventDrop && preventDrop != preventLoss && preventDrop->hasCharges())
	{
		if(preventDrop->getCharges() > 1) //weird, but transform failed to remove for some hosters
			server.game().transformItem(preventDrop, preventDrop->getId(), std::max(0, ((int32_t)preventDrop->getCharges() - 1)));
		else
			server.game().internalRemoveItem(nullptr, preventDrop);
	}

	removeConditions(CONDITIONEND_DEATH);
	if(skillLoss)
	{
		uint64_t lossExperience = getLostExperience();
		removeExperience(lossExperience, false);
		double percent = 1. - ((double)(experience - lossExperience) / experience);

		//Magic level loss
		uint32_t sumMana = 0;
		uint64_t lostMana = 0;
		for(uint32_t i = 1; i <= magLevel; ++i)
			sumMana += vocation->getReqMana(i);

		sumMana += manaSpent;
		lostMana = (uint64_t)std::ceil(sumMana * ((double)(percent * lossPercent[LOSS_MANA]) / 100.));
		while(lostMana > manaSpent && magLevel > 0)
		{
			lostMana -= manaSpent;
			manaSpent = vocation->getReqMana(magLevel);
			magLevel--;
		}

		manaSpent -= std::max((int32_t)0, (int32_t)lostMana);
		uint64_t nextReqMana = vocation->getReqMana(magLevel + 1);
		if(nextReqMana > vocation->getReqMana(magLevel))
			magLevelPercent = Player::getPercentLevel(manaSpent, nextReqMana);
		else
			magLevelPercent = 0;

		//Skill loss
		uint32_t lostSkillTries, sumSkillTries;
		for(int16_t i = 0; i < 7; ++i) //for each skill
		{
			lostSkillTries = sumSkillTries = 0;
			for(uint32_t c = 11; c <= skills[i][SKILL_LEVEL]; ++c) //sum up all required tries for all skill levels
				sumSkillTries += vocation->getReqSkillTries(i, c);

			sumSkillTries += skills[i][SKILL_TRIES];
			lostSkillTries = (uint32_t)std::ceil(sumSkillTries * ((double)(percent * lossPercent[LOSS_SKILLS]) / 100.));
			while(lostSkillTries > skills[i][SKILL_TRIES])
			{
				lostSkillTries -= skills[i][SKILL_TRIES];
				skills[i][SKILL_TRIES] = vocation->getReqSkillTries(i, skills[i][SKILL_LEVEL]);
				if(skills[i][SKILL_LEVEL] < 11)
				{
					skills[i][SKILL_LEVEL] = 10;
					skills[i][SKILL_TRIES] = lostSkillTries = 0;
					break;
				}
				else
					skills[i][SKILL_LEVEL]--;
			}

			skills[i][SKILL_TRIES] = std::max((int32_t)0, (int32_t)(skills[i][SKILL_TRIES] - lostSkillTries));
		}

		blessings = 0;
		loginPosition = masterPosition;
		if(!inventory[+slots_t::BACKPACK]) {
			boost::intrusive_ptr<Item> container = Item::CreateItem(server.configManager().getNumber(ConfigManager::DEATH_CONTAINER));
			__internalAddThing(+slots_t::BACKPACK, container.get());
		}

		sendIcons();
		sendStats();
		sendSkills();

		sendReLoginWindow();
		server.game().removeCreature(this, false);
	}
	else
	{
		setLossSkill(true);
		if(preventLoss)
		{
			loginPosition = masterPosition;
			sendReLoginWindow();
			server.game().removeCreature(this, false);
		}
	}

	return true;
}

void Player::dropCorpse(DeathList deathList)
{
	if(lootDrop == LOOT_DROP_NONE)
	{
		pzLocked = false;
		if(health <= 0)
		{
			health = healthMax;
			mana = manaMax;
		}

		setDropLoot(LOOT_DROP_FULL);
		sendStats();
		sendIcons();

		server.game().addCreatureHealth(this);
		server.game().internalTeleport(this, masterPosition, true);
	}
	else
	{
		Creature::dropCorpse(deathList);
		if(server.configManager().getBool(ConfigManager::DEATH_LIST))
			IOLoginData::getInstance()->playerDeath(this, deathList);
	}
}

boost::intrusive_ptr<Item> Player::createCorpse(DeathList deathList)
{
	boost::intrusive_ptr<Item> corpse = Creature::createCorpse(deathList);
	if(!corpse)
		return nullptr;

	std::stringstream ss;
	ss << "You recognize " << getNameDescription() << ". " << (sex % 2 ? "He" : "She") << " was killed by ";
	if(deathList[0].isCreatureKill())
	{
		ss << deathList[0].getKillerCreature()->getNameDescription();
		if(deathList[0].getKillerCreature()->hasDirectOwner())
			ss << " summoned by " << deathList[0].getKillerCreature()->getDirectOwner()->getNameDescription();
	}
	else
		ss << deathList[0].getKillerName();

	if(deathList.size() > 1)
	{
		if(deathList[0].getKillerType() != deathList[1].getKillerType())
		{
			if(deathList[1].isCreatureKill())
			{
				ss << " and by " << deathList[1].getKillerCreature()->getNameDescription();
				if(deathList[1].getKillerCreature()->hasDirectOwner())
					ss << " summoned by " << deathList[1].getKillerCreature()->getDirectOwner()->getNameDescription();
			}
			else
				ss << " and by " << deathList[1].getKillerName();
		}
		else if(deathList[1].isCreatureKill())
		{
			if(deathList[0].getKillerCreature()->getName() != deathList[1].getKillerCreature()->getName())
			{
				ss << " and by " << deathList[1].getKillerCreature()->getNameDescription();
				if(deathList[1].getKillerCreature()->hasDirectOwner())
					ss << " summoned by " << deathList[1].getKillerCreature()->getDirectOwner()->getNameDescription();
			}
		}
		else if(asLowerCaseString(deathList[0].getKillerName()) != asLowerCaseString(deathList[1].getKillerName()))
			ss << " and by " << deathList[1].getKillerName();
	}

	ss << ".";
	corpse->setSpecialDescription(ss.str().c_str());
	return corpse;
}

void Player::addExhaust(uint32_t ticks, Exhaust_t type)
{
	if(Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_EXHAUST, ticks, 0, false, type))
		addCondition(condition);
}

void Player::addInFightTicks(bool pzLock/* = false*/)
{
	if(hasFlag(PlayerFlag_NotGainInFight))
		return;

	if(pzLock)
		pzLocked = true;

	if(Condition* condition = Condition::createCondition(CONDITIONID_DEFAULT,
		CONDITION_INFIGHT, server.configManager().getNumber(ConfigManager::PZ_LOCKED)))
		addCondition(condition);
}

void Player::addDefaultRegeneration(uint32_t addTicks)
{
	Condition* condition = getCondition(CONDITION_REGENERATION, CONDITIONID_DEFAULT);
	if(condition)
		condition->setTicks(condition->getTicks() + addTicks);
	else if((condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_REGENERATION, addTicks)))
	{
		condition->setParam(CONDITIONPARAM_HEALTHGAIN, vocation->getGainAmount(GAIN_HEALTH));
		condition->setParam(CONDITIONPARAM_HEALTHTICKS, vocation->getGainTicks(GAIN_HEALTH) * 1000);
		condition->setParam(CONDITIONPARAM_MANAGAIN, vocation->getGainAmount(GAIN_MANA));
		condition->setParam(CONDITIONPARAM_MANATICKS, vocation->getGainTicks(GAIN_MANA) * 1000);
		addCondition(condition);
	}
}

void Player::removeList()
{
	autoList.erase(id);
	if(!isGhost())
	{
		for(AutoList<Player>::iterator it = autoList.begin(); it != autoList.end(); ++it)
			it->second->notifyLogOut(this);
	}
	else
	{
		for(AutoList<Player>::iterator it = autoList.begin(); it != autoList.end(); ++it)
		{
			if(it->second->canSeeCreature(this))
				it->second->notifyLogOut(this);
		}
	}
}

void Player::addList()
{
	if(!isGhost())
	{
		for(AutoList<Player>::iterator it = autoList.begin(); it != autoList.end(); ++it)
			it->second->notifyLogIn(this);
	}
	else
	{
		for(AutoList<Player>::iterator it = autoList.begin(); it != autoList.end(); ++it)
		{
			if(it->second->canSeeCreature(this))
				it->second->notifyLogIn(this);
		}
	}

	autoList[id] = this;
}

void Player::kickPlayer(bool displayEffect, bool forceLogout)
{
	if(!client)
	{
		if(server.creatureEvents().playerLogout(this, forceLogout))
			server.game().removeCreature(this);
	}
	else
		client->logout(displayEffect, forceLogout);
}

void Player::notifyLogIn(Player* loginPlayer)
{
	if(!client)
		return;

	VIPListSet::iterator it = VIPList.find(loginPlayer->getGUID());
	if(it != VIPList.end())
		client->sendVIPLogIn(loginPlayer->getGUID());
}

void Player::notifyLogOut(Player* logoutPlayer)
{
	if(!client)
		return;

	VIPListSet::iterator it = VIPList.find(logoutPlayer->getGUID());
	if(it != VIPList.end())
		client->sendVIPLogOut(logoutPlayer->getGUID());
}

bool Player::removeVIP(uint32_t _guid)
{
	VIPListSet::iterator it = VIPList.find(_guid);
	if(it == VIPList.end())
		return false;

	VIPList.erase(it);
	return true;
}

bool Player::addVIP(uint32_t _guid, std::string& name, bool isOnline, bool internal/* = false*/)
{
	if(guid == _guid)
	{
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "You cannot add yourself.");

		return false;
	}

	if(VIPList.size() > (group ? group->getMaxVips(isPremium()) : 20))
	{
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "You cannot add more buddies.");

		return false;
	}

	VIPListSet::iterator it = VIPList.find(_guid);
	if(it != VIPList.end())
	{
		if(!internal)
			sendTextMessage(MSG_STATUS_SMALL, "This player is already in your list.");

		return false;
	}

	VIPList.insert(_guid);
	if(client && !internal)
		client->sendVIP(_guid, name, isOnline);

	return true;
}

//close container and its child containers
void Player::autoCloseContainers(const Container* container)
{
	typedef std::vector<uint32_t> CloseList;
	CloseList closeList;
	for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it)
	{
		Container* tmp = it->second;
		while(tmp != nullptr)
		{
			if(tmp->isRemoved() || tmp == container)
			{
				closeList.push_back(it->first);
				break;
			}

			tmp = dynamic_cast<Container*>(tmp->getParent());
		}
	}

	for(CloseList::iterator it = closeList.begin(); it != closeList.end(); ++it)
	{
		closeContainer(*it);
		if(client)
			client->sendCloseContainer(*it);
	}
}

bool Player::hasCapacity(const Item* item, uint32_t count) const
{
	if(hasFlag(PlayerFlag_CannotPickupItem))
		return false;

	if(hasFlag(PlayerFlag_HasInfiniteCapacity) || item->getTopParent() == this)
		return true;

	double itemWeight = 0;
	if(item->isStackable())
		itemWeight = item->getKind()->weight * count;
	else
		itemWeight = item->getWeight();

	return (itemWeight < getFreeCapacity());
}

ReturnValue Player::__queryAdd(int32_t index, const Thing* thing, uint32_t count, uint32_t flags) const
{
	const Item* item = thing->getItem();
	if(!item)
		return RET_NOTPOSSIBLE;

	bool childIsOwner = ((flags & FLAG_CHILDISOWNER) == FLAG_CHILDISOWNER), skipLimit = ((flags & FLAG_NOLIMIT) == FLAG_NOLIMIT);
	if(childIsOwner)
	{
		//a child container is querying the player, just check if enough capacity
		if(skipLimit || hasCapacity(item, count))
			return RET_NOERROR;

		return RET_NOTENOUGHCAPACITY;
	}

	if(!item->isPickupable())
		return RET_CANNOTPICKUP;

	ReturnValue ret = RET_NOERROR;
	if((item->getSlotPosition() & SLOTP_HEAD) || (item->getSlotPosition() & SLOTP_NECKLACE) ||
		(item->getSlotPosition() & SLOTP_BACKPACK) || (item->getSlotPosition() & SLOTP_ARMOR) ||
		(item->getSlotPosition() & SLOTP_LEGS) || (item->getSlotPosition() & SLOTP_FEET) ||
		(item->getSlotPosition() & SLOTP_RING))
		ret = RET_CANNOTBEDRESSED;
	else if(item->getSlotPosition() & SLOTP_TWO_HAND)
		ret = RET_PUTTHISOBJECTINBOTHHANDS;
	else if((item->getSlotPosition() & SLOTP_RIGHT) || (item->getSlotPosition() & SLOTP_LEFT))
		ret = RET_PUTTHISOBJECTINYOURHAND;

	switch(index)
	{
		case +slots_t::HEAD:
			if(item->getSlotPosition() & SLOTP_HEAD)
				ret = RET_NOERROR;
			break;
		case +slots_t::NECKLACE:
			if(item->getSlotPosition() & SLOTP_NECKLACE)
				ret = RET_NOERROR;
			break;
		case +slots_t::BACKPACK:
			if(item->getSlotPosition() & SLOTP_BACKPACK)
				ret = RET_NOERROR;
			break;
		case +slots_t::ARMOR:
			if(item->getSlotPosition() & SLOTP_ARMOR)
				ret = RET_NOERROR;
			break;
		case +slots_t::RIGHT:
			if(item->getSlotPosition() & SLOTP_RIGHT)
			{
				//check if we already carry an item in the other hand
				if(item->getSlotPosition() & SLOTP_TWO_HAND)
				{
					if(inventory[+slots_t::LEFT] && inventory[+slots_t::LEFT] != item)
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					else
						ret = RET_NOERROR;
				}
				else if(inventory[+slots_t::LEFT])
				{
					const boost::intrusive_ptr<Item> leftItem = inventory[+slots_t::LEFT];
					WeaponType_t type = item->getWeaponType(), leftType = leftItem->getWeaponType();
					if(leftItem->getSlotPosition() & SLOTP_TWO_HAND)
						ret = RET_DROPTWOHANDEDITEM;
					else if(item == leftItem && count == item->getItemCount())
						ret = RET_NOERROR;
					else if(leftType == WEAPON_SHIELD && type == WEAPON_SHIELD)
						ret = RET_CANONLYUSEONESHIELD;
					else if(!leftItem->isWeapon() || !item->isWeapon() ||
						leftType == WEAPON_SHIELD || leftType == WEAPON_AMMO
						|| type == WEAPON_SHIELD || type == WEAPON_AMMO)
						ret = RET_NOERROR;
					else
						ret = RET_CANONLYUSEONEWEAPON;
				}
				else
					ret = RET_NOERROR;
			}
			break;
		case +slots_t::LEFT:
			if(item->getSlotPosition() & SLOTP_LEFT)
			{
				//check if we already carry an item in the other hand
				if(item->getSlotPosition() & SLOTP_TWO_HAND)
				{
					if(inventory[+slots_t::RIGHT] && inventory[+slots_t::RIGHT] != item)
						ret = RET_BOTHHANDSNEEDTOBEFREE;
					else
						ret = RET_NOERROR;
				}
				else if(inventory[+slots_t::RIGHT])
				{
					const boost::intrusive_ptr<Item> rightItem = inventory[+slots_t::RIGHT];
					WeaponType_t type = item->getWeaponType(), rightType = rightItem->getWeaponType();
					if(rightItem->getSlotPosition() & SLOTP_TWO_HAND)
						ret = RET_DROPTWOHANDEDITEM;
					else if(item == rightItem && count == item->getItemCount())
						ret = RET_NOERROR;
					else if(rightType == WEAPON_SHIELD && type == WEAPON_SHIELD)
						ret = RET_CANONLYUSEONESHIELD;
					else if(!rightItem->isWeapon() || !item->isWeapon() ||
						rightType == WEAPON_SHIELD || rightType == WEAPON_AMMO
						|| type == WEAPON_SHIELD || type == WEAPON_AMMO)
						ret = RET_NOERROR;
					else
						ret = RET_CANONLYUSEONEWEAPON;
				}
				else
					ret = RET_NOERROR;
			}
			break;
		case +slots_t::LEGS:
			if(item->getSlotPosition() & SLOTP_LEGS)
				ret = RET_NOERROR;
			break;
		case +slots_t::FEET:
			if(item->getSlotPosition() & SLOTP_FEET)
				ret = RET_NOERROR;
			break;
		case +slots_t::RING:
			if(item->getSlotPosition() & SLOTP_RING)
				ret = RET_NOERROR;
			break;
		case +slots_t::AMMO:
			if(item->getSlotPosition() & SLOTP_AMMO)
				ret = RET_NOERROR;
			break;
		case +slots_t::WHEREEVER:
		case -1:
			ret = RET_NOTENOUGHROOM;
			break;
		default:
			ret = RET_NOTPOSSIBLE;
			break;
	}

	if(ret == RET_NOERROR || ret == RET_NOTENOUGHROOM)
	{
		//need an exchange with source?
		if(getInventoryItem((slots_t)index) != nullptr && (!getInventoryItem((slots_t)index)->isStackable()
			|| getInventoryItem((slots_t)index)->getId() != item->getId()))
			return RET_NEEDEXCHANGE;

		if(!server.moveEvents().onPlayerEquip(const_cast<Player*>(this), const_cast<Item*>(item), (slots_t)index, true))
			return RET_CANNOTBEDRESSED;

		//check if enough capacity
		if(!hasCapacity(item, count))
			return RET_NOTENOUGHCAPACITY;
	}

	return ret;
}

ReturnValue Player::__queryMaxCount(int32_t index, const Thing* thing, uint32_t count, uint32_t& maxQueryCount,
	uint32_t flags) const
{
	const Item* item = thing->getItem();
	if(!item)
	{
		maxQueryCount = 0;
		return RET_NOTPOSSIBLE;
	}

	const Thing* destThing = __getThing(index);
	const Item* destItem = nullptr;
	if(destThing)
		destItem = destThing->getItem();

	if(destItem)
	{
		if(destItem->isStackable() && item->getId() == destItem->getId())
			maxQueryCount = 100 - destItem->getItemCount();
		else
			maxQueryCount = 0;
	}
	else
	{
		if(item->isStackable())
			maxQueryCount = 100;
		else
			maxQueryCount = 1;

		return RET_NOERROR;
	}

	if(maxQueryCount < count)
		return RET_NOTENOUGHROOM;

	return RET_NOERROR;
}

ReturnValue Player::__queryRemove(const Thing* thing, uint32_t count, uint32_t flags) const
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
		return RET_NOTPOSSIBLE;

	const Item* item = thing->getItem();
	if(!item)
		return RET_NOTPOSSIBLE;

	if(count == 0 || (item->isStackable() && count > item->getItemCount()))
		return RET_NOTPOSSIBLE;

	 if(item->isNotMoveable() && !hasBitSet(FLAG_IGNORENOTMOVEABLE, flags))
		return RET_NOTMOVEABLE;

	return RET_NOERROR;
}

Cylinder* Player::__queryDestination(int32_t& index, const Thing* thing, Item** destItem,
	uint32_t& flags)
{
	if(index == 0 /*drop to capacity window*/ || index == INDEX_WHEREEVER)
	{
		*destItem = nullptr;
		const Item* item = thing->getItem();
		if(!item)
			return this;

		//find a appropiate slot
		for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
		{
			if(!inventory[i] && __queryAdd(i, item, item->getItemCount(), 0) == RET_NOERROR)
			{
				index = i;
				return this;
			}
		}

		//try containers
		std::list<std::pair<Container*, int32_t> > deepList;
		for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
		{
			if(inventory[i] == tradeItem)
				continue;

			if(Container* container = dynamic_cast<Container*>(inventory[i].get()))
			{
				if(container->__queryAdd(-1, item, item->getItemCount(), 0) == RET_NOERROR)
				{
					index = INDEX_WHEREEVER;
					*destItem = nullptr;
					return container;
				}

				deepList.push_back(std::make_pair(container, 0));
			}
		}

		//check deeper in the containers
		int32_t deepness = server.configManager().getNumber(ConfigManager::PLAYER_DEEPNESS);
		for(std::list<std::pair<Container*, int32_t> >::iterator dit = deepList.begin(); dit != deepList.end(); ++dit)
		{
			Container* c = (*dit).first;
			if(!c || c->empty())
				continue;

			int32_t level = (*dit).second;
			for(ItemList::const_iterator it = c->getItems(); it != c->getEnd(); ++it)
			{
				if((*it) == tradeItem)
					continue;

				if(Container* subContainer = dynamic_cast<Container*>((*it).get()))
				{
					if(subContainer->__queryAdd(-1, item, item->getItemCount(), 0) == RET_NOERROR)
					{
						index = INDEX_WHEREEVER;
						*destItem = nullptr;
						return subContainer;
					}

					if(deepness < 0 || level < deepness)
						deepList.push_back(std::make_pair(subContainer, (level + 1)));
				}
			}
		}

		return this;
	}

	Thing* destThing = __getThing(index);
	if(destThing)
		*destItem = destThing->getItem();

	if(Cylinder* subCylinder = dynamic_cast<Cylinder*>(destThing))
	{
		index = INDEX_WHEREEVER;
		*destItem = nullptr;
		return subCylinder;
	}

	return this;
}

void Player::__addThing(Creature* actor, Thing* thing)
{
	__addThing(actor, 0, thing);
}

void Player::__addThing(Creature* actor, int32_t index, Thing* thing)
{
	if(index < 0 || index > 11)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	if(index == 0)
	{
		return /*RET_NOTENOUGHROOM*/;
	}

	Item* item = thing->getItem();
	if(!item)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	item->setParent(this);
	inventory[index] = item;

	//send to client
	sendAddInventoryItem((slots_t)index, item);

	//event methods
	onAddInventoryItem((slots_t)index, item);
}

void Player::__updateThing(Thing* thing, uint16_t itemId, uint32_t count)
{
	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(item == nullptr)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	ItemKindPC oldKind = item->getKind();
	ItemKindPC newKind = server.items()[itemId];
	if (!newKind) {
		return;
	}

	item->setKind(newKind);
	item->setSubType(count);

	//send to client
	sendUpdateInventoryItem((slots_t)index, item, item);
	//event methods
	onUpdateInventoryItem((slots_t)index, item, oldKind, item, newKind);
}

void Player::__replaceThing(uint32_t index, Thing* thing)
{
	if(index > 11)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* oldItem = getInventoryItem((slots_t)index);
	if(!oldItem)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	Item* item = thing->getItem();
	if(!item)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	ItemKindPC oldKind = oldItem->getKind();
	ItemKindPC newKind = item->getKind();

	//send to client
	sendUpdateInventoryItem((slots_t)index, oldItem, item);
	//event methods
	onUpdateInventoryItem((slots_t)index, oldItem, oldKind, item, newKind);

	item->setParent(this);
	inventory[index] = item;
}

void Player::__removeThing(Thing* thing, uint32_t count)
{
	Item* item = thing->getItem();
	if(!item)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	int32_t index = __getIndexOfThing(thing);
	if(index == -1)
	{
		return /*RET_NOTPOSSIBLE*/;
	}

	if(item->isStackable())
	{
		if(count == item->getItemCount())
		{
			//send change to client
			sendRemoveInventoryItem((slots_t)index, item);
			//event methods
			onRemoveInventoryItem((slots_t)index, item);

			item->setParent(nullptr);
			inventory[index] = nullptr;
		}
		else
		{
			item->setItemCount(std::max(0, (int32_t)(item->getItemCount() - count)));

			//send change to client
			sendUpdateInventoryItem((slots_t)index, item, item);
			//event methods
			onUpdateInventoryItem((slots_t)index, item, item->getKind(), item, item->getKind());
		}
	}
	else
	{
		//send change to client
		sendRemoveInventoryItem((slots_t)index, item);
		//event methods
		onRemoveInventoryItem((slots_t)index, item);

		item->setParent(nullptr);
		inventory[index] = nullptr;
	}
}

Thing* Player::__getThing(uint32_t index) const
{
	if(index > +slots_t::PRE_FIRST && index < +slots_t::LAST)
		return inventory[index].get();

	return nullptr;
}

int32_t Player::__getIndexOfThing(const Thing* thing) const
{
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(inventory[i] == thing)
			return i;
	}

	return -1;
}

int32_t Player::__getFirstIndex() const
{
	return +slots_t::FIRST;
}

int32_t Player::__getLastIndex() const
{
	return +slots_t::LAST;
}

uint32_t Player::__getItemTypeCount(uint16_t itemId, int32_t subType /*= -1*/, bool itemCount /*= true*/, bool includeSlots /* = true */) const
{
	Item* item = nullptr;
	Container* container = nullptr;

	uint32_t count = 0;
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(!(item = inventory[i].get()))
			continue;

		if(includeSlots && item->getId() == itemId)
			count += Item::countByType(item, subType, itemCount);

		if(!(container = item->getContainer()))
			continue;

		for(ContainerIterator it = container->begin(), end = container->end(); it != end; ++it)
		{
			if((*it)->getId() == itemId)
				count += Item::countByType(*it, subType, itemCount);
		}
	}

	return count;

}

std::map<uint32_t, uint32_t>& Player::__getAllItemTypeCount(std::map<uint32_t,
	uint32_t>& countMap, bool itemCount/* = true*/) const
{
	Item* item = nullptr;
	Container* container = nullptr;
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(!(item = inventory[i].get()))
			continue;

		countMap[item->getId()] += Item::countByType(item, -1, itemCount);
		if(!(container = item->getContainer()))
			continue;

		for(ContainerIterator it = container->begin(), end = container->end(); it != end; ++it)
			countMap[(*it)->getId()] += Item::countByType(*it, -1, itemCount);
	}

	return countMap;
}

void Player::postAddNotification(Creature* actor, Thing* thing, const Cylinder* oldParent,
	int32_t index, cylinderlink_t link /*= LINK_OWNER*/)
{
	if(link == LINK_OWNER) //calling movement scripts
		server.moveEvents().onPlayerEquip(this, thing->getItem(), (slots_t)index, false);

	bool requireListUpdate = true;
	if(link == LINK_OWNER || link == LINK_TOPPARENT)
	{
		if(const Item* item = (oldParent ? oldParent->getItem() : nullptr))
		{
			assert(item->getContainer() != nullptr);
			requireListUpdate = item->getContainer()->getHoldingPlayer() != this;
		}
		else
			requireListUpdate = oldParent != this;

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if(const Item* item = thing->getItem())
	{
		if(const Container* container = item->getContainer())
			onSendContainer(container);

		if(shopOwner && requireListUpdate)
			updateInventoryGoods(item->getKind());
	}
	else if(const Creature* creature = thing->getCreature())
	{
		if(creature != this)
			return;

		typedef std::vector<Container*> Containers;
		Containers containers;
		for(ContainerVector::iterator it = containerVec.begin(); it != containerVec.end(); ++it)
		{
			if(!Position::areInRange<1,1,0>(it->second->getPosition(), getPosition()))
				containers.push_back(it->second);
		}

		for(Containers::const_iterator it = containers.begin(); it != containers.end(); ++it)
			autoCloseContainers(*it);
	}
}

void Player::postRemoveNotification(Creature* actor, Thing* thing, const Cylinder* newParent,
	int32_t index, bool isCompleteRemoval, cylinderlink_t link /*= LINK_OWNER*/)
{
	if(link == LINK_OWNER) //calling movement scripts
		server.moveEvents().onPlayerDeEquip(this, thing->getItem(), (slots_t)index, isCompleteRemoval);

	bool requireListUpdate = true;
	if(link == LINK_OWNER || link == LINK_TOPPARENT)
	{
		if(const Item* item = (newParent ? newParent->getItem() : nullptr))
		{
			assert(item->getContainer() != nullptr);
			requireListUpdate = item->getContainer()->getHoldingPlayer() != this;
		}
		else
			requireListUpdate = newParent != this;

		updateInventoryWeight();
		updateItemsLight();
		sendStats();
	}

	if(const Item* item = thing->getItem())
	{
		if(const Container* container = item->getContainer())
		{
			if(container->isRemoved() || !Position::areInRange<1,1,0>(getPosition(), container->getPosition()))
				autoCloseContainers(container);
			else if(container->getTopParent() == this)
				onSendContainer(container);
			else if(const Container* topContainer = dynamic_cast<const Container*>(container->getTopParent()))
			{
				if(const Depot* depot = dynamic_cast<const Depot*>(topContainer))
				{
					bool isOwner = false;
					for(DepotMap::iterator it = depots.begin(); it != depots.end(); ++it)
					{
						if(it->second.first != depot)
							continue;

						isOwner = true;
						onSendContainer(container);
					}

					if(!isOwner)
						autoCloseContainers(container);
				}
				else
					onSendContainer(container);
			}
			else
				autoCloseContainers(container);
		}

		if(shopOwner && requireListUpdate)
			updateInventoryGoods(item->getKind());
	}
}

void Player::__internalAddThing(Thing* thing)
{
	__internalAddThing(0, thing);
}

void Player::__internalAddThing(uint32_t index, Thing* thing)
{
	Item* item = thing->getItem();
	if(!item)
	{
		return;
	}

	//index == 0 means we should equip this item at the most appropiate slot
	if(index == 0)
	{
		return;
	}

	if(index > 0 && index < 11)
	{
		if(inventory[index])
		{
			return;
		}

		inventory[index] = item;
		item->setParent(this);
	}
}

bool Player::setFollowCreature(Creature* creature, bool fullPathSearch /*= false*/)
{
	bool deny = false;
	CreatureEventList followEvents = getCreatureEvents(CREATURE_EVENT_FOLLOW);
	for(CreatureEventList::iterator it = followEvents.begin(); it != followEvents.end(); ++it)
	{
		if(creature && !(*it)->executeFollow(this, creature))
			deny = true;
	}

	if(deny || !Creature::setFollowCreature(creature, fullPathSearch))
	{
		setFollowCreature(nullptr);
		setAttackedCreature(nullptr);
		if(!deny)
			sendCancelMessage(RET_THEREISNOWAY);

		sendCancelTarget();
		stopEventWalk();
		return false;
	}

	return true;
}

bool Player::setAttackedCreature(Creature* creature)
{
	if(!Creature::setAttackedCreature(creature))
	{
		sendCancelTarget();
		return false;
	}

	if(chaseMode == CHASEMODE_FOLLOW && creature)
	{
		if(followCreature != creature) //chase opponent
			setFollowCreature(creature);
	}
	else
		setFollowCreature(nullptr);

	if(creature)
		server.dispatcher().addTask(Task::create(std::bind(&Game::checkCreatureAttack, &server.game(), getID())));

	return true;
}

void Player::getPathSearchParams(const Creature* creature, FindPathParams& fpp) const
{
	Creature::getPathSearchParams(creature, fpp);
	fpp.fullPathSearch = true;
}

void Player::doAttacking(uint32_t interval)
{
	if(!lastAttack)
		lastAttack = OTSYS_TIME() - getAttackSpeed() - 1;
	else if((OTSYS_TIME() - lastAttack) < getAttackSpeed())
		return;

	if(hasCondition(CONDITION_PACIFIED) && !hasCustomFlag(PlayerCustomFlag_IgnorePacification))
	{
		lastAttack = OTSYS_TIME();
		return;
	}

	Item* tool = getWeapon();
	if(WeaponPC weapon = server.weapons().getWeapon(tool))
	{
		if(weapon->interruptSwing() && !canDoAction())
		{
			setNextActionTask(SchedulerTask::create(std::chrono::milliseconds(getNextActionTime()), std::bind(&Game::checkCreatureAttack, &server.game(), getID())));
		}
		else if((!weapon->hasExhaustion() || !hasCondition(CONDITION_EXHAUST, EXHAUST_COMBAT)) && weapon->useWeapon(this, tool, attackedCreature))
			lastAttack = OTSYS_TIME();
	}
	else if(Weapon::useFist(this, attackedCreature))
		lastAttack = OTSYS_TIME();
}

double Player::getGainedExperience(Creature* attacker) const
{
	if(!skillLoss)
		return 0;

	if (server.game().getWorldType() != WORLD_TYPE_PVP_ENFORCED && getZone() != ZONE_PVP)
		return 0;

	double rate = server.configManager().getDouble(ConfigManager::RATE_PVP_EXPERIENCE);
	if(rate <= 0)
		return 0;

	Player* attackerPlayer = attacker->getPlayer();
	if(!attackerPlayer || attackerPlayer == this)
		return 0;

	double attackerLevel = (double)attackerPlayer->getLevel(), min = server.configManager().getDouble(
		ConfigManager::EFP_MIN_THRESHOLD), max = server.configManager().getDouble(ConfigManager::EFP_MAX_THRESHOLD);
	if((min > 0 && level < (uint32_t)std::floor(attackerLevel * min)) || (max > 0 &&
		level > (uint32_t)std::floor(attackerLevel * max)))
		return 0;

	/*
		Formula
		a = attackers level * 0.9
		b = victims level
		c = victims experience

		result = (1 - (a / b)) * 0.05 * c
		Not affected by special multipliers(!)
	*/
	uint32_t a = (uint32_t)std::floor(attackerLevel * 0.9), b = level;
	uint64_t c = getExperience();
	return (double)std::max((uint64_t)0, (uint64_t)std::floor(getDamageRatio(attacker)
		* std::max((double)0, ((double)(1 - (((double)a / b))))) * 0.05 * c)) * rate;
}

void Player::onFollowCreature(const Creature* creature)
{
	if(!creature)
		stopEventWalk();
}

void Player::setChaseMode(chaseMode_t mode)
{
	chaseMode_t prevChaseMode = chaseMode;
	chaseMode = mode;

	if(prevChaseMode == chaseMode)
		return;

	if(chaseMode == CHASEMODE_FOLLOW)
	{
		if(!followCreature && attackedCreature) //chase opponent
			setFollowCreature(attackedCreature);
	}
	else if(attackedCreature)
	{
		setFollowCreature(nullptr);
		stopEventWalk();
	}
}

void Player::onWalkAborted()
{
	setNextWalkActionTask(nullptr);
	sendCancelWalk();
}

void Player::onWalkComplete()
{
	if(!walkTask)
		return;

	walkTaskEvent = server.scheduler().addTask(std::move(walkTask));
}

void Player::stopWalk()
{
	if(listWalkDir.empty())
		return;

	stopEventWalk();
}

void Player::getCreatureLight(LightInfo& light) const
{
	if(internalLight.level > itemsLight.level)
		light = internalLight;
	else
		light = itemsLight;
}

void Player::updateItemsLight(bool internal /*=false*/)
{
	LightInfo maxLight;
	LightInfo curLight;
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(Item* item = getInventoryItem((slots_t)i))
		{
			item->getLight(curLight);
			if(curLight.level > maxLight.level)
				maxLight = curLight;
		}
	}
	if(itemsLight.level != maxLight.level || itemsLight.color != maxLight.color)
	{
		itemsLight = maxLight;
		if(!internal)
			server.game().changeLight(this);
	}
}

void Player::onAddCondition(ConditionType_t type, bool hadCondition)
{
	Creature::onAddCondition(type, hadCondition);
	if(getLastPosition().x && type != CONDITION_GAMEMASTER) // don't send if player have just logged in (its already done in protocolgame), or condition have no icons
		sendIcons();
}

void Player::onAddCombatCondition(ConditionType_t type, bool hadCondition)
{
	std::string tmp;
	switch(type)
	{
		//client hardcoded
		case CONDITION_FIRE:
			tmp = "burning";
			break;
		case CONDITION_POISON:
			tmp = "poisoned";
			break;
		case CONDITION_ENERGY:
			tmp = "electrified";
			break;
		case CONDITION_FREEZING:
			tmp = "freezing";
			break;
		case CONDITION_DAZZLED:
			tmp = "dazzled";
			break;
		case CONDITION_CURSED:
			tmp = "cursed";
			break;
		case CONDITION_DROWN:
			tmp = "drowning";
			break;
		case CONDITION_DRUNK:
			tmp = "drunk";
			break;
		case CONDITION_MANASHIELD:
			tmp = "protected by a magic shield";
			break;
		case CONDITION_PARALYZE:
			tmp = "paralyzed";
			break;
		case CONDITION_HASTE:
			tmp = "hasted";
			break;
		case CONDITION_ATTRIBUTES:
			tmp = "strengthened";
			break;
		default:
			break;
	}

	if(!tmp.empty())
		sendTextMessage(MSG_STATUS_DEFAULT, "You are " + tmp + ".");
}

void Player::onEndCondition(ConditionType_t type)
{
	Creature::onEndCondition(type);
	if(type == CONDITION_INFIGHT)
	{
		clearAttacked();

		pzLocked = false;
		if(skull < SKULL_RED)
			setSkull(SKULL_NONE);

		server.game().updateCreatureSkull(this);
	}

	sendIcons();
}

void Player::onCombatRemoveCondition(const CreatureP& attacker, Condition* condition)
{
	//Creature::onCombatRemoveCondition(attacker, condition);
	bool remove = true;
	if(condition->getId() > 0)
	{
		remove = false;
		//Means the condition is from an item, id == slot
		if(server.game().getWorldType() == WORLD_TYPE_PVP_ENFORCED)
		{
			if(Item* item = getInventoryItem((slots_t)condition->getId()))
			{
				//25% chance to destroy the item
				if(25 >= random_range(0, 100))
					server.game().internalRemoveItem(nullptr, item);
			}
		}
	}

	if(remove)
	{
		if(!canDoAction())
		{
			int32_t delay = getNextActionTime();
			delay -= (delay % EVENT_CREATURE_THINK_INTERVAL);
			if(delay < 0)
				removeCondition(condition);
			else
				condition->setTicks(delay);
		}
		else
			removeCondition(condition);
	}
}

void Player::onTickCondition(ConditionType_t type, int32_t interval, bool& _remove)
{
	Creature::onTickCondition(type, interval, _remove);
	if(type == CONDITION_HUNTING)
		useStamina(-(interval * server.configManager().getNumber(ConfigManager::RATE_STAMINA_LOSS)));
}

void Player::onAttackedCreature(Creature* target)
{
	Creature::onAttackedCreature(target);
	if(hasFlag(PlayerFlag_NotGainInFight))
		return;

	addInFightTicks();
	Player* targetPlayer = target->getPlayer();
	if(!targetPlayer)
		return;

	addAttacked(targetPlayer);
	if(targetPlayer == this && targetPlayer->getZone() != ZONE_PVP)
	{
		targetPlayer->sendCreatureSkull(this);
		return;
	}

	if(Combat::isInPvpZone(*this, *targetPlayer) || isPartner(targetPlayer) || (server.configManager().getBool(
		ConfigManager::ALLOW_FIGHTBACK) && targetPlayer->hasAttacked(this)))
		return;

	if(!pzLocked)
	{
		pzLocked = true;
		sendIcons();
	}

	if(getZone() != target->getZone())
		return;

	if(skull == SKULL_NONE)
	{
		if(targetPlayer->getSkull() != SKULL_NONE)
			targetPlayer->sendCreatureSkull(this);
		else if(!hasCustomFlag(PlayerCustomFlag_NotGainSkull))
		{
			setSkull(SKULL_WHITE);
			server.game().updateCreatureSkull(this);
		}
	}
}

void Player::onSummonAttackedCreature(Creature* summon, Creature* target)
{
	Creature::onSummonAttackedCreature(summon, target);
	onAttackedCreature(target);
}

void Player::onAttacked()
{
	Creature::onAttacked();
	addInFightTicks();
}

bool Player::checkLoginDelay(uint32_t playerId) const
{
	return (!hasCustomFlag(PlayerCustomFlag_IgnoreLoginDelay) && OTSYS_TIME() <= (lastLoad + server.configManager().getNumber(
		ConfigManager::LOGIN_PROTECTION)) && !hasBeenAttacked(playerId));
}

void Player::onThinkingStopped()
{
	Creature::onThinkingStopped();
	if(getParty())
		getParty()->clearPlayerPoints(this);
}

void Player::onPlacedCreature()
{
	//scripting event - onLogin
	if(!server.creatureEvents().playerLogin(this))
		kickPlayer(true, true);
}

void Player::onAttackedCreatureDrain(Creature* target, int32_t points)
{
	Creature::onAttackedCreatureDrain(target, points);
	if(party && target && (!target->hasController() || !target->getController())
		&& target->getMonster() && target->getMonster()->isHostile()) //we have fulfilled a requirement for shared experience
		getParty()->addPlayerDamageMonster(this, points);

	char buffer[100];
	sprintf(buffer, "You deal %d damage to %s.", points, target->getNameDescription().c_str());
	sendTextMessage(MSG_STATUS_DEFAULT, buffer);
}

void Player::onSummonAttackedCreatureDrain(Creature* summon, Creature* target, int32_t points)
{
	Creature::onSummonAttackedCreatureDrain(summon, target, points);

	char buffer[100];
	sprintf(buffer, "Your %s deals %d damage to %s.", summon->getName().c_str(), points, target->getNameDescription().c_str());
	sendTextMessage(MSG_EVENT_DEFAULT, buffer);
}

void Player::onTargetCreatureGainHealth(Creature* target, int32_t points)
{
	Creature::onTargetCreatureGainHealth(target, points);
	if(target && getParty())
	{
		Player* tmpPlayer = nullptr;
		if(target->getPlayer())
			tmpPlayer = target->getPlayer();
		else if(target->hasController())
			tmpPlayer = target->getController().get();

		if(isPartner(tmpPlayer))
			getParty()->addPlayerHealedMember(this, points);
	}
}

bool Player::onKilledCreature(Creature* target, uint32_t& flags)
{
	if(!Creature::onKilledCreature(target, flags))
		return false;

	if(hasFlag(PlayerFlag_NotGenerateLoot))
		target->setDropLoot(LOOT_DROP_NONE);

	Condition* condition = nullptr;
	if(target->getMonster() && !target->hasController() && !hasFlag(PlayerFlag_HasInfiniteStamina)
		&& (condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_HUNTING,
		server.configManager().getNumber(ConfigManager::HUNTING_DURATION))))
		addCondition(condition);

	if(hasFlag(PlayerFlag_NotGainInFight) || !hasBitSet((uint32_t)KILLFLAG_JUSTIFY, flags) || getZone() != target->getZone())
		return true;

	Player* targetPlayer = target->getPlayer();
	if(!targetPlayer || Combat::isInPvpZone(*this, *targetPlayer) || !hasCondition(CONDITION_INFIGHT) || isPartner(targetPlayer))
		return true;

	if(!targetPlayer->hasAttacked(this) && target->getSkull() == SKULL_NONE && targetPlayer != this
		&& ((server.configManager().getBool(ConfigManager::USE_FRAG_HANDLER) && addUnjustifiedKill(
		targetPlayer)) || hasBitSet((uint32_t)KILLFLAG_LASTHIT, flags)))
		flags |= (uint32_t)KILLFLAG_UNJUSTIFIED;

	pzLocked = true;
	if((condition = Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INFIGHT,
		server.configManager().getNumber(ConfigManager::WHITE_SKULL_TIME))))
		addCondition(condition);

	return true;
}

bool Player::gainExperience(double& gainExp, bool fromMonster)
{
	if(!rateExperience(gainExp, fromMonster))
		return false;

	//soul regeneration
	if(gainExp >= level)
	{
		if(Condition* condition = Condition::createCondition(
			CONDITIONID_DEFAULT, CONDITION_SOUL, 4 * 60 * 1000))
		{
			condition->setParam(CONDITIONPARAM_SOULGAIN,
				vocation->getGainAmount(GAIN_SOUL));
			condition->setParam(CONDITIONPARAM_SOULTICKS,
				(vocation->getGainTicks(GAIN_SOUL) * 1000));
			addCondition(condition);
		}
	}

	addExperience((uint64_t)gainExp);
	return true;
}

bool Player::rateExperience(double& gainExp, bool fromMonster)
{
	if(hasFlag(PlayerFlag_NotGainExperience) || gainExp <= 0)
		return false;

	if(!fromMonster)
		return true;

	gainExp *= rates[SKILL__LEVEL] * server.game().getExperienceStage(level,
		vocation->getExperienceMultiplier());
	if(!hasFlag(PlayerFlag_HasInfiniteStamina))
	{
		int32_t minutes = getStaminaMinutes();
		if(minutes >= server.configManager().getNumber(ConfigManager::STAMINA_LIMIT_TOP))
		{
			if(isPremium() || !server.configManager().getNumber(ConfigManager::STAMINA_BONUS_PREMIUM))
				gainExp *= server.configManager().getDouble(ConfigManager::RATE_STAMINA_ABOVE);
		}
		else if(minutes < (server.configManager().getNumber(ConfigManager::STAMINA_LIMIT_BOTTOM)) && minutes > 0)
			gainExp *= server.configManager().getDouble(ConfigManager::RATE_STAMINA_UNDER);
		else if(minutes <= 0)
			gainExp = 0;
	}
	else if(isPremium() || !server.configManager().getNumber(ConfigManager::STAMINA_BONUS_PREMIUM))
		gainExp *= server.configManager().getDouble(ConfigManager::RATE_STAMINA_ABOVE);

	return true;
}

void Player::onGainExperience(double& gainExp, bool fromMonster, bool multiplied)
{
	if(party && party->isSharedExperienceEnabled() && party->isSharedExperienceActive())
	{
		party->shareExperience(gainExp, fromMonster, multiplied);
		rateExperience(gainExp, fromMonster);
		return; //we will get a share of the experience through the sharing mechanism
	}

	if(gainExperience(gainExp, fromMonster))
		Creature::onGainExperience(gainExp, fromMonster, true);
}

void Player::onGainSharedExperience(double& gainExp, bool fromMonster, bool multiplied)
{
	if(gainExperience(gainExp, fromMonster))
		Creature::onGainSharedExperience(gainExp, fromMonster, true);
}

bool Player::isImmune(CombatType_t type) const
{
	return hasCustomFlag(PlayerCustomFlag_IsImmune) || Creature::isImmune(type);
}

bool Player::isImmune(ConditionType_t type) const
{
	return hasCustomFlag(PlayerCustomFlag_IsImmune) || Creature::isImmune(type);
}

bool Player::isAttackable() const
{
	return (!hasFlag(PlayerFlag_CannotBeAttacked) && !isAccountManager());
}

void Player::changeHealth(int32_t healthChange)
{
	int32_t previousHealth = health;

	Creature::changeHealth(healthChange);

	if (health != previousHealth) {
		sendStats();
	}
}

void Player::changeMana(int32_t manaChange)
{
	int32_t previousMana = mana;

	if(!hasFlag(PlayerFlag_HasInfiniteMana))
		Creature::changeMana(manaChange);

	if (mana != previousMana) {
		sendStats();
	}
}

void Player::changeSoul(int32_t soulChange)
{
	if(!hasFlag(PlayerFlag_HasInfiniteSoul))
		soul = std::min((int32_t)soulMax, (int32_t)soul + soulChange);

	sendStats();
}

bool Player::canLogout(bool checkInfight)
{
	if(checkInfight && hasCondition(CONDITION_INFIGHT))
		return false;

	return !isConnecting && !pzLocked && !getTile()->hasFlag(TILESTATE_NOLOGOUT);
}

bool Player::changeOutfit(Outfit_t outfit, bool checkList)
{
	uint32_t outfitId = Outfits::getInstance()->getOutfitId(outfit.lookType);
	if(checkList && (!canWearOutfit(outfitId, outfit.lookAddons) || !requestedOutfit))
		return false;

	requestedOutfit = false;
	if(outfitAttributes)
	{
		uint32_t oldId = Outfits::getInstance()->getOutfitId(defaultOutfit.lookType);
		outfitAttributes = !Outfits::getInstance()->removeAttributes(getID(), oldId, sex);
	}

	defaultOutfit = outfit;
	outfitAttributes = Outfits::getInstance()->addAttributes(getID(), outfitId, sex, defaultOutfit.lookAddons);
	return true;
}

bool Player::canWearOutfit(uint32_t outfitId, uint32_t addons)
{
	OutfitMap::iterator it = outfits.find(outfitId);
	if(it == outfits.end() || (it->second.isPremium && !isPremium()) || getAccess() < it->second.accessLevel
		|| ((it->second.addons & addons) != addons && !hasCustomFlag(PlayerCustomFlag_CanWearAllAddons)))
		return false;

	if(!it->second.storageId)
		return true;

	std::string value;
	return getStorage(it->second.storageId, value) && value == it->second.storageValue;
}

bool Player::addOutfit(uint32_t outfitId, uint32_t addons)
{
	Outfit outfit;
	if(!Outfits::getInstance()->getOutfit(outfitId, sex, outfit))
		return false;

	OutfitMap::iterator it = outfits.find(outfitId);
	if(it != outfits.end())
		outfit.addons |= it->second.addons;

	outfit.addons |= addons;
	outfits[outfitId] = outfit;
	return true;
}

bool Player::removeOutfit(uint32_t outfitId, uint32_t addons)
{
	OutfitMap::iterator it = outfits.find(outfitId);
	if(it == outfits.end())
		return false;

	if(addons == 0xFF) //remove outfit
		outfits.erase(it);
	else //remove addons
		outfits[outfitId].addons = it->second.addons & (~addons);

	return true;
}

void Player::generateReservedStorage()
{
	uint32_t baseKey = PSTRG_OUTFITSID_RANGE_START + 1;
	const OutfitMap& defaultOutfits = Outfits::getInstance()->getOutfits(sex);
	for(OutfitMap::const_iterator it = outfits.begin(); it != outfits.end(); ++it)
	{
		OutfitMap::const_iterator dit = defaultOutfits.find(it->first);
		if(dit == defaultOutfits.end() || (dit->second.isDefault && (dit->second.addons
			& it->second.addons) == it->second.addons))
			continue;

		std::stringstream ss;
		ss << ((it->first << 16) | (it->second.addons & 0xFF));
		storageMap[baseKey] = ss.str();

		baseKey++;
		if(baseKey <= PSTRG_OUTFITSID_RANGE_START + PSTRG_OUTFITSID_RANGE_SIZE)
			continue;

		LOGw("[Player::genReservedStorageRange] Player " << getName() << " with more than 500 outfits!");
		break;
	}
}

void Player::setSex(uint16_t newSex)
{
	sex = newSex;
	const OutfitMap& defaultOutfits = Outfits::getInstance()->getOutfits(sex);
	for(OutfitMap::const_iterator it = defaultOutfits.begin(); it != defaultOutfits.end(); ++it)
	{
		if(it->second.isDefault)
			addOutfit(it->first, it->second.addons);
	}
}

Skulls_t Player::getSkull() const
{
	if(hasFlag(PlayerFlag_NotGainInFight) || hasCustomFlag(PlayerCustomFlag_NotGainSkull))
		return SKULL_NONE;

	return skull;
}

Skulls_t Player::getSkullClient(const Creature* creature) const
{
	if(const Player* player = creature->getPlayer())
	{
		if(server.game().getWorldType() != WORLD_TYPE_PVP)
			return SKULL_NONE;

		if((player == this || (skull != SKULL_NONE && player->getSkull() < SKULL_RED)) && player->hasAttacked(this))
			return SKULL_YELLOW;

		if(player->getSkull() == SKULL_NONE && isPartner(player) && server.game().getWorldType() != WORLD_TYPE_NO_PVP)
			return SKULL_GREEN;
	}

	return Creature::getSkullClient(creature);
}

bool Player::hasAttacked(const Player* attacked) const
{
	return !hasFlag(PlayerFlag_NotGainInFight) && attacked &&
		attackedSet.find(attacked->getID()) != attackedSet.end();
}

void Player::addAttacked(const Player* attacked)
{
	if(hasFlag(PlayerFlag_NotGainInFight) || !attacked)
		return;

	uint32_t attackedId = attacked->getID();
	if(attackedSet.find(attackedId) == attackedSet.end())
		attackedSet.insert(attackedId);
}

void Player::setSkullEnd(time_t _time, bool login, Skulls_t _skull)
{
	if(server.game().getWorldType() == WORLD_TYPE_PVP_ENFORCED)
		return;

	bool requireUpdate = false;
	if(_time > time(nullptr))
	{
		requireUpdate = true;
		setSkull(_skull);
	}
	else if(skull == _skull)
	{
		requireUpdate = true;
		setSkull(SKULL_NONE);
		_time = 0;
	}

	if(requireUpdate)
	{
		skullEnd = _time;
		if(!login)
			server.game().updateCreatureSkull(this);
	}
}

bool Player::addUnjustifiedKill(const Player* attacked)
{
	if(server.game().getWorldType() == WORLD_TYPE_PVP_ENFORCED || attacked == this || hasFlag(
		PlayerFlag_NotGainInFight) || hasCustomFlag(PlayerCustomFlag_NotGainSkull))
		return false;

	if(client)
	{
		char buffer[90];
		sprintf(buffer, "Warning! The murder of %s was not justified.",
			attacked->getName().c_str());
		client->sendTextMessage(MSG_STATUS_WARNING, buffer);
	}

	time_t now = time(nullptr), today = (now - 84600), week = (now - (7 * 84600));
	std::vector<time_t> dateList;
	IOLoginData::getInstance()->getUnjustifiedDates(guid, dateList, now);

	dateList.push_back(now);
	uint32_t tc = 0, wc = 0, mc = dateList.size();
	for(std::vector<time_t>::iterator it = dateList.begin(); it != dateList.end(); ++it)
	{
		if((*it) > week)
			wc++;

		if((*it) > today)
			tc++;
	}

	uint32_t d = server.configManager().getNumber(ConfigManager::RED_DAILY_LIMIT), w = server.configManager().getNumber(
		ConfigManager::RED_WEEKLY_LIMIT), m = server.configManager().getNumber(ConfigManager::RED_MONTHLY_LIMIT);
	if(skull < SKULL_RED && ((d > 0 && tc >= d) || (w > 0 && wc >= w) || (m > 0 && mc >= m)))
		setSkullEnd(now + server.configManager().getNumber(ConfigManager::RED_SKULL_LENGTH), false, SKULL_RED);

	if(!server.configManager().getBool(ConfigManager::USE_BLACK_SKULL))
	{
		d += server.configManager().getNumber(ConfigManager::BAN_DAILY_LIMIT);
		w += server.configManager().getNumber(ConfigManager::BAN_WEEKLY_LIMIT);
		m += server.configManager().getNumber(ConfigManager::BAN_MONTHLY_LIMIT);
		if((d <= 0 || tc < d) && (w <= 0 || wc < w) && (m <= 0 || mc < m))
			return true;

		if(!IOBan::getInstance()->addAccountBanishment(accountId, (now + server.configManager().getNumber(
			ConfigManager::KILLS_BAN_LENGTH)), 20, ACTION_BANISHMENT, "Unjustified player killing.", 0, guid))
			return true;

		sendTextMessage(MSG_INFO_DESCR, "You have been banished.");
		server.game().addMagicEffect(getPosition(), MAGIC_EFFECT_WRAPS_GREEN);
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(1000), std::bind(
			&Game::kickPlayer, &server.game(), getID(), false)));
	}
	else
	{
		d += server.configManager().getNumber(ConfigManager::BLACK_DAILY_LIMIT);
		w += server.configManager().getNumber(ConfigManager::BLACK_WEEKLY_LIMIT);
		m += server.configManager().getNumber(ConfigManager::BLACK_MONTHLY_LIMIT);
		if(skull < SKULL_BLACK && ((d > 0 && tc >= d) || (w > 0 && wc >= w) || (m > 0 && mc >= m)))
		{
			setSkullEnd(now + server.configManager().getNumber(ConfigManager::BLACK_SKULL_LENGTH), false, SKULL_BLACK);
			setAttackedCreature(nullptr);
			killSummons();
		}
	}

	return true;
}

void Player::setPromotionLevel(uint32_t pLevel)
{
	if(pLevel > promotionLevel)
	{
		uint32_t tmpLevel = 0, currentVoc = vocation_id;
		for(uint32_t i = promotionLevel; i < pLevel; ++i)
		{
			currentVoc = Vocations::getInstance()->getPromotedVocation(currentVoc);
			if(!currentVoc)
				break;

			tmpLevel++;
			Vocation* voc = Vocations::getInstance()->getVocation(currentVoc);
			if(voc->isPremiumNeeded() && !isPremium() && server.configManager().getBool(ConfigManager::PREMIUM_FOR_PROMOTION))
				continue;

			vocation_id = currentVoc;
		}

		promotionLevel += tmpLevel;
	}
	else if(pLevel < promotionLevel)
	{
		uint32_t tmpLevel = 0, currentVoc = vocation_id;
		for(uint32_t i = pLevel; i < promotionLevel; ++i)
		{
			Vocation* voc = Vocations::getInstance()->getVocation(currentVoc);
			if(voc->getFromVocation() == currentVoc)
				break;

			tmpLevel++;
			currentVoc = voc->getFromVocation();
			if(voc->isPremiumNeeded() && !isPremium() && server.configManager().getBool(ConfigManager::PREMIUM_FOR_PROMOTION))
				continue;

			vocation_id = currentVoc;
		}

		promotionLevel -= tmpLevel;
	}

	setVocation(vocation_id);
}

uint16_t Player::getBlessings() const
{
	if(!isPremium() && server.configManager().getBool(ConfigManager::BLESSING_ONLY_PREMIUM))
		return 0;

	uint16_t count = 0;
	for(int16_t i = 0; i < 16; ++i)
	{
		if(hasBlessing(i))
			count++;
	}

	return count;
}

uint64_t Player::getLostExperience() const
{
	if(!skillLoss)
		return 0;

	double percent = (double)(lossPercent[LOSS_EXPERIENCE] - vocation->getLessLoss() - (getBlessings() * server.configManager().getNumber(
		ConfigManager::BLESS_REDUCTION))) / 100.;
	if(level <= 25)
		return (uint64_t)std::floor((double)(experience * percent) / 10.);

	int32_t base = level;
	double levels = (double)(base + 50) / 100.;

	uint64_t lost = 0;
	while(levels > 1.0f)
	{
		lost += (getExpForLevel(base) - getExpForLevel(base - 1));
		base--;
		levels -= 1.;
	}

	if(levels > 0.)
		lost += (uint64_t)std::floor((double)(getExpForLevel(base) - getExpForLevel(base - 1)) * levels);

	return (uint64_t)std::floor((double)(lost * percent));
}

uint32_t Player::getAttackSpeed()
{
	Item* weapon = getWeapon();
	if(weapon && weapon->getAttackSpeed() != 0)
		return weapon->getAttackSpeed();

	return vocation->getAttackSpeed();
}

void Player::learnInstantSpell(const std::string& name)
{
	if(!hasLearnedInstantSpell(name))
		learnedInstantSpellList.push_back(name);
}

void Player::unlearnInstantSpell(const std::string& name)
{
	if(!hasLearnedInstantSpell(name))
		return;

	LearnedInstantSpellList::iterator it = std::find(learnedInstantSpellList.begin(), learnedInstantSpellList.end(), name);
	if(it != learnedInstantSpellList.end())
		learnedInstantSpellList.erase(it);
}

bool Player::hasLearnedInstantSpell(const std::string& name) const
{
	if(hasFlag(PlayerFlag_CannotUseSpells))
		return false;

	if(hasFlag(PlayerFlag_IgnoreSpellCheck))
		return true;

	for(LearnedInstantSpellList::const_iterator it = learnedInstantSpellList.begin(); it != learnedInstantSpellList.end(); ++it)
	{
		if(boost::iequals(*it, name))
			return true;
	}

	return false;
}

void Player::manageAccount(const std::string &text)
{
	std::stringstream msg;
	msg << "Account Manager: ";

	bool noSwap = true;
	switch(accountManager)
	{
		case MANAGER_NAMELOCK:
		{
			if(!talkState[1])
			{
				managerString = text;
				trimString(managerString);
				if(managerString.length() < 4)
					msg << "Your name you want is too short, please select a longer name.";
				else if(managerString.length() > 20)
					msg << "The name you want is too long, please select a shorter name.";
				else if(!isValidName(managerString))
					msg << "That name seems to contain invalid symbols, please choose another name.";
				else if(IOLoginData::getInstance()->playerExists(managerString, true))
					msg << "A player with that name already exists, please choose another name.";
				else
				{
					std::string tmp = asLowerCaseString(managerString);
					if(tmp.substr(0, 4) != "god " && tmp.substr(0, 3) != "cm " && tmp.substr(0, 3) != "gm ")
					{
						talkState[1] = true;
						talkState[2] = true;
						msg << managerString << ", are you sure?";
					}
					else
						msg << "Your character is not a staff member, please tell me another name!";
				}
			}
			else if(checkText(text, "no") && talkState[2])
			{
				talkState[1] = talkState[2] = false;
				msg << "What else would you like to name your character?";
			}
			else if(checkText(text, "yes") && talkState[2])
			{
				if(!IOLoginData::getInstance()->playerExists(managerString, true))
				{
					uint32_t tmp;
					if(IOLoginData::getInstance()->getGuidByName(tmp, managerString2) &&
						IOLoginData::getInstance()->changeName(tmp, managerString, managerString2) &&
						IOBan::getInstance()->removePlayerBanishment(tmp, PLAYERBAN_LOCK))
					{
						if(House* house = Houses::getInstance()->getHouseByPlayerId(tmp))
							house->updateDoorDescription(managerString);

						talkState[1] = true;
						talkState[2] = false;
						msg << "Your character has been successfully renamed, you should now be able to login at it without any problems.";
					}
					else
					{
						talkState[1] = talkState[2] = false;
						msg << "Failed to change your name, please try again.";
					}
				}
				else
				{
					talkState[1] = talkState[2] = false;
					msg << "A player with that name already exists, please choose another name.";
				}
			}
			else
				msg << "Sorry, but I can't understand you, please try to repeat that!";

			break;
		}
		case MANAGER_ACCOUNT:
		{
			AccountP account = IOLoginData::getInstance()->loadAccount(managerNumber);
			if (account == nullptr) {
				return;
			}

			if(checkText(text, "cancel") || (checkText(text, "account") && !talkState[1]))
			{
				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;

				msg << "Do you want to change your 'password', request a 'recovery key', add a 'character', or 'delete' a character?";
			}
			else if(checkText(text, "delete") && talkState[1])
			{
				talkState[1] = false;
				talkState[2] = true;
				msg << "Which character would you like to delete?";
			}
			else if(talkState[2])
			{
				std::string tmp = text;
				trimString(tmp);
				if(!isValidName(tmp, false))
					msg << "That name contains invalid characters, try to say your name again, you might have typed it wrong.";
				else
				{
					talkState[2] = false;
					talkState[3] = true;
					managerString = tmp;
					msg << "Do you really want to delete the character named " << managerString << "?";
				}
			}
			else if(checkText(text, "yes") && talkState[3])
			{
				switch(IOLoginData::getInstance()->deleteCharacter(managerNumber, managerString))
				{
					case DELETE_INTERNAL:
						msg << "An error occured while deleting your character. Either the character does not belong to you or it doesn't exist.";
						break;

					case DELETE_SUCCESS:
						msg << "Your character has been deleted.";
						break;

					case DELETE_HOUSE:
						msg << "Your character owns a house. To make sure you really want to lose your house by deleting your character, you have to login and leave the house or pass it to someone else first.";
						break;

					case DELETE_LEADER:
						msg << "Your character is the leader of a guild. You need to disband or pass the leadership someone else to delete your character.";
						break;

					case DELETE_ONLINE:
						msg << "A character with that name is currently online, to delete a character it has to be offline.";
						break;
				}

				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;
			}
			else if(checkText(text, "no") && talkState[3])
			{
				talkState[1] = true;
				talkState[3] = false;
				msg << "Tell me what character you want to delete.";
			}
			else if(checkText(text, "password") && talkState[1])
			{
				talkState[1] = false;
				talkState[4] = true;
				msg << "Tell me your new password please.";
			}
			else if(talkState[4])
			{
				std::string tmp = text;
				trimString(tmp);
				if(tmp.length() < 6)
					msg << "That password is too short, at least 6 digits are required. Please select a longer password.";
				else if(!isValidPassword(tmp))
					msg << "Your password contains invalid characters... please tell me another one.";
				else
				{
					talkState[4] = false;
					talkState[5] = true;
					managerString = tmp;
					msg << "Should '" << managerString << "' be your new password?";
				}
			}
			else if(checkText(text, "yes") && talkState[5])
			{
				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;

				IOLoginData::getInstance()->setPassword(managerNumber, managerString);
				msg << "Your password has been changed.";
			}
			else if(checkText(text, "no") && talkState[5])
			{
				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;

				msg << "Then not.";
			}
			else if(checkText(text, "character") && talkState[1])
			{
				if(account->getCharacters().size() <= 15)
				{
					talkState[1] = false;
					talkState[6] = true;
					msg << "What would you like as your character name?";
				}
				else
				{
					talkState[1] = true;
					for(int8_t i = 2; i <= 12; i++)
						talkState[i] = false;

					msg << "Your account reach the limit of 15 players, you can 'delete' a character if you want to create a new one.";
				}
			}
			else if(talkState[6])
			{
				managerString = text;
				trimString(managerString);
				if(managerString.length() < 4)
					msg << "Your name you want is too short, please select a longer name.";
				else if(managerString.length() > 20)
					msg << "The name you want is too long, please select a shorter name.";
				else if(!isValidName(managerString))
					msg << "That name seems to contain invalid symbols, please choose another name.";
				else if(IOLoginData::getInstance()->playerExists(managerString, true))
					msg << "A player with that name already exists, please choose another name.";
				else
				{
					std::string tmp = asLowerCaseString(managerString);
					if(tmp.substr(0, 4) != "god " && tmp.substr(0, 3) != "cm " && tmp.substr(0, 3) != "gm ")
					{
						talkState[6] = false;
						talkState[7] = true;
						msg << managerString << ", are you sure?";
					}
					else
						msg << "Your character is not a staff member, please tell me another name!";
				}
			}
			else if(checkText(text, "no") && talkState[7])
			{
				talkState[6] = true;
				talkState[7] = false;
				msg << "What else would you like to name your character?";
			}
			else if(checkText(text, "yes") && talkState[7])
			{
				talkState[7] = false;
				talkState[8] = true;
				msg << "Should your character be a 'male' or a 'female'.";
			}
			else if(talkState[8] && (checkText(text, "female") || checkText(text, "male")))
			{
				talkState[8] = false;
				talkState[9] = true;
				if(checkText(text, "female"))
				{
					msg << "A female, are you sure?";
					managerSex = PLAYERSEX_FEMALE;
				}
				else
				{
					msg << "A male, are you sure?";
					managerSex = PLAYERSEX_MALE;
				}
			}
			else if(checkText(text, "no") && talkState[9])
			{
				talkState[8] = true;
				talkState[9] = false;
				msg << "Tell me... would you like to be a 'male' or a 'female'?";
			}
			else if(checkText(text, "yes") && talkState[9])
			{
				if(server.configManager().getBool(ConfigManager::START_CHOOSEVOC))
				{
					talkState[9] = false;
					talkState[11] = true;

					bool firstPart = true;
					for(VocationsMap::iterator it = Vocations::getInstance()->getFirstVocation(); it != Vocations::getInstance()->getLastVocation(); ++it)
					{
						if(it->first == it->second->getFromVocation() && it->first != 0)
						{
							if(firstPart)
							{
								msg << "What do you want to be... " << it->second->getDescription();
								firstPart = false;
							}
							else if(it->first - 1 != 0)
								msg << ", " << it->second->getDescription();
							else
								msg << " or " << it->second->getDescription() << ".";
						}
					}
				}
				else if(!IOLoginData::getInstance()->playerExists(managerString, true))
				{
					talkState[1] = true;
					for(int8_t i = 2; i <= 12; i++)
						talkState[i] = false;

					if(IOLoginData::getInstance()->createCharacter(managerNumber, managerString, managerNumber2, (uint16_t)managerSex))
						msg << "Your character has been created.";
					else
						msg << "Your character couldn't be created, please try again.";
				}
				else
				{
					talkState[6] = true;
					talkState[9] = false;
					msg << "A player with that name already exists, please choose another name.";
				}
			}
			else if(talkState[11])
			{
				for(VocationsMap::iterator it = Vocations::getInstance()->getFirstVocation(); it != Vocations::getInstance()->getLastVocation(); ++it)
				{
					std::string tmp = asLowerCaseString(it->second->getName());
					if(checkText(text, tmp) && it != Vocations::getInstance()->getLastVocation() && it->first == it->second->getFromVocation() && it->first != 0)
					{
						msg << "So you would like to be " << it->second->getDescription() << "... are you sure?";
						managerNumber2 = it->first;
						talkState[11] = false;
						talkState[12] = true;
					}
				}

				if(msg.str().length() == 17)
					msg << "I don't understand what vocation you would like to be... could you please repeat it?";
			}
			else if(checkText(text, "yes") && talkState[12])
			{
				if(!IOLoginData::getInstance()->playerExists(managerString, true))
				{
					talkState[1] = true;
					for(int8_t i = 2; i <= 12; i++)
						talkState[i] = false;

					if(IOLoginData::getInstance()->createCharacter(managerNumber, managerString, managerNumber2, (uint16_t)managerSex))
						msg << "Your character has been created.";
					else
						msg << "Your character couldn't be created, please try again.";
				}
				else
				{
					talkState[6] = true;
					talkState[9] = false;
					msg << "A player with that name already exists, please choose another name.";
				}
			}
			else if(checkText(text, "no") && talkState[12])
			{
				talkState[11] = true;
				talkState[12] = false;
				msg << "No? Then what would you like to be?";
			}
			else if(checkText(text, "recovery key") && talkState[1])
			{
				talkState[1] = false;
				talkState[10] = true;
				msg << "Would you like a recovery key?";
			}
			else if(checkText(text, "yes") && talkState[10])
			{
				if(account->getRecoveryKey() != "0")
					msg << "Sorry, you already have a recovery key, for security reasons I may not give you a new one.";
				else
				{
					managerString = generateRecoveryKey(4, 4);
					IOLoginData::getInstance()->setRecoveryKey(managerNumber, managerString);
					msg << "Your recovery key is: " << managerString << ".";
				}

				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;
			}
			else if(checkText(text, "no") && talkState[10])
			{
				msg << "Then not.";
				talkState[1] = true;
				for(int8_t i = 2; i <= 12; i++)
					talkState[i] = false;
			}
			else
				msg << "Please read the latest message that I have specified, I don't understand the current requested action.";

			break;
		}
		case MANAGER_NEW:
		{
			if(checkText(text, "account") && !talkState[1])
			{
				msg << "What would you like your password to be?";
				talkState[1] = true;
				talkState[2] = true;
			}
			else if(talkState[2])
			{
				std::string tmp = text;
				trimString(tmp);
				if(tmp.length() < 6)
					msg << "That password is too short, at least 6 digits are required. Please select a longer password.";
				else if(!isValidPassword(tmp))
					msg << "Your password contains invalid characters... please tell me another one.";
				else
				{
					talkState[3] = true;
					talkState[2] = false;
					managerString = tmp;
					msg << managerString << " is it? 'yes' or 'no'?";
				}
			}
			else if(checkText(text, "yes") && talkState[3])
			{
				if(server.configManager().getBool(ConfigManager::GENERATE_ACCOUNT_NUMBER))
				{
					do
						sprintf(managerChar, "%d%d%d%d%d%d%d", random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9), random_range(2, 9));
					while(IOLoginData::getInstance()->accountNameExists(managerChar));

					uint32_t id = (uint32_t)IOLoginData::getInstance()->createAccount(managerChar, managerString);
					if(id)
					{
						accountManager = MANAGER_ACCOUNT;
						managerNumber = id;

						noSwap = talkState[1] = false;
						msg << "Your account has been created, you may manage it now, but remember your account name: '"
							<< managerChar << "' and password: '" << managerString
							<< "'! If the account name is too hard to remember, please note it somewhere.";
					}
					else
						msg << "Your account could not be created, please try again.";

					for(int8_t i = 2; i <= 5; i++)
						talkState[i] = false;
				}
				else
				{
					msg << "What would you like your account name to be?";
					talkState[3] = false;
					talkState[4] = true;
				}
			}
			else if(checkText(text, "no") && talkState[3])
			{
				talkState[2] = true;
				talkState[3] = false;
				msg << "What would you like your password to be then?";
			}
			else if(talkState[4])
			{
				std::string tmp = text;
				trimString(tmp);
				if(tmp.length() < 3)
					msg << "That account name is too short, at least 3 digits are required. Please select a longer account name.";
				else if(tmp.length() > 25)
					msg << "That account name is too long, not more than 25 digits are required. Please select a shorter account name.";
				else if(!isValidAccountName(tmp))
					msg << "Your account name contains invalid characters, please choose another one.";
				else if(asLowerCaseString(tmp) == asLowerCaseString(managerString))
					msg << "Your account name cannot be same as password, please choose another one.";
				else
				{
					sprintf(managerChar, "%s", tmp.c_str());
					msg << managerChar << ", are you sure?";
					talkState[4] = false;
					talkState[5] = true;
				}
			}
			else if(checkText(text, "yes") && talkState[5])
			{
				if(!IOLoginData::getInstance()->accountNameExists(managerChar))
				{
					uint32_t id = (uint32_t)IOLoginData::getInstance()->createAccount(managerChar, managerString);
					if(id)
					{
						accountManager = MANAGER_ACCOUNT;
						managerNumber = id;

						noSwap = talkState[1] = false;
						msg << "Your account has been created, you may manage it now, but remember your account name: '"
							<< managerChar << "' and password: '" << managerString << "'!";
					}
					else
						msg << "Your account could not be created, please try again.";

					for(int8_t i = 2; i <= 5; i++)
						talkState[i] = false;
				}
				else
				{
					msg << "An account with that name already exists, please try another account name.";
					talkState[4] = true;
					talkState[5] = false;
				}
			}
			else if(checkText(text, "no") && talkState[5])
			{
				talkState[5] = false;
				talkState[4] = true;
				msg << "What else would you like as your account name?";
			}
			else if(checkText(text, "recover") && !talkState[6])
			{
				talkState[6] = true;
				talkState[7] = true;
				msg << "What was your account name?";
			}
			else if(talkState[7])
			{
				managerString = text;
				if(IOLoginData::getInstance()->getAccountId(managerString, (uint32_t&)managerNumber))
				{
					talkState[7] = false;
					talkState[8] = true;
					msg << "What was your recovery key?";
				}
				else
				{
					msg << "Sorry, but account with such name doesn't exists.";
					talkState[6] = talkState[7] = false;
				}
			}
			else if(talkState[8])
			{
				managerString2 = text;
				if(IOLoginData::getInstance()->validRecoveryKey(managerNumber, managerString2) && managerString2 != "0")
				{
					sprintf(managerChar, "%s%d", server.configManager().getString(ConfigManager::SERVER_NAME).c_str(), random_range(100, 999));
					IOLoginData::getInstance()->setPassword(managerNumber, managerChar);
					msg << "Correct! Your new password is: " << managerChar << ".";
				}
				else
					msg << "Sorry, but this key doesn't match to account you gave me.";

				talkState[7] = talkState[8] = false;
			}
			else
				msg << "Sorry, but I can't understand you, please try to repeat that.";

			break;
		}
		default:
			return;
			break;
	}

	sendTextMessage(MSG_STATUS_CONSOLE_BLUE, msg.str().c_str());
	if(!noSwap)
		sendTextMessage(MSG_STATUS_CONSOLE_ORANGE, "Hint: Type 'account' to manage your account and if you want to start over then type 'cancel'.");
}

bool Player::isGuildInvited(uint32_t guildId) const
{
	for(InvitedToGuildsList::const_iterator it = invitedToGuildsList.begin(); it != invitedToGuildsList.end(); ++it)
	{
		if((*it) == guildId)
			return true;
	}

	return false;
}

void Player::leaveGuild()
{
	sendClosePrivate(CHANNEL_GUILD);
	guildLevel = GUILDLEVEL_NONE;
	guildId = rankId = 0;
	guildName = rankName = guildNick = "";
}

bool Player::isPremium() const
{
	if(server.configManager().getBool(ConfigManager::FREE_PREMIUM) || hasFlag(PlayerFlag_IsAlwaysPremium))
		return true;

	return premiumDays;
}

bool Player::setGuildLevel(GuildLevel_t newLevel, uint32_t rank/* = 0*/)
{
	std::string name;
	if(!IOGuild::getInstance()->getRankEx(rank, name, guildId, newLevel))
		return false;

	guildLevel = newLevel;
	rankName = name;
	rankId = rank;
	return true;
}

void Player::setGroupId(int32_t newId)
{
	if(Group* tmp = Groups::getInstance()->getGroup(newId))
	{
		groupId = newId;
		group = tmp;
	}
}

void Player::setGroup(Group* newGroup)
{
	if(newGroup)
	{
		group = newGroup;
		groupId = group->getId();
	}
}

PartyShields_t Player::getPartyShield(const Creature* creature) const
{
	const Player* player = creature->getPlayer();
	if(!player)
		return Creature::getPartyShield(creature);

	if(Party* party = getParty())
	{
		if(party->getLeader() == player)
		{
			if(party->isSharedExperienceActive())
			{
				if(party->isSharedExperienceEnabled())
					return SHIELD_YELLOW_SHAREDEXP;

				if(party->canUseSharedExperience(player))
					return SHIELD_YELLOW_NOSHAREDEXP;

				return SHIELD_YELLOW_NOSHAREDEXP_BLINK;
			}

			return SHIELD_YELLOW;
		}

		if(party->isPlayerMember(player))
		{
			if(party->isSharedExperienceActive())
			{
				if(party->isSharedExperienceEnabled())
					return SHIELD_BLUE_SHAREDEXP;

				if(party->canUseSharedExperience(player))
					return SHIELD_BLUE_NOSHAREDEXP;

				return SHIELD_BLUE_NOSHAREDEXP_BLINK;
			}

			return SHIELD_BLUE;
		}

		if(isInviting(player))
			return SHIELD_WHITEBLUE;
	}

	if(player->isInviting(this))
		return SHIELD_WHITEYELLOW;

	return SHIELD_NONE;
}

bool Player::isInviting(const Player* player) const
{
	if(!player || !getParty() || getParty()->getLeader() != this)
		return false;

	return getParty()->isPlayerInvited(player);
}

bool Player::isPartner(const Player* player) const
{
	if(!player || !getParty() || !player->getParty())
		return false;

	return (getParty() == player->getParty());
}

void Player::sendPlayerPartyIcons(Player* player)
{
	sendCreatureShield(player);
	sendCreatureSkull(player);
}

bool Player::addPartyInvitation(Party* party)
{
	if(!party)
		return false;

	PartyList::iterator it = std::find(invitePartyList.begin(), invitePartyList.end(), party);
	if(it != invitePartyList.end())
		return false;

	invitePartyList.push_back(party);
	return true;
}

bool Player::removePartyInvitation(Party* party)
{
	if(!party)
		return false;

	PartyList::iterator it = std::find(invitePartyList.begin(), invitePartyList.end(), party);
	if(it != invitePartyList.end())
	{
		invitePartyList.erase(it);
		return true;
	}
	return false;
}

void Player::clearPartyInvitations()
{
	if(invitePartyList.empty())
		return;

	PartyList list;
	for(PartyList::iterator it = invitePartyList.begin(); it != invitePartyList.end(); ++it)
		list.push_back(*it);

	invitePartyList.clear();
	for(PartyList::iterator it = list.begin(); it != list.end(); ++it)
		(*it)->removeInvite(this);
}

void Player::increaseCombatValues(int32_t& min, int32_t& max, bool useCharges, bool countWeapon)
{
	if(min > 0)
		min = (int32_t)(min * vocation->getMultiplier(MULTIPLIER_HEALING));
	else
		min = (int32_t)(min * vocation->getMultiplier(MULTIPLIER_MAGIC));

	if(max > 0)
		max = (int32_t)(max * vocation->getMultiplier(MULTIPLIER_HEALING));
	else
		max = (int32_t)(max * vocation->getMultiplier(MULTIPLIER_MAGIC));

	Item* weapon = nullptr;
	if(!countWeapon)
		weapon = getWeapon();

	Item* item = nullptr;
	int32_t minValue = 0, maxValue = 0;
	for(int32_t i = +slots_t::FIRST; i < +slots_t::LAST; ++i)
	{
		if(!(item = getInventoryItem((slots_t)i)) || (server.moveEvents().hasEquipEvent(item)
			&& !isItemAbilityEnabled((slots_t)i)))
			continue;

		ItemKindPC kind = item->getKind();
		if(min > 0)
		{
			minValue += kind->abilities.increment[HEALING_VALUE];
			if(kind->abilities.increment[HEALING_PERCENT])
				min = (int32_t)std::ceil((double)(min * kind->abilities.increment[HEALING_PERCENT]) / 100.);
		}
		else
		{
			minValue -= kind->abilities.increment[MAGIC_VALUE];
			if(kind->abilities.increment[MAGIC_PERCENT])
				min = (int32_t)std::ceil((double)(min * kind->abilities.increment[MAGIC_PERCENT]) / 100.);
		}

		if(max > 0)
		{
			maxValue += kind->abilities.increment[HEALING_VALUE];
			if(kind->abilities.increment[HEALING_PERCENT])
				max = (int32_t)std::ceil((double)(max * kind->abilities.increment[HEALING_PERCENT]) / 100.);
		}
		else
		{
			maxValue -= kind->abilities.increment[MAGIC_VALUE];
			if(kind->abilities.increment[MAGIC_PERCENT])
				max = (int32_t)std::ceil((double)(max * kind->abilities.increment[MAGIC_PERCENT]) / 100.);
		}

		bool removeCharges = false;
		for(int32_t j = INCREMENT_FIRST; j <= INCREMENT_LAST; ++j)
		{
			if(!kind->abilities.increment[(Increment_t)j])
				continue;

			removeCharges = true;
			break;
		}

		if(useCharges && removeCharges && item != weapon && item->hasCharges())
			server.game().transformItem(item, item->getId(), std::max((int32_t)0, (int32_t)item->getCharges() - 1));
	}

	min += minValue;
	max += maxValue;
}

bool Player::transferMoneyTo(const std::string& name, uint64_t amount)
{
	if(!server.configManager().getBool(ConfigManager::BANK_SYSTEM) || amount > balance)
		return false;

	PlayerP target = server.game().getPlayerByNameEx(name);
	if(!target)
		return false;

	balance -= amount;
	target->balance += amount;
	if(target->isVirtual())
	{
		IOLoginData::getInstance()->savePlayer(target.get());
	}

	return true;
}

void Player::sendCritical() const
{
	if(server.configManager().getBool(ConfigManager::DISPLAY_CRITICAL_HIT))
		server.game().addAnimatedText(getPosition(), TEXTCOLOR_DARKRED, "CRITICAL!");
}

void Player::setFlags(uint64_t flags) {if(group) group->setFlags(flags);}
bool Player::hasFlag(PlayerFlags value) const {return group != nullptr && group->hasFlag(value);}
void Player::setCustomFlags(uint64_t flags) {if(group) group->setCustomFlags(flags);}
bool Player::hasCustomFlag(PlayerCustomFlags value) const {return group != nullptr && group->hasCustomFlag(value);}


bool Player::hasSomethingToThinkAbout() const {
	return true;
}


uint16_t Player::getAccess() const {return group ? group->getAccess() : 0;}
uint16_t Player::getGhostAccess() const {return group ? group->getGhostAccess() : 0;}

void Player::disconnect() {if(client) client->disconnect();}


void Player::sendAddTileItem(const Tile* tile, const Position& pos, const Item* item, const char* callSource) {
	if (client == nullptr) {
		return;
	}

	int32_t index = tile->getClientIndexOfThing(this, item);
	if (index < 0) {
		return;
	}

	client->sendAddTileItem(tile, StackPosition(pos, index), item, callSource);
}


void Player::sendUpdateTileItem(const Tile* tile, const Position& pos, const Item* oldItem, const Item* newItem) {
	if (client == nullptr) {
		return;
	}

	int32_t index = tile->getClientIndexOfThing(this, newItem);
	if (index < 0) {
		return;
	}

	client->sendUpdateTileItem(tile, StackPosition(pos, index), newItem);
}


void Player::sendRemoveTileItem(const Tile* tile, const StackPosition& position, const Item* item, const char* callSource) {
	if (client == nullptr) {
		return;
	}

	client->sendRemoveTileItem(tile, position, item, callSource);
}


void Player::sendUpdateTile(const Tile* tile, const Position& pos)
	{if(client) client->sendUpdateTile(tile, pos);}

void Player::sendChannelMessage(std::string author, std::string text, SpeakClasses type, uint8_t channel)
	{if(client) client->sendChannelMessage(author, text, type, channel);}
void Player::sendCreatureAppear(const Creature* creature, const char* callSource)
	{if(client) client->sendAddCreature(creature, StackPosition(creature->getPosition(), creature->getTile()->getClientIndexOfThing(
		this, creature)), callSource);}
void Player::sendCreatureDisappear(const Creature* creature, uint32_t stackpos, const char* callSource)
	{if(client) client->sendRemoveCreature(creature, StackPosition(creature->getPosition(), stackpos), callSource);}
void Player::sendCreatureMove(const Creature* creature, const Tile* newTile, const Position& newPos,
	const Tile* oldTile, const Position& oldPos, uint32_t oldStackpos, bool teleport, const char* callSource)
	{if(client) client->sendMoveCreature(creature, newTile, StackPosition(newPos, newTile->getClientIndexOfThing(
		this, creature)), oldTile, StackPosition(oldPos, oldStackpos), teleport, callSource);}

void Player::sendCreatureTurn(const Creature* creature)
	{if(client) client->sendCreatureTurn(creature, StackPosition(creature->getPosition(), creature->getTile()->getClientIndexOfThing(this, creature)));}
void Player::sendCreatureSay(const Creature* creature, SpeakClasses type, const std::string& text, Position* pos)
	{if(client) client->sendCreatureSay(creature, type, text, pos);}
void Player::sendCreatureSquare(const Creature* creature, SquareColor_t color)
	{if(client) client->sendCreatureSquare(creature, color);}
void Player::sendCreatureChangeOutfit(const Creature* creature, const Outfit_t& outfit)
	{if(client) client->sendCreatureOutfit(creature, outfit);}
void Player::sendCreatureLight(const Creature* creature)
	{if(client) client->sendCreatureLight(creature);}
void Player::sendCreatureShield(const Creature* creature)
	{if(client) client->sendCreatureShield(creature);}

//container
void Player::sendContainer(uint32_t cid, const Container* container, bool hasParent)
	{if(client) client->sendContainer(cid, container, hasParent);}

//inventory
void Player::sendAddInventoryItem(slots_t slot, const Item* item)
	{if(client) client->sendAddInventoryItem(slot, item);}
void Player::sendUpdateInventoryItem(slots_t slot, const Item* oldItem, const Item* newItem)
	{if(client) client->sendUpdateInventoryItem(slot, newItem);}
void Player::sendRemoveInventoryItem(slots_t slot, const Item* item)
	{if(client) client->sendRemoveInventoryItem(slot);}

void Player::sendAnimatedText(const Position& pos, uint8_t color, std::string text) const
	{if(client) client->sendAnimatedText(pos,color,text);}
void Player::sendCancel(const std::string& msg) const
	{if(client) client->sendCancel(msg);}
void Player::sendCancelTarget() const
	{if(client) client->sendCancelTarget();}
void Player::sendCancelWalk() const
	{if(client) client->sendCancelWalk();}
void Player::sendChangeSpeed(const Creature* creature, uint32_t newSpeed) const
	{if(client) client->sendChangeSpeed(creature, newSpeed);}
void Player::sendCreatureHealth(const Creature* creature) const
	{if(client) client->sendCreatureHealth(creature);}
void Player::sendDistanceShoot(const Position& from, const Position& to, uint8_t type) const
	{if(client) client->sendDistanceShoot(from, to, type);}
void Player::sendOutfitWindow() const {if(client) client->sendOutfitWindow();}
void Player::sendQuests() const {if(client) client->sendQuests();}
void Player::sendQuestInfo(Quest* quest) const {if(client) client->sendQuestInfo(quest);}
void Player::sendCreatureSkull(const Creature* creature) const
	{if(client) client->sendCreatureSkull(creature);}
void Player::sendFYIBox(std::string message)
	{if(client) client->sendFYIBox(message);}
void Player::sendCreatePrivateChannel(uint16_t channelId, const std::string& channelName)
	{if(client) client->sendCreatePrivateChannel(channelId, channelName);}
void Player::sendClosePrivate(uint16_t channelId) const
	{if(client) client->sendClosePrivate(channelId);}
void Player::sendMagicEffect(const Position& pos, uint8_t type) const
	{if(client) client->sendMagicEffect(pos, type);}
void Player::sendSkills() const
	{if(client) client->sendSkills();}
void Player::sendTextMessage(MessageClasses type, const std::string& message) const
	{if(client) client->sendTextMessage(type, message);}
void Player::sendReLoginWindow() const
	{if(client) client->sendReLoginWindow();}
void Player::sendTextWindow(Item* item, uint16_t maxLen, bool canWrite) const
	{if(client) client->sendTextWindow(windowTextId, item, maxLen, canWrite);}
void Player::sendTextWindow(uint32_t itemId, const std::string& text) const
	{if(client) client->sendTextWindow(windowTextId, itemId, text);}
void Player::sendToChannel(Creature* creature, SpeakClasses type, const std::string& text, uint16_t channelId, uint32_t time) const
	{if(client) client->sendToChannel(creature, type, text, channelId, time);}
void Player::sendShop() const
	{if(client) client->sendShop(shopOffer);}
void Player::sendGoods() const
	{if(client) client->sendGoods(shopOffer);}
void Player::sendCloseShop() const
	{if(client) client->sendCloseShop();}
void Player::sendTradeItemRequest(const Player* player, const Item* item, bool ack) const
	{if(client) client->sendTradeItemRequest(player, item, ack);}
void Player::sendTradeClose() const
	{if(client) client->sendCloseTrade();}
void Player::sendWorldLight(LightInfo& lightInfo)
	{if(client) client->sendWorldLight(lightInfo);}
void Player::sendChannelsDialog()
	{if(client) client->sendChannelsDialog();}
void Player::sendOpenPrivateChannel(const std::string& receiver)
	{if(client) client->sendOpenPrivateChannel(receiver);}
void Player::sendOutfitWindow()
	{if(client) client->sendOutfitWindow();}
void Player::sendCloseContainer(uint32_t cid)
	{if(client) client->sendCloseContainer(cid);}
void Player::sendChannel(uint16_t channelId, const std::string& channelName)
	{if(client) client->sendChannel(channelId, channelName);}
void Player::sendRuleViolationsChannel(uint16_t channelId)
	{if(client) client->sendRuleViolationsChannel(channelId);}
void Player::sendRemoveReport(const std::string& name)
	{if(client) client->sendRemoveReport(name);}
void Player::sendLockRuleViolation()
	{if(client) client->sendLockRuleViolation();}
void Player::sendRuleViolationCancel(const std::string& name)
	{if(client) client->sendRuleViolationCancel(name);}
void Player::sendTutorial(uint8_t tutorialId)
	{if(client) client->sendTutorial(tutorialId);}
void Player::sendAddMarker(const Position& pos, MapMarks_t markType, const std::string& desc)
	{if (client) client->sendAddMarker(pos, markType, desc);}

void Player::updateBaseSpeed()
{
	if(!hasFlag(PlayerFlag_SetMaxSpeed))
		baseSpeed = vocation->getBaseSpeed() + (2 * (level - 1));
	else
		baseSpeed = SPEED_MAX;
}

uint32_t Player::getVocAttackSpeed() const {return vocation->getAttackSpeed();}
