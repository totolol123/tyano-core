//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"
#include "connection.h"

#include "protocol.h"
#include "protocolgame.h"
#include "protocolold.h"
#include "admin.h"
#include "status.h"

#include "outputmessage.h"
#include "scheduler.h"

#include "dispatcher.h"
#include "service.h"
#include "configmanager.h"
#include "tools.h"
#include "task.h"
#include "textlogger.h"
#include "server.h"
#include "schedulertask.h"

bool Connection::m_logError = true;

#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t Connection::connectionCount = 0;
#endif


LOGGER_DEFINITION(ConnectionManager);


Connection_ptr ConnectionManager::createConnection(boost::asio::ip::tcp::socket* socket,
	boost::asio::io_service& io_service, ServicePort_ptr servicer)
{
	LOGt("ConnectionManager::createConnection()");

	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);
	Connection_ptr connection = std::make_shared<Connection>(socket, io_service, servicer);

	m_connections.push_back(connection);
	return connection;
}

void ConnectionManager::releaseConnection(Connection_ptr connection)
{
	LOGt("ConnectionManager::releaseConnection()");

	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);

	std::list<Connection_ptr>::iterator it =std::find(m_connections.begin(), m_connections.end(), connection);
	if(it != m_connections.end())
		m_connections.erase(it);
	else
		LOGe("[ConnectionManager::releaseConnection] Connection not found");
}

void ConnectionManager::shutdown()
{
	LOGt("ConnectionManager::shutdown()");

	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);
	for(std::list<Connection_ptr>::iterator it = m_connections.begin(); it != m_connections.end(); ++it)
	{
		try
		{
			boost::system::error_code error;
			(*it)->m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			(*it)->m_socket->close(error);
		}
		catch(boost::system::system_error&) {}
	}

	m_connections.clear();
}

void Connection::close()
{
	//any thread
	LOGt("Connection::close()");

	boost::recursive_mutex::scoped_lock lockClass(m_connectionLock);
	if(m_connectionState == CONNECTION_STATE_CLOSED || m_connectionState == CONNECTION_STATE_REQUEST_CLOSE)
		return;

	m_connectionState = CONNECTION_STATE_REQUEST_CLOSE;
	server.dispatcher().addTask(Task::create(std::bind(&Connection::closeConnection, this)));
}

bool ConnectionManager::isDisabled(uint32_t clientIp, int32_t protocolId)
{
	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);
	int32_t maxLoginTries = server.configManager().getNumber(ConfigManager::LOGIN_TRIES);
	if(!maxLoginTries || !clientIp)
		return false;

	IpLoginMap::const_iterator it = ipLoginMap.find(clientIp);
	return it != ipLoginMap.end() && it->second.lastProtocol != protocolId && it->second.loginsAmount > maxLoginTries
		&& (int32_t)time(nullptr) < it->second.lastLogin + server.configManager().getNumber(ConfigManager::LOGIN_TIMEOUT) / 1000;
}

void ConnectionManager::addAttempt(uint32_t clientIp, int32_t protocolId, bool success)
{
	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);
	if(!clientIp)
		return;

	IpLoginMap::iterator it = ipLoginMap.find(clientIp);
	if(it == ipLoginMap.end())
	{
		LoginBlock tmp;
		tmp.lastLogin = tmp.loginsAmount = 0;
		tmp.lastProtocol = 0x00;

		ipLoginMap[clientIp] = tmp;
		it = ipLoginMap.find(clientIp);
        }

	if(it->second.loginsAmount > server.configManager().getNumber(ConfigManager::LOGIN_TRIES))
		it->second.loginsAmount = 0;

	int32_t currentTime = time(nullptr);
	if(!success || (currentTime < it->second.lastLogin + (int32_t)server.configManager().getNumber(ConfigManager::RETRY_TIMEOUT) / 1000))
		it->second.loginsAmount++;
	else
		it->second.loginsAmount = 0;

	it->second.lastLogin = currentTime;
	it->second.lastProtocol = protocolId;
}

