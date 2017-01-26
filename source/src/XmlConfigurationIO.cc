  /// \file XmlConfigurationIO.cc
/*
 *
 * XmlConfigurationIO.cc source template automatically generated by a class generator
 * Creation date : jeu. janv. 26 2017
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


#include "dqm4hep/XmlConfigurationIO.h"
#include "dqm4hep/ConfigurationHandle.h"
#include "dqm4hep/tinyxml.h"
#include "dqm4hep/Logging.h"

namespace dqm4hep {

  namespace core {

    StatusCode XmlConfigurationIO::read(const std::string &xmlFileName, ParameterDirectory *pDirectory)
    {
      TiXmlDocument xmlDocument(xmlFileName);

      if (!xmlDocument.LoadFile())
      {
        LOG4CXX_FATAL( dqmMainLogger , "XmlConfigurationIO::read - Invalid xml file." );
        return STATUS_CODE_FAILURE;
      }

      TiXmlElement *pRootElement = xmlDocument.RootElement();

      return this->read(pRootElement, pDirectory);
    }

    //-------------------------------------------------------------------------------------------------

    StatusCode XmlConfigurationIO::write(const std::string &xmlFileName, ParameterDirectory *pDirectory)
    {
      TiXmlDocument document;

      TiXmlDeclaration *pDeclaration = new TiXmlDeclaration( "1.0", "", "" );
      document.LinkEndChild(pDeclaration);

      TiXmlElement *pRootElement = new TiXmlElement("dqm4hep");
      document.LinkEndChild(pRootElement);

      return this->write(pRootElement, pDirectory->createHandle());
    }

    //-------------------------------------------------------------------------------------------------

    std::string XmlConfigurationIO::getType() const
    {
      return "xml";
    }

    //-------------------------------------------------------------------------------------------------

    StatusCode XmlConfigurationIO::read(TiXmlElement *pXmlElement, ParameterDirectory *pDirectory)
    {
      for(TiXmlElement *pChildXmlElement = pXmlElement->FirstChildElement() ; pChildXmlElement != nullptr ; pChildXmlElement = pChildXmlElement->NextSiblingElement())
      {
        const std::string &name(pChildXmlElement->ValueStr());

        if(name == "parameter")
        {
          const char *pName = pChildXmlElement->Attribute("name");

          if(nullptr == pName)
            return STATUS_CODE_FAILURE;

          std::string value(nullptr == pChildXmlElement->GetText() ? "" : pChildXmlElement->GetText());
          pDirectory->getParameters().set(pName, value);
        }
        else
        {
          ParameterDirectory *pSubDirectory = pDirectory->mkdir(name);

          if(nullptr == pSubDirectory)
            return STATUS_CODE_FAILURE;

          this->read(pChildXmlElement, pSubDirectory);
        }
      }

      return STATUS_CODE_SUCCESS;
    }

    //-------------------------------------------------------------------------------------------------

    StatusCode XmlConfigurationIO::write(TiXmlElement *pXmlElement, const ConfigurationHandle &configHandle)
    {
      StringVector directoryList(configHandle.getSubDirectorList());

      for(auto iter = directoryList.begin(), endIter = directoryList.end() ; endIter != iter ; ++iter)
      {
        ConfigurationHandle subConfigHandle(configHandle.createHandle(*iter));
        TiXmlElement *pChildXmlElement = new TiXmlElement(*iter);
        pXmlElement->LinkEndChild(pChildXmlElement);

        RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->write(pChildXmlElement, subConfigHandle));
      }

      StringVector parameterNames(configHandle.getParameterNames());

      for(auto iter = parameterNames.begin(), endIter = parameterNames.end() ; endIter != iter ; ++iter)
      {
        StringParameter parameter;
        RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, configHandle.getParameter(*iter, parameter));

        TiXmlElement *pParameterXmlElement = new TiXmlElement("parameter");
        pParameterXmlElement->SetAttribute("value", parameter.get());
        pXmlElement->LinkEndChild(pParameterXmlElement);
      }

      return STATUS_CODE_SUCCESS;
    }

  }

}