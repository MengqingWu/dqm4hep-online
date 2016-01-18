  /// \file DQMDimEventClient.cc
/*
 *
 * DQMDimEventClient.cc source template automatically generated by a class generator
 * Creation date : mar. sept. 8 2015
 *
 * This file is part of DQM4HEP libraries.
 * 
 * DQM4HEP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * based upon these libraries are permitted. Any copy of these libraries
 * must include this copyright notice.
 * 
 * DQM4HEP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with DQM4HEP.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @author Remi Ete
 * @copyright CNRS , IPNL
 */

// -- dqm4hep headers
#include "dqm4hep/DQMDimEventClient.h"
#include "dqm4hep/DQMDataStream.h"
#include "dqm4hep/DQMEventStreamer.h"

namespace dqm4hep
{

DimEventRpcInfo::DimEventRpcInfo(DQMDimEventClient *pEventClient) :
	DimRpcInfo((char*) ("DQM4HEP/EventCollector/" + pEventClient->getCollectorName() + "/EVENT_RAW_BUFFER_UPDATE").c_str(), (void*) NULL, 0),
	m_pEventClient(pEventClient)
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

void DimEventRpcInfo::rpcInfoHandler()
{
	char *pBuffer = (char*) getData();
	unsigned int bufferSize = getSize();

	if(NULL == pBuffer || 0 == bufferSize)
		return;

	DQMEvent *pEvent = NULL;

	try
	{
		// lock/unlock on de-serialization if the interface
		// uses the sendEvent(evt) and queryEvent(...)
		// functionalities at the same time
		pthread_mutex_lock(&m_pEventClient->m_mutex);
		m_pEventClient->m_dataStream.setBuffer(pBuffer, bufferSize);
		StatusCode statusCode = m_pEventClient->getEventStreamer()->deserialize(pEvent, &m_pEventClient->m_dataStream);
		pthread_mutex_unlock(&m_pEventClient->m_mutex);

		if(statusCode != STATUS_CODE_SUCCESS)
			throw StatusCodeException(statusCode);
	}
	catch(const StatusCodeException &exception)
	{
		if(NULL != pEvent)
			delete pEvent;

		return;
	}

	StatusCode statusCode = m_pEventClient->addEvent(pEvent);

	if(statusCode != STATUS_CODE_SUCCESS)
		delete pEvent;

	return;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

DQMDimEventClient::DQMDimEventClient() :
	m_isConnected(false),
	m_collectorName("EventCollector"),
	m_pDimEventRpcInfo(NULL),
	m_maximumQueueSize(100),
	m_pEventStreamer(NULL),
	m_updateMode(false),
	m_serverClientId(0),
	m_dataStream(5*1024*1024) // 5 Mo should be enough to start ...
{
	pthread_mutex_init(&m_mutex, NULL);
}

//-------------------------------------------------------------------------------------------------

DQMDimEventClient::~DQMDimEventClient() 
{
	if(isConnectedToService())
		disconnectFromService();

	if(m_pEventStreamer)
		delete m_pEventStreamer;

	pthread_mutex_destroy(&m_mutex);
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::setCollectorName(const std::string &collectorName)
{
	if(isConnectedToService())
		return STATUS_CODE_ALREADY_INITIALIZED;

	m_collectorName = collectorName;

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

const std::string &DQMDimEventClient::getCollectorName() const
{
	return m_collectorName;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::connectToService()
{
	if(isConnectedToService())
		return STATUS_CODE_SUCCESS;

	DimClient::setExitHandler( ("DQM4HEP/EventCollector/" + getCollectorName() ).c_str() );

	m_pDimEventRpcInfo = new DimEventRpcInfo(this);

	m_pClientIdInfo = new DimUpdatedInfo(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTERED").c_str(), static_cast<int>(0), this);
	m_pServerStateInfo = new DimUpdatedInfo(("DQM4HEP/EventCollector/" + getCollectorName() + "/SERVER_STATE").c_str(), static_cast<int>(0), this);
	m_pEventUpdateInfo = new DimUpdatedInfo(("DQM4HEP/EventCollector/" + getCollectorName() + "/EVENT_RAW_UPDATE").c_str(), (void*) NULL, 0, this);

	m_isConnected = true;

	DimCurrentInfo serverStateInfo(("DQM4HEP/EventCollector/" + getCollectorName() + "/SERVER_STATE").c_str(), static_cast<int>(0));
	int serverRunning = serverStateInfo.getInt();

	if(!serverRunning)
	{
		streamlog_out(WARNING) << "Server collector application not running yet" << std::endl;
		return STATUS_CODE_SUCCESS;
	}

	// send command to register the client on the server
	// the server is expected to update m_pClientIdInfo info with the client id
	streamlog_out(MESSAGE) << "Registering client into server collector application !" << std::endl;
	DimClient::sendCommandNB(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTRATION").c_str(), 1);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::disconnectFromService()
{
	if(!isConnectedToService())
		return STATUS_CODE_SUCCESS;

	delete m_pDimEventRpcInfo;
	delete m_pClientIdInfo;
	delete m_pServerStateInfo;
	delete m_pEventUpdateInfo;

	m_isConnected = false;

	// un-register client on the server
	if(m_serverClientId > 0)
	{
		DimClient::sendCommandNB(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTRATION").c_str(), 0);
	}

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

bool DQMDimEventClient::isConnectedToService() const
{
	return m_isConnected;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::sendEvent(const DQMEvent *const pEvent)
{
	pthread_mutex_lock(&m_mutex);

	if(NULL == m_pEventStreamer)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_NOT_INITIALIZED;
	}

	pthread_mutex_unlock(&m_mutex);

	if(NULL == pEvent)
		return STATUS_CODE_INVALID_PARAMETER;

	// lock/unlock on de-serialization if the interface
	// uses the sendEvent(evt) and queryEvent(...)
	// functionalities at the same time
	pthread_mutex_lock(&m_mutex);

	m_dataStream.reset();
	StatusCode statusCode = m_pEventStreamer->serialize(pEvent, &m_dataStream);

	if(statusCode != STATUS_CODE_SUCCESS)
	{
		pthread_mutex_unlock(&m_mutex);
		return statusCode;
	}

	char *pBuffer = m_dataStream.getBuffer();
	unsigned int bufferSize = m_dataStream.getBufferSize();

	if(NULL == pBuffer || 0 == bufferSize)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_FAILURE;
	}

	std::string commandName = "DQM4HEP/EventCollector/" + this->getCollectorName() + "/COLLECT_RAW_EVENT";
	DimClient::sendCommandNB((char*) commandName.c_str(), (void *) pBuffer, bufferSize);

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::setEventStreamer(DQMEventStreamer *pEventStreamer)
{
	if(isConnectedToService())
		return STATUS_CODE_NOT_ALLOWED;

	pthread_mutex_lock(&m_mutex);

	if(NULL != m_pEventStreamer)
		delete m_pEventStreamer;

	m_pEventStreamer = pEventStreamer;

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

const DQMEventStreamer *DQMDimEventClient::getEventStreamer() const
{
	return m_pEventStreamer;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::setMaximumQueueSize(unsigned int queueSize)
{
	if(isConnectedToService())
		return STATUS_CODE_NOT_ALLOWED;

	pthread_mutex_lock(&m_mutex);

	if(queueSize == 0)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_INVALID_PARAMETER;
	}

	m_maximumQueueSize = queueSize;

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::clearQueue()
{
	pthread_mutex_lock(&m_mutex);

	while(!m_eventQueue.empty())
	{
		DQMEvent *pEvent = m_eventQueue.front();
		delete pEvent;
		m_eventQueue.pop();
	}

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventClient::setSubEventIdentifier(const std::string &identifier)
{
	pthread_mutex_lock(&m_mutex);

	m_subEventIdentifier = identifier;

	if(!isConnectedToService())
	{
		pthread_mutex_unlock(&m_mutex);
		return;
	}

	std::string subEventIdentifierCommandName = "DQM4HEP/EventCollector/" + m_collectorName + "/SUB_EVENT_IDENTIFIER";
	DimClient::sendCommandNB((char*) subEventIdentifierCommandName.c_str(), (char *) m_subEventIdentifier.c_str());

	pthread_mutex_unlock(&m_mutex);
}

//-------------------------------------------------------------------------------------------------

const std::string &DQMDimEventClient::getSubEventIdentifier() const
{
	return m_subEventIdentifier;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::queryEvent(DQMEvent *&pEvent, int timeout)
{
	// check for initialization
	if(!isConnectedToService())
		return STATUS_CODE_NOT_INITIALIZED;

	pthread_mutex_lock(&m_mutex);

	if(NULL == m_pEventStreamer)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_NOT_INITIALIZED;
	}

	// check for valid parameters
	if(timeout < 0)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_INVALID_PARAMETER;
	}

	// rpc service name
	std::string serviceName = "DQM4HEP/EventCollector/" + this->getCollectorName() + "/EVENT_RAW_REQUEST";
	pEvent = NULL;

	// declare the rpc service info
	DimRpcInfo eventDimRpcInfo((char*) serviceName.c_str(), timeout, (void*) NULL, 0);

	// send the query
	eventDimRpcInfo.setData((char*) m_subEventIdentifier.c_str());
	// receive the query result
	char *pEventRawBuffer = static_cast<char*>(eventDimRpcInfo.getData());
	int bufferSize = eventDimRpcInfo.getSize();

	// check for message validity
	if(NULL == pEventRawBuffer || 0 == bufferSize)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_FAILURE;
	}

	// deserialize the event raw buffer
	m_dataStream.setBuffer(pEventRawBuffer, bufferSize);
	StatusCode statusCode = m_pEventStreamer->deserialize(pEvent, &m_dataStream);
	pthread_mutex_unlock(&m_mutex);

	return statusCode;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::queryEvent()
{
	// check for initialization
	if(!isConnectedToService())
		return STATUS_CODE_NOT_INITIALIZED;

	pthread_mutex_lock(&m_mutex);

	if(NULL == m_pEventStreamer)
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_NOT_INITIALIZED;
	}

	pthread_mutex_unlock(&m_mutex);

	// send the query
	m_pDimEventRpcInfo->setData((char*) m_subEventIdentifier.c_str());

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

//StatusCode DQMDimEventClient::querySubEvent(const std::string &subEventIdentifier)
//{
//	// check for initialization
//	if(!isConnectedToService() || NULL == m_pEventStreamer)
//		return STATUS_CODE_NOT_INITIALIZED;
//
//	// send the query
//	std::string identifier = subEventIdentifier;
//	m_pDimEventRpcInfo->setData((char*) identifier.c_str());
//
//	return STATUS_CODE_SUCCESS;
//}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::takeEvent(DQMEvent *&pEvent)
{
	pthread_mutex_lock(&m_mutex);

	pEvent = NULL;

	if(m_eventQueue.empty())
	{
		pthread_mutex_unlock(&m_mutex);
		return STATUS_CODE_UNCHANGED;
	}

	// get oldest element
	pEvent = m_eventQueue.front();
	// and reduce the queue
	m_eventQueue.pop();

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventClient::setUpdateMode(bool updateMode)
{
	pthread_mutex_lock(&m_mutex);

	m_updateMode = updateMode;

	if(!isConnectedToService())
	{
		pthread_mutex_unlock(&m_mutex);
		return;
	}

	// send command to server
	DimClient::sendCommandNB((char*) (std::string("DQM4HEP/EventCollector/") + m_collectorName + "/UPDATE_MODE" ).c_str(), static_cast<int>(m_updateMode));

	pthread_mutex_unlock(&m_mutex);
}

//-------------------------------------------------------------------------------------------------

bool DQMDimEventClient::updateMode() const
{
	return m_updateMode;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMDimEventClient::addEvent(DQMEvent *pEvent)
{
	pthread_mutex_lock(&m_mutex);

	// reduce the event queue if fulfilled
	if(m_eventQueue.size() == m_maximumQueueSize)
	{
		DQMEvent *pOldestEvent = m_eventQueue.front();
		m_eventQueue.pop();
		delete pOldestEvent;
	}

	m_eventQueue.push(pEvent);

	pthread_mutex_unlock(&m_mutex);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

void DQMDimEventClient::infoHandler()
{
	DimInfo *pInfo = getInfo();

	if(!pInfo)
		return;

	// client has just been registered
	if(pInfo == m_pClientIdInfo)
	{
		int clientId = pInfo->getInt();

		streamlog_out(MESSAGE) << "Client id on server : " << clientId << std::endl;

		if(clientId <= 0)
			return;

		m_serverClientId = clientId;
		std::string subEventIdentifierCommandName = "DQM4HEP/EventCollector/" + m_collectorName + "/SUB_EVENT_IDENTIFIER";
		std::string updateModeCommandName = "DQM4HEP/EventCollector/" + m_collectorName + "/UPDATE_MODE";

		// send to server the client parameters
		DimClient::sendCommandNB((char*) updateModeCommandName.c_str(), static_cast<int>(m_updateMode));
		DimClient::sendCommandNB((char*) subEventIdentifierCommandName.c_str(), (char *) m_subEventIdentifier.c_str());

		return;
	}

	if(pInfo == m_pServerStateInfo)
	{
		int serverRunning = pInfo->getInt();

		// server is running
		if(serverRunning > 0)
		{
			// client is already registered, so nothing to do ...
			if(m_serverClientId > 0)
				return;
			// client is not registered yet, we need to do it !
			else
			{
				streamlog_out(MESSAGE) << "Registering client into event collector server application !" << std::endl;
				DimClient::sendCommandNB(("DQM4HEP/EventCollector/" + getCollectorName() + "/CLIENT_REGISTRATION").c_str(), 1);
				return;
			}
		}
		// server is shutting down.
		// client is no longer connected and client id is no longer valid.
		else if(m_serverClientId > 0)
		{
			m_serverClientId = 0;
			streamlog_out(MESSAGE) << "Unregistered from event collector server application !" << std::endl;
			return;
		}
	}

	if(pInfo == m_pEventUpdateInfo)
	{
		if(m_serverClientId == 0)
			return;

		char *pBuffer = (char*) pInfo->getData();
		unsigned int bufferSize = pInfo->getSize();

		if(NULL == pBuffer || 0 == bufferSize)
			return;

		DQMEvent *pEvent = NULL;

		try
		{
			// lock/unlock on de-serialization if the interface
			// uses the sendEvent(evt) and queryEvent(...)
			// functionalities at the same time
			pthread_mutex_lock(&m_mutex);
			m_dataStream.setBuffer(pBuffer, bufferSize);
			StatusCode statusCode = m_pEventStreamer->deserialize(pEvent, &m_dataStream);
			pthread_mutex_unlock(&m_mutex);

			if(statusCode != STATUS_CODE_SUCCESS)
				throw StatusCodeException(statusCode);
		}
		catch(const StatusCodeException &exception)
		{
			if(NULL != pEvent)
				delete pEvent;

			return;
		}

		StatusCode statusCode = this->addEvent(pEvent);

		if(statusCode != STATUS_CODE_SUCCESS)
			delete pEvent;

		return;
	}
}

} 

