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

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <netdb.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#include <openframe/openframe.h>

#include "ApnsMessage.h"
#include "PushController.h"

namespace apns {
  using namespace openframe::loglevel;

/**************************************************************************
 ** ApnsAbstract Class                                                   **
 **************************************************************************/

  void ApnsAbstract::_deviceTokenToBinary(char *binaryDeviceToken, const std::string &deviceToken, const size_t len) {
    unsigned int i = 0;
    int j = 0;
    int tmpi;
    char tmp[3];

    while(i < deviceToken.length() && j < len) {
      if (deviceToken[i] == ' ') {
        i++;
      } // if
      else {
        tmp[0] = deviceToken[i];
        tmp[1] = deviceToken[i + 1];
        tmp[2] = '\0';

        sscanf(tmp, "%x", &tmpi);
        binaryDeviceToken[j] = tmpi;

        i += 2;
        j++;
      } // else
    } // while
  } // ApnsAbstract::_deviceTokenToBinary

  const std::string ApnsAbstract::_binaryToDeviceToken(const char *binaryDeviceToken, const size_t len) {
    std::stringstream s;

    s.str("");

    for(size_t i=0; i < len; i++) {
      s << _char2hex(binaryDeviceToken[i]);
    } // for

    return s.str();
  } // ApnsAbstract::_binaryToDeviceToken

  const std::string ApnsAbstract::_char2hex(const char dec) {
    char dig1 = (dec&0xF0)>>4;
    char dig2 = (dec&0x0F);

    if ( 0<= dig1 && dig1<= 9) dig1+=48;    //0,48inascii
    if (10<= dig1 && dig1<=15) dig1+=97-10; //a,97inascii
    if ( 0<= dig2 && dig2<= 9) dig2+=48;
    if (10<= dig2 && dig2<=15) dig2+=97-10;

    std::string r;
    r.append(&dig1, 1);
    r.append(&dig2, 1);

    return r;
  } // ApnsAbstract::char2hex

  const std::string ApnsAbstract::generateRandomDeviceToken() {
    int r;
    std::string deviceToken;

    for(int i=0; i < 64/2; i++) {
      r = rand() % 255;
      deviceToken += _char2hex(r);
    } // for

    return deviceToken;
  } // ApnsAbstract:generateRandomDeviceToken

  const bool ApnsAbstract::_testDeviceTokenTools() {
    char binaryDeviceToken[DEVICE_BINARY_SIZE];
    std::string deviceTokenOrig = generateRandomDeviceToken();
    std::string deviceToken;

    _deviceTokenToBinary(binaryDeviceToken, deviceTokenOrig, DEVICE_BINARY_SIZE);
    deviceToken = _binaryToDeviceToken(binaryDeviceToken, DEVICE_BINARY_SIZE);

    LOG(LogInfo, << "Testing deviceTokenOrig tools: "
                 << deviceTokenOrig
                 << std::endl);
    LOG(LogInfo, << "Testing deviceToken     tools: "
                 << deviceToken
                 << std::endl);

    return true;
  } // ApnsAbstract::_testDeviceTokenTools

  const std::string ApnsAbstract::_safeBinaryOutput(const char *ptr, const size_t len) {
    std::stringstream s;
    s.str("");

    for(size_t i=0; i < len; i++) {
      if ((int) ptr[i] < 32 || (int) ptr[i] > 126)
        s << "[\\x" << _char2hex(ptr[i]) << "]";
      else
        s << ptr[i];
    } // for

    return s.str();
  } // ApnsAbstract::_safeBinaryOutput
} // namespace apns
