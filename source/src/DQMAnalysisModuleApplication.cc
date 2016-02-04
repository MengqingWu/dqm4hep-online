  /// \file DQMAnalysisModuleApplication.cc
/*
 *
 * DQMAnalysisModuleApplication.cc source template automatically generated by a class generator
 * Creation date : dim. nov. 9 2014
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
#include "dqm4hep/DQMAnalysisModuleApplication.h"
#include "dqm4hep/DQMLogging.h"
#include "dqm4hep/DQMPlugin.h"
#include "dqm4hep/DQMPluginManager.h"
#include "dqm4hep/DQMMonitorElement.h"
#include "dqm4hep/DQMRunControl.h"
#include "dqm4hep/DQMRun.h"
#include "dqm4hep/DQMFileHandler.h"
#include "dqm4hep/DQMFileHandlerFactory.h"
#include "dqm4hep/DQMEventStreamer.h"
#include "dqm4hep/DQMAnalysisModule.h"
#include "dqm4hep/DQMCycle.h"
#include "dqm4hep/DQMArchiver.h"
#include "dqm4hep/DQMEvent.h"
#include "dqm4hep/DQMEventClient.h"
#include "dqm4hep/DQMRunControlClient.h"
#include "dqm4hep/DQMDimRunControlClient.h"
#include "dqm4hep/DQMXmlHelper.h"
#include "dqm4hep/tinyxml.h"

// -- std headers
#include <fstream>
#include <stdexcept>

// -- dim headers
#include "dis.hxx"

namespace dqm4hep
{

DQMAnalysisModuleApplication::DQMAnalysisModuleApplication() :
		DQMModuleApplication(),
		m_pEventClient(NULL),
		m_pRunControlClient(NULL),
		m_pCycle(NULL),
		m_applicationType("AnalysisModule"),
		m_applicationName("UNKNOWN"),
		m_runNumber(-1)
{
	m_pEventClient = new DQMEventClient();
	m_pArchiver = new DQMArchiver();
}

//-------------------------------------------------------------------------------------------------

DQMAnalysisModuleApplication::~DQMAnalysisModuleApplication()
{
	delete m_pEventClient;
	delete m_pArchiver;

	if(m_pRunControlClient)
		delete m_pRunControlClient;

	if(m_pCycle)
		delete m_pCycle;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::readSettings(const std::string &settingsFileName)
{
	if(this->isInitialized())
		return STATUS_CODE_ALREADY_INITIALIZED;

	size_t splitterPosition = settingsFileName.find(":");

	std::string fileHandlerType;
	std::string filePattern;

	if(splitterPosition != std::string::npos)
	{
		fileHandlerType = settingsFileName.substr(0, splitterPosition);
		filePattern = settingsFileName.substr(splitterPosition+1);
	}
	else
	{
		fileHandlerType = "";
		filePattern = settingsFileName;
	}

	streamlog_out(DEBUG) << "File handler type : " << fileHandlerType << std::endl;

	DQMFileHandlerFactory fileHandlerFactory;
	DQMFileHandler *pFileHandler = fileHandlerFactory.createFileHandler(fileHandlerType);

	if(!pFileHandler)
		return STATUS_CODE_FAILURE;

	StatusCode statusCode = pFileHandler->download(filePattern);

	if(statusCode != STATUS_CODE_SUCCESS)
	{
		delete pFileHandler;
		return statusCode;
	}

	std::string localSettingsFile = pFileHandler->getLocalFileName();
	delete pFileHandler;

	TiXmlDocument xmlDocument(localSettingsFile);

    if (!xmlDocument.LoadFile())
    {
        std::cout << "DQMAnalysisModuleApplication::readSettings - Invalid xml file." << std::endl;
        return STATUS_CODE_FAILURE;
    }

    const TiXmlHandle xmlDocumentHandle(&xmlDocument);
    const TiXmlHandle xmlHandle = TiXmlHandle(xmlDocumentHandle.FirstChildElement().Element());

	// configure module
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, configureModule(xmlHandle));

	DimBrowser browser;
	int nServers = browser.getServers();

	std::string moduleServerName = "DQM4HEP/Module/" + this->getModule()->getName();

	if(nServers)
	{
		char *serverName;
		char *node;

		while(browser.getNextServer(serverName, node))
		{
			if(moduleServerName == serverName)
			{
				streamlog_out(ERROR) << "Module '" << this->getModule()->getName() << "' already registered over the network.\n"
						<< "Please, change the module name or stop the other module application with the same name !" << std::endl;
				return STATUS_CODE_ALREADY_PRESENT;
			}
		}
	}

	DimServer::start( moduleServerName.c_str() );

	// configure remaining part of the application
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->configureCycle(xmlHandle));
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->configureArchiver(xmlHandle));
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->configureNetwork(xmlHandle));

	m_settings.m_settingsFileName = settingsFileName;
	m_settings.print();

	setInitialized(true);

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::run()
{
	if(!this->isInitialized())
		return STATUS_CODE_NOT_INITIALIZED;

	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->startServices());

	// get casted module for easier manipulation
	DQMAnalysisModule *pAnalysisModule = dynamic_cast<DQMAnalysisModule *>(this->getModule());
	std::string moduleName = pAnalysisModule->getName();

	// infinite loop
	while(!this->shouldStopApplication())
	{
		// wait for a start of run
		bool firstMessage = true;

		while(!m_pRunControlClient->isRunning() && !this->shouldStopApplication())
		{
			if(firstMessage)
			{
				streamlog_out(MESSAGE) << "... Waiting for next run ..." << std::endl;
				firstMessage = false;
			}

			timespec timesleep;
			timesleep.tv_sec = 0;
			timesleep.tv_nsec = 1000000L;
			nanosleep(&timesleep, NULL);
		}

		if(this->shouldStopApplication())
			break;

		DQMRun *pRun = m_pRunControlClient->getCurrentRun();

		if(NULL == pRun)
			continue;

		m_runNumber = pRun->getRunNumber();

		streamlog_out(MESSAGE) << "Starting new run no " << pRun->getRunNumber() << std::endl;

		// open an archive
		if(m_settings.m_shouldOpenArchive)
		{
			std::stringstream archiveFileName;
			std::string directory =
					m_settings.m_archiveDirectory.at(m_settings.m_archiveDirectory.size() - 1) == '/' ?
							m_settings.m_archiveDirectory : m_settings.m_archiveDirectory + "/";

			archiveFileName << directory
					<< "DQMArchive"
					<< "_M" << moduleName
					<<  "_I" << pRun->getRunNumber();

			RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pArchiver->open(archiveFileName.str(), "RECREATE"));
			streamlog_out(MESSAGE) << "Archive '" << m_pArchiver->getFileName() << "' opened" << std::endl;
		}

		// start of run !
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, pAnalysisModule->startOfRun(pRun));

		// start receiving data
		m_pEventClient->setUpdateMode(true);

		// while run has not ended, process cycles
		while(m_pRunControlClient->isRunning() && !this->shouldStopApplication())
		{
			// process a cycle
			streamlog_out(MESSAGE) << "**************************************************" << std::endl;
			streamlog_out(MESSAGE) << "***                Start of cycle              ***" << std::endl;
			streamlog_out(MESSAGE) << "**************************************************" << std::endl;
			m_pCycle->startCycle();

			while(1)
			{
				if( m_pCycle->isEndOfCycleReached() )
					break;

				if( m_pCycle->isTimeoutReached() )
					break;

				if( this->shouldStopApplication() )
					break;

				DQMEvent *pEvent = NULL;
				RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_UNCHANGED, !=, m_pEventClient->takeEvent(pEvent));

				if(NULL == pEvent)
					continue;

				try
				{
					StatusCode statusCode = pAnalysisModule->processEvent(pEvent);
					m_pCycle->eventProcessed(pEvent);
				}
				catch(StatusCodeException &exception)
				{
					streamlog_out(ERROR) << "Module::processEvent(evt) returned " << exception.toString() << " !" << std::endl;
				}
				catch(...)
				{
					streamlog_out(ERROR) << "Module::processEvent(evt) : caught unknown exception !" << std::endl;
				}

				if(pEvent)
					delete pEvent;
			}

			m_pCycle->stopCycle();

			streamlog_out(MESSAGE) << "**************************************************" << std::endl;
			streamlog_out(MESSAGE) << "***            End of cycle reached            ***" << std::endl;
			streamlog_out(MESSAGE) << "**************************************************" << std::endl;
			streamlog_out(MESSAGE) << "***                STATISTICS                  ***" << std::endl;
			streamlog_out(MESSAGE) << "*** N processed events : " << m_pCycle->getNProcessedEvents() << std::endl;
			streamlog_out(MESSAGE) << "*** Event rate :         " << m_pCycle->getProcessingRate()*1000.f << " evts/s" << std::endl;
			streamlog_out(MESSAGE) << "*** Processing time :    " << m_pCycle->getTotalCycleTime().operator long long()/1000.f << " s" << std::endl;
			streamlog_out(MESSAGE) << "**************************************************" << std::endl;

			// archive publication if user asked for
			if(m_settings.m_shouldOpenArchive)
				RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pArchiver->archive(pAnalysisModule));


//			DQMMonitorElementList monitorElementListToPublish;
//			RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementManager()
//					->getMonitorElementListToPublish(monitorElementListToPublish));

			// TODO find a way to update the run number in monitor elements
//			// set monitor elements infos
//			for(DQMMonitorElementList::iterator iter = monitorElementListToPublish.begin(), endIter = monitorElementListToPublish.end() ;
//					endIter != iter ; ++iter)
//				(*iter)->setRunNumber(this->getCurrentRunNumber());

			// send monitor elements to collector
			RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementSender()->sendMonitorElements());

			// reset the monitor elements with end of run reset policy
			RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementManager()
					->resetMonitorElements(END_OF_CYCLE_RESET_POLICY));

			// stop current run if run number has changed or run stopped
			if(m_pRunControlClient->getCurrentRunNumber() != this->getCurrentRunNumber())
				break;
		}

		// stop receive data
		m_pEventClient->setUpdateMode(false);

		// fill the run end time
		pRun->setEndTime(time(NULL));

		// process the end of run module stuff
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, pAnalysisModule->endOfRun(pRun));

		// reset the monitor elements that have been reset at end of run
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementManager()
				->resetMonitorElements(END_OF_RUN_RESET_POLICY));

		// archive the monitor element if archive is opened
		if(m_settings.m_shouldOpenArchive)
			RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pArchiver->close());
	}

	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, stopServices());

	streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication: Exiting application ..." << std::endl;

	return this->getReturnCode();
}

//-------------------------------------------------------------------------------------------------

const std::string &DQMAnalysisModuleApplication::getType() const
{
	return m_applicationType;
}

//-------------------------------------------------------------------------------------------------

const std::string &DQMAnalysisModuleApplication::getName() const
{
	return m_applicationName;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::configureNetwork(const TiXmlHandle xmlHandle)
{
	try
	{
		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureNetwork: configuring ..." << std::endl;

		TiXmlElement *const pXmlElement = xmlHandle.FirstChildElement("network").Element();

		if(NULL == pXmlElement)
		{
			streamlog_out(ERROR) << "<network> element not found !" << std::endl;
			return STATUS_CODE_NOT_FOUND;
		}

		const TiXmlHandle networkHandle(pXmlElement);

		const TiXmlElement *const pRunControlXmlElement = networkHandle.FirstChildElement("runcontrol").Element();
		const TiXmlElement *const pEventCollectorXmlElement = networkHandle.FirstChildElement("eventcollector").Element();
		const TiXmlElement *const pSubEventIdentifierXmlElement = networkHandle.FirstChildElement("subeventidentifier").Element();
		const TiXmlElement *const pEventStreamerXmlElement = networkHandle.FirstChildElement("eventstreamer").Element();
		const TiXmlElement *const pMonitorElementCollectorXmlElement = networkHandle.FirstChildElement("monitorelementcollector").Element();

		if(NULL == pRunControlXmlElement
		|| NULL == pEventCollectorXmlElement
		|| NULL == pEventStreamerXmlElement
		|| NULL == pMonitorElementCollectorXmlElement)
		{
			streamlog_out(ERROR) << "Incomplete network xml element" << std::endl;
			return STATUS_CODE_NOT_FOUND;
		}

		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pRunControlXmlElement, "name", m_settings.m_runControlName));
		THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, DQMXmlHelper::getAttribute(pRunControlXmlElement, "type", m_settings.m_runControlType));

		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pEventCollectorXmlElement, "name", m_settings.m_eventCollector));
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pEventStreamerXmlElement, "name", m_settings.m_eventStreamer));

		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pMonitorElementCollectorXmlElement, "name", m_settings.m_monitorElementCollector));

		if(NULL != pSubEventIdentifierXmlElement)
		{
			THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pSubEventIdentifierXmlElement, "name", m_settings.m_subEventIdentifier));
		}

		// monitor element collector name
		this->getMonitorElementSender()->setCollectorName(m_settings.m_monitorElementCollector);

		// configure streamer
		DQMEventStreamer *pEventStreamer = DQMPluginManager::instance()->createPluginClass<DQMEventStreamer>(m_settings.m_eventStreamer);

		if(!pEventStreamer)
			throw StatusCodeException(STATUS_CODE_NOT_FOUND);

		// configure data client
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventClient->setEventStreamer(pEventStreamer));
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventClient->setCollectorName(m_settings.m_eventCollector));
		m_pEventClient->setSubEventIdentifier(m_settings.m_subEventIdentifier);

		// configure run control
		DQMRunControlClient *pRunControlClient = DQMPluginManager::instance()->createPluginClass<DQMRunControlClient>(m_settings.m_runControlType);

		if(NULL == pRunControlClient)
			throw StatusCodeException(STATUS_CODE_NOT_FOUND);

		pRunControlClient->setRunControlName( m_settings.m_runControlName );
		m_pRunControlClient = pRunControlClient;

		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureNetwork: configuring ... OK" << std::endl;
	}
	catch(const StatusCodeException &exception)
	{
		return exception.getStatusCode();
	}

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::configureCycle(const TiXmlHandle xmlHandle)
{
	try
	{
		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureCycle: configuring ..." << std::endl;

		const TiXmlElement *const pXmlElement = xmlHandle.FirstChildElement("cycle").Element();

		// default values
		m_settings.m_cycleType = "TimerCycle";
		m_settings.m_cycleValue = 10; // 10 seconds
		m_settings.m_cycleTimeout = 3; // 3 seconds

		if(pXmlElement)
		{
			THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, DQMXmlHelper::getAttribute(pXmlElement, "type", m_settings.m_cycleType));
			THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, DQMXmlHelper::getAttribute(pXmlElement, "value", m_settings.m_cycleValue));
			THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, DQMXmlHelper::getAttribute(pXmlElement, "timeout", m_settings.m_cycleTimeout));
		}

		// find the cycle plug-in
		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureCycle: querying cycle to PluginManager ... OK" << std::endl;
		m_pCycle = DQMPluginManager::instance()->createPluginClass<DQMCycle>(m_settings.m_cycleType);

		if(!m_pCycle)
			throw StatusCodeException(STATUS_CODE_FAILURE);

		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureCycle: querying cycle to PluginManager ... OK" << std::endl;

		m_pCycle->setCycleValue(m_settings.m_cycleValue);
		m_pCycle->setTimeout(m_settings.m_cycleTimeout);

		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureCycle: configuring ... OK" << std::endl;
	}
	catch(const StatusCodeException &exception)
	{
		return exception.getStatusCode();
	}

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::configureModule(const TiXmlHandle xmlHandle)
{
	try
	{
		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureModule: configuring ..." << std::endl;

		TiXmlElement *const pXmlElement = xmlHandle.FirstChildElement("module").Element();

		std::string type;
		std::string name;

		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pXmlElement, "type", type));
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pXmlElement, "name", name));

		if(this->getModuleType().empty())
			this->setModuleType(type);

		if(this->getModuleName().empty())
			this->setModuleName(name);

		streamlog_out( MESSAGE ) << "Query analysis module to PluginManager ... " << std::endl;

		DQMAnalysisModule *pAnalysisModule = DQMPluginManager::instance()->createPluginClass<DQMAnalysisModule>(this->getModuleType());

		if(!pAnalysisModule)
			throw StatusCodeException(STATUS_CODE_FAILURE);

		this->setModule(pAnalysisModule);

		streamlog_out( MESSAGE ) << "Query analysis module to PluginManager ... OK" << std::endl;

		TiXmlHandle moduleHandle(pXmlElement);

		streamlog_out( MESSAGE ) << "Reading settings of active module '" << this->getModule()->getName() << "' ..." << std::endl;
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getModule()->readSettings(moduleHandle));
		streamlog_out( MESSAGE ) << "Reading settings of active module '" << this->getModule()->getName() << "' ... OK" << std::endl;

		streamlog_out( MESSAGE ) << "Initializing active module '" << this->getModule()->getName() << "' ..." << std::endl;
		THROW_RESULT_IF( STATUS_CODE_SUCCESS, !=, this->getModule()->initModule() );
		streamlog_out( MESSAGE ) << "Initializing active module '" << this->getModule()->getName() << "' ... OK" << std::endl;

		m_applicationName = this->getModule()->getName();

		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureModule: configuring ... OK" << std::endl;
	}
	catch(const StatusCodeException &exception)
	{
		return exception.getStatusCode();
	}

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::configureArchiver(const TiXmlHandle xmlHandle)
{
	try
	{
		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureArchiver: configuring ..." << std::endl;

		const TiXmlElement *const pXmlElement = xmlHandle.FirstChildElement("archiver").Element();

		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pXmlElement, "open", m_settings.m_shouldOpenArchive));
		THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, DQMXmlHelper::getAttribute(pXmlElement, "directory", m_settings.m_archiveDirectory));

		streamlog_out(MESSAGE) << "DQMAnalysisModuleApplication::configureArchiver: configuring ... OK" << std::endl;
	}
	catch(const StatusCodeException &exception)
	{
		return exception.getStatusCode();
	}

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::startServices()
{
	// start clients
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventClient->connectToService());
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pRunControlClient->connectToService());
	RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementSender()->connectToService());

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

StatusCode DQMAnalysisModuleApplication::stopServices()
{
	// start clients
	if(m_pEventClient && m_pEventClient->isConnectedToService())
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pEventClient->connectToService());

	if(m_pRunControlClient && m_pRunControlClient->isConnectedToService())
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pRunControlClient->connectToService());

	if(this->getMonitorElementSender() && this->getMonitorElementSender()->isConnectedToService())
		RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->getMonitorElementSender()->connectToService());

	return STATUS_CODE_SUCCESS;
}

//-------------------------------------------------------------------------------------------------

int DQMAnalysisModuleApplication::getCurrentRunNumber() const
{
	return m_runNumber;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

DQMAnalysisModuleApplication::Settings::Settings() :
	m_shouldOpenArchive(false),
	m_runControlType("DimRunControlClient"),
	m_runControlName("DEFAULT"),
	m_eventCollector("DEFAULT"),
	m_subEventIdentifier(""),
	m_monitorElementCollector("DEFAULT"),
	m_eventStreamer(""),
	m_cycleType("TimeCycle"),
	m_cycleValue(30.f),
	m_cycleTimeout(5),
	m_archiveDirectory("/tmp")
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

void DQMAnalysisModuleApplication::Settings::print()
{
	std::string openArchive = m_shouldOpenArchive ? "yes" : "no";
	streamlog_out(MESSAGE) << "**************************************" << std::endl;
	streamlog_out(MESSAGE) << "******* Application parameters *******" << std::endl;
	streamlog_out(MESSAGE) << "*** Should open archive :            " << openArchive << std::endl;
	streamlog_out(MESSAGE) << "*** Run control type :               " << m_runControlType << std::endl;
	streamlog_out(MESSAGE) << "*** Run control name :               " << m_runControlName << std::endl;
	streamlog_out(MESSAGE) << "*** Event collector name :           " << m_eventCollector << std::endl;
	streamlog_out(MESSAGE) << "*** Sub event identifier :           " << m_subEventIdentifier << std::endl;
	streamlog_out(MESSAGE) << "*** Event streamer :                 " << m_eventStreamer << std::endl;
	streamlog_out(MESSAGE) << "*** Cycle type :                     " << m_cycleType << std::endl;
	streamlog_out(MESSAGE) << "*** Cycle value :                    " << m_cycleValue << std::endl;
	streamlog_out(MESSAGE) << "*** Cycle timeout :                  " << m_cycleTimeout << std::endl;
	streamlog_out(MESSAGE) << "*** Archive directory :              " << m_archiveDirectory << std::endl;
	streamlog_out(MESSAGE) << "*** Monitor element collector name : " << m_monitorElementCollector << std::endl;
	streamlog_out(MESSAGE) << "**************************************" << std::endl;
}

} 

