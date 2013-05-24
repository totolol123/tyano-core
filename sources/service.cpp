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
#include "service.h"

#include "connection.h"
#include "outputmessage.h"
#include "textlogger.h"

#include "game.h"
#include "configmanager.h"
#include "scheduler.h"
#include "schedulertask.h"
#include "server.h"


LOGGER_DEFINITION(ServicePort);


bool ServicePort::add(Service_ptr newService)
{
	for(ServiceVec::const_iterator it = m_services.begin(); it != m_services.end(); ++it)
	{
		if((*it)->isSingleSocket())
			return false;
	}

	m_services.push_back(newService);
	return true;
}

void ServicePort::onOpen(std::weak_ptr<ServicePort> weakService, uint16_t port)
{
	LOGt("ServicePort::onOpen()");

	if(weakService.expired())
		return;

	if(ServicePort_ptr service = weakService.lock())
	{
		service->open(port);
	}
}

bool ServicePort::open(uint16_t port)
{
	using namespace boost::asio::ip;

	LOGt("ServicePort::open()");

	m_serverPort = port;
	m_pendingStart = false;

	tcp::endpoint endpoint;
	if(server.configManager().getBool(ConfigManager::BIND_IP_ONLY))
		endpoint = tcp::endpoint(address_v4::from_string(server.configManager().getString(ConfigManager::IP)), port);
	else
		endpoint = tcp::endpoint(address_v4::any(), port);

	try
	{
		m_acceptor = new tcp::acceptor(m_io_service, endpoint);
		accept();

		return true;
	}
	catch(boost::system::system_error& e)
	{
		LOGe("ServicePort::open(" << endpoint << ") - " << e.what());

		m_pendingStart = true;
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(5000), std::bind(
			&ServicePort::onOpen, std::weak_ptr<ServicePort>(shared_from_this()), m_serverPort)));
		return false;
	}
}

void ServicePort::close()
{
	if(!m_acceptor)
		return;

	if(m_acceptor->is_open())
	{
		boost::system::error_code error;
		m_acceptor->close(error);
		if(error) {
			PRINT_ASIO_ERROR("Closing listen socket");
		}
	}

	delete m_acceptor;
	m_acceptor = nullptr;
}

void ServicePort::accept()
{
	if(!m_acceptor)
		return;

	try
	{
		// FIXME leak!
		boost::asio::ip::tcp::socket* socket = new boost::asio::ip::tcp::socket(m_io_service);
		m_acceptor->async_accept(*socket, boost::bind(
			&ServicePort::handle, this, socket, boost::asio::placeholders::error));
	}
	catch(boost::system::system_error& e)
	{
		LOGe("ServicePort::accept() - " << e.what());
	}
}

void ServicePort::handle(boost::asio::ip::tcp::socket* socket, const boost::system::error_code& error)
{
	if(!error)
	{
		if(m_services.empty())
		{
#ifdef __DEBUG_NET__
			LOGe("[ServerPort::handle] No services running!");
#endif
			return;
		}

		boost::system::error_code error;
		const boost::asio::ip::tcp::endpoint ip = socket->remote_endpoint(error);

		uint32_t remoteIp = 0;
		if(!error)
			remoteIp = htonl(ip.address().to_v4().to_ulong());

		Connection_ptr connection;
		if(remoteIp && ConnectionManager::getInstance()->acceptConnection(remoteIp) &&
			(connection = ConnectionManager::getInstance()->createConnection(
			socket, m_io_service, shared_from_this())))
		{
			if(m_services.front()->isSingleSocket())
				connection->handle(m_services.front()->makeProtocol(connection));
			else
				connection->accept();
		}
		else if(socket->is_open())
		{
			boost::system::error_code error;
			socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, error);

			socket->close(error);
			delete socket;
		}

#ifdef __DEBUG_NET_DETAIL__
		LOGt("[ServerPort::handle] OK");
#endif
		accept();
	}
	else if(error != boost::asio::error::operation_aborted)
	{
		PRINT_ASIO_ERROR("Handling");
		close();
		if(!m_pendingStart)
		{
			m_pendingStart = true;
			server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(5000), std::bind(
				&ServicePort::onOpen, std::weak_ptr<ServicePort>(shared_from_this()), m_serverPort)));
		}
	}
#ifdef __DEBUG_NET__
	else
		LOGe("[ServerPort::handle] Operation aborted.");
#endif
}

std::string ServicePort::getProtocolNames() const
{
	if(m_services.empty())
		return "";

	std::string str = m_services.front()->getProtocolName();
	for(int32_t i = 1, j = m_services.size(); i < j; ++i)
	{
		str += ", ";
		str += m_services[i]->getProtocolName();
	}

	return str;
}

Protocol* ServicePort::makeProtocol(bool checksum, NetworkMessage& msg) const
{
	uint8_t protocolId = msg.GetByte();
	for(ServiceVec::const_iterator it = m_services.begin(); it != m_services.end(); ++it)
	{
		if((*it)->getProtocolId() == protocolId && ((checksum && (*it)->hasChecksum()) || !(*it)->hasChecksum()))
			return (*it)->makeProtocol(Connection_ptr());
	}

	return nullptr;
}



LOGGER_DEFINITION(ServiceManager);


void ServiceManager::run()
{
	assert(!running);
	try
	{
		running = true;
		m_io_service.run();
	}
	catch(boost::system::system_error& e)
	{
		running = false;
		LOGe("ServiceManager::run() - " << e.what());
	}
}

void ServiceManager::stop()
{
	if(!running)
		return;

	running = false;
	for(AcceptorsMap::iterator it = m_acceptors.begin(); it != m_acceptors.end(); ++it)
	{
		try
		{
			m_io_service.post(std::bind(&ServicePort::close, it->second));
		}
		catch(boost::system::system_error& e)
		{
			LOG_MESSAGE(LogType::ERROR, e.what(), "NETWORK")
		}
	}

	m_acceptors.clear();
	OutputMessagePool::getInstance()->stop();

	deathTimer.expires_from_now(boost::posix_time::seconds(3));
	deathTimer.async_wait(std::bind(&ServiceManager::die, this));

	m_io_service.stop();
}

std::list<uint16_t> ServiceManager::getPorts() const
{
	std::list<uint16_t> ports;
	for(AcceptorsMap::const_iterator it = m_acceptors.begin(); it != m_acceptors.end(); ++it)
		ports.push_back(it->first);

	ports.unique();
	return ports;
}
