/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** HAL9000, Internet Relay Chat Bot                                     **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************
 $Id: APNS.cpp,v 1.12 2003/09/05 22:23:41 omni Exp $
 **************************************************************************/

#include <fstream>
#include <string>
#include <queue>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <list>
#include <map>
#include <new>
#include <iostream>
#include <fstream>
#include <sstream>

#include <time.h>

#include <openframe/openframe.h>

#include "ApnsMessage.h"
#include "PushController.h"

namespace apns {

/**************************************************************************
 ** APNS Class                                                           **
 **************************************************************************/
  const unsigned int ApnsMessage::PAYLOAD_MAXIMUM_SIZE	 	= 256;
  const unsigned int ApnsMessage::DEFAULT_MAXIMUM_RETRIES 	= 3;
  const unsigned int ApnsMessage::MAXIMUM_DICTIONARY_VALUES 	= 5;
  const unsigned int ApnsMessage::DEFAULT_EXPIRY	 	= 60;

  ApnsMessage::ApnsMessage(const std::string &deviceToken) :
    _deviceToken(deviceToken), _actionKeyCaption("View") {

    if (deviceToken.length() < 64 || deviceToken.length() > 70)
      // invalid device token
      throw ApnsMessage_Exception("Invalid device token.");


    _environment = APNS_ENVIRONMENT_DEVEL;
    _maxRetries = DEFAULT_MAXIMUM_RETRIES;
    _expiry = time(NULL) + DEFAULT_EXPIRY;
    _retries = 0;

    return;
  } // PushController::PushController

  ApnsMessage::~ApnsMessage() {

    return;
  } // ApnsMessage::~ApnsMessage

  const std::string ApnsMessage::getPayload() {
    std::stringstream s;

    s.str("");

    s << "{\"aps\":{";

    if (_text.length()) {
      s << "\"alert\":";

      if (_actionKeyCaption.length()) {
        s << "{\"body\":\""
          << escape(_text)
          << "\",\"action-loc-key\":\""
          << escape(_actionKeyCaption)
          << "\"},";
      } // if
      else {
        s << "{\""
          << _text
          << "\"},";
      } // else
    } // if

    if (_badgeNumber > 99 || _badgeNumber < 0)
        _badgeNumber = 1;

    s << "\"badge\":"
      << _badgeNumber
      << ",\"sound\":\""
      << (!_soundName.length() ? "default" : escape(_soundName))
      << "\"}";


    dictVectorType::iterator ptr;
    int i = 0;
    for(ptr = _dictVector.begin(); i < 5 && ptr != _dictVector.end(); ptr++) {
      if (i > 0)
        s << ",";

      s << "\"" << escape((*ptr).first) << "\":\"" << escape((*ptr).second) << "\"";
      i++;
    } // for

    s << "}";

    if (s.str().length() > PAYLOAD_MAXIMUM_SIZE)
      throw ApnsMessage_Exception("Payload exceeds maximum size.");

    return s.str();
  } // ApnsMessage::getPayload

  const std::string ApnsMessage::escape(const std::string &escape) {
    std::string ret = "";
    size_t pos;

    for(pos=0; pos < escape.length(); pos++) {
      if (escape[pos] == '"')
        ret += "\\";

      ret += escape[pos];
    } // for

    return ret;
  } // ApnsMessage::escape
} // namespace apns

