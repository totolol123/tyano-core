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

#include "protocol.h"
#include "scheduler.h"

#include "connection.h"
#include "outputmessage.h"

#include "schedulertask.h"
#include "server.h"
#include "tools.h"
#include "rsa.h"


LOGGER_DEFINITION(Protocol);


extern RSA g_RSA;

void Protocol::onSendMessage(OutputMessage_ptr msg)
{
	LOGt("Protocol::onSendMessage()");

	if(!m_rawMessages)
	{
		msg->writeMessageLength();
		if(m_encryptionEnabled)
		{
			LOGt("Protocol::onSendMessage() - encrypt");
			XTEA_encrypt(*msg);
		}

		if(m_checksumEnabled)
		{
			LOGt("Protocol::onSendMessage() - add crypto header");
			msg->addCryptoHeader(m_checksumEnabled);
		}
	}

	if(msg == m_outputBuffer)
		m_outputBuffer.reset();
}

void Protocol::onRecvMessage(NetworkMessage& msg)
{
	LOGt("Protocol::onRecvMessage()");

	if(m_encryptionEnabled)
	{
		LOGt("Protocol::onRecvMessage() - decrypt");
		XTEA_decrypt(msg);
	}

	parsePacket(msg);
}

OutputMessage_ptr Protocol::getOutputBuffer()
{
	LOGt("Protocol::getOutputBuffer()");

	if(m_outputBuffer)
		return m_outputBuffer;

	if(m_connection)
	{
		m_outputBuffer = OutputMessagePool::getInstance()->getOutputMessage(this);
		return m_outputBuffer;
	}

	return OutputMessage_ptr();
}

void Protocol::releaseProtocol()
{
	if(m_refCount > 0)
		server.scheduler().addTask(SchedulerTask::create(std::chrono::milliseconds(SCHEDULER_MINTICKS), std::bind(&Protocol::releaseProtocol, this)));
	else
		deleteProtocolTask();
}

void Protocol::deleteProtocolTask()
{
	//dispather thread
	assert(!m_refCount);
	setConnection(Connection_ptr());
	delete this;
}

void Protocol::XTEA_encrypt(OutputMessage& msg)
{
	uint32_t k[4];
	for(uint8_t i = 0; i < 4; i++)
		k[i] = m_key[i];

	int32_t messageLength = msg.getMessageLength();
	//add bytes until reach 8 multiple
	uint32_t n;
	if((messageLength % 8) != 0)
	{
		n = 8 - (messageLength % 8);
		msg.AddPaddingBytes(n);
		messageLength = messageLength + n;
	}

	int32_t readPos = 0;
	uint32_t* buffer = (uint32_t*)msg.getOutputBuffer();
	while(readPos < messageLength / 4)
	{
		uint32_t v0 = buffer[readPos], v1 = buffer[readPos + 1], delta = 0x61C88647, sum = 0;
		for(int32_t i = 0; i < 32; i++)
		{
			v0 += ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + k[sum & 3]);
			sum -= delta;
			v1 += ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + k[sum>>11 & 3]);
		}

		buffer[readPos] = v0;
		buffer[readPos + 1] = v1;
		readPos += 2;
	}
}

bool Protocol::XTEA_decrypt(NetworkMessage& msg)
{
	if((msg.getMessageLength() - 6) % 8 != 0)
	{
		LOGe("[Protocol::XTEA_decrypt] Not valid encrypted message size (IP: " << convertIPAddress(getIP()) << ")");
		return false;
	}

	uint32_t k[4];
	for(uint8_t i = 0; i < 4; i++)
		k[i] = m_key[i];

	int32_t messageLength = msg.getMessageLength() - 6, readPos = 0;
	uint32_t* buffer = (uint32_t*)(msg.getBuffer() + msg.getReadPos());
	while(readPos < messageLength / 4)
	{
		uint32_t v0 = buffer[readPos], v1 = buffer[readPos + 1], delta = 0x61C88647, sum = 0xC6EF3720;
		for(int32_t i = 0; i < 32; i++)
		{
			v1 -= ((v0 << 4 ^ v0 >> 5) + v0) ^ (sum + k[sum>>11 & 3]);
			sum += delta;
			v0 -= ((v1 << 4 ^ v1 >> 5) + v1) ^ (sum + k[sum & 3]);
		}

		buffer[readPos] = v0;
		buffer[readPos + 1] = v1;
		readPos += 2;
	}
	//

	int32_t tmp = msg.GetU16();
	if(tmp > msg.getMessageLength() - 8)
	{
		LOGe("[Protocol::XTEA_decrypt] Not valid unencrypted message size (IP: " << convertIPAddress(getIP()) << ")");
		return false;
	}

	msg.setMessageLength(tmp);
	return true;
}

bool Protocol::RSA_decrypt(NetworkMessage& msg)
{
	return RSA_decrypt(&g_RSA, msg);
}

bool Protocol::RSA_decrypt(RSA* rsa, NetworkMessage& msg)
{
	if(msg.getMessageLength() - msg.getReadPos() != 128)
	{
		LOGe("[Protocol::RSA_decrypt] Not valid packet size");
		return false;
	}

	rsa->decrypt((char*)(msg.getBuffer() + msg.getReadPos()), 128);
	if(!msg.GetByte())
		return true;

	LOGe("[Protocol::RSA_decrypt] First byte != 0");
	return false;
}

uint32_t Protocol::getIP() const
{
	if(getConnection())
		return getConnection()->getIP();

	return 0;
}