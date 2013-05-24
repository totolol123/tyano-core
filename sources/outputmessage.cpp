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

#include "connection.h"
#include "dispatcher.h"
#include "outputmessage.h"
#include "protocol.h"
#include "server.h"
#include "task.h"
#include "tools.h"

LOGGER_DEFINITION(OutputMessagePool);


#ifdef __ENABLE_SERVER_DIAGNOSTIC__
uint32_t OutputMessagePool::outputMessagePoolCount = OUTPUT_POOL_SIZE;
#endif

OutputMessagePool::OutputMessagePool()
	: m_shutdown(false)
{
	for(uint32_t i = 0; i < OUTPUT_POOL_SIZE; ++i)
	{
		OutputMessage* msg = new OutputMessage();
		m_outputMessages.push_back(msg);
#ifdef __TRACK_NETWORK__
		m_allMessages.push_back(msg);
#endif
	}

	m_frameTime = OTSYS_TIME();
}

void OutputMessagePool::startExecutionFrame()
{
	LOGt("OutputMessagePool::startExecutionFrame()");

	//boost::recursive_mutex::scoped_lock lockClass(m_outputPoolLock);
	m_frameTime = OTSYS_TIME();
	m_shutdown = false;
}

OutputMessagePool::~OutputMessagePool()
{
	for(InternalList::iterator it = m_outputMessages.begin(); it != m_outputMessages.end(); ++it)
		delete (*it);

	m_outputMessages.clear();
}

void OutputMessagePool::send(OutputMessage_ptr msg)
{
	LOGt("OutputMessagePool::send()");

	m_outputPoolLock.lock();
	OutputMessage::OutputMessageState state = msg->getState();
	m_outputPoolLock.unlock();

	if(state == OutputMessage::STATE_ALLOCATED_NO_AUTOSEND)
	{
		if(msg->getConnection())
		{
			if(!msg->getConnection()->send(msg) && msg->getProtocol())
				msg->getProtocol()->onSendMessage(msg);
		}
		else
			LOGt("OutputMessagePool::send() - connection is null");
	}
	else
		LOGe("OutputMessagePool::send() - msg.state != STATE_ALLOCATED_NO_AUTOSEND");
}

void OutputMessagePool::sendAll()
{
	boost::recursive_mutex::scoped_lock lockClass(m_outputPoolLock);
	OutputMessageList::iterator it;
	for(it = m_addQueue.begin(); it != m_addQueue.end();)
	{
		//drop messages that are older than 10 seconds
		if(OTSYS_TIME() - (*it)->getFrame() > 10000)
		{
			if((*it)->getProtocol())
				(*it)->getProtocol()->onSendMessage(*it);

			it = m_addQueue.erase(it);
			continue;
		}

		(*it)->setState(OutputMessage::STATE_ALLOCATED);
		m_autoSend.push_back(*it);
		++it;
	}

	LOGt("OutputMessagePool::sendAll() - queue = " << m_addQueue.size() << " > autoSend = " << m_autoSend.size());

	m_addQueue.clear();
	for(it = m_autoSend.begin(); it != m_autoSend.end();)
	{
		OutputMessage_ptr omsg = (*it);
		#ifdef __NO_PLAYER_SENDBUFFER__
		//use this define only for debugging
		if(true)
		#else
		//It will send only messages bigger then 1 kb or with a lifetime greater than 10 ms
		if(omsg->getMessageLength() > 1024 || (m_frameTime - omsg->getFrame() > 10))
		#endif
		{
			LOGt("OutputMessagePool::sendAll() - sending message");

			if(omsg->getConnection())
			{
				if(!omsg->getConnection()->send(omsg) && omsg->getProtocol())
					omsg->getProtocol()->onSendMessage(omsg);
			}
			else
				LOGt("OutputMessagePool::sendAll() - connection is null");

			it = m_autoSend.erase(it);
		}
		else
			++it;
	}
}

void OutputMessagePool::releaseMessage(OutputMessage* msg)
{
	LOGt("OutputMessagePool::releaseMessage()");

	server.dispatcher().addTask(Task::create(std::bind(
		&OutputMessagePool::internalReleaseMessage, this, msg)), true);
}

void OutputMessagePool::internalReleaseMessage(OutputMessage* msg)
{
	LOGt("OutputMessagePool::internalReleaseMessage()");

	if(msg->getProtocol())
		msg->getProtocol()->unRef();
	else
		LOGe("[OutputMessagePool::internalReleaseMessage] protocol not found.");

	if(msg->getConnection())
		msg->getConnection()->unRef();
	else
		LOGe("[OutputMessagePool::internalReleaseMessage] connection not found.");

	msg->freeMessage();
#ifdef __TRACK_NETWORK__
	msg->clearTrack();
#endif

	m_outputPoolLock.lock();
	m_outputMessages.push_back(msg);
	m_outputPoolLock.unlock();
}

OutputMessage_ptr OutputMessagePool::getOutputMessage(Protocol* protocol, bool autoSend /*= true*/)
{
	LOGt("OutputMessagePool::getOutputMessage(autoSend = " << (autoSend ? "true" : "false") << ")");

	if(m_shutdown)
		return OutputMessage_ptr();

	boost::recursive_mutex::scoped_lock lockClass(m_outputPoolLock);
	if(!protocol->getConnection())
		return OutputMessage_ptr();

	if(m_outputMessages.empty())
	{
#ifdef __ENABLE_SERVER_DIAGNOSTIC__
		outputMessagePoolCount++;
#endif
		OutputMessage* msg = new OutputMessage();
		m_outputMessages.push_back(msg);
#ifdef __TRACK_NETWORK__
		m_allMessages.push_back(msg);
#endif
	}

	OutputMessage_ptr omsg;
	omsg.reset(m_outputMessages.back(),
		std::bind(&OutputMessagePool::releaseMessage, this, std::placeholders::_1));

	m_outputMessages.pop_back();
	configureOutputMessage(omsg, protocol, autoSend);
	return omsg;
}

void OutputMessagePool::configureOutputMessage(OutputMessage_ptr msg, Protocol* protocol, bool autoSend)
{
	LOGt("OutputMessagePool::configureOutputMessage(autoSend = " << (autoSend ? "true" : "false") << ")");

	TRACK_MESSAGE(msg);
	msg->Reset();
	if(autoSend)
	{
		msg->setState(OutputMessage::STATE_ALLOCATED);
		m_autoSend.push_back(msg);
	}
	else
		msg->setState(OutputMessage::STATE_ALLOCATED_NO_AUTOSEND);

	Connection_ptr connection = protocol->getConnection();
	assert(connection);

	msg->setProtocol(protocol);
	protocol->addRef();
	msg->setConnection(connection);
	connection->addRef();

	msg->setFrame(m_frameTime);
}

void OutputMessagePool::autoSend(OutputMessage_ptr msg)
{
	LOGt("OutputMessagePool::autoSend()");

	m_outputPoolLock.lock();
	m_addQueue.push_back(msg);
	m_outputPoolLock.unlock();
}



LOGGER_DEFINITION(OutputMessage);


void OutputMessage::addCryptoHeader(bool addChecksum)
{
	if(addChecksum)
		addHeader((uint32_t)(adlerChecksum((uint8_t*)(m_MsgBuf + m_outputBufferStart), m_MsgSize)));

	addHeader((uint16_t)(m_MsgSize));
}