bool ConnectionManager::acceptConnection(uint32_t clientIp)
{
	LOGt("Connection::acceptConnection(clientIp = " << boost::asio::ip::address_v4(clientIp) << ")");

	if(!clientIp)
		return false;

	boost::recursive_mutex::scoped_lock lockClass(m_connectionManagerLock);
	uint64_t currentTime = OTSYS_TIME();

	IpConnectMap::iterator it = ipConnectMap.find(clientIp);
	if(it == ipConnectMap.end())
	{
		ConnectBlock tmp;
		tmp.startTime = currentTime;
		tmp.blockTime = 0;
		tmp.count = 1;

		ipConnectMap[clientIp] = tmp;
		return true;
        }

	it->second.count++;
	if(it->second.blockTime > currentTime)
		return false;

	if(currentTime - it->second.startTime > 1000)
	{
		uint32_t tmp = it->second.count;
		it->second.startTime = currentTime;

		it->second.count = it->second.blockTime = 0;
		if(tmp > 10)
		{
			it->second.blockTime = currentTime + 10000;
			return false;
		}
	}

	return true;
}



LOGGER_DEFINITION(Connection);


void Connection::closeConnection()
{
	//dispatcher thread
	LOGt("Connection::closeConnection()");

	m_connectionLock.lock();
	if(m_connectionState != CONNECTION_STATE_REQUEST_CLOSE)
	{
		LOGe("[Connection::closeConnection] m_connectionState = " << m_connectionState);
		m_connectionLock.unlock();
		return;
	}

	if(m_protocol)
	{
		m_protocol->setConnection(Connection_ptr());
		m_protocol->releaseProtocol();
		m_protocol = nullptr;
	}

	m_connectionState = CONNECTION_STATE_CLOSING;
	if(!m_pendingWrite || m_writeError)
	{
		closeSocket();
		releaseConnection();
		m_connectionState = CONNECTION_STATE_CLOSED;
	}

	m_connectionLock.unlock();
}

void Connection::closeSocket()
{
	LOGt("Connection::closeSocket()");

	m_connectionLock.lock();
	if(m_socket->is_open())
	{
		m_pendingRead = m_pendingWrite = 0;
		try
		{
			boost::system::error_code error;
			m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			m_socket->close(error);
		}
		catch(boost::system::system_error& e)
		{
			if(m_logError)
			{
				LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
				m_logError = false;
			}
		}
	}

	m_connectionLock.unlock();
}

void Connection::releaseConnection()
{
	LOGt("Connection::releaseConnection()");

	if(m_refCount > 0) //Reschedule it and try again.
		server.scheduler().addTask(SchedulerTask::create(Milliseconds(SCHEDULER_MINTICKS),
			std::bind(&Connection::releaseConnection, this)));
	else
		deleteConnection();
}

void Connection::onStop()
{
	LOGt("Connection::onStop()");

	//service thread
	m_connectionLock.lock();
	m_readTimer.cancel();
	m_writeTimer.cancel();

	try
	{
		if(m_socket->is_open())
		{
			boost::system::error_code error;
			m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);
			m_socket->close();
		}
	}
	catch(boost::system::system_error&) {}
	delete m_socket;
	m_socket = nullptr;

	m_connectionLock.unlock();
	ConnectionManager::getInstance()->releaseConnection(shared_from_this());
}

void Connection::deleteConnection()
{
	LOGt("Connection::deleteConnection()");

	//dispather thread
	assert(!m_refCount);
	try
	{
		m_service.dispatch(std::bind(&Connection::onStop, this));
	}
	catch(boost::system::system_error& e)
	{
		if(m_logError)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
			m_logError = false;
		}
	}
}

void Connection::handle(Protocol* protocol)
{
	LOGt("Connection::handle()");

	m_protocol = protocol;
	m_protocol->onConnect();
	accept();
}

void Connection::accept()
{
	LOGt("Connection::accept()");

	try
	{
		++m_pendingRead;
		m_readTimer.expires_from_now(boost::posix_time::seconds(Connection::readTimeout));
		m_readTimer.async_wait(boost::bind(&Connection::handleReadTimeout,
			std::weak_ptr<Connection>(shared_from_this()), boost::asio::placeholders::error));

		// Read size of the first packet
		boost::asio::async_read(getHandle(),
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::headerLength),
			std::bind(&Connection::parseHeader, shared_from_this(), std::placeholders::_1));
	}
	catch(boost::system::system_error& e)
	{
		if(m_logError)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
			m_logError = false;
			close();
		}
	}
}

