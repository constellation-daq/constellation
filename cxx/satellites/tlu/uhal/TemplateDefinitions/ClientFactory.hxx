/*
---------------------------------------------------------------------------

    This file is part of uHAL.

    uHAL is a hardware access library and programming framework
    originally developed for upgrades of the Level-1 trigger of the CMS
    experiment at CERN.

    uHAL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    uHAL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with uHAL.  If not, see <http://www.gnu.org/licenses/>.


      Andrew Rose, Imperial College, London
      email: awr01 <AT> imperial.ac.uk

      Marc Magrans de Abril, CERN
      email: marc.magrans.de.abril <AT> cern.ch

---------------------------------------------------------------------------
*/


namespace uhal
{

  template <class T>
  bool ClientFactory::RegistrationHelper<T>::init(const std::string& aUri, const std::string& aDescription)
  {
    ClientFactory::getInstance().registerClient<T>(aUri, aDescription);
    return true;
  }


  template <class T>
  void ClientFactory::add ( const std::string& aProtocol , const std::string& aDescription, bool aUserDefined )
  {
    std::unordered_map<std::string , ClientInfo >::const_iterator lIt = mClientMap.find ( aProtocol ) ;

    if ( lIt != mClientMap.end() )
    {
      log ( Warning() , "Protocol \"" , aProtocol , "\" already exists in map of creators. Continuing for now, but be warned." );
      return;
    }

    ClientInfo lClientInfo;
    lClientInfo.creator.reset( new Creator<T>() );
    lClientInfo.userDefined = aUserDefined;
    lClientInfo.description = aDescription;
    mClientMap[aProtocol] = lClientInfo;
  }


  template <class T>
  void ClientFactory::registerClient ( const std::string& aProtocol , const std::string& aDescription )
  {
    add<T>(aProtocol, aDescription, true);
  }


  template <class T>
  std::shared_ptr<ClientInterface> ClientFactory::Creator<T>::create ( const std::string& aId , const URI& aUri )
  {
    return std::shared_ptr<ClientInterface> ( new T ( aId , aUri ) );
  }

}
