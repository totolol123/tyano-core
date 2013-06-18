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
#include "const.h"

#include "spells.h"
#include "tools.h"

#include "house.h"
#include "housetile.h"
#include "combat.h"

#include "condition.h"
#include "creatureevent.h"
#include "items.h"
#include "monster.h"
#include "monsters.h"
#include "configmanager.h"
#include "game.h"
#include "player.h"
#include "server.h"


LOGGER_DEFINITION(Spells);


Spells::Spells()
	: m_interface("Spell Interface")
{}


ReturnValue Spells::onPlayerSay(Player* player, const std::string& words)
{
	std::string reWords = words;
	trimString(reWords);

	InstantSpellP instantSpell = getInstantSpell(reWords);
	if(!instantSpell)
		return RET_NOTPOSSIBLE;

	size_t size = instantSpell->getWords().length();
	std::string param = reWords.substr(size, reWords.length() - size), reParam;
	if(instantSpell->getHasParam() && !param.empty() && param[0] == ' ')
	{
		size_t quote = param.find('"', 1);
		if(quote != std::string::npos)
		{
			size_t tmp = param.find('"', quote + 1);
			if(tmp == std::string::npos)
				tmp = param.length();

			reParam = param.substr(quote + 1, tmp - quote - 1);
		}
		else if(param.find(' ', 1) == std::string::npos)
			reParam = param.substr(1, param.length());

		trimString(reParam);
	}

	if(!instantSpell->playerCastInstant(player, reParam))
		return RET_NEEDEXCHANGE;

	SpeakClasses type = SPEAK_SAY;
	if(server.configManager().getBool(ConfigManager::EMOTE_SPELLS))
		type = SPEAK_MONSTER_SAY;

	if(!server.configManager().getBool(ConfigManager::SPELL_NAME_INSTEAD_WORDS))
		return server.game().internalCreatureSay(player, type, reWords, player->isGhost()) ?
			RET_NOERROR : RET_NOTPOSSIBLE;

	std::string ret = instantSpell->getName();
	if(param.length())
	{
		trimString(param);
		size_t tmp = 0, rtmp = param.length();
		if(param[0] == '"')
			tmp = 1;

		if(param[rtmp] == '"')
			rtmp -= 1;

		ret += ": " + param.substr(tmp, rtmp);
	}

	return server.game().internalCreatureSay(player, type, ret, player->isGhost()) ?
		RET_NOERROR : RET_NOTPOSSIBLE;
}

void Spells::clear()
{
	runes.clear();
	instants.clear();
	m_interface.reInitState();
}

SpellP Spells::getEvent(const std::string& nodeName)
{
	std::string tmpNodeName = asLowerCaseString(nodeName);
	if(tmpNodeName == "rune")
		return std::make_shared<RuneSpell>(&m_interface);

	if(tmpNodeName == "instant")
		return std::make_shared<InstantSpell>(&m_interface);

	if(tmpNodeName == "conjure")
		return std::make_shared<ConjureSpell>(&m_interface);

	return nullptr;
}

bool Spells::registerEvent(const SpellP& event, xmlNodePtr p, bool override)
{
	if(InstantSpellP instant = std::dynamic_pointer_cast<InstantSpell>(event))
	{
		InstantSpellMap::iterator it = instants.find(instant->getWords());
		if(it == instants.end())
		{
			instants[instant->getWords()] = instant;
			return true;
		}

		if(override)
		{
			it->second = instant;
			return true;
		}

		LOGw("[Spells::registerEvent] Duplicate registered instant spell with words: " << instant->getWords());
		return false;
	}

	if(RuneSpellP rune = std::dynamic_pointer_cast<RuneSpell>(event))
	{
		RuneSpellMap::iterator it = runes.find(rune->getRuneItemId());
		if(it == runes.end())
		{
			runes[rune->getRuneItemId()] = rune;
			return true;
		}

		if(override)
		{
			it->second = rune;
			return true;
		}

		LOGw("[Spells::registerEvent] Duplicate registered rune with id: " << rune->getRuneItemId());
		return false;
	}

	return false;
}

SpellP Spells::getSpellByName(const std::string& name)
{
	SpellP spell;
	if((spell = getRuneSpellByName(name)) || (spell = getInstantSpellByName(name)))
		return spell;

	return nullptr;
}

RuneSpellP Spells::getRuneSpell(uint32_t id)
{
	RuneSpellMap::iterator it = runes.find(id);
	if(it != runes.end())
		return it->second;

	return nullptr;
}

RuneSpellP Spells::getRuneSpellByName(const std::string& name)
{
	for(RuneSpellMap::iterator it = runes.begin(); it != runes.end(); ++it)
	{
		if(boost::iequals(it->second->getName(), name))
			return it->second;
	}

	return nullptr;
}

InstantSpellP Spells::getInstantSpell(const std::string words)
{
	InstantSpellP result = nullptr;
	for(InstantSpellMap::iterator it = instants.begin(); it != instants.end(); ++it)
	{
		InstantSpellP instantSpell = it->second;
		if(strncasecmp(instantSpell->getWords().c_str(), words.c_str(), instantSpell->getWords().length()) == 0)
		{
			if(!result || instantSpell->getWords().length() > result->getWords().length())
				result = instantSpell;
		}
	}

	if(result && words.length() > result->getWords().length())
	{
		std::string param = words.substr(result->getWords().length(), words.length());
		if(param[0] != ' ' || (param.length() > 1 && (!result->getHasParam() || param.find(' ', 1) != std::string::npos) && param[1] != '"'))
			return nullptr;
	}

	return result;
}

uint32_t Spells::getInstantSpellCount(const Player* player)
{
	uint32_t count = 0;
	for(InstantSpellMap::iterator it = instants.begin(); it != instants.end(); ++it)
	{
		if(it->second->canCast(player))
			++count;
	}

	return count;
}

InstantSpellP Spells::getInstantSpellByIndex(const Player* player, uint32_t index)
{
	uint32_t count = 0;
	for(InstantSpellMap::iterator it = instants.begin(); it != instants.end(); ++it)
	{
		InstantSpellP instantSpell = it->second;
		if(instantSpell->canCast(player))
		{
			if(count == index)
				return instantSpell;

			++count;
		}
	}

	return nullptr;
}

InstantSpellP Spells::getInstantSpellByName(const std::string& name)
{
	for(InstantSpellMap::iterator it = instants.begin(); it != instants.end(); ++it)
	{
		if(boost::iequals(it->second->getName(), name))
			return it->second;
	}

	return nullptr;
}

Position Spells::getCasterPosition(const Creature* creature, Direction dir)
{
	return getNextPosition(dir, creature->getPosition());
}

