// ***************************************************************************
//
// Reality - The Matrix Online Server Emulator
// Copyright (C) 2006-2010 Rajko Stojadinovic
// http://mxoemu.info
//
// ---------------------------------------------------------------------------
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ---------------------------------------------------------------------------
//
// ***************************************************************************

#include "DatabaseEnv.h"
#include "../Log.h"
#include "../Util.h"
#include "../Threading/Threading.h"
#include "PreparedStatement.h"

SQLCallbackBase::~SQLCallbackBase()
{

}

// Mock mode is a FALLBACK entered only if the real DB connection fails during
// Initialize(); it must NOT be the default, or Initialize() short-circuits at
// its `if (m_isMockMode)` check and never connects — making every query return
// NULL (which broke auth: the user lookup always came back empty).
Database::Database() : ThreadContext(), m_isMockMode(false)
{
	_counter=0;
	ThreadRunning = true;
}

Database::~Database()
{
	Shutdown();
}

bool Database::Initialize(const char* Hostname, unsigned int port, const char* Username, const char* Password, const char* DatabaseName, uint32 ConnectionCount, uint32 BufferSize)
{
	uint32 i;
	MYSQL * temp, * temp2;
	my_bool my_true = true;

	mHostname = string(Hostname);
	mUsername = string(Username);
	mPassword = string(Password);
	mDatabaseName = string(DatabaseName);
	mPort = port;

	INFO_LOG(format("MySQLDatabase Connecting to `%1%`, database `%2%`...") % Hostname % DatabaseName);

	for( i = 0; i < ConnectionCount; ++i )
	{
		temp = mysql_init( NULL );
		if(mysql_options(temp, MYSQL_SET_CHARSET_NAME, "utf8"))
			WARNING_LOG("MySQLDatabase Could not set utf8 character set.");

		if (mysql_options(temp, MYSQL_OPT_RECONNECT, &my_true))
			WARNING_LOG("MYSQL_OPT_RECONNECT could not be set, connection drops may occur but will be counteracted.");

		if (m_isMockMode)
		{
			DatabaseConnection* mockConn = new DatabaseConnection(NULL);
			m_connections.push_back(mockConn);
			m_freeConnections.push(mockConn);
			continue;
		}

		bool connected = false;
		for(int attempt = 0; attempt < 5; ++attempt)
		{
			temp2 = mysql_real_connect( temp, Hostname, Username, Password, DatabaseName, port, NULL, 0 );
			if( temp2 != NULL )
			{
				connected = true;
				break;
			}
			WARNING_LOG(format("MySQLDatabase Connection failed on attempt %1% due to: `%2%`. Retrying in 2 seconds...") % (attempt + 1) % mysql_error( temp ) );
			Sleep(2000); // Wait 2 seconds before retrying
		}

		if( !connected )
		{
			CRITICAL_LOG(format("MySQLDatabase Connection failed after 5 attempts due to: `%1%`") % mysql_error( temp ) );
			WARNING_LOG("Please ensure MySQL is running and `schema.sql` has been imported into the database.");
			WARNING_LOG("Entering MOCK DATABASE MODE. State will not be saved.");
			m_isMockMode = true;
			DatabaseConnection* mockConn = new DatabaseConnection(NULL);
			m_connections.push_back(mockConn);
			m_freeConnections.push(mockConn);
		}
		else 
		{
			DatabaseConnection* dbConn = new DatabaseConnection(temp2);
			m_connections.push_back(dbConn);
			m_freeConnections.push(dbConn);
		}
	}

	// Spawn Database thread
	ThreadPool.ExecuteTask(this);

	// launch the query thread
	qt = new QueryThread(this);
	ThreadPool.ExecuteTask(qt);

	// Ensure schema migrations and persistence tables exist
	if (!m_isMockMode) {
		Execute("ALTER TABLE `characters` ADD COLUMN IF NOT EXISTS `rebirth_count` INT DEFAULT 0;");
		Execute("ALTER TABLE `hardlines` ADD COLUMN IF NOT EXISTS `FactionTag` INT UNSIGNED NOT NULL DEFAULT 0;");
		Execute("ALTER TABLE `inventory` ADD COLUMN IF NOT EXISTS `item_metadata` TEXT DEFAULT NULL;");
		Execute("CREATE TABLE IF NOT EXISTS `ai_ltm` ("
				"`botId` INT UNSIGNED NOT NULL, "
				"`targetId` INT UNSIGNED NOT NULL, "
				"`trustScore` FLOAT NOT NULL DEFAULT 0, "
				"`dangerScore` FLOAT NOT NULL DEFAULT 0, "
				"PRIMARY KEY (`botId`, `targetId`)"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
		Execute("CREATE TABLE IF NOT EXISTS `abilities` ("
				"`charId` BIGINT(30) UNSIGNED NOT NULL, "
				"`abilityId` SMALLINT(6) UNSIGNED NOT NULL, "
				"`level` SMALLINT(6) UNSIGNED NOT NULL DEFAULT 1, "
				"`slot` SMALLINT(6) UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (`charId`, `abilityId`)"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
		Execute("CREATE TABLE IF NOT EXISTS `crews` ("
				"`id` INT(11) UNSIGNED NOT NULL AUTO_INCREMENT, "
				"`name` VARCHAR(64) NOT NULL, "
				"`faction` INT(11) UNSIGNED NOT NULL DEFAULT 0, "
				"`leader_goid` INT(11) UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (`id`)"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
		Execute("CREATE TABLE IF NOT EXISTS `crew_members` ("
				"`crew_id` INT(11) UNSIGNED NOT NULL, "
				"`member_goid` INT(11) UNSIGNED NOT NULL, "
				"`rank` TINYINT(3) UNSIGNED NOT NULL DEFAULT 1, "
				"PRIMARY KEY (`crew_id`, `member_goid`)"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
		Execute("CREATE TABLE IF NOT EXISTS `territory_map` ("
				"`territory_id` INT(11) UNSIGNED NOT NULL, "
				"`faction` INT(11) UNSIGNED NOT NULL DEFAULT 0, "
				"`control_points` INT(11) UNSIGNED NOT NULL DEFAULT 0, "
				"PRIMARY KEY (`territory_id`, `faction`)"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8;");
		Execute("INSERT IGNORE INTO `territory_map` (`territory_id`, `faction`, `control_points`) VALUES (1, 1, 0), (1, 2, 0), (1, 3, 0);");
		INFO_LOG("Database: Schema migrations and table checks completed successfully.");
	}

	return true;
}

DatabaseConnection &Database::GetFreeConnection()
{
	std::unique_lock<std::mutex> lock(m_poolMutex);
	m_poolCond.wait(lock, [this]() { return !m_freeConnections.empty(); });
	DatabaseConnection* con = m_freeConnections.front();
	m_freeConnections.pop();
	return *con;
}

void Database::ReleaseConnection(DatabaseConnection &con)
{
	std::lock_guard<std::mutex> lock(m_poolMutex);
	m_freeConnections.push(&con);
	m_poolCond.notify_one();
}

QueryResult *Database::Query( string QueryString )
{
	// Send the query
	QueryResult * qResult = NULL;
	DatabaseConnection &con = GetFreeConnection();

	if( _SendQuery( con, QueryString.c_str(), false ) )
		qResult = _StoreQueryResult( con );

	ReleaseConnection(con);
	return qResult;
}

QueryResult * Database::FQuery( string QueryString, DatabaseConnection &con)
{	
	// Send the query
	QueryResult * qResult = NULL;
	if( _SendQuery( con, QueryString.c_str(), false ) )
		qResult = _StoreQueryResult( con );

	return qResult;
}

void Database::FWaitExecute( string QueryString, DatabaseConnection &con)
{	
	// Send the query
	_SendQuery( con, QueryString.c_str(), false );
}

bool Database::ExecutePrepared(PreparedStatement* stmt)
{
	if (!stmt)
		return false;

	return Execute(stmt->GetQueryString(this));
}

QueryResult* Database::QueryPrepared(PreparedStatement* stmt)
{
	if (!stmt)
		return nullptr;

	return Query(stmt->GetQueryString(this));
}

void QueryBuffer::AddQuery(string fmt)
{
	queries.push_back(fmt);
}

void Database::PerformQueryBuffer(QueryBuffer * b, DatabaseConnection &con)
{
	if(!b->queries.size())
		return;

	while (b->queries.size() > 0)
	{
		string &currQuery = b->queries.front();
		_SendQuery(con, currQuery.c_str(), false);
		b->queries.pop_front();
	}
}

void Database::ExecuteAsync( string QueryString )
{
    if (m_isMockMode) return;
    
    string* q = new string(QueryString);
    queries_queue.push(q);
}

void Database::PerformQueryBuffer(QueryBuffer * b)
{
	if(!b->queries.size())
		return;

	DatabaseConnection &con = GetFreeConnection();

	while (b->queries.size() > 0)
	{
		string &currQuery = b->queries.front();
		_SendQuery(con, currQuery.c_str(), false);
		b->queries.pop_front();
	}

	ReleaseConnection(con);
}


bool Database::Execute( string QueryString)
{
	if(!ThreadRunning)
		return WaitExecute(QueryString);

	queries_queue.push(new string(QueryString));
	return true;
}

//this will wait for completion
bool Database::WaitExecute( string QueryString)
{
	DatabaseConnection &con = GetFreeConnection();
	bool Result = _SendQuery(con, QueryString.c_str(), false);
	ReleaseConnection(con);
	return Result;
}

bool Database::run()
{
	SetThreadName("Database Executor");
	ThreadRunning = true;
	string *query = queries_queue.pop();
	DatabaseConnection &con = GetFreeConnection();
	while(query)
	{
		_SendQuery( con, query->c_str(), false );
		delete query;
		if(!m_threadRunning)
			break;

		query = queries_queue.pop();
	}

	ReleaseConnection(con);

	if(queries_queue.get_size() > 0)
	{
		// execute all the remaining queries
		query = queries_queue.pop_nowait();
		while(query)
		{
			DatabaseConnection &con = GetFreeConnection();
			_SendQuery( con, query->c_str(), false );
			ReleaseConnection(con);
			delete query;
			query=queries_queue.pop_nowait();
		}
	}

	ThreadRunning = false;
	return false;
}

void AsyncQuery::AddQuery( string fmt )
{
	AsyncQueryResult res;
	res.query = NULL;
	res.result = NULL;
	size_t len = fmt.length();
	if(len>0)
	{
		res.query = new char[len+1];
		res.query[len] = 0;
		memcpy(res.query, fmt.c_str(), len);
		queries.push_back(res);
	}
}

void AsyncQuery::Perform()
{
	DatabaseConnection &conn = db->GetFreeConnection();
	for(vector<AsyncQueryResult>::iterator itr = queries.begin(); itr != queries.end(); ++itr)
		itr->result = db->FQuery(itr->query, conn);

	db->ReleaseConnection(conn);
	func->run(queries);

	delete this;
}

AsyncQuery::~AsyncQuery()
{
	delete func;
	for(vector<AsyncQueryResult>::iterator itr = queries.begin(); itr != queries.end(); ++itr)
	{
		if(itr->result)
			delete itr->result;

		delete[] itr->query;
	}
	queries.clear();
}

void Database::EndThreads()
{
	Terminate();
	while(ThreadRunning || qt)
	{
		if(query_buffer.get_size() == 0)
			query_buffer.GetCond().Broadcast();

		if(queries_queue.get_size() == 0)
			queries_queue.GetCond().Broadcast();


		Sleep(100);
		if(!ThreadRunning)
			break;

		Sleep(1000);
	}
}

bool QueryThread::run( )
{
	db->thread_proc_query( );
	return true;
}

QueryThread::~QueryThread()
{
	db->qt = NULL;
}

void Database::thread_proc_query()
{
	QueryBuffer * q;
	DatabaseConnection &con = GetFreeConnection();

	q = query_buffer.pop( );
	while( q != NULL )
	{
		PerformQueryBuffer( q, con );
		delete q;

		if( !m_threadRunning )
			break;

		q = query_buffer.pop( );
	}

	ReleaseConnection(con);

	// kill any queries
	q = query_buffer.pop_nowait( );
	while( q != NULL )
	{
		PerformQueryBuffer( q );
		delete q;

		q = query_buffer.pop_nowait( );
	}
}

void Database::QueueAsyncQuery(AsyncQuery * query)
{
	query->db = this;
	/*if(qt == NULL)
	{
	query->Perform();
	return;
	}

	qqueries_queue.push(query);*/
	query->Perform();
}

void Database::AddQueryBuffer(QueryBuffer * b)
{
	if( qt != NULL )
		query_buffer.push( b );
	else
	{
		PerformQueryBuffer( b );
		delete b;
	}
}

void Database::FreeQueryResult(QueryResult * p)
{
	delete p;
}

string Database::EscapeString(std::string Escape)
{
	if (m_isMockMode) return Escape;

	char a2[16384] = {0};

	DatabaseConnection &con = GetFreeConnection();
	const char * ret;
	if(mysql_real_escape_string(con.conn, a2, Escape.c_str(), (unsigned long)Escape.length()) == 0)
		ret = Escape.c_str();
	else
		ret = a2;

	ReleaseConnection(con);
	return string(ret);
}

void Database::EscapeLongString(const char * str, uint32 len, stringstream& out)
{
	if (m_isMockMode) {
		out.write(str, len);
		return;
	}
	char a2[65536*3] = {0};

	DatabaseConnection &con = GetFreeConnection();
	const char * ret;
	if(mysql_real_escape_string(con.conn, a2, str, (unsigned long)len) == 0)
		ret = str;
	else
		ret = a2;

	out.write(a2, (std::streamsize)strlen(a2));
	ReleaseConnection(con);
}

string Database::EscapeString(const char * esc, DatabaseConnection * con)
{
	if (m_isMockMode || !con || !con->conn) return string(esc);
	char a2[16384] = {0};
	const char * ret;
	if(mysql_real_escape_string(con->conn, a2, (char*)esc, (unsigned long)strlen(esc)) == 0)
		ret = esc;
	else
		ret = a2;

	return string(ret);
}

bool Database::_SendQuery(DatabaseConnection &con, const char* Sql, bool Self)
{
	if (m_isMockMode) return true;
	mysql_thread_init();
	//dunno what it does ...leaving untouched 
	int result = mysql_query(con.conn, Sql);
	if(result > 0)
	{
		if( Self == false && _HandleError(con, mysql_errno( con.conn ) ) )
		{
			// Re-send the query, the connection was successful.
			// The true on the end will prevent an endless loop here, as it will
			// stop after sending the query twice.
			result = _SendQuery(con, Sql, true);
		}
		else
			ERROR_LOG(format("Sql query failed due to [%1%], Query: [%2%]\n") % mysql_error( con.conn ) % Sql);
	}

	return (result == 0 ? true : false);
}

bool Database::_HandleError(DatabaseConnection &con, uint32 ErrorNumber)
{
	// Handle errors that should cause a reconnect to the Database.
	switch(ErrorNumber)
	{
	case 2006:  // Mysql server has gone away
	case 2008:  // Client ran out of memory
	case 2013:  // Lost connection to sql server during query
	case 2055:  // Lost connection to sql server - system error
		{
			// Let's instruct a reconnect to the db when we encounter these errors.
			return _Reconnect( con );
		}break;
	}

	return false;
}

QueryResult::QueryResult(MYSQL_RES *res, uint32 fields, uint32 rows) : mResult(res), mFieldCount(fields), mRowCount(rows)
{
	mCurrentRow = new Field[fields];
}

QueryResult::~QueryResult()
{
	mysql_free_result(mResult);
	delete [] mCurrentRow;
}

bool QueryResult::NextRow()
{
	MYSQL_ROW row = mysql_fetch_row(mResult);
	if(row == NULL)
		return false;

	for(uint32 i = 0; i < mFieldCount; ++i)
		mCurrentRow[i].SetValue(row[i]);

	return true;
}

QueryResult * Database::_StoreQueryResult(DatabaseConnection &con)
{
	if (m_isMockMode) return NULL;
	QueryResult *res;
	MYSQL_RES * pRes = mysql_store_result( con.conn );
	uint32 uFields = (uint32)mysql_field_count( con.conn );
	// Use mysql_num_rows on the stored result, NOT mysql_affected_rows on the
	// connection: for a SELECT the latter is connection-state dependent and can
	// return 0 (e.g. when the query ran on a worker thread), which made valid
	// single-row results — like the auth user lookup — get dropped as NULL.
	uint32 uRows = pRes ? (uint32)mysql_num_rows( pRes ) : 0;

	if( uRows == 0 || uFields == 0 || pRes == 0 )
	{
		if( pRes != NULL )
			mysql_free_result( pRes );

		return NULL;
	}

	res = new QueryResult( pRes, uFields, uRows );
	res->NextRow();

	return res;
}

bool Database::_Reconnect(DatabaseConnection &conn)
{
	MYSQL * temp, *temp2;

	temp = mysql_init( NULL );
	temp2 = mysql_real_connect( temp, mHostname.c_str(), mUsername.c_str(), mPassword.c_str(), mDatabaseName.c_str(), mPort, NULL , 0 );
	if( temp2 == NULL )
	{
		CRITICAL_LOG(format("Could not reconnect to database because of `%1%`") % mysql_error( temp ) );
		mysql_close( temp );
		return false;
	}

	if( conn.conn != NULL )
		mysql_close( conn.conn );

	conn.conn = temp;
	return true;
}

void Database::CleanupLibs()
{
	mysql_library_end();
}

Database *Database::Create()
{
	return new Database();
}

void Database::Shutdown()
{
	for(connectionsList::iterator it=m_connections.begin();it!=m_connections.end();++it)
	{
		if( (*it)->conn != NULL )
		{
			mysql_close((*it)->conn);
			(*it)->conn = NULL;
		}
		delete *it;
	}
	m_connections.clear();
}
