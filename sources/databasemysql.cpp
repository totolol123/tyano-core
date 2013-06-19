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
#include "scheduler.h"

#include "database.h"
#include "databasemysql.h"

#ifdef __MYSQL_ALT_INCLUDE__
#include "errmsg.h"
#else
#include <mysql/errmsg.h>
#endif

#include "configmanager.h"
#include "schedulertask.h"
#include "server.h"


LOGGER_DEFINITION(DatabaseMySQL);


DatabaseMySQL::DatabaseMySQL()
	: m_attempts(0)
{
}


DatabaseMySQL::~DatabaseMySQL() {
	mysql_close(&m_handle);
	mysql_library_end();
}


void DatabaseMySQL::start() {
	mysql_library_init(0, nullptr, nullptr);

	m_connected = false;
	if(!mysql_init(&m_handle))
	{
		LOGe("Failed to initialize MySQL connection handler.");
		return;
	}

	uint32_t readTimeout = server.configManager().getNumber(ConfigManager::MYSQL_READ_TIMEOUT);
	if(readTimeout)
		mysql_options(&m_handle, MYSQL_OPT_READ_TIMEOUT, (const char*)&readTimeout);

	uint32_t writeTimeout = server.configManager().getNumber(ConfigManager::MYSQL_WRITE_TIMEOUT);
	if(writeTimeout)
		mysql_options(&m_handle, MYSQL_OPT_WRITE_TIMEOUT, (const char*)&writeTimeout);

	connect();
	if(mysql_get_client_version() <= 50019)
	{
		//MySQL servers <= 5.0.19 has a bug where MYSQL_OPT_RECONNECT is (incorrectly) reset by mysql_real_connect calls
		//See http://dev.mysql.com/doc/refman/5.0/en/mysql-options.html for more information.
		LOGw("Outdated MySQL server detected, consider upgrading to a newer version.");
	}

	if(server.configManager().getBool(ConfigManager::HOUSE_STORAGE))
	{
		//we cannot lock mutex here :)
		if(DBResultP result = storeQuery("SHOW variables LIKE 'max_allowed_packet';"))
		{
			if(result->getDataLong("Value") < 16776192)
			{
				LOGw("WARNING: max_allowed_packet might be set too low for binary map storage.");
				LOGw("Use the following query to raise max_allow_packet: SET GLOBAL max_allowed_packet = 16776192;");
			}
		}
	}

	int32_t keepAlive = server.configManager().getNumber(ConfigManager::SQL_KEEPALIVE);
	if(keepAlive)
		server.scheduler().addTask(SchedulerTask::create(Milliseconds(keepAlive * 1000), std::bind(&DatabaseMySQL::keepAlive, this)));
}


bool DatabaseMySQL::getParam(DBParam_t param)
{
	switch(param)
	{
		case DBPARAM_MULTIINSERT:
			return true;
		default:
			break;
	}

	return false;
}

bool DatabaseMySQL::rollback()
{
	if(!m_connected)
		return false;

	if(mysql_rollback(&m_handle))
	{
		LOGe("mysql_rollback() - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << mysql_errno(&m_handle) << ")");
		return false;
	}

	return true;
}

bool DatabaseMySQL::commit()
{
	if(!m_connected)
		return false;

	if(mysql_commit(&m_handle))
	{
		LOGe("mysql_commit() - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << mysql_errno(&m_handle) << ")");
		return false;
	}

	return true;
}

bool DatabaseMySQL::executeQuery(const std::string &query)
{
	if(!m_connected)
		return false;

	bool state = true;
	if(mysql_real_query(&m_handle, query.c_str(), query.length()))
	{
		int32_t error = mysql_errno(&m_handle);
		if((error == CR_UNKNOWN_ERROR || error == CR_SERVER_LOST || error == CR_SERVER_GONE_ERROR) && reconnect())
			return executeQuery(query);

		state = false;
		LOGe("mysql_real_query(): " << query << " - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << error << ")");
	}

	if(MYSQL_RES* tmp = mysql_store_result(&m_handle))
	{
		mysql_free_result(tmp);
		tmp = nullptr;
	}

	return state;
}

DBResultP DatabaseMySQL::storeQuery(const std::string &query)
{
	if(!m_connected)
		return nullptr;

	int32_t error = 0;
	if(mysql_real_query(&m_handle, query.c_str(), query.length()))
	{
		error = mysql_errno(&m_handle);
		if((error == CR_UNKNOWN_ERROR || error == CR_SERVER_LOST || error == CR_SERVER_GONE_ERROR) && reconnect())
			return storeQuery(query);

		LOGe("mysql_real_query(): " << query << " - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << error << ")");
		return nullptr;

	}

	if(MYSQL_RES* tmp = mysql_store_result(&m_handle))
	{
		return verifyResult(DBResultP(new MySQLResult(tmp)));
	}

	error = mysql_errno(&m_handle);
	if((error == CR_UNKNOWN_ERROR || error == CR_SERVER_LOST || error == CR_SERVER_GONE_ERROR) && reconnect())
		return storeQuery(query);

	LOGe("mysql_store_result(): " << query << " - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << error << ")");
	return nullptr;
}

