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

#include "database.h"
#include "databasemysql.h"

boost::recursive_mutex DBQuery::databaseLock;


DBResultP Database::verifyResult(DBResultP result) {
	if (!result->next()) {
		return nullptr;
	}

	return result;
}

DBInsert::DBInsert(Database& db)
	: m_db(db), m_multiLine(db.getParam(DBPARAM_MULTIINSERT)), m_rows(0)
{}

void DBInsert::setQuery(const std::string& query)
{
	m_query = query;
	m_buf = "";
	m_rows = 0;
}

bool DBInsert::addRow(const std::string& row)
{
	if(!m_multiLine) // executes INSERT for current row
		return m_db.executeQuery(m_query + "(" + row + ")");

	m_rows++;
	int32_t size = m_buf.length();
	// adds new row to buffer
	if(!size)
		m_buf = "(" + row + ")";
	else if(size > 8192)
	{
		if(!execute())
			return false;

		m_buf = "(" + row + ")";
	}
	else
		m_buf += ",(" + row + ")";

	return true;
}

bool DBInsert::addRow(std::stringstream& row)
{
	bool ret = addRow(row.str());
	row.str("");
	return ret;
}

bool DBInsert::execute()
{
	if(!m_multiLine || m_buf.length() < 1) // INSERTs were executed on-fly
		return true;

	if(!m_rows) //no rows to execute
		return true;

	m_rows = 0;
	// executes buffer
	bool res = m_db.executeQuery(m_query + m_buf);
	m_buf = "";
	return res;
}




DBTransaction::DBTransaction(Database& database)
	: m_database(database), m_state(STATE_NO_START)
{}

DBTransaction::~DBTransaction()
{
	if(m_state == STATE_START)
		m_database.rollback();
}

bool DBTransaction::begin()
{
	m_state = STATE_START;
	return m_database.beginTransaction();
}

bool DBTransaction::commit()
{
	if(m_state != STATE_START)
		return false;

	m_state = STEATE_COMMIT;
	return m_database.commit();
}