bool BaseSpell::castSpell(Creature* creature)
{
	if(!creature)
		return false;

	bool success = true;
	CreatureEventList castEvents = creature->getCreatureEvents(CREATURE_EVENT_CAST);
	for(CreatureEventList::iterator it = castEvents.begin(); it != castEvents.end(); ++it)
	{
		if(!(*it)->executeCast(creature) && success)
			success = false;
	}

	return success;
}

bool BaseSpell::castSpell(Creature* creature, Creature* target)
{
	if(!creature || !target)
		return false;

	bool success = true;
	CreatureEventList castEvents = creature->getCreatureEvents(CREATURE_EVENT_CAST);
	for(CreatureEventList::iterator it = castEvents.begin(); it != castEvents.end(); ++it)
	{
		if(!(*it)->executeCast(creature, target) && success)
			success = false;
	}

	return success;
}



LOGGER_DEFINITION(CombatSpell);


CombatSpell::CombatSpell(Combat* _combat, bool _needTarget, bool _needDirection) :
	Event(&server.spells().getInterface())
{
	combat =_combat;
	needTarget = _needTarget;
	needDirection = _needDirection;
}

CombatSpell::~CombatSpell()
{
	if(combat)
		delete combat;
}

bool CombatSpell::loadScriptCombat()
{
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		combat = env->getCombatObject(env->getLastCombatId());

		env->resetCallback();
		m_interface->releaseEnv();
	}

	return combat != nullptr;
}

bool CombatSpell::castSpell(Creature* creature)
{
	if(!BaseSpell::castSpell(creature))
		return false;

	if(isScripted())
	{
		LuaVariant var;
		var.type = VARIANT_POSITION;
		if(needDirection)
			var.pos = StackPosition(Spells::getCasterPosition(creature, creature->getDirection()), 0);
		else
			var.pos = StackPosition(creature->getPosition(), 0);

		return executeCastSpell(creature, var);
	}

	Position pos;
	if(needDirection)
		pos = Spells::getCasterPosition(creature, creature->getDirection());
	else
		pos = creature->getPosition();

	combat->doCombat(creature, pos);
	return true;
}

bool CombatSpell::castSpell(Creature* creature, Creature* target)
{
	if(!BaseSpell::castSpell(creature, target))
		return false;

	if(isScripted())
	{
		LuaVariant var;
		if(combat->hasArea())
		{
			var.type = VARIANT_POSITION;

			if(needTarget)
				var.pos = StackPosition(target->getPosition(), 0);
			else if(needDirection)
				var.pos = StackPosition(Spells::getCasterPosition(creature, creature->getDirection()), 0);
			else
				var.pos = StackPosition(creature->getPosition(), 0);
		}
		else
		{
			var.type = VARIANT_NUMBER;
			var.number = target->getID();
		}

		return executeCastSpell(creature, var);
	}

	if(combat->hasArea())
	{
		if(!needTarget)
			return castSpell(creature);

		combat->doCombat(creature, target->getPosition());
	}
	else
		combat->doCombat(creature, target);

	return true;
}

bool CombatSpell::executeCastSpell(Creature* creature, const LuaVariant& var)
{
	//onCastSpell(cid, var)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			env->streamVariant(scriptstream, "var", var);

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());
			lua_State* L = m_interface->getState();

			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));
			m_interface->pushVariant(L, var);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[CombatSpell::executeCastSpell] Call stack overflow.");
		return false;
	}
}



LOGGER_DEFINITION(Spell);


Spell::Spell()
{
	level = 0;
	magLevel = 0;
	mana = 0;
	manaPercent = 0;
	soul = 0;
	range = -1;
	exhaustion = 1000;
	needTarget = false;
	needWeapon = false;
	selfTarget = false;
	blockingSolid = false;
	blockingCreature = false;
	enabled = true;
	premium = false;
	isAggressive = true;
	learnable = false;
}

bool Spell::configureSpell(xmlNodePtr p)
{
	int32_t intValue;
	std::string strValue;
	if(readXMLString(p, "name", strValue))
	{
		name = strValue;
		const char* reservedList[] =
		{
			"melee", "physical", "poison", "earth", "fire", "ice", "freeze", "energy", "drown", "death", "curse", "holy",
			"lifedrain", "manadrain", "healing", "speed", "outfit", "invisible", "drunk", "firefield", "poisonfield",
			"energyfield", "firecondition", "poisoncondition", "energycondition", "drowncondition", "freezecondition",
			"cursecondition"
		};

		for(uint32_t i = 0; i < sizeof(reservedList) / sizeof(const char*); ++i)
		{
			if(boost::iequals(reservedList[i], name))
			{
				LOGe("[Spell::configureSpell] Spell is using a reserved name: " << reservedList[i]);
				return false;
			}
		}
	}
	else
	{
		LOGe("[Spell::configureSpell] Spell without name.");
		return false;
	}

	if(readXMLInteger(p, "lvl", intValue) || readXMLInteger(p, "level", intValue))
	 	level = intValue;

	if(readXMLInteger(p, "maglv", intValue) || readXMLInteger(p, "magiclevel", intValue))
	 	magLevel = intValue;

	if(readXMLInteger(p, "mana", intValue))
	 	mana = intValue;

	if(readXMLInteger(p, "manapercent", intValue))
	 	manaPercent = intValue;

	if(readXMLInteger(p, "soul", intValue))
	 	soul = intValue;

	if(readXMLInteger(p, "exhaustion", intValue))
		exhaustion = intValue;

	if(readXMLString(p, "enabled", strValue))
		enabled = booleanString(strValue);

	if(readXMLString(p, "prem", strValue) || readXMLString(p, "premium", strValue))
		premium = booleanString(strValue);

	if(readXMLString(p, "needtarget", strValue))
		needTarget = booleanString(strValue);

	if(readXMLString(p, "needweapon", strValue))
		needWeapon = booleanString(strValue);

	if(readXMLString(p, "selftarget", strValue))
		selfTarget = booleanString(strValue);

	if(readXMLString(p, "needlearn", strValue))
		learnable = booleanString(strValue);

	if(readXMLInteger(p, "range", intValue))
		range = intValue;

	if(readXMLString(p, "blocking", strValue))
		blockingCreature = blockingSolid = booleanString(strValue);

	if(readXMLString(p, "blocktype", strValue))
	{
		std::string tmpStrValue = asLowerCaseString(strValue);
		if(tmpStrValue == "all")
			blockingCreature = blockingSolid = true;
		else if(tmpStrValue == "solid")
			blockingSolid = true;
		else if(tmpStrValue == "creature")
			blockingCreature = true;
		else
			LOGw("[Spell::configureSpell] Blocktype \"" <<strValue << "\" does not exist.");
	}

	if(readXMLString(p, "aggressive", strValue))
		isAggressive = booleanString(strValue);

	std::string error = "";
	xmlNodePtr vocationNode = p->children;
	while(vocationNode)
	{
		if(!parseVocationNode(vocationNode, vocSpellMap, vocStringVec, error))
			LOGw("[Spell::configureSpell] " << error);

		vocationNode = vocationNode->next;
	}

	return true;
}

