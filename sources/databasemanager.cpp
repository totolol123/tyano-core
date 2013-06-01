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

#include "databasemanager.h"
#include "databasemysql.h"
#include "tools.h"

#include "configmanager.h"
#include "server.h"


LOGGER_DEFINITION(DatabaseManager);


DatabaseManager::DatabaseManager()
	: _database(new DatabaseMySQL)
{
	_database->use();
}


DatabaseManager::~DatabaseManager() {}


Database& DatabaseManager::getDatabase() const {
	return *_database;
}


bool DatabaseManager::optimizeTables()
{
	Database& db = getDatabase();

	DBQuery query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << db.escapeString(server.configManager().getString(ConfigManager::SQL_DB)) << " AND `DATA_FREE` > 0;";
	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
		return false;

	query.str("");
	do
	{
		LOGt("Optimizing table '" << result->getDataString("TABLE_NAME") << "'...");
		query << "OPTIMIZE TABLE `" << result->getDataString("TABLE_NAME") << "`;";
		if(!db.executeQuery(query.str()))
			LOGe("Cannot optimize database table '" << result->getDataString("TABLE_NAME") << "'.");

		query.str("");
	}
	while(result->next());

	return true;
}

bool DatabaseManager::triggerExists(std::string trigger)
{
	Database& db = getDatabase();
	DBQuery query;
	query << "SELECT `TRIGGER_NAME` FROM `information_schema`.`triggers` WHERE `TRIGGER_SCHEMA` = " << db.escapeString(server.configManager().getString(ConfigManager::SQL_DB)) << " AND `TRIGGER_NAME` = " << db.escapeString(trigger) << ";";

	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
		return false;

	return true;
}

bool DatabaseManager::tableExists(std::string table)
{
	Database& db = getDatabase();
	DBQuery query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << db.escapeString(server.configManager().getString(ConfigManager::SQL_DB)) << " AND `TABLE_NAME` = " << db.escapeString(table) << ";";

	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
		return false;

	return true;
}

bool DatabaseManager::isDatabaseSetup()
{
	Database& db = getDatabase();
	DBQuery query;
	query << "SELECT `TABLE_NAME` FROM `information_schema`.`tables` WHERE `TABLE_SCHEMA` = " << db.escapeString(server.configManager().getString(ConfigManager::SQL_DB)) << ";";

	DBResultP result;
	if(!(result = db.storeQuery(query.str())))
		return false;

	return true;
}

int32_t DatabaseManager::getDatabaseVersion()
{
	if(!tableExists("server_config"))
		return 0;

	int32_t value = 0;
	if(getDatabaseConfig("db_version", value))
		return value;

	return 1;
}

