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

#ifndef _SERVICE_H
#define _SERVICE_H

class Connection;
class Protocol;
class ServiceBase;
class ServicePort;

typedef std::shared_ptr<Connection>  Connection_ptr;
typedef std::shared_ptr<ServiceBase> Service_ptr;
typedef std::shared_ptr<ServicePort> ServicePort_ptr;


class ServiceBase : boost::noncopyable
{
	public:
		virtual ~ServiceBase() {}
		virtual Protocol* makeProtocol(const Connection_ptr& connection) const = 0;

		virtual uint8_t getProtocolId() const = 0;
		virtual bool isSingleSocket() const = 0;
		virtual bool hasChecksum() const = 0;
		virtual const char* getProtocolName() const = 0;
};

template <typename ProtocolType>
class Service : public ServiceBase
{
	public:
		Protocol* makeProtocol(const Connection_ptr& connection) const {return new ProtocolType(connection);}

		uint8_t getProtocolId() const {return ProtocolType::protocolId;}
		bool isSingleSocket() const {return ProtocolType::isSingleSocket;}
		bool hasChecksum() const {return ProtocolType::hasChecksum;}
		const char* getProtocolName() const {return ProtocolType::protocolName();}
};

class NetworkMessage;
class ServicePort : boost::noncopyable, public std::enable_shared_from_this<ServicePort>
{
	public:
		ServicePort(boost::asio::io_service& io_service): m_io_service(io_service),
			m_acceptor(nullptr), m_serverPort(0), m_pendingStart(false) {}
		~ServicePort() {close();}

		bool add(Service_ptr);
		static void onOpen(std::weak_ptr<ServicePort> weakService, uint16_t port);

		bool open(uint16_t port);
		void close();

		void handle(boost::asio::ip::tcp::socket* socket, const boost::system::error_code& error);

		bool isSingleSocket() const {return m_services.size() && m_services.front()->isSingleSocket();}
		std::string getProtocolNames() const;

		Protocol* makeProtocol(bool checksum, NetworkMessage& msg) const;

	private:

		void accept();


		LOGGER_DECLARATION;

		typedef std::vector<Service_ptr> ServiceVec;
		ServiceVec m_services;

		boost::asio::io_service& m_io_service;
		boost::asio::ip::tcp::acceptor* m_acceptor;

		uint16_t m_serverPort;
		bool m_pendingStart;
};



class ServiceManager : boost::noncopyable
{
	ServiceManager(const ServiceManager&);
	public:
		ServiceManager(): m_io_service(), deathTimer(m_io_service), running(false) {}
		~ServiceManager() {stop();}

		template <typename ProtocolType>
		bool add(uint16_t port);

		void run();
		void stop();

		bool isRunning() const {return !m_acceptors.empty();}
		std::list<uint16_t> getPorts() const;

	private:
		void die() {m_io_service.stop();}


		LOGGER_DECLARATION;

		boost::asio::io_service m_io_service;
		boost::asio::deadline_timer deathTimer;
		bool running;

		typedef std::map<uint16_t, ServicePort_ptr> AcceptorsMap;
		AcceptorsMap m_acceptors;
};

template <typename ProtocolType>
bool ServiceManager::add(uint16_t port)
{
	if(!port)
	{
		LOGe("No port provided for service " << ProtocolType::protocolName() << ". Service disabled.");
		return false;
	}

	ServicePort_ptr servicePort;
	AcceptorsMap::iterator it = m_acceptors.find(port);
	if(it == m_acceptors.end())
	{
		servicePort.reset(new ServicePort(m_io_service));
		if (!servicePort->open(port)) {
			return false;
		}
		m_acceptors[port] = servicePort;
	}
	else
	{
		servicePort = it->second;
		if(servicePort->isSingleSocket() || ProtocolType::isSingleSocket)
		{
			LOGe(ProtocolType::protocolName() << " and " << servicePort->getProtocolNames()
					<< " cannot use the same port (" << port << ").");
			return false;
		}
	}

	return servicePort->add(Service_ptr(new Service<ProtocolType>()));
}

#endif // _SERVICE_H