bool Spell::playerSpellCheck(Player* player) const
{
	if(player->hasFlag(PlayerFlag_CannotUseSpells))
		return false;

	if(player->hasFlag(PlayerFlag_IgnoreSpellCheck))
		return true;

	if(!isEnabled())
		return false;

	bool exhausted = false;
	if(isAggressive)
	{
		if(!player->hasFlag(PlayerFlag_IgnoreProtectionZone) && player->getZone() == ZONE_PROTECTION)
		{
			player->sendCancelMessage(RET_ACTIONNOTPERMITTEDINPROTECTIONZONE);
			return false;
		}

		if(player->hasCondition(CONDITION_EXHAUST, EXHAUST_COMBAT))
			exhausted = true;
	}
	else if(player->hasCondition(CONDITION_EXHAUST, EXHAUST_HEALING))
		exhausted = true;

	if(exhausted && !player->hasFlag(PlayerFlag_HasNoExhaustion))
	{
		player->sendCancelMessage(RET_YOUAREEXHAUSTED);
		if(isInstant())
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);

		return false;
	}

	if(isPremium() && !player->isPremium())
	{
		player->sendCancelMessage(RET_YOUNEEDPREMIUMACCOUNT);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if((int32_t)player->getLevel() < level)
	{
		player->sendCancelMessage(RET_NOTENOUGHLEVEL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if((int32_t)player->getMagicLevel() < magLevel)
	{
		player->sendCancelMessage(RET_NOTENOUGHMAGICLEVEL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(player->getMana() < getManaCost(player) && !player->hasFlag(PlayerFlag_HasInfiniteMana))
	{
		player->sendCancelMessage(RET_NOTENOUGHMANA);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(player->getSoul() < soul && !player->hasFlag(PlayerFlag_HasInfiniteSoul))
	{
		player->sendCancelMessage(RET_NOTENOUGHSOUL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(isInstant() && isLearnable() && !player->hasLearnedInstantSpell(getName()))
	{
		player->sendCancelMessage(RET_YOUNEEDTOLEARNTHISSPELL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!vocSpellMap.empty())
	{
		if(vocSpellMap.find(player->getVocationId()) == vocSpellMap.end())
		{
			player->sendCancelMessage(RET_YOURVOCATIONCANNOTUSETHISSPELL);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	if(needWeapon)
	{
		switch(player->getWeaponType())
		{
			case WEAPON_SWORD:
			case WEAPON_CLUB:
			case WEAPON_AXE:
			case WEAPON_FIST:
				break;

			default:
			{
				player->sendCancelMessage(RET_YOUNEEDAWEAPONTOUSETHISSPELL);
				server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
				return false;
			}
		}
	}

	return true;
}

bool Spell::playerInstantSpellCheck(Player* player, Creature* creature)
{
	if(!playerSpellCheck(player))
		return false;

	const Position& toPos = creature->getPosition();
	const Position& playerPos = player->getPosition();
	if(playerPos.z > toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(playerPos.z < toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Tile* tile = server.game().getTile(toPos);
	if(!tile)
	{
		return false;
	}

	ReturnValue ret;
	if((ret = Combat::canDoCombat(player, tile, isAggressive)) != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(blockingCreature && creature)
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(blockingSolid && tile->hasProperty(BLOCKSOLID))
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!needTarget)
	{
		if(!isAggressive || player->getSkull() != SKULL_BLACK)
			return true;

		player->sendCancelMessage(RET_YOUMAYNOTCASTAREAONBLACKSKULL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!creature)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Player* targetPlayer = creature->getPlayer();
	if(!isAggressive || !targetPlayer || Combat::isInPvpZone(player, targetPlayer)
		|| player->getSkullClient(targetPlayer) != SKULL_NONE)
		return true;

	if(player->getSecureMode() == SECUREMODE_ON)
	{
		player->sendCancelMessage(RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(player->getSkull() == SKULL_BLACK)
	{
		player->sendCancelMessage(RET_YOUMAYNOTATTACKTHISPLAYER);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	return true;
}

bool Spell::playerInstantSpellCheck(Player* player, const Position& toPos)
{
	if(!playerSpellCheck(player))
		return false;

	const Position& playerPos = player->getPosition();
	if(playerPos.z > toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(playerPos.z < toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Tile* tile = server.game().getTile(toPos);
	if(!tile)
	{
		return false;
	}

	ReturnValue ret;
	if((ret = Combat::canDoCombat(player, tile, isAggressive)) != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(blockingCreature && tile->getTopVisibleCreature(player))
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(blockingSolid && tile->hasProperty(BLOCKSOLID))
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(player->getSkull() == SKULL_BLACK && isAggressive && range == -1) //-1 is (usually?) an area spell
	{
		player->sendCancelMessage(RET_YOUMAYNOTCASTAREAONBLACKSKULL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	return true;
}

bool Spell::playerRuneSpellCheck(Player* player, const ExtendedPosition& destination)
{
	if(!playerSpellCheck(player))
		return false;

	if (!destination.hasPosition(true))
		return true;

	Position toPos = destination.getPosition(true);

	const Position& playerPos = player->getPosition();
	if(playerPos.z > toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGOUPSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(playerPos.z < toPos.z)
	{
		player->sendCancelMessage(RET_FIRSTGODOWNSTAIRS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Tile* tile = server.game().getTile(toPos);
	if(!tile)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(range != -1 && !server.game().canThrowObjectTo(playerPos, toPos, true, range, range))
	{
		player->sendCancelMessage(RET_DESTINATIONOUTOFREACH);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	ReturnValue ret;
	if((ret = Combat::canDoCombat(player, tile, isAggressive)) != RET_NOERROR)
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Creature* targetCreature = tile->getTopVisibleCreature(player);
	if(blockingCreature && targetCreature)
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(blockingSolid && tile->hasProperty(BLOCKSOLID))
	{
		player->sendCancelMessage(RET_NOTENOUGHROOM);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!needTarget)
	{
		if(!isAggressive || player->getSkull() != SKULL_BLACK)
			return true;

		player->sendCancelMessage(RET_YOUMAYNOTCASTAREAONBLACKSKULL);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!targetCreature)
	{
		player->sendCancelMessage(RET_CANONLYUSETHISRUNEONCREATURES);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Player* targetPlayer = targetCreature->getPlayer();
	if(!isAggressive || !targetPlayer || Combat::isInPvpZone(player, targetPlayer)
		|| player->getSkullClient(targetPlayer) != SKULL_NONE)
		return true;

	if(player->getSecureMode() == SECUREMODE_ON)
	{
		player->sendCancelMessage(RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(player->getSkull() == SKULL_BLACK)
	{
		player->sendCancelMessage(RET_YOUMAYNOTATTACKTHISPLAYER);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	return true;
}

void Spell::postCastSpell(Player* player, bool finishedCast /*= true*/, bool payCost /*= true*/) const
{
	if(finishedCast)
	{
		if(!player->hasFlag(PlayerFlag_HasNoExhaustion) && exhaustion > 0)
			player->addExhaust(exhaustion, isAggressive ? EXHAUST_COMBAT : EXHAUST_HEALING);

		if(isAggressive && !player->hasFlag(PlayerFlag_NotGainInFight))
			player->addInFightTicks();
	}

	if(payCost)
		postCastSpell(player, (uint32_t)getManaCost(player), (uint32_t)getSoulCost());
}

void Spell::postCastSpell(Player* player, uint32_t manaCost, uint32_t soulCost) const
{
	if(manaCost > 0)
	{
		player->changeMana(-(int32_t)manaCost);
		if(!player->hasFlag(PlayerFlag_NotGainMana) && (player->getZone() != ZONE_PVP
			|| server.configManager().getBool(ConfigManager::PVPZONE_ADDMANASPENT)))
			player->addManaSpent(manaCost);
	}

	if(soulCost > 0)
		player->changeSoul(-(int32_t)soulCost);
}

int32_t Spell::getManaCost(const Player* player) const
{
	if(player && manaPercent)
		return (int32_t)std::floor(double(player->getMaxMana() * manaPercent) / 100);

	return mana;
}

ReturnValue Spell::CreateIllusion(Creature* creature, const Outfit_t outfit, int32_t time)
{
	ConditionOutfit* outfitCondition = new ConditionOutfit(CONDITIONID_COMBAT, CONDITION_OUTFIT, time, false, 0);
	if(!outfitCondition)
		return RET_NOTPOSSIBLE;

	outfitCondition->addOutfit(outfit);
	creature->addCondition(outfitCondition);
	return RET_NOERROR;
}

ReturnValue Spell::CreateIllusion(Creature* creature, const std::string& name, int32_t time)
{
	uint32_t mId = server.monsters().getIdByName(name);
	if(!mId)
		return RET_CREATUREDOESNOTEXIST;

	const MonsterType* mType = server.monsters().getMonsterType(mId);
	if(!mType)
		return RET_CREATUREDOESNOTEXIST;

	Player* player = creature->getPlayer();
	if(player && !player->hasFlag(PlayerFlag_CanIllusionAll) && !mType->isIllusionable)
		return RET_NOTPOSSIBLE;

	return CreateIllusion(creature, mType->outfit, time);
}

ReturnValue Spell::CreateIllusion(Creature* creature, uint32_t itemId, int32_t time)
{
	ItemKindPC kind = server.items()[itemId];
	if(!kind)
		return RET_NOTPOSSIBLE;

	Outfit_t outfit;
	outfit.lookTypeEx = itemId;
	return CreateIllusion(creature, outfit, time);
}



LOGGER_DEFINITION(InstantSpell);


InstantSpell::InstantSpell(LuaScriptInterface* _interface) : TalkAction(_interface)
{
	needDirection = false;
	hasParam = false;
	checkLineOfSight = true;
	casterTargetOrDirection = false;
	limitRange = 0;
	function = nullptr;
}

bool InstantSpell::configureEvent(xmlNodePtr p)
{
	if(!Spell::configureSpell(p))
		return false;

	if(!TalkAction::configureEvent(p))
		return false;

	std::string strValue;
	if(readXMLString(p, "param", strValue) || readXMLString(p, "params", strValue))
 		hasParam = booleanString(strValue);

	if(readXMLString(p, "direction", strValue))
		needDirection = booleanString(strValue);

	if(readXMLString(p, "casterTargetOrDirection", strValue))
		casterTargetOrDirection = booleanString(strValue);

	if(readXMLString(p, "blockwalls", strValue))
		checkLineOfSight = booleanString(strValue);

	int32_t intValue;
	if(readXMLInteger(p, "limitRange", intValue))
		limitRange = intValue;

	return true;
}

bool InstantSpell::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "summonmonster")
		function = SummonMonster;
	else if(tmpFunctionName == "searchplayer")
	{
		isAggressive = false;
		function = SearchPlayer;
	}
	else if(tmpFunctionName == "levitate")
	{
		isAggressive = false;
		function = Levitate;
	}
	else if(tmpFunctionName == "illusion")
	{
		isAggressive = false;
		function = Illusion;
	}
	else
	{
		LOGw("[InstantSpell::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

bool InstantSpell::playerCastInstant(Player* player, const std::string& param)
{
	LuaVariant var;
	if(selfTarget)
	{
		var.type = VARIANT_NUMBER;
		var.number = player->getID();
		if(!playerInstantSpellCheck(player, player))
			return false;
	}
	else if(needTarget || casterTargetOrDirection)
	{
		Creature* target = nullptr;
		bool useDirection = false;
		if(hasParam)
		{
			Player* targetPlayer = nullptr;
			ReturnValue ret = server.game().getPlayerByNameWildcard(param, targetPlayer);

			target = targetPlayer;
			if(limitRange && target && !Position::areInRange(Position(limitRange, limitRange, 0), target->getPosition(), player->getPosition()))
				useDirection = true;

			if((!target || target->getHealth() <= 0) && !useDirection)
			{
				if(!casterTargetOrDirection)
				{
					player->sendCancelMessage(ret);
					server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
					return false;
				}

				useDirection = true;
			}
		}
		else
		{
			target = player->getAttackedCreature();
			if(limitRange && target && !Position::areInRange(Position(limitRange, limitRange, 0), target->getPosition(), player->getPosition()))
				useDirection = true;

			if((!target || target->getHealth() <= 0) && !useDirection)
			{
				if(!casterTargetOrDirection)
				{
					player->sendCancelMessage(RET_YOUCANONLYUSEITONCREATURES);
					server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
					return false;
				}

				useDirection = true;
			}
		}

		if(!useDirection)
		{
			bool canSee = player->canSeeCreature(target);
			if(!canSee || !canThrowSpell(player, target))
			{
				player->sendCancelMessage(canSee ? RET_CREATUREISNOTREACHABLE : RET_PLAYERWITHTHISNAMEISNOTONLINE);
				server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
				return false;
			}

			var.type = VARIANT_NUMBER;
			var.number = target->getID();
			if(!playerInstantSpellCheck(player, target))
				return false;
		}
		else
		{
			var.type = VARIANT_POSITION;
			var.pos = StackPosition(Spells::getCasterPosition(player, player->getDirection()), 0);
			if(!playerInstantSpellCheck(player, var.pos))
				return false;
		}
	}
	else if(hasParam)
	{
		var.type = VARIANT_STRING;
		var.text = param;
		if(!playerSpellCheck(player))
			return false;
	}
	else
	{
		var.type = VARIANT_POSITION;
		if(needDirection)
			var.pos = StackPosition(Spells::getCasterPosition(player, player->getDirection()), 0);
		else
			var.pos = StackPosition(player->getPosition(), 0);

		if(!playerInstantSpellCheck(player, var.pos))
			return false;
	}

	if(!internalCastSpell(player, var))
		return false;

	Spell::postCastSpell(player);
	return true;
}

bool InstantSpell::canThrowSpell(const Creature* creature, const Creature* target) const
{
	const Position& fromPos = creature->getPosition();
	const Position& toPos = target->getPosition();
	return (!(fromPos.z != toPos.z || (range == -1 && !server.game().canThrowObjectTo(fromPos, toPos, checkLineOfSight))
		|| (range != -1 && !server.game().canThrowObjectTo(fromPos, toPos, checkLineOfSight, range, range))));
}

bool InstantSpell::castSpell(Creature* creature)
{
	if(!BaseSpell::castSpell(creature))
		return false;

	LuaVariant var;
	if(casterTargetOrDirection)
	{
		Creature* target = creature->getAttackedCreature();
		if(target && target->getHealth() > 0)
		{
			if(!creature->canSeeCreature(target) || !canThrowSpell(creature, target))
				return false;

			var.type = VARIANT_NUMBER;
			var.number = target->getID();
			return internalCastSpell(creature, var);
		}

		return false;
	}

	if(needDirection)
	{
		var.type = VARIANT_POSITION;
		var.pos = StackPosition(Spells::getCasterPosition(creature, creature->getDirection()), 0);
	}
	else
	{
		var.type = VARIANT_POSITION;
		var.pos = StackPosition(creature->getPosition(), 0);
	}

	return internalCastSpell(creature, var);
}

bool InstantSpell::castSpell(Creature* creature, Creature* target)
{
	if(!BaseSpell::castSpell(creature, target))
		return false;

	if(!needTarget)
		return castSpell(creature);

	LuaVariant var;
	var.type = VARIANT_NUMBER;
	var.number = target->getID();
	return internalCastSpell(creature, var);
}

bool InstantSpell::internalCastSpell(Creature* creature, const LuaVariant& var)
{
	if(isScripted())
		return executeCastSpell(creature, var);

	if(function)
		return function(this, creature, var.text);

	return false;
}

bool InstantSpell::executeCastSpell(Creature* creature, const LuaVariant& var)
{
	//onCastSpell(cid, var)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			env->streamVariant(scriptstream, "var", var);

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());
			lua_State* L = m_interface->getState();

			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));
			m_interface->pushVariant(L, var);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[InstantSpell::executeCastSpell] Call stack overflow.");
		return false;
	}
}

bool InstantSpell::SearchPlayer(const InstantSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player || player->isRemoved())
		return false;

	Player* targetPlayer = nullptr;
	ReturnValue ret = server.game().getPlayerByNameWildcard(param, targetPlayer);
	if(ret != RET_NOERROR || !targetPlayer || targetPlayer->isRemoved())
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(targetPlayer->hasCustomFlag(PlayerCustomFlag_NotSearchable) && !player->hasCustomFlag(PlayerCustomFlag_GamemasterPrivileges))
	{
		player->sendCancelMessage(RET_PLAYERWITHTHISNAMEISNOTONLINE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	std::stringstream ss;
	ss << targetPlayer->getName() << " " << server.game().getSearchString(player->getPosition(), targetPlayer->getPosition(), true, true) << ".";
	player->sendTextMessage(MSG_INFO_DESCR, ss.str().c_str());
	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	return true;
}

bool InstantSpell::SummonMonster(const InstantSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	MonsterType* mType = server.monsters().getMonsterType(param);
	if(!mType)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	int32_t manaCost = (int32_t)(mType->manaCost * server.configManager().getDouble(ConfigManager::RATE_MONSTER_MANA));
	if(!player->hasFlag(PlayerFlag_CanSummonAll))
	{
		if(player->getSkull() == SKULL_BLACK)
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(!mType->isSummonable)
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if(player->getMana() < manaCost)
		{
			player->sendCancelMessage(RET_NOTENOUGHMANA);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if((int32_t)player->getSummonCount() >= server.configManager().getNumber(ConfigManager::MAX_PLAYER_SUMMONS))
		{
			player->sendCancel("You cannot summon more creatures.");
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	ReturnValue ret = server.game().placeSummon(creature, param);
	if(ret == RET_NOERROR)
	{
		spell->postCastSpell(player, (uint32_t)manaCost, (uint32_t)spell->getSoulCost());
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_BLUE);
	}
	else
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return (ret == RET_NOERROR);
}

bool InstantSpell::Levitate(const InstantSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	const Position& currentPos = creature->getPosition();
	Position destPos = Spells::getCasterPosition(creature, creature->getDirection());

	ReturnValue ret = RET_NOERROR;
	std::string tmpParam = asLowerCaseString(param);

	uint16_t blockedFloor = 7;
	bool up = false;
	if(tmpParam == "up")
	{
		up = true;
		blockedFloor = 8;
	}
	else if(tmpParam != "down")
		ret = RET_NOTPOSSIBLE;

	if(ret == RET_NOERROR)
	{
		ret = RET_NOTPOSSIBLE;
		if(currentPos.z != blockedFloor)
		{
			Tile* tmpTile = nullptr;
			if(up)
			{
				tmpTile = server.game().getTile(currentPos.x, currentPos.y, currentPos.z - 1);
				destPos.z--;
			}
			else
			{
				tmpTile = server.game().getTile(destPos);
				destPos.z++;
			}

			if(!tmpTile || (tmpTile->ground == nullptr && !tmpTile->hasProperty(IMMOVABLEBLOCKSOLID)))
			{
				Tile* tile = player->getTile();
				tmpTile = server.game().getTile(destPos.x, destPos.y, destPos.z);
				if(tile && tmpTile && tmpTile->ground
					&& !tmpTile->hasProperty(IMMOVABLEBLOCKSOLID) && !tmpTile->floorChange()
					&& tile->hasFlag(TILESTATE_HOUSE) == tmpTile->hasFlag(TILESTATE_HOUSE)
					&& tile->hasFlag(TILESTATE_PROTECTIONZONE) == tmpTile->hasFlag(TILESTATE_PROTECTIONZONE)
				)
						ret = server.game().internalMoveCreature(nullptr, player, tile, tmpTile, FLAG_IGNOREBLOCKITEM | FLAG_IGNOREBLOCKCREATURE);
			}
		}
	}

	if(ret == RET_NOERROR)
	{
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_TELEPORT, player->isGhost());
		return true;
	}

	player->sendCancelMessage(ret);
	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF, player->isGhost());
	return false;
}

bool InstantSpell::Illusion(const InstantSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	ReturnValue ret = CreateIllusion(creature, param, 60000);

	if(ret == RET_NOERROR)
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_RED);
	else
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return (ret == RET_NOERROR);
}

bool InstantSpell::canCast(const Player* player) const
{
	if(player->hasFlag(PlayerFlag_CannotUseSpells))
		return false;

	if(player->hasFlag(PlayerFlag_IgnoreSpellCheck) || (!isLearnable() && (vocSpellMap.empty()
		|| vocSpellMap.find(player->getVocationId()) != vocSpellMap.end())))
		return true;

	return player->hasLearnedInstantSpell(getName());
}



LOGGER_DEFINITION(ConjureSpell);


ConjureSpell::ConjureSpell(LuaScriptInterface* _interface):
	InstantSpell(_interface),
	function(nullptr),
	conjureId(0),
	conjureCount(1),
	conjureReagentId(0)
{
	isAggressive = false;
}

bool ConjureSpell::configureEvent(xmlNodePtr p)
{
	if(!InstantSpell::configureEvent(p))
		return false;

	int32_t intValue;
	if(readXMLInteger(p, "conjureId", intValue))
		conjureId = intValue;

	if(readXMLInteger(p, "conjureCount", intValue))
		conjureCount = intValue;
	else if(conjureId != 0)
	{
		//load the default charge from items.xml
		ItemKindPC kind = server.items()[conjureId];
		if(kind && kind->charges != 0)
			conjureCount = kind->charges;
	}

	if(readXMLInteger(p, "reagentId", intValue))
		conjureReagentId = intValue;

	return true;
}

bool ConjureSpell::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "conjureitem" || tmpFunctionName == "conjurerune")
		function = ConjureItem;
	else if(tmpFunctionName == "conjurefood")
		function = ConjureFood;
	else
	{
		LOGw("[ConjureSpell::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

ReturnValue ConjureSpell::internalConjureItem(Player* player, uint32_t conjureId, uint32_t conjureCount,
	bool transform/* = false*/, uint32_t reagentId/* = 0*/, slots_t slot/* = SLOT_WHEREVER*/, bool test/* = false*/)
{
	if(!transform)
	{
		boost::intrusive_ptr<Item> newItem = Item::CreateItem(conjureId, conjureCount);
		if(!newItem)
			return RET_NOTPOSSIBLE;

		return server.game().internalPlayerAddItem(player, player, newItem.get(), true);
	}

	if(!reagentId)
		return RET_NOTPOSSIBLE;

	Item* item = player->getInventoryItem(slot);
	if(item && item->getId() == reagentId)
	{
		if(item->isStackable() && item->getItemCount() != 1)
			return RET_YOUNEEDTOSPLITYOURSPEARS;

		if(test)
			return RET_NOERROR;

		Item* newItem = server.game().transformItem(item, conjureId, conjureCount);
		if(!newItem)
			return RET_NOTPOSSIBLE;

		server.game().startDecay(newItem);
		return RET_NOERROR;
	}

	return RET_YOUNEEDAMAGICITEMTOCASTSPELL;
}

bool ConjureSpell::ConjureItem(const ConjureSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(!player->hasFlag(PlayerFlag_IgnoreSpellCheck) && player->getZone() == ZONE_PVP)
	{
		player->sendCancelMessage(RET_CANNOTCONJUREITEMHERE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	ReturnValue result = RET_NOERROR;
	if(spell->getReagentId() != 0)
	{
		ReturnValue resLeft = internalConjureItem(player, spell->getConjureId(), spell->getConjureCount(),
			true, spell->getReagentId(), slots_t::LEFT, true);
		if(resLeft == RET_NOERROR)
		{
			resLeft = internalConjureItem(player, spell->getConjureId(), spell->getConjureCount(),
				true, spell->getReagentId(), slots_t::LEFT);
			if(resLeft == RET_NOERROR)
				spell->postCastSpell(player, false);
		}

		ReturnValue resRight = internalConjureItem(player, spell->getConjureId(), spell->getConjureCount(),
			true, spell->getReagentId(), slots_t::RIGHT, true);
		if(resRight == RET_NOERROR)
		{
			if(resLeft == RET_NOERROR && !spell->playerSpellCheck(player))
				return false;

			resRight = internalConjureItem(player, spell->getConjureId(), spell->getConjureCount(),
				true, spell->getReagentId(), slots_t::RIGHT);
			if(resRight == RET_NOERROR)
				spell->postCastSpell(player, false);
		}

		if(resLeft == RET_NOERROR || resRight == RET_NOERROR)
		{
			spell->postCastSpell(player, true, false);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_RED);
			return true;
		}

		result = resLeft;
		if((result == RET_NOERROR && resRight != RET_NOERROR) ||
			(result == RET_YOUNEEDAMAGICITEMTOCASTSPELL && resRight == RET_YOUNEEDTOSPLITYOURSPEARS))
			result = resRight;
	}
	else if(internalConjureItem(player, spell->getConjureId(), spell->getConjureCount()) == RET_NOERROR)
	{
		spell->postCastSpell(player);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_RED);
		return true;
	}

	if(result != RET_NOERROR)
		player->sendCancelMessage(result);

	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	return false;
}

bool ConjureSpell::ConjureFood(const ConjureSpell* spell, Creature* creature, const std::string& param)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	static uint32_t foodType[] =
	{
		ITEM_MEAT,
		ITEM_HAM,
		ITEM_GRAPE,
		ITEM_APPLE,
		ITEM_BREAD,
		ITEM_CHEESE,
		ITEM_ROLL
	};

	if(internalConjureItem(player, foodType[random_range<size_t>(0, (sizeof(foodType) / sizeof(uint32_t)) - 1)], 1) == RET_NOERROR)
	{
		if(random_range(0, 100) > 50)
			internalConjureItem(player, foodType[random_range<size_t>(0, (sizeof(foodType) / sizeof(uint32_t)) - 1)], 1);

		spell->postCastSpell(player);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_GREEN);
		return true;
	}

	return false;
}

bool ConjureSpell::playerCastInstant(Player* player, const std::string& param)
{
	if(!playerSpellCheck(player))
		return false;

	if(isScripted())
	{
		LuaVariant var;
		var.type = VARIANT_STRING;
		var.text = param;
		return executeCastSpell(player, var);
	}

	if(function)
		return function(this, player, param);

	return false;
}



LOGGER_DEFINITION(RuneSpell);


RuneSpell::RuneSpell(LuaScriptInterface* _interface) :
		Action(_interface),
		hasCharges(true),
		runeId(0),
		function(nullptr),
		comboLastCheck(0),
		comboFriendsNumber(0),
		comboFriendsListLastClear(0)
{
	allowFarUse = true;
}

bool RuneSpell::configureEvent(xmlNodePtr p)
{
	if(!Spell::configureSpell(p))
		return false;

	if(!Action::configureEvent(p))
		return false;

	int32_t intValue;
	if(readXMLInteger(p, "id", intValue))
		runeId = intValue;
	else
	{
		LOGe("[RuneSpell::configureSpell] Rune spell without id.");
		return false;
	}

	std::string strValue;
	if(readXMLString(p, "charges", strValue))
		hasCharges = booleanString(strValue);

	ItemKindP kind = server.items().getMutableKind(runeId);
	if (!kind) {
		LOGe("Cannot find rune with ID " << runeId << ".");
		return false;
	}

	if(level)
		kind->runeLevel = level;

	if(magLevel)
		kind->runeMagLevel = magLevel;

	kind->vocationString = parseVocationString(vocStringVec);
	return true;
}

bool RuneSpell::loadFunction(const std::string& functionName)
{
	std::string tmpFunctionName = asLowerCaseString(functionName);
	if(tmpFunctionName == "chameleon")
		function = Illusion;
	else if(tmpFunctionName == "convince")
		function = Convince;
	else
	{
		LOGw("[RuneSpell::loadFunction] Function \"" << functionName << "\" does not exist.");
		return false;
	}

	m_scripted = EVENT_SCRIPT_FALSE;
	return true;
}

bool RuneSpell::Illusion(const RuneSpell* spell, Creature* creature, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	Thing* thing = server.game().internalGetThing(player, destination, STACKPOS_MOVE);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Item* illusionItem = thing->getItem();
	if(!illusionItem || illusionItem->isNotMoveable())
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	ReturnValue ret = CreateIllusion(creature, illusionItem->getId(), 60000);

	if(ret == RET_NOERROR)
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_RED);
	else
	{
		player->sendCancelMessage(ret);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
	}

	return (ret == RET_NOERROR);
}

bool RuneSpell::Convince(const RuneSpell* spell, Creature* creature, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination)
{
	Player* player = creature->getPlayer();
	if(!player)
		return false;

	if(!player->hasFlag(PlayerFlag_CanConvinceAll))
	{
		if(player->getSkull() == SKULL_BLACK)
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}

		if((int32_t)player->getSummonCount() >= server.configManager().getNumber(ConfigManager::MAX_PLAYER_SUMMONS))
		{
			player->sendCancelMessage(RET_NOTPOSSIBLE);
			server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
			return false;
		}
	}

	Thing* thing = server.game().internalGetThing(player, destination, STACKPOS_LOOK);
	if(!thing)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	Creature* convinceCreature = thing->getCreature();
	if(!convinceCreature)
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	int32_t manaCost = 0;
	if(Monster* monster = convinceCreature->getMonster())
		manaCost = (int32_t)(monster->getManaCost() * server.configManager().getDouble(ConfigManager::RATE_MONSTER_MANA));

	if(!player->hasFlag(PlayerFlag_HasInfiniteMana) && player->getMana() < manaCost)
	{
		player->sendCancelMessage(RET_NOTENOUGHMANA);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	if(!creature->getMonster() || !creature->getMonster()->convince(creature))
	{
		player->sendCancelMessage(RET_NOTPOSSIBLE);
		server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_POFF);
		return false;
	}

	spell->postCastSpell(player, (uint32_t)manaCost, (uint32_t)spell->getSoulCost());
	server.game().addMagicEffect(player->getPosition(), MAGIC_EFFECT_WRAPS_RED);
	return true;
}

ReturnValue RuneSpell::canExecuteAction(const Player* player, const ExtendedPosition& toPos)
{
	if(player->hasFlag(PlayerFlag_CannotUseSpells))
		return RET_CANNOTUSETHISOBJECT;

	ReturnValue ret = Action::canExecuteAction(player, toPos);
	if(ret != RET_NOERROR)
		return ret;

	if(toPos.getType() == ExtendedPosition::Type::BACKPACK_SEARCH)
	{
		if(needTarget)
			return RET_CANONLYUSETHISRUNEONCREATURES;

		if(!selfTarget)
			return RET_NOTENOUGHROOM;
	}

	return RET_NOERROR;
}

bool RuneSpell::executeUse(Player* player, Item* item, const ExtendedPosition& origin, const ExtendedPosition& destination, bool extendedUse, uint32_t creatureId)
{
	if(!playerRuneSpellCheck(player, destination))
		return false;

	bool result = false;
	if(isScripted())
	{
		LuaVariant var;
		if(creatureId && needTarget)
		{
			var.type = VARIANT_NUMBER;
			var.number = creatureId;
		}
		else
		{
			var.type = VARIANT_EXTENDEDPOSITION;
			var.epos = destination;
		}

		result = internalCastSpell(player, var);
	}
	else if(function)
		result = function(this, player, item, origin, destination);

	if(result)
	{
		Spell::postCastSpell(player);
		if(item->getId() == 2268 && creatureId)
        {
            // check combos in last XX miliseconds
            uint32_t time = OTSYS_TIME();
            AttackersMap tmpAttackersMap;
            AttackersMap tmpAttackersMap2;
            uint32_t comboTime = server.configManager().getNumber(ConfigManager::COMBO_CT);
            uint32_t comboMembers = server.configManager().getNumber(ConfigManager::COMBO_CM);
            uint32_t comboFriends = server.configManager().getNumber(ConfigManager::COMBO_CF);
            uint32_t comboFriendsClearTime = server.configManager().getNumber(ConfigManager::COMBO_FCT);
            uint32_t comboFriendsClearInterval = server.configManager().getNumber(ConfigManager::COMBO_FCI);
            uint32_t comboShowAnim = server.configManager().getNumber(ConfigManager::COMBO_SA);
            // remove old friends from list
            if(comboTime && comboFriends && time - comboFriendsListLastClear >= comboFriendsClearInterval)
            {
                    for(ComboFriendsList::iterator cflit = comboFriendsList.begin(); cflit != comboFriendsList.end(); ++cflit)
                            for(AttackersMap::iterator rfit = cflit->second.begin(); rfit != cflit->second.end(); ++rfit)
                                    if(rfit->second < time - comboFriendsClearTime)
                                            cflit->second.erase(rfit->first);
            }
            if(comboTime && comboMembers && time - comboLastCheck >= comboTime)
            {
                    for(AttackersMap::iterator acit = comboAttackersCount.begin(); acit != comboAttackersCount.end(); ++acit)
                    {
                              if(acit->second >= comboMembers)
                              {
                                          // make combo members list
                                          for(AttackersMap::iterator alit = comboAttackersList.begin(); alit != comboAttackersList.end(); ++alit)
                                          {
                                                  if(alit->second == acit->first)
                                                  {
                                                            comboList[alit->first] = alit->first;
                                                  }
                                          }
                                          // show combo animations
                                          if(comboShowAnim)
                                          {
                                                  Creature* creature = server.game().getCreatureByID(acit->first);
                                                  if(creature != nullptr)
                                                  {
                                                        char buffer[20];
                                                        sprintf(buffer, "COMBO %d", acit->second);
                                                  }
                                                  for(AttackersMap::iterator sait = comboList.begin(); sait != comboList.end(); ++sait)
                                                  {
                                                        Creature* creature = server.game().getCreatureByID(sait->first);
                                                        if(creature != nullptr)
                                                        {
                                                        }
                                                  }
                                          }
                                          comboFriendsNumber = 0;
                                          // make friends list
                                          for(AttackersMap::iterator cit = comboList.begin(); cit != comboList.end(); ++cit)
                                          {
                                                    if(comboFriendsList.find(cit->first) != comboFriendsList.end())
                                                    {
                                                              tmpAttackersMap2 = comboFriendsList[cit->first];
                                                              for(AttackersMap::iterator tait = tmpAttackersMap2.begin(); tait != tmpAttackersMap2.end(); ++tait)
                                                                        tmpAttackersMap[tait->first] = tait->first;
                                                    }
                                          }
                                          for(AttackersMap::iterator ccit = tmpAttackersMap.begin(); ccit != tmpAttackersMap.end(); ++ccit)
                                                    if(comboList.find(ccit->first) != comboList.end())
                                                              ++comboFriendsNumber;
                                          // broadcast combo members
                                          if(comboFriendsNumber >= comboFriends)
                                          {
                                                    std::stringstream comboNamesList;
                                              for(AttackersMap::iterator ccit2 = tmpAttackersMap.begin(); ccit2 != tmpAttackersMap.end(); ++ccit2)
                                                        if(comboList.find(ccit2->first) != comboList.end())
                                                        {
                                                                  Creature* creature = server.game().getCreatureByID(ccit2->first);
                                    if(Player* player = creature->getPlayer())
                                               player->addExhaust(5*1000, EXHAUST_COMBAT);
                                                        }
                                              comboNamesList << " - did combo!";
                                              server.game().broadcastMessage(comboNamesList.str(), MSG_STATUS_WARNING);
                                              // add new 'friends'
                                              for(AttackersMap::iterator cit = comboList.begin(); cit != comboList.end(); ++cit)
                                              {
                                                        tmpAttackersMap.clear();
                                                        if(comboFriendsList.find(cit->first) != comboFriendsList.end())
                                                                  tmpAttackersMap = comboFriendsList[cit->first];
                                                        for(AttackersMap::iterator tcit = comboList.begin(); tcit != comboList.end(); ++tcit)
                                                        {
                                                                  tmpAttackersMap[tcit->first] = time;
                                                        }
                                                        comboFriendsList[cit->first] = tmpAttackersMap;
                                                        
                                              }
                                          }
                                          comboList.clear();
                              }    
                    }
                    comboAttackersList.clear();
                    comboAttackersCount.clear();
                    comboLastCheck = time;
            }
            // add player to attackers list
            comboAttackersList[player->getID()] = creatureId;
            comboAttackersCount[creatureId] += 1;
        }  
		if(hasCharges && item && server.configManager().getBool(ConfigManager::REMOVE_RUNE_CHARGES))
			server.game().transformItem(item, item->getId(), std::max((int32_t)0, ((int32_t)item->getItemCount()) - 1));
	}

	return result;
}

bool RuneSpell::castSpell(Creature* creature)
{
	if(!BaseSpell::castSpell(creature))
		return false;

	LuaVariant var;
	var.type = VARIANT_NUMBER;
	var.number = creature->getID();
	return internalCastSpell(creature, var);
}

bool RuneSpell::castSpell(Creature* creature, Creature* target)
{
	if(!BaseSpell::castSpell(creature, target))
		return false;

	LuaVariant var;
	var.type = VARIANT_NUMBER;
	var.number = target->getID();
	return internalCastSpell(creature, var);
}

bool RuneSpell::internalCastSpell(Creature* creature, const LuaVariant& var)
{
	if(isScripted())
		return executeCastSpell(creature, var);

	return false;
}

bool RuneSpell::executeCastSpell(Creature* creature, const LuaVariant& var)
{
	//onCastSpell(cid, var)
	if(m_interface->reserveEnv())
	{
		ScriptEnviroment* env = m_interface->getEnv();
		if(m_scripted == EVENT_SCRIPT_BUFFER)
		{
			env->setRealPos(creature->getPosition());
			std::stringstream scriptstream;

			scriptstream << "local cid = " << env->addThing(creature) << std::endl;
			env->streamVariant(scriptstream, "var", var);

			scriptstream << m_scriptData;
			bool result = true;
			if(m_interface->loadBuffer(scriptstream.str()))
			{
				lua_State* L = m_interface->getState();
				result = m_interface->getGlobalBool(L, "_result", true);
			}

			m_interface->releaseEnv();
			return result;
		}
		else
		{
			env->setScriptId(m_scriptId, m_interface);
			env->setRealPos(creature->getPosition());
			lua_State* L = m_interface->getState();

			m_interface->pushFunction(m_scriptId);
			lua_pushnumber(L, env->addThing(creature));
			m_interface->pushVariant(L, var);

			bool result = m_interface->callFunction(2);
			m_interface->releaseEnv();
			return result;
		}
	}
	else
	{
		LOGe("[RuneSpell::executeCastSpell] Call stack overflow.");
		return false;
	}
}