uint32_t DatabaseManager::updateDatabase()
{
	Database& db = getDatabase();
	uint32_t version = getDatabaseVersion();

	DBQuery query;
	switch(version)
	{
		case 0:
		{
			LOGi("Updating database to version: 1...");
			db.executeQuery("CREATE TABLE IF NOT EXISTS `server_config` (`config` VARCHAR(35) NOT nullptr DEFAULT '', `value` INT NOT nullptr) ENGINE = InnoDB;");

			db.executeQuery("INSERT INTO `server_config` VALUES ('db_version', 1);");

			if(!tableExists("server_motd"))
			{
				//update bans table
				if(db.executeQuery("CREATE TABLE IF NOT EXISTS `bans2` (`id` INT UNSIGNED NOT nullptr auto_increment, `type` TINYINT(1) NOT nullptr COMMENT 'this field defines if its ip, account, player, or any else ban', `value` INT UNSIGNED NOT nullptr COMMENT 'ip, player guid, account number', `param` INT UNSIGNED NOT nullptr DEFAULT 4294967295 COMMENT 'mask', `active` TINYINT(1) NOT nullptr DEFAULT TRUE, `expires` INT UNSIGNED NOT nullptr, `added` INT UNSIGNED NOT nullptr, `admin_id` INT UNSIGNED NOT nullptr DEFAULT 0, `comment` TEXT NOT nullptr, `reason` INT UNSIGNED NOT nullptr DEFAULT 0, `action` INT UNSIGNED NOT nullptr DEFAULT 0, PRIMARY KEY (`id`), KEY `type` (`type`, `value`)) ENGINE = InnoDB;"))
				{
					if(DBResultP result = db.storeQuery("SELECT * FROM `bans`;"))
					{
						do
						{
							switch(result->getDataInt("type"))
							{
								case 1:
									query << "INSERT INTO `bans2` (`type`, `value`, `param`, `active`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`) VALUES (1, " << result->getDataInt("ip") << ", " << result->getDataInt("mask") << ", " << (result->getDataInt("time") <= time(nullptr) ? 0 : 1) << ", " << result->getDataInt("time") << ", 0, " << result->getDataInt("banned_by") << ", " << db.escapeString(result->getDataString("comment")) << ", " << result->getDataInt("reason_id") << ", " << result->getDataInt("action_id") << ");";
									break;

								case 2:
									query << "INSERT INTO `bans2` (`type`, `value`, `active`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`) VALUES (2, " << result->getDataInt("player") << ", " << (result->getDataInt("time") <= time(nullptr) ? 0 : 1) << ", 0, " << result->getDataInt("time") << ", " << result->getDataInt("banned_by") << ", " << db.escapeString(result->getDataString("comment")) << ", " << result->getDataInt("reason_id") << ", " << result->getDataInt("action_id") << ");";
									break;

								case 3:
									query << "INSERT INTO `bans2` (`type`, `value`, `active`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`) VALUES (3, " << result->getDataInt("player") << ", " << (result->getDataInt("time") <= time(nullptr) ? 0 : 1) << ", " << result->getDataInt("time") << ", 0, " << result->getDataInt("banned_by") << ", " << db.escapeString(result->getDataString("comment")) << ", " << result->getDataInt("reason_id") << ", " << result->getDataInt("action_id") << ");";
									break;

								case 4:
								case 5:
									query << "INSERT INTO `bans2` (`type`, `value`, `active`, `expires`, `added`, `admin_id`, `comment`, `reason`, `action`) VALUES (" << result->getDataInt("type") << ", " << result->getDataInt("player") << ", " << (result->getDataInt("time") <= time(nullptr) ? 0 : 1) << ", 0, " << result->getDataInt("time") << ", " << result->getDataInt("banned_by") << ", " << db.escapeString(result->getDataString("comment")) << ", " << result->getDataInt("reason_id") << ", " << result->getDataInt("action_id") << ");";
									break;

								default:
									break;
							}

							if(query.str() != "")
							{
								db.executeQuery(query.str());
								query.str("");
							}
						}
						while(result->next());
					}

					db.executeQuery("DROP TABLE `bans`;");
					db.executeQuery("RENAME TABLE `bans2` TO `bans`;");
				}

				std::string queryList[] = {
					//create server_record table
					"CREATE TABLE IF NOT EXISTS `server_record` (`record` INT NOT nullptr, `timestamp` BIGINT NOT nullptr, PRIMARY KEY (`timestamp`)) ENGINE = InnoDB;",
					//create server_reports table
					"CREATE TABLE IF NOT EXISTS `server_reports` (`id` INT NOT nullptr AUTO_INCREMENT, `player_id` INT UNSIGNED NOT nullptr DEFAULT 0, `posx` INT NOT nullptr DEFAULT 0, `posy` INT NOT nullptr DEFAULT 0, `posz` INT NOT nullptr DEFAULT 0, `timestamp` BIGINT NOT nullptr DEFAULT 0, `report` TEXT NOT nullptr, `reads` INT NOT nullptr DEFAULT 0, PRIMARY KEY (`id`), KEY (`player_id`), KEY (`reads`)) ENGINE = InnoDB;",
					//create server_motd table
					"CREATE TABLE `server_motd` (`id` INT NOT nullptr AUTO_INCREMENT, `text` TEXT NOT nullptr, PRIMARY KEY (`id`)) ENGINE = InnoDB;",
					//create global_storage table
					"CREATE TABLE IF NOT EXISTS `global_storage` (`key` INT UNSIGNED NOT nullptr, `value` INT NOT nullptr, PRIMARY KEY (`key`)) ENGINE = InnoDB;",
					//insert data to server_record table
					"INSERT INTO `server_record` VALUES (0, 0);",
					//insert data to server_motd table
					"INSERT INTO `server_motd` VALUES (1, 'Welcome to The Forgotten Server!');",
					//update players table
					"ALTER TABLE `players` ADD `balance` BIGINT UNSIGNED NOT nullptr DEFAULT 0 AFTER `blessings`;",
					"ALTER TABLE `players` ADD `stamina` BIGINT UNSIGNED NOT nullptr DEFAULT 201660000 AFTER `balance`;",
					"ALTER TABLE `players` ADD `loss_items` INT NOT nullptr DEFAULT 10 AFTER `loss_skills`;",
					"ALTER TABLE `players` ADD `marriage` INT UNSIGNED NOT nullptr DEFAULT 0;",
					"UPDATE `players` SET `loss_experience` = 10, `loss_mana` = 10, `loss_skills` = 10, `loss_items` = 10;",
					//update accounts table
					"ALTER TABLE `accounts` DROP `type`;",
					//update player deaths table
					"ALTER TABLE `player_deaths` DROP `is_player`;",
					//update house table
					"ALTER TABLE `houses` CHANGE `warnings` `warnings` INT NOT nullptr DEFAULT 0;",
					"ALTER TABLE `houses` ADD `lastwarning` INT UNSIGNED NOT nullptr DEFAULT 0;",
					//update triggers
					"DROP TRIGGER IF EXISTS `ondelete_accounts`;",
					"DROP TRIGGER IF EXISTS `ondelete_players`;",
					"CREATE TRIGGER `ondelete_accounts` BEFORE DELETE ON `accounts` FOR EACH ROW BEGIN DELETE FROM `bans` WHERE `type` != 1 AND `type` != 2 AND `value` = OLD.`id`; END;",
					"CREATE TRIGGER `ondelete_players` BEFORE DELETE ON `players` FOR EACH ROW BEGIN DELETE FROM `bans` WHERE `type` = 2 AND `value` = OLD.`id`; UPDATE `houses` SET `owner` = 0 WHERE `owner` = OLD.`id`; END;"
				};
				for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
					db.executeQuery(queryList[i]);
			}

			return 1;
		}

		case 1:
		{
			LOGi("Updating database to version: 2...");
			db.executeQuery("ALTER TABLE `players` ADD `promotion` INT NOT nullptr DEFAULT 0;");

			DBResultP result;
			if((result = db.storeQuery("SELECT `player_id`, `value` FROM `player_storage` WHERE `key` = 30018 AND `value` > 0")))
			{
				do
				{
					query << "UPDATE `players` SET `promotion` = " << result->getDataLong("value") << " WHERE `id` = " << result->getDataInt("player_id") << ";";
					db.executeQuery(query.str());
					query.str("");
				}
				while(result->next());
			}

			db.executeQuery("DELETE FROM `player_storage` WHERE `key` = 30018;");
			db.executeQuery("ALTER TABLE `accounts` ADD `name` VARCHAR(32) NOT nullptr DEFAULT '';");
			if((result = db.storeQuery("SELECT `id` FROM `accounts`;")))
			{
				do
				{
					query << "UPDATE `accounts` SET `name` = '" << result->getDataInt("id") << "' WHERE `id` = " << result->getDataInt("id") << ";";
					db.executeQuery(query.str());
					query.str("");
				}
				while(result->next());
			}

			db.executeQuery("ALTER TABLE `players` ADD `deleted` TINYINT(1) NOT nullptr DEFAULT 0;");
			db.executeQuery("ALTER TABLE `player_items` CHANGE `attributes` `attributes` BLOB NOT nullptr;");

			registerDatabaseConfig("db_version", 2);
			return 2;
		}

		case 2:
		{
			LOGi("Updating database to version: 3...");
			db.executeQuery("UPDATE `players` SET `vocation` = `vocation` - 4 WHERE `vocation` >= 5 AND `vocation` <= 8;");

			DBResultP result;
			if((result = db.storeQuery("SELECT COUNT(`id`) AS `count` FROM `players` WHERE `vocation` > 4;"))
				&& result->getDataInt("count"))
			{
				LOGw("There are still " << result->getDataInt("count") << " players with vocation above 4, please mind to update them manually.");
			}

			registerDatabaseConfig("db_version", 3);
			return 3;
		}

		case 3:
		{
			LOGi("Updating database to version: 4...");
			db.executeQuery("ALTER TABLE `houses` ADD `name` VARCHAR(255) NOT nullptr;");
			std::string queryList[] = {
				"ALTER TABLE `houses` ADD `size` INT UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD `town` INT UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD `price` INT UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD `rent` INT UNSIGNED NOT nullptr DEFAULT 0;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 4);
			return 4;
		}

		case 4:
		{
			LOGi("Updating database to version: 5...");
			db.executeQuery("ALTER TABLE `player_deaths` ADD `altkilled_by` VARCHAR(255) NOT nullptr;");
			registerDatabaseConfig("db_version", 5);
			return 5;
		}

		case 5:
		{
			LOGi("Updating database to version: 6...");
			std::string queryList[] = {
				"ALTER TABLE `global_storage` CHANGE `value` `value` VARCHAR(255) NOT nullptr DEFAULT '0'",
				"ALTER TABLE `player_storage` CHANGE `value` `value` VARCHAR(255) NOT nullptr DEFAULT '0'",
				"ALTER TABLE `tiles` CHANGE `x` `x` INT(5) UNSIGNED NOT nullptr, CHANGE `y` `y` INT(5) UNSIGNED NOT nullptr, CHANGE `z` `z` TINYINT(2) UNSIGNED NOT nullptr;",
				"ALTER TABLE `tiles` ADD INDEX (`x`, `y`, `z`);",
				"ALTER TABLE `tile_items` ADD INDEX (`sid`);"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 6);
			return 6;
		}

		case 6:
		{
			LOGi("Updating database to version: 7...");
			if(server.configManager().getBool(ConfigManager::INGAME_GUILD_MANAGEMENT))
			{
				if(DBResultP result = db.storeQuery("SELECT `r`.`id`, `r`.`guild_id` FROM `guild_ranks` r LEFT JOIN `guilds` g ON `r`.`guild_id` = `g`.`id` WHERE `g`.`ownerid` = `g`.`id` AND `r`.`level` = 3;"))
				{
					do
					{
						query << "UPDATE `guilds`, `players` SET `guilds`.`ownerid` = `players`.`id` WHERE `guilds`.`id` = " << result->getDataInt("guild_id") << " AND `players`.`rank_id` = " << result->getDataInt("id") << ";";
						db.executeQuery(query.str());
						query.str("");
					}
					while(result->next());
				}
			}

			registerDatabaseConfig("db_version", 7);
			return 7;
		}

		case 7:
		{
			LOGi("Updating database to version: 8...");
			std::string queryList[] = {
				"ALTER TABLE `server_motd` CHANGE `id` `id` INT UNSIGNED NOT nullptr;",
				"ALTER TABLE `server_motd` DROP PRIMARY KEY;",
				"ALTER TABLE `server_motd` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `server_motd` ADD UNIQUE (`id`, `world_id`);",
				"ALTER TABLE `server_record` DROP PRIMARY KEY;",
				"ALTER TABLE `server_record` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `server_record` ADD UNIQUE (`timestamp`, `record`, `world_id`);",
				"ALTER TABLE `server_reports` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `server_reports` ADD INDEX (`world_id`);",
				"ALTER TABLE `players` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `global_storage` DROP PRIMARY KEY;",
				"ALTER TABLE `global_storage` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `global_storage` ADD UNIQUE (`key`, `world_id`);",
				"ALTER TABLE `guilds` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `guilds` ADD UNIQUE (`name`, `world_id`);",
				"ALTER TABLE `house_lists` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `house_lists` ADD UNIQUE (`house_id`, `world_id`, `listid`);",
				"ALTER TABLE `houses` CHANGE `id` `id` INT NOT nullptr;",
				"ALTER TABLE `houses` DROP PRIMARY KEY;",
				"ALTER TABLE `houses` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD UNIQUE (`id`, `world_id`);",
				"ALTER TABLE `tiles` CHANGE `id` `id` INT NOT nullptr;",
				"ALTER TABLE `tiles` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `tiles` ADD UNIQUE (`id`, `world_id`);",
				"ALTER TABLE `tile_items` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `tile_items` ADD UNIQUE (`tile_id`, `world_id`, `sid`);"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 8);
			return 8;
		}

		case 8:
		{
			LOGi("Updating database to version: 9...");
			db.executeQuery("ALTER TABLE `bans` ADD `statement` VARCHAR(255) NOT nullptr;");
			registerDatabaseConfig("db_version", 9);
			return 9;
		}

		case 9:
		{
			LOGi("Updating database to version: 10...");
			registerDatabaseConfig("db_version", 10);
			return 10;
		}

		case 10:
		{
			LOGi("Updating database to version: 11...");
			db.executeQuery("ALTER TABLE `players` ADD `description` VARCHAR(255) NOT nullptr DEFAULT '';");
			if(tableExists("map_storage"))
			{
				db.executeQuery("ALTER TABLE `map_storage` RENAME TO `house_data`;");
				db.executeQuery("ALTER TABLE `house_data` ADD `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0 AFTER `house_id`;");
			}
			else if(!tableExists("house_storage"))
			{
				query << "CREATE TABLE `house_data` (`house_id` INT UNSIGNED NOT nullptr, `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0, `data` LONGBLOB NOT nullptr, UNIQUE (`house_id`, `world_id`)";
				query << ")";
				query << " ENGINE = InnoDB";

				query << ";";
				db.executeQuery(query.str());
				query.str("");
			}
			else
				db.executeQuery("ALTER TABLE `house_storage` RENAME TO `house_data`;");

			registerDatabaseConfig("db_version", 11);
			return 11;
		}

		case 11:
		{
			LOGi("Updating database to version: 12...");
			db.executeQuery("UPDATE `players` SET `stamina` = 151200000 WHERE `stamina` > 151200000;");
			db.executeQuery("UPDATE `players` SET `loss_experience` = `loss_experience` * 10, `loss_mana` = `loss_mana` * 10, `loss_skills` = `loss_skills` * 10, `loss_items` = `loss_items` * 10;");
			std::string queryList[] = {
				"ALTER TABLE `players` CHANGE `stamina` `stamina` INT NOT nullptr DEFAULT 151200000;",
				"ALTER TABLE `players` CHANGE `loss_experience` `loss_experience` INT NOT nullptr DEFAULT 100;",
				"ALTER TABLE `players` CHANGE `loss_mana` `loss_mana` INT NOT nullptr DEFAULT 100;",
				"ALTER TABLE `players` CHANGE `loss_skills` `loss_skills` INT NOT nullptr DEFAULT 100;",
				"ALTER TABLE `players` CHANGE `loss_items` `loss_items` INT NOT nullptr DEFAULT 100;",
				"ALTER TABLE `players` ADD `loss_containers` INT NOT nullptr DEFAULT 100 AFTER `loss_skills`;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 12);
			return 12;
		}

		case 12:
		{
			LOGi("Updating database to version: 13...");
			std::string queryList[] = {
				"ALTER TABLE `accounts` DROP KEY `group_id`;",
				"ALTER TABLE `players` DROP KEY `group_id`;",
				"DROP TABLE `groups`;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);


			registerDatabaseConfig("db_version", 13);
			return 13;
		}

		case 13:
		{
			LOGi("Updating database to version: 14...");
			std::string queryList[] = {
				"ALTER TABLE `houses` ADD `doors` INT UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD `beds` INT UNSIGNED NOT nullptr DEFAULT 0;",
				"ALTER TABLE `houses` ADD `guild` TINYINT(1) UNSIGNED NOT nullptr DEFAULT FALSE;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 14);
			return 14;
		}

		case 14:
		{
			LOGi("Updating database to version: 15...");
			db.executeQuery("DROP TABLE `player_deaths`;"); //no support for moving, sorry!
			std::string queryList[] = {
"CREATE TABLE `player_deaths`\
(\
	`id` INT NOT nullptr AUTO_INCREMENT,\
	`player_id` INT NOT nullptr,\
	`date` BIGINT UNSIGNED NOT nullptr,\
	`level` INT UNSIGNED NOT nullptr,\
	FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE,\
	PRIMARY KEY(`id`),\
	INDEX(`date`)\
) ENGINE = InnoDB;",
"CREATE TABLE `killers`\
(\
	`id` INT NOT nullptr AUTO_INCREMENT,\
	`death_id` INT NOT nullptr,\
	`final_hit` TINYINT(1) UNSIGNED NOT nullptr DEFAULT FALSE,\
	PRIMARY KEY(`id`),\
	FOREIGN KEY (`death_id`) REFERENCES `player_deaths` (`id`) ON DELETE CASCADE\
) ENGINE = InnoDB;",
"CREATE TABLE `player_killers`\
(\
	`kill_id` INT NOT nullptr,\
	`player_id` INT NOT nullptr,\
	FOREIGN KEY (`kill_id`) REFERENCES `killers` (`id`) ON DELETE CASCADE,\
	FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE\
) ENGINE = InnoDB;",
"CREATE TABLE `environment_killers`\
(\
	`kill_id` INT NOT nullptr,\
	`name` VARCHAR(255) NOT nullptr,\
	FOREIGN KEY (`kill_id`) REFERENCES `killers` (`id`) ON DELETE CASCADE\
) ENGINE = InnoDB;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 15);
			return 15;
		}

		case 15:
		{
			LOGi("Updating database to version: 16...");
			std::string queryList[] = {
				"ALTER TABLE `players` DROP `redskull`;",
				"ALTER TABLE `players` CHANGE `redskulltime` `redskulltime` INT NOT nullptr DEFAULT 0;",
				"ALTER TABLE `killers` ADD `unjustified` TINYINT(1) UNSIGNED NOT nullptr DEFAULT FALSE;",
				"UPDATE `players` SET `redskulltime` = 0;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 16);
			return 16;
		}

		case 16:
		{
			LOGi("Updating database to version: 17...");
			db.executeQuery("CREATE TABLE IF NOT EXISTS `player_namelocks`\
(\
	`player_id` INT NOT nullptr DEFAULT 0,\
	`name` VARCHAR(255) NOT nullptr,\
	`new_name` VARCHAR(255) NOT nullptr,\
	`date` BIGINT NOT nullptr DEFAULT 0,\
	KEY (`player_id`),\
	FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE\
) ENGINE = InnoDB;");

			registerDatabaseConfig("db_version", 17);
			return 17;
		}

		case 17:
		{
			LOGi("Updating database to version: 18...");
			std::string queryList[] = {
				"ALTER TABLE `player_depotitems` DROP KEY `player_id`;",
				"ALTER TABLE `player_depotitems` DROP `depot_id`;",
				"ALTER TABLE `player_depotitems` ADD KEY (`player_id`);",
				"ALTER TABLE `house_data` ADD FOREIGN KEY (`house_id`, `world_id`) REFERENCES `houses`(`id`, `world_id`) ON DELETE CASCADE;",
				"ALTER TABLE `house_lists` ADD FOREIGN KEY (`house_id`, `world_id`) REFERENCES `houses`(`id`, `world_id`) ON DELETE CASCADE;",
				"ALTER TABLE `guild_invites` ADD FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE;",
				"ALTER TABLE `guild_invites` ADD FOREIGN KEY (`guild_id`) REFERENCES `guilds`(`id`) ON DELETE CASCADE;",
				"ALTER TABLE `tiles` ADD `house_id` INT UNSIGNED NOT nullptr AFTER `world_id`;",
				"ALTER TABLE `tiles` ADD FOREIGN KEY (`house_id`, `world_id`) REFERENCES `houses`(`id`, `world_id`) ON DELETE CASCADE;",
				"ALTER TABLE `houses` ADD `clear` TINYINT(1) UNSIGNED NOT nullptr DEFAULT FALSE;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 18);
			return 18;
		}

		case 18:
		{
			LOGi("Updating database to version: 19...");
			std::string queryList[] = {
				"ALTER TABLE `houses` ADD `tiles` INT UNSIGNED NOT nullptr DEFAULT 0 AFTER `beds`;",
				"CREATE TABLE `house_auctions`\
(\
	`house_id` INT UNSIGNED NOT nullptr,\
	`world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0,\
	`player_id` INT NOT nullptr,\
	`bid` INT UNSIGNED NOT nullptr DEFAULT 0,\
	`limit` INT UNSIGNED NOT nullptr DEFAULT 0,\
	`endtime` BIGINT UNSIGNED NOT nullptr DEFAULT 0,\
	UNIQUE (`house_id`, `world_id`),\
	FOREIGN KEY (`house_id`, `world_id`) REFERENCES `houses`(`id`, `world_id`) ON DELETE CASCADE,\
	FOREIGN KEY (`player_id`) REFERENCES `players` (`id`) ON DELETE CASCADE\
) ENGINE = InnoDB;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 19);
			return 19;
		}

		case 19:
		{
			LOGi("Updating database to version: 20...");
			std::string queryList[] = {
				"ALTER TABLE `players` CHANGE `redskulltime` `skulltime` INT NOT nullptr DEFAULT 0;",
				"ALTER TABLE `players` ADD `skull` TINYINT(1) UNSIGNED NOT nullptr DEFAULT 0 AFTER `save`;"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 20);
			return 20;
		}

		case 20:
		{
			LOGi("Updating database to version: 21...");
			std::string queryList[] = {
				"UPDATE `bans` SET `type` = 3 WHERE `type` = 5;",
				"UPDATE `bans` SET `param` = 2 WHERE `type` = 2;",
				"UPDATE `bans` SET `param` = 0 WHERE `type` IN (3,4);"
			};
			for(uint32_t i = 0; i < sizeof(queryList) / sizeof(std::string); i++)
				db.executeQuery(queryList[i]);

			registerDatabaseConfig("db_version", 21);
			return 21;
		}

		case 21:
		{
			LOGi("Updating database to version: 22...");
			db.executeQuery("CREATE TABLE `account_viplist` (`account_id` INT NOT nullptr, `world_id` TINYINT(2) UNSIGNED NOT nullptr DEFAULT 0, `player_id` INT NOT nullptr, KEY (`account_id`), KEY (`player_id`), KEY (`world_id`), UNIQUE (`account_id`, `player_id`), FOREIGN KEY (`account_id`) REFERENCES `accounts`(`id`) ON DELETE CASCADE, FOREIGN KEY (`player_id`) REFERENCES `players`(`id`) ON DELETE CASCADE) ENGINE = InnoDB;");

			registerDatabaseConfig("db_version", 22);
			return 22;
		}

		case 22:
		{
			LOGi("Updating database to version: 23...");
			if(server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER))
			{
				query << "SELECT `id`, `key` FROM `accounts` WHERE `key` ";
				query << "!=";
				query << " '0';";
				if(DBResultP result = db.storeQuery(query.str()))
				{
					query.str("");
					do
					{
						std::string key = result->getDataString("key");
						_encrypt(key, false);

						query << "UPDATE `accounts` SET `key` = " << db.escapeString(key) << " WHERE `id` = " << result->getDataInt("id") << db.getUpdateLimiter();
						db.executeQuery(query.str());
						query.str("");
					}
					while(result->next());
				}
			}

			query << "DELETE FROM `server_config` WHERE `config` " << db.getStringComparison() << "'password_type';";
			db.executeQuery(query.str());
			query.str("");

			registerDatabaseConfig("encryption", server.configManager().getNumber(ConfigManager::ENCRYPTION));
			registerDatabaseConfig("db_version", 23);
			return 23;
		}

		default:
			break;
	}

	return 0;
}

bool DatabaseManager::getDatabaseConfig(std::string config, int32_t &value)
{
	value = 0;

	Database& db = getDatabase();
	DBResultP result;

	DBQuery query;
	query << "SELECT `value` FROM `server_config` WHERE `config` = " << db.escapeString(config) << ";";
	if(!(result = db.storeQuery(query.str())))
		return false;

	value = result->getDataInt("value");
	return true;
}

void DatabaseManager::registerDatabaseConfig(std::string config, int32_t value)
{
	Database& db = getDatabase();
	DBQuery query;

	int32_t tmp = 0;
	if(!getDatabaseConfig(config, tmp))
		query << "INSERT INTO `server_config` VALUES (" << db.escapeString(config) << ", " << value << ");";
	else
		query << "UPDATE `server_config` SET `value` = " << value << " WHERE `config` = " << db.escapeString(config) << ";";

	db.executeQuery(query.str());
}

void DatabaseManager::checkEncryption()
{
	Encryption_t newValue = (Encryption_t)server.configManager().getNumber(ConfigManager::ENCRYPTION);
	int32_t value = (int32_t)ENCRYPTION_PLAIN;
	if(getDatabaseConfig("encryption", value))
	{
		if(newValue != (Encryption_t)value)
		{
			switch(newValue)
			{
				case ENCRYPTION_MD5:
				{
					if((Encryption_t)value != ENCRYPTION_PLAIN)
					{
						LOGe("You cannot change the encryption to MD5, change it back in config.lua to \"sha1\".");
						return;
					}

					Database& db = getDatabase();
					DBQuery query;
					db.executeQuery("UPDATE `accounts` SET `password` = MD5(`password`), `key` = MD5(`key`);");

					registerDatabaseConfig("encryption", (int32_t)newValue);
					LOGi("Encryption updated to MD5.");
					break;
				}

				case ENCRYPTION_SHA1:
				{
					if((Encryption_t)value != ENCRYPTION_PLAIN)
					{
						LOGe("You cannot change the encryption to SHA1, change it back in config.lua to \"md5\".");
						return;
					}

					Database& db = getDatabase();
					DBQuery query;
					db.executeQuery("UPDATE `accounts` SET `password` = SHA1(`password`), `key` = SHA1(`key`);");

					registerDatabaseConfig("encryption", (int32_t)newValue);
					LOGi("Encryption set to SHA1.");
					break;
				}

				default:
				{
					LOGe("WARNING: You cannot switch from hashed passwords to plain text, change back the passwordType in config.lua to the passwordType you were previously using.");
					break;
				}
			}
		}
	}
	else
	{
		registerDatabaseConfig("encryption", (int32_t)newValue);
		if(server.configManager().getBool(ConfigManager::ACCOUNT_MANAGER))
		{
			switch(newValue)
			{
				case ENCRYPTION_MD5:
				{
					Database& db = getDatabase();
					DBQuery query;
					query << "UPDATE `accounts` SET `password` = " << db.escapeString(transformToMD5("1", false)) << " WHERE `id` = 1 AND `password` = '1';";
					db.executeQuery(query.str());
					break;
				}

				case ENCRYPTION_SHA1:
				{
					Database& db = getDatabase();
					DBQuery query;
					query << "UPDATE `accounts` SET `password` = " << db.escapeString(transformToSHA1("1", false)) << " WHERE `id` = 1 AND `password` = '1';";
					db.executeQuery(query.str());
					break;
				}

				default:
					break;
			}
		}
	}
}

void DatabaseManager::checkTriggers()
{
	std::string triggerName[] =
	{
		"ondelete_accounts",
		"oncreate_guilds",
		"ondelete_guilds",
		"oncreate_players",
		"ondelete_players",
	};

	std::string triggerStatement[] =
	{
		"CREATE TRIGGER `ondelete_accounts` BEFORE DELETE ON `accounts` FOR EACH ROW BEGIN DELETE FROM `bans` WHERE `type` NOT IN(1, 2) AND `value` = OLD.`id`; END;",
		"CREATE TRIGGER `oncreate_guilds` AFTER INSERT ON `guilds` FOR EACH ROW BEGIN INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Leader', 3, NEW.`id`); INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Vice-Leader', 2, NEW.`id`); INSERT INTO `guild_ranks` (`name`, `level`, `guild_id`) VALUES ('Member', 1, NEW.`id`); END;",
		"CREATE TRIGGER `ondelete_guilds` BEFORE DELETE ON `guilds` FOR EACH ROW BEGIN UPDATE `players` SET `guildnick` = '', `rank_id` = 0 WHERE `rank_id` IN (SELECT `id` FROM `guild_ranks` WHERE `guild_id` = OLD.`id`); END;",
		"CREATE TRIGGER `oncreate_players` AFTER INSERT ON `players` FOR EACH ROW BEGIN INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 0, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 1, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 2, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 3, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 4, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 5, 10); INSERT INTO `player_skills` (`player_id`, `skillid`, `value`) VALUES (NEW.`id`, 6, 10); END;",
		"CREATE TRIGGER `ondelete_players` BEFORE DELETE ON `players` FOR EACH ROW BEGIN DELETE FROM `bans` WHERE `type` = 2 AND `value` = OLD.`id`; UPDATE `houses` SET `owner` = 0 WHERE `owner` = OLD.`id`; END;"
	};

	DBQuery query;
	for(uint32_t i = 0; i < sizeof(triggerName) / sizeof(std::string); i++)
	{
		if(!triggerExists(triggerName[i]))
		{
			LOGd("Trigger: " << triggerName[i] << " does not exist, creating it...");
			getDatabase().executeQuery(triggerStatement[i]);
		}
	}
}
