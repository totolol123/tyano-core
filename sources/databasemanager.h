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

#ifndef _DATABASEMANAGER_H
#define _DATABASEMANAGER_H

class Database;


class DatabaseManager
{
	public:

		DatabaseManager();
		~DatabaseManager();

		bool optimizeTables();

		bool tableExists(std::string table);
		bool triggerExists(std::string trigger);

		Database& getDatabase() const;
		int32_t getDatabaseVersion();
		bool isDatabaseSetup();
		uint32_t updateDatabase();
		bool getDatabaseConfig(std::string config, int32_t &value);
		void registerDatabaseConfig(std::string config, int32_t value);
		void checkEncryption();
		void checkTriggers();

	private:

		LOGGER_DECLARATION;

		std::unique_ptr<Database> _database;

};

#endif // _DATABASEMANAGER_H