std::string DatabaseMySQL::escapeBlob(const char* s, uint32_t length)
{
	if(!s)
		return "''";

	char* output = new char[length * 2 + 1];
	mysql_real_escape_string(&m_handle, output, s, length);

	std::string res = "'";
	res += output;
	res += "'";

	delete[] output;
	return res;
}

void DatabaseMySQL::keepAlive()
{
	int32_t delay = server.configManager().getNumber(ConfigManager::SQL_KEEPALIVE);
	if(delay)
	{
		if(time(nullptr) > (m_use + delay) && mysql_ping(&m_handle))
			reconnect();

		server.scheduler().addTask(SchedulerTask::create(Milliseconds(delay * 1000), std::bind(&DatabaseMySQL::keepAlive, this)));
	}
}

bool DatabaseMySQL::connect()
{
	if(m_connected)
	{
		m_connected = false;
		mysql_close(&m_handle);
	}

	if(!mysql_real_connect(&m_handle, server.configManager().getString(ConfigManager::SQL_HOST).c_str(), server.configManager().getString(ConfigManager::SQL_USER).c_str(),
		server.configManager().getString(ConfigManager::SQL_PASS).c_str(), server.configManager().getString(ConfigManager::SQL_DB).c_str(), server.configManager().getNumber(
		ConfigManager::SQL_PORT), nullptr, 0))
	{
		LOGe("Failed connecting to database - MYSQL ERROR: " << mysql_error(&m_handle) << " (" << mysql_errno(&m_handle) << ")");
		return false;
	}

	m_connected = true;
	m_attempts = 0;
	return true;
}

bool DatabaseMySQL::reconnect()
{
	while(m_attempts <= MAX_RECONNECT_ATTEMPTS)
	{
		m_attempts++;
		if(connect())
			return true;
	}

	LOGe("Unable to reconnect - too many attempts, limit exceeded!");
	return false;
}




LOGGER_DEFINITION(MySQLResult);


bool MySQLResult::isNull(const std::string& field) {
	listNames_t::iterator it = m_listNames.find(field);
	if(it != m_listNames.end()) {
		return (m_row[it->second] == nullptr);
	}

	if(refetch())
		return isNull(field);

	LOGe("Error during isNull(" << field << ").");
	return 0; // Failed
}


uint32_t MySQLResult::getUnsigned32(const std::string& field) {
	listNames_t::iterator it = m_listNames.find(field);
	if(it != m_listNames.end())
	{
		if(!m_row[it->second])
			return 0;

		return static_cast<uint32_t>(atoll(m_row[it->second]));
	}

	if(refetch())
		return getUnsigned32(field);

	LOGe("Error during getDataInt(" << field << ").");
	return 0; // Failed
}


int32_t MySQLResult::getDataInt(const std::string &s)
{
	listNames_t::iterator it = m_listNames.find(s);
	if(it != m_listNames.end())
	{
		if(!m_row[it->second])
			return 0;

		return atoi(m_row[it->second]);
	}

	if(refetch())
		return getDataInt(s);

	LOGe("Error during getDataInt(" << s << ").");
	return 0; // Failed
}

int64_t MySQLResult::getDataLong(const std::string &s)
{
	listNames_t::iterator it = m_listNames.find(s);
	if(it != m_listNames.end())
	{
		if(!m_row[it->second])
			return 0;

		return atoll(m_row[it->second]);
	}

	if(refetch())
		return getDataLong(s);

	LOGe("Error during getDataLong(" << s << ").");
	return 0; // Failed
}

std::string MySQLResult::getDataString(const std::string &s)
{
	listNames_t::iterator it = m_listNames.find(s);
	if(it != m_listNames.end())
	{
		if(!m_row[it->second])
			return "";

		return std::string(m_row[it->second]);
	}

	if(refetch())
		return getDataString(s);

	LOGe("Error during getDataString(" << s << ").");
	return ""; // Failed
}

const char* MySQLResult::getDataStream(const std::string &s, uint64_t &size)
{
	listNames_t::iterator it = m_listNames.find(s);
	if(it != m_listNames.end())
	{
		if(!m_row[it->second])
		{
			size = 0;
			return nullptr;
		}

		size = mysql_fetch_lengths(m_handle)[it->second];
		return m_row[it->second];
	}

	if(refetch())
		return getDataStream(s, size);

	LOGe("Error during getDataStream(" << s << ").");
	size = 0;
	return nullptr; // Failed
}


bool MySQLResult::next()
{
	m_row = mysql_fetch_row(m_handle);
	return m_row;
}

void MySQLResult::fetch()
{
	m_listNames.clear();
	int32_t i = 0;

	MYSQL_FIELD* field;
	while((field = mysql_fetch_field(m_handle)))
		m_listNames[field->name] = i++;
}

bool MySQLResult::refetch()
{
	if(m_attempts >= MAX_REFETCH_ATTEMPTS)
		return false;

	fetch();
	++m_attempts;
	return true;
}

MySQLResult::MySQLResult(MYSQL_RES* result)
{
	m_attempts = 0;
	if(result)
	{
		m_handle = result;
		fetch();
	}
	else
		delete this;
}


MySQLResult::~MySQLResult() {
	if (m_handle != nullptr) {
		mysql_free_result(m_handle);
	}
}
