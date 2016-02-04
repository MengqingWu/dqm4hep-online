  /// \file DQMCycle.cc
/*
 *
 * DQMCycle.cc source template automatically generated by a class generator
 * Creation date : ven. sept. 5 2014
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
#include "dqm4hep/DQMCycle.h"

#include "TSystem.h"

namespace dqm4hep
{

DQMCycle::DQMCycle() :
		m_processingRate(0.f),
		m_cycleValue(0.f),
		m_cycleTimeout(10), // 10 seconds is the default value
		m_nProcessedEvents(0)
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

DQMCycle::~DQMCycle()
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

float DQMCycle::getCycleValue() const
{
	return m_cycleValue;
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::setCycleValue(float value)
{
	m_cycleValue = value;
}

//-------------------------------------------------------------------------------------------------

unsigned int DQMCycle::getTimeout() const
{
	return m_cycleTimeout;
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::setTimeout(unsigned int timeout)
{
	m_cycleTimeout = timeout;
}

//-------------------------------------------------------------------------------------------------

float DQMCycle::getProcessingRate() const
{
	return m_processingRate;
}

//-------------------------------------------------------------------------------------------------

unsigned int DQMCycle::getNProcessedEvents() const
{
	return m_nProcessedEvents;
}

//-------------------------------------------------------------------------------------------------

TTime DQMCycle::getTotalCycleTime() const
{
	return (m_endTime - m_startTime);
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::eventProcessed(const DQMEvent *const pEvent)
{
	if(this->getState() != RUNNING_STATE)
		return;

	if(NULL == pEvent)
		return;

	m_nProcessedEvents++;
	m_lastEventProcessedTime = gSystem->Now();

	// call back for daughter class
	this->onEventProcessed(pEvent);

	// call back from listeners
	for(std::set<DQMCycleListener*>::iterator iter = m_listeners.begin(), endIter = m_listeners.end() ;
			endIter != iter ; ++iter)
		(*iter)->onEventProcessed(this, pEvent);
}

//-------------------------------------------------------------------------------------------------

const TTime &DQMCycle::getStartTime() const
{
	return m_startTime;
}

//-------------------------------------------------------------------------------------------------

const TTime &DQMCycle::getEndTime() const
{
	return m_endTime;
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::startCycle()
{
	m_startTime = gSystem->Now();
	m_lastEventProcessedTime = m_startTime;
	m_endTime = TTime(0);

	m_nProcessedEvents = 0;
	m_processingRate = 0.f;
	m_state = RUNNING_STATE;

	// call back for daughter class
	this->onCycleStarted();

	// call back from listeners
	for(std::set<DQMCycleListener*>::iterator iter = m_listeners.begin(), endIter = m_listeners.end() ;
			endIter != iter ; ++iter)
		(*iter)->onCycleStarted(this);
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::stopCycle()
{
	m_endTime = gSystem->Now();
	m_state = STOPPED_STATE;

	TTime timeDifference = this->getTotalCycleTime();

	if(timeDifference != TTime(0))
		m_processingRate = (static_cast<float>(m_nProcessedEvents) / timeDifference.operator long long());

	// call back for daughter class
	this->onCycleStopped();

	// call back from listeners
	for(std::set<DQMCycleListener*>::iterator iter = m_listeners.begin(), endIter = m_listeners.end() ;
			endIter != iter ; ++iter)
		(*iter)->onCycleStopped(this);
}

//-------------------------------------------------------------------------------------------------

DQMState DQMCycle::getState() const
{
	return m_state;
}

//-------------------------------------------------------------------------------------------------

bool DQMCycle::isTimeoutReached() const
{
	if(m_lastEventProcessedTime == TTime(0))
		return false;

	if( gSystem->Now() > m_lastEventProcessedTime + TTime(this->getTimeout()*1000) )
		return true;

	return false;
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::addListener(DQMCycleListener *pListener)
{
	if(NULL != pListener)
		m_listeners.insert(pListener);
}

//-------------------------------------------------------------------------------------------------

void DQMCycle::removeListener(DQMCycleListener *pListener)
{
	m_listeners.erase(pListener);
}

} 

