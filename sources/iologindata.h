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

#ifndef _IOLOGINDATA_H
#define _IOLOGINDATA_H

#include "const.h"

class  Account;
struct Creature;
struct DeathEntry;
class  DBInsert;
class  DBResult;
class  Group;
class  Item;
class  Player;

typedef std::vector<DeathEntry>  DeathList;


enum DeleteCharacter_t
{
	DELETE_INTERNAL,
	DELETE_LEADER,
	DELETE_HOUSE,
	DELETE_ONLINE,
	DELETE_SUCCESS
};

typedef std::pair<int32_t, Item*> itemBlock;
typedef std::list<itemBlock> ItemBlockList;

class IOLoginData
{
	public:
		static IOLoginData* getInstance()
		{
			static IOLoginData instance;
			return &instance;
		}

		Account loadAccount(uint32_t accountId, bool preLoad = false);
		bool saveAccount(Account account);

		bool getAccountId(const std::string& name, uint32_t& number);
		bool getAccountName(uint32_t number, std::string& name);

		bool hasFlag(uint32_t accountId, PlayerFlags value);
		bool hasCustomFlag(uint32_t accountId, PlayerCustomFlags value);
		bool hasFlag(PlayerFlags value, const std::string& accName);
		bool hasCustomFlag(PlayerCustomFlags value, const std::string& accName);

		bool accountIdExists(uint32_t accountId);
		bool accountNameExists(const std::string& name);

		bool getPassword(uint32_t accountId, std::string& password, std::string name = "");
		bool setPassword(uint32_t accountId, std::string newPassword);
		bool validRecoveryKey(uint32_t accountId, std::string recoveryKey);
		bool setRecoveryKey(uint32_t accountId, std::string newRecoveryKey);

		uint64_t createAccount(std::string name, std::string password);
		void removePremium(Account account);

		const Group* getPlayerGroupByAccount(uint32_t accountId);

		bool loadPlayer(Player* player, const std::string& name, bool preLoad = false);
		bool savePlayer(Player* player, bool preSave = true, bool shallow = false);

		bool playerDeath(Player* player, const DeathList& dl);
		bool playerMail(Creature* actor, std::string name, uint32_t townId, Item* item);

		bool hasFlag(const std::string& name, PlayerFlags value);
		bool hasCustomFlag(const std::string& name, PlayerCustomFlags value);
		bool hasFlag(PlayerFlags value, uint32_t guid);
		bool hasCustomFlag(PlayerCustomFlags value, uint32_t guid);

		bool isPremium(uint32_t guid);

		bool playerExists(uint32_t guid, bool multiworld = false, bool checkCache = true);
		bool playerExists(std::string& name, bool multiworld = false, bool checkCache = true);
		bool getNameByGuid(uint32_t guid, std::string& name, bool multiworld = false);
		bool getGuidByName(uint32_t& guid, std::string& name, bool multiworld = false);
		bool getGuidByNameEx(uint32_t& guid, bool& specialVip, std::string& name);

		bool changeName(uint32_t guid, std::string newName, std::string oldName);
		bool createCharacter(uint32_t accountId, std::string characterName, int32_t vocationId, uint16_t sex);
		DeleteCharacter_t deleteCharacter(uint32_t accountId, const std::string characterName);

		uint32_t getLevel(uint32_t guid) const;
		uint32_t getLastIP(uint32_t guid) const;

		uint32_t getLastIPByName(const std::string& name) const;
		uint32_t getAccountIdByName(const std::string& name) const;

		bool getUnjustifiedDates(uint32_t guid, std::vector<time_t>& dateList, time_t _time);
		bool getDefaultTownByName(const std::string& name, uint32_t& townId);

		bool updatePremiumDays();
		bool updateOnlineStatus(uint32_t guid, bool login);
		bool resetGuildInformation(uint32_t guid);

	private:
		IOLoginData() {}

		LOGGER_DECLARATION;

		typedef std::map<std::string, uint32_t> GuidCacheMap;
		GuidCacheMap guidCacheMap;

		typedef std::map<uint32_t, std::string> NameCacheMap;
		NameCacheMap nameCacheMap;

		typedef std::map<int32_t, std::pair<boost::intrusive_ptr<Item>, int32_t> > ItemMap;

		bool saveItems(const Player* player, const ItemBlockList& itemList, DBInsert& query_insert);
		void loadItems(ItemMap& itemMap, DBResult* result);

		bool storeNameByGuid(uint32_t guid);
};

#endif // _IOLOGINDATA_H
