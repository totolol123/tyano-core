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

#ifndef _DATABASE_H
#define _DATABASE_H

#include "enums.h"

class DBQuery;
class DBResult;


enum DBParam_t
{
	DBPARAM_MULTIINSERT = 1
};

class Database
{
	public:

		virtual ~Database() {}

		/**
		* Database information.
		*
		* Returns currently used database attribute.
		*
		* @param DBParam_t parameter to get
		* @return suitable for given parameter
		*/
		virtual bool getParam(DBParam_t param) {return false;}

		/**
		* Database connected.
		*
		* Returns whether or not the database is connected.
		*
		* @return whether or not the database is connected.
		*/
		virtual bool isConnected() {return m_connected;}

		/**
		* Database ...
		*/
		virtual void use() {m_use = time(nullptr);}
		virtual void start() {};

	protected:
		/**
		* Transaction related methods.
		*
		* Methods for starting, commiting and rolling back transaction. Each of the returns boolean value.
		*
		* @return true on success, false on error
		* @note
		*	If your database system doesn't support transactions you should return true - it's not feature test, code should work without transaction, just will lack integrity.
		*/
		friend class DBTransaction;

		virtual bool beginTransaction() {return 0;}
		virtual bool rollback() {return 0;}
		virtual bool commit() {return 0;}

	public:
		/**
		* Executes command.
		*
		* Executes query which doesn't generates results (eg. INSERT, UPDATE, DELETE...).
		*
		* @param std::string query command
		* @return true on success, false on error
		*/
		virtual bool executeQuery(const std::string &query) {return 0;}

		/**
		* Queries database.
		*
		* Executes query which generates results (mostly SELECT).
		*
		* @param std::string query
		* @return results object (null on error)
		*/
		virtual DBResult* storeQuery(const std::string &query) {return 0;}

		/**
		* Escapes string for query.
		*
		* Prepares string to fit SQL queries including quoting it.
		*
		* @param std::string string to be escaped
		* @return quoted string
		*/
		virtual std::string escapeString(const std::string &s) {return "''";}

		/**
		* Escapes binary stream for query.
		*
		* Prepares binary stream to fit SQL queries.
		*
		* @param char* binary stream
		* @param long stream length
		* @return quoted string
		*/
		virtual std::string escapeBlob(const char* s, uint32_t length) {return "''";}

		/**
		 * Retrieve id of last inserted row
		 *
		 * @return id on success, 0 if last query did not result on any rows with auto_increment keys
		 */
		virtual uint64_t getLastInsertId() {return 0;}

		/**
		* Get case insensitive string comparison operator
		*
		* @return the case insensitive operator
		*/
		virtual std::string getStringComparison() {return "= ";}
		virtual std::string getUpdateLimiter() {return " LIMIT 1;";}

	protected:
		Database() : m_connected(false), m_use(0) {}

		DBResult* verifyResult(DBResult* result);

		bool m_connected;
		time_t m_use;

	private:
		static Database* _instance;
};

class DBResult
{
	public:
		/** Get the Integer value of a field in database
		*\returns The Integer value of the selected field and row
		*\param s The name of the field
		*/
		virtual int32_t getDataInt(const std::string &s) {return 0;}

		/** Get the Long value of a field in database
		*\returns The Long value of the selected field and row
		*\param s The name of the field
		*/
		virtual int64_t getDataLong(const std::string &s) {return 0;}

		/** Get the String of a field in database
		*\returns The String of the selected field and row
		*\param s The name of the field
		*/
		virtual std::string getDataString(const std::string &s) {return "''";}

		/** Get the blob of a field in database
		*\returns a PropStream that is initiated with the blob data field, if not exist it returns nullptr.
		*\param s The name of the field
		*/
		virtual const char* getDataStream(const std::string &s, uint64_t &size) {return 0;}

		/** Result freeing
		*/
		virtual void free() {/*delete this;*/}

		/** Moves to next result in set
		*\returns true if moved, false if there are no more results.
		*/
		virtual bool next() {return false;}

	protected:
		DBResult() {}
		virtual ~DBResult() {}
};

/**
 * Thread locking hack.
 *
 * By using this class for your queries you lock and unlock database for threads.
*/
class DBQuery : public std::stringstream
{
	friend class _Database;
	public:
		DBQuery() {databaseLock.lock();}
		~DBQuery() {str(""); databaseLock.unlock();}

	protected:
		static boost::recursive_mutex databaseLock;
};

/**
 * INSERT statement.
 *
 * Gives possibility to optimize multiple INSERTs on databases that support multiline INSERTs.
 */
class DBInsert
{
	public:
		/**
		* Associates with given database handler.
		*
		* @param Database* database wrapper
		*/
		DBInsert(Database& db);

		/**
		* Sets query prototype.
		*
		* @param std::string& INSERT query
		*/
		void setQuery(const std::string& query);

		/**
		* Adds new row to INSERT statement.
		*
		* On databases that doesn't support multiline INSERTs it simply execute INSERT for each row.
		*
		* @param std::string& row data
		*/
		bool addRow(const std::string& row);
		/**
		* Allows to use addRow() with stringstream as parameter.
		*/
		bool addRow(std::stringstream& row);

		/**
		* Executes current buffer.
		*/
		bool execute();

	protected:
		Database& m_db;
		bool m_multiLine;

		uint32_t m_rows;
		std::string m_query, m_buf;
};


class DBTransaction
{
	public:
		DBTransaction(Database& database);
		~DBTransaction();

		bool begin();
		bool commit();

	private:
		Database& m_database;

		enum TransactionStates_t
		{
			STATE_NO_START,
			STATE_START,
			STEATE_COMMIT
		};

		TransactionStates_t m_state;
};

#endif // _DATABASE_H