void Connection::parseHeader(const boost::system::error_code& error)
{
	LOGt("Connection::parseHeader()");

	m_connectionLock.lock();
	m_readTimer.cancel();

	int32_t size = m_msg.decodeHeader();
	if(error || size <= 0 || size >= NETWORKMESSAGE_MAXSIZE - 16)
		handleReadError(error);

	if(m_connectionState != CONNECTION_STATE_OPEN || m_readError)
	{
		close();
		m_connectionLock.unlock();
		return;
	}

	--m_pendingRead;
	try
	{
		++m_pendingRead;
		m_readTimer.expires_from_now(boost::posix_time::seconds(Connection::readTimeout));
		m_readTimer.async_wait(boost::bind(&Connection::handleReadTimeout,
			std::weak_ptr<Connection>(shared_from_this()), boost::asio::placeholders::error));

		// Read packet content
		m_msg.setMessageLength(size + NetworkMessage::headerLength);
		boost::asio::async_read(getHandle(), boost::asio::buffer(m_msg.getBodyBuffer(), size),
				std::bind(&Connection::parsePacket, shared_from_this(), std::placeholders::_1));
	}
	catch(boost::system::system_error& e)
	{
		if(m_logError)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
			m_logError = false;
			close();
		}
	}

	m_connectionLock.unlock();
}

void Connection::parsePacket(const boost::system::error_code& error)
{
	LOGt("Connection::parsePacket()");

	m_connectionLock.lock();
	m_readTimer.cancel();
	if(error)
		handleReadError(error);

	if(m_connectionState != CONNECTION_STATE_OPEN || m_readError)
	{
		close();
		m_connectionLock.unlock();
		return;
	}

	--m_pendingRead;
	uint32_t length = m_msg.getMessageLength() - m_msg.getReadPos() - 4, checksumReceived = m_msg.PeekU32(), checksum = 0;
	if(length > 0)
		checksum = adlerChecksum((uint8_t*)(m_msg.getBuffer() + m_msg.getReadPos() + 4), length);

	bool checksumEnabled = false;
	if(checksumReceived == checksum)
	{
		m_msg.SkipBytes(4);
		checksumEnabled = true;
	}

	if(!m_receivedFirst)
	{
		// First message received
		m_receivedFirst = true;
		if(!m_protocol)
		{
			// Game protocol has already been created at this point
			m_protocol = m_servicePort->makeProtocol(checksumEnabled, m_msg);
			if(!m_protocol)
			{
				close();
				m_connectionLock.unlock();
				return;
			}

			m_protocol->setConnection(shared_from_this());
		}
		else
			m_msg.SkipBytes(1); // Skip protocol

		m_protocol->onRecvFirstMessage(m_msg);
	}
	else
		m_protocol->onRecvMessage(m_msg); // Send the packet to the current protocol

	try
	{
		++m_pendingRead;
		m_readTimer.expires_from_now(boost::posix_time::seconds(Connection::readTimeout));
		m_readTimer.async_wait(boost::bind(&Connection::handleReadTimeout,
				std::weak_ptr<Connection>(shared_from_this()), boost::asio::placeholders::error));

		// Wait to the next packet
		boost::asio::async_read(getHandle(),
			boost::asio::buffer(m_msg.getBuffer(), NetworkMessage::headerLength),
			std::bind(&Connection::parseHeader, shared_from_this(), std::placeholders::_1));
	}
	catch(boost::system::system_error& e)
	{
		if(m_logError)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
			m_logError = false;
			close();
		}
	}

	m_connectionLock.unlock();
}

bool Connection::send(OutputMessage_ptr msg)
{
	LOGt("Connection::send()");

	m_connectionLock.lock();
	if(m_connectionState != CONNECTION_STATE_OPEN || m_writeError)
	{
		m_connectionLock.unlock();
		return false;
	}

	TRACK_MESSAGE(msg);
	if(!m_pendingWrite)
	{
		if(msg->getProtocol())
			msg->getProtocol()->onSendMessage(msg);

		internalSend(msg);
	}
	else if(m_pendingWrite > 100 && server.configManager().getBool(ConfigManager::FORCE_CLOSE_SLOW_CONNECTION))
	{
		LOGd("Forcing slow connection to disconnect!");
		close();
	}
	else
	{	
		OutputMessagePool::getInstance()->autoSend(msg);
	}

	m_connectionLock.unlock();
	return true;
}

