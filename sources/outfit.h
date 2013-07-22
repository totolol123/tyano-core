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

#ifndef _OUTFIT_H
#define _OUTFIT_H

#include "const.h"
#include "tools.h"

#define OUTFITS_MAX_NUMBER 25


enum AddonRequirement_t
{
	REQUIREMENT_NONE = 0,
	REQUIREMENT_FIRST,
	REQUIREMENT_SECOND,
	REQUIREMENT_BOTH,
	REQUIREMENT_ANY
};

struct Outfit
{
	Outfit()
	{
		// temporary workaround. TODO: refactor outfits!
		name.~basic_string();
		storageValue.~basic_string();

		memset(this, 0, sizeof(*this));

		new(&name) std::string;
		new(&storageValue) std::string;

		isDefault = true;
		requirement = REQUIREMENT_BOTH;
	}


	int16_t getAbsorb(CombatType_t combatType) const {
		CombatTypeIndex index = combatTypeIndexFromCombatType(combatType);
		if (index == COMBATINDEX_NONE) {
			return 0;
		}

		return _absorb[index-1];
	}


	int16_t getReflect(CombatType_t combatType, Reflect_t reflectType) const {
		CombatTypeIndex index = combatTypeIndexFromCombatType(combatType);
		if (index == COMBATINDEX_NONE) {
			return 0;
		}

		return _reflect[index-1][reflectType];
	}


	void setAbsorb(CombatType_t combatType, int16_t absorb) {
		CombatTypeIndex index = combatTypeIndexFromCombatType(combatType);
		if (index == COMBATINDEX_NONE) {
			return;
		}

		_absorb[index-1] = absorb;
	}


	void setReflect(CombatType_t combatType, Reflect_t reflectType, int16_t reflect) {
		CombatTypeIndex index = combatTypeIndexFromCombatType(combatType);
		if (index == COMBATINDEX_NONE) {
			return;
		}

		_reflect[index-1][reflectType] = reflect;
	}


	bool isDefault, manaShield, invisible, regeneration;
	AddonRequirement_t requirement;

	uint16_t accessLevel, addons;
	int32_t skills[SKILL_LAST + 1], skillsPercent[SKILL_LAST + 1], stats[STAT_LAST + 1], statsPercent[STAT_LAST + 1],
		speed, healthGain, healthTicks, manaGain, manaTicks, conditionSuppressions;

	uint32_t outfitId, lookType, storageId;
	std::string name, storageValue;


private:

	int16_t _absorb[COMBATINDEX_COUNT-1];
	int16_t _reflect[COMBATINDEX_COUNT-1][REFLECT_COUNT];

};

typedef std::list<Outfit> OutfitList;
typedef std::map<uint32_t, Outfit> OutfitMap;

class Outfits
{
	public:
		static Outfits* getInstance()
		{
			static Outfits instance;
			return &instance;
		}

		bool loadFromXml();
		bool parseOutfitNode(xmlNodePtr p);

		const OutfitMap& getOutfits(uint16_t sex) {return outfitsMap[sex];}

		bool getOutfit(uint32_t outfitId, uint16_t sex, Outfit& outfit);
		bool getOutfit(uint32_t lookType, Outfit& outfit);

		bool addAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex, uint16_t addons);
		bool removeAttributes(uint32_t playerId, uint32_t outfitId, uint16_t sex);

		uint32_t getOutfitId(uint32_t lookType);

		int16_t getOutfitAbsorb(uint32_t lookType, uint16_t sex, CombatType_t combat);
		int16_t getOutfitReflect(uint32_t lookType, uint16_t sex, CombatType_t combat);

	private:

		Outfits() {}


		LOGGER_DECLARATION;

		OutfitList allOutfits;
		std::map<uint16_t, OutfitMap> outfitsMap;
};

#endif // _OUTFIT_H
