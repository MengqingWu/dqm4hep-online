  /// \file DQMFileHandlerFactory.cc
/*
 *
 * DQMFileHandlerFactory.cc source template automatically generated by a class generator
 * Creation date : lun. janv. 11 2016
 *
 * This file is part of DQM4HRP libraries.
 * 
 * DQM4HRP is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * based upon these libraries are permitted. Any copy of these libraries
 * must include this copyright notice.
 * 
 * DQM4HRP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with DQM4HRP.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @author Remi Ete
 * @copyright CNRS , IPNL
 */


#include "dqm4hep/DQMFileHandlerFactory.h"
#include "dqm4hep/DQMFileHandler.h"
#include "dqm4hep/DQMDBFileHandler.h"
#include "dqm4hep/DQMLocalFileHandler.h"

namespace dqm4hep
{

DQMFileHandlerFactory::DQMFileHandlerFactory() 
{
	/* nop */
}

//-------------------------------------------------------------------------------------------------

DQMFileHandler *DQMFileHandlerFactory::createFileHandler(const std::string &fileHandlerType) const
{
	if(fileHandlerType == "local")
		return new DQMLocalFileHandler();
	else if(fileHandlerType == "db")
		return new DQMDBFileHandler();
	else
		return new DQMLocalFileHandler();
}

} 

