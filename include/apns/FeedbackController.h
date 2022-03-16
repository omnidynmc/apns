/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, mySQL APRS Injector                                        **
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
 $Id: DCC.h,v 1.8 2003/09/04 00:22:00 omni Exp $
 **************************************************************************/

#ifndef LIBAPNS_FEEDBACKCONTROLLER_H
#define LIBAPNS_FEEDBACKCONTROLLER_H

#include <set>

#include <netdb.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "ApnsAbstract.h"
#include "SslController.h"

namespace apns {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  typedef struct {
    char timestamp[4];		// uint32_t
    char tokenLen[2];		// uint16_t
    char deviceToken[DEVICE_BINARY_SIZE];
  } ApnsFeedbackResponse_t;

  class FeedbackMessage {
    public:
      FeedbackMessage(const time_t timestamp, const unsigned int tokenLen, const std::string deviceToken) :
        _timestamp(timestamp), _deviceToken(deviceToken), _tokenLen(tokenLen) { }
      virtual ~FeedbackMessage() { }

      void deviceToken(const std::string &deviceToken) { _deviceToken = deviceToken; }
      const std::string deviceToken() const { return _deviceToken; }
      void timestamp(const time_t timestamp) { _timestamp = timestamp; }
      const time_t timestamp() const { return _timestamp; }
      void tokenLen(const unsigned int tokenLen) { _tokenLen = tokenLen; }
      const unsigned int tokenLen() const { return _tokenLen; }

    private:
      time_t _timestamp;
      std::string _deviceToken;
      unsigned int _tokenLen;
  }; // FeedbackMessage

  class FeedbackController_Exception : public ApnsAbstract_Exception {
    public:
      FeedbackController_Exception(const std::string message) throw() : ApnsAbstract_Exception(message) { };
  }; // class FeedbackController_Exception

  class FeedbackController : public SslController {
    public:
      FeedbackController(const std::string &, const int, const std::string &, const std::string &, const std::string &, const time_t);
      virtual ~FeedbackController();

      /**********************
       ** Type Definitions **
       **********************/
      static const int FEEDBACK_RESPONSE_SIZE;

      typedef std::set<FeedbackMessage *> messageQueueType;

      /***************
       ** Variables **
       ***************/
      void timeout(const time_t timeout) { _timeout = timeout; }
      void testFeedbackResponse() { _testFeedbackResponse(); }
      const inline time_t timeout() { return _timeout; }
      const messageQueueType::size_type numQueue() { return _messageFeedbackQueue.size(); }
      const messageQueueType::size_type getQueue(messageQueueType &messageQueue) {
        messageQueue = _messageFeedbackQueue;
        _messageFeedbackQueue.clear();

        return messageQueue.size();
      } // getQueue

      const bool run();

    protected:

    private:
      void _readFeedbackFromApns();
      void _testFeedbackResponse();
      void _processFeedbackFromApns(const ApnsFeedbackResponse_t *);

      messageQueueType _messageFeedbackQueue;	// storage for all feedback we receive
      time_t _timeout;				// timeout in seconds to close connection
      time_t _nextCheckTs;			// next time we check feedback service
  }; // class FeedbackController

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/

} // namespace apns
#endif