void Connection::internalSend(OutputMessage_ptr msg)
{
	TRACK_MESSAGE(msg);
	try
	{
		++m_pendingWrite;
		m_writeTimer.expires_from_now(boost::posix_time::seconds(Connection::writeTimeout));
		m_writeTimer.async_wait(boost::bind(&Connection::handleWriteTimeout,
			std::weak_ptr<Connection>(shared_from_this()), boost::asio::placeholders::error));

		boost::asio::async_write(getHandle(),
			boost::asio::buffer(msg->getOutputBuffer(), msg->getMessageLength()),
			std::bind(&Connection::onWrite, shared_from_this(), msg, std::placeholders::_1));
	}
	catch(boost::system::system_error& e)
	{
		if(m_logError)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK");
			m_logError = false;
		}
	}
}

uint32_t Connection::getIP() const
{
	//ip is expressed in network byte order
	boost::system::error_code error;
	const boost::asio::ip::tcp::endpoint ip = m_socket->remote_endpoint(error);
	if(!error)
		return htonl(ip.address().to_v4().to_ulong());

	return 0;
}

void Connection::onWrite(OutputMessage_ptr msg, const boost::system::error_code& error)
{
	LOGt("Connection::onWrite()");

	m_connectionLock.lock();
	m_writeTimer.cancel();

	TRACK_MESSAGE(msg);
	msg.reset();
	if(error)
		handleWriteError(error);

	if(m_connectionState != CONNECTION_STATE_OPEN || m_writeError)
	{
		closeSocket();
		close();

		m_connectionLock.unlock();
		return;
	}

	--m_pendingWrite;
	m_connectionLock.unlock();
}

void Connection::handleReadError(const boost::system::error_code& error)
{
	LOGt("Connection::handleReadError()");

	boost::recursive_mutex::scoped_lock lockClass(m_connectionLock);
	if(error == boost::asio::error::operation_aborted) //Operation aborted because connection will be closed
		{}
	else if(error == boost::asio::error::eof ||
		error == boost::asio::error::connection_reset ||
		error == boost::asio::error::connection_aborted)
	{
		//Connection closed remotely or nothing more to read
		close();
	}
	else
	{
		close();
	}

	m_readError = true;
}

void Connection::onReadTimeout()
{
	boost::recursive_mutex::scoped_lock lockClass(m_connectionLock);
	if(m_pendingRead > 0 || m_readError)
	{
		closeSocket();
		close();
	}
}

void Connection::onWriteTimeout()
{
	boost::recursive_mutex::scoped_lock lockClass(m_connectionLock);
	if(m_pendingWrite > 0 || m_writeError)
	{
		closeSocket();
		close();
	}
}

void Connection::handleReadTimeout(std::weak_ptr<Connection> weak, const boost::system::error_code& error)
{
	if(error == boost::asio::error::operation_aborted || weak.expired())
		return;

	if(std::shared_ptr<Connection> connection = weak.lock())
	{
		connection->onReadTimeout();
	}
}

void Connection::handleWriteError(const boost::system::error_code& error)
{
	boost::recursive_mutex::scoped_lock lockClass(m_connectionLock);
	if(error == boost::asio::error::operation_aborted) //Operation aborted because connection will be closed
		{}
	else if(error == boost::asio::error::eof ||
		error == boost::asio::error::connection_reset ||
		error == boost::asio::error::connection_aborted)
	{
		//Connection closed remotely or nothing more to read
		close();
	}
	else
	{
		close();
	}

	m_writeError = true;
}

void Connection::handleWriteTimeout(std::weak_ptr<Connection> weak, const boost::system::error_code& error)
{
	if(error == boost::asio::error::operation_aborted || weak.expired())
		return;

	if(std::shared_ptr<Connection> connection = weak.lock())
	{
		connection->onWriteTimeout();
	}
}
