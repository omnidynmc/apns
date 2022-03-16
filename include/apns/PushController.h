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

#ifndef LIBAPNS_PUSHCONTROLLER_H
#define LIBAPNS_PUSHCONTROLLER_H

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

#define DEVICE_BINARY_SIZE  32
#define MAXPAYLOAD_SIZE     256

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  typedef struct {
    char command;
    char tokenLen[2];
    char deviceToken[DEVICE_BINARY_SIZE];
    char payloadLen[2];
    char payload[MAXPAYLOAD_SIZE];
  } ApnsPacket_Simple_t;

/*
typedef struct {
  uint8_t command;
  uint32_t identifier;
  uint32_t expiry;
  uint16_t tokenLen;
  char deviceToken[DEVICE_BINARY_SIZE];
  uint16_t payloadLen;
  char payload[MAXPAYLOAD_SIZE];
} ApnsPacket_Enhanced_t;
*/

  typedef struct {
    char command;
    char identifier[4];
    char expiry[4];
    char tokenLen[2];
    char deviceToken[DEVICE_BINARY_SIZE];
    char payloadLen[2];
    char payload[MAXPAYLOAD_SIZE];
  } ApnsPacket_Enhanced_t;

  typedef struct {
    char command;
    char status;
    char identifier[4];
  } ApnsResponse_t;

  class ApnsMessage;

  class PushController_Exception : public ApnsAbstract_Exception {
    public:
      PushController_Exception(const std::string message) throw() : ApnsAbstract_Exception(message) { };
  }; // class PushController_Exception

  class PushController : public SslController {
    public:
      PushController(const std::string &, const int, const std::string &, const std::string &, const std::string &, const time_t);
      virtual ~PushController();

      typedef std::set<ApnsMessage *> messageQueueType;

      /**********************
       ** Type Definitions **
       **********************/
      static const time_t DEFAULT_STATS_INTERVAL;
      static const time_t CONNECT_RETRY_TIMEOUT;
      static const int ERROR_RESPONSE_SIZE;
      static const int ERROR_RESPONSE_COMMAND;

      enum pushCommandsEnum {
        COMMAND_PUSH_SIMPLE	= 0,
        COMMAND_PUSH_ENHANCED	= 1
      };

      enum errorResponseMessagesEnum {
        ERR_NO_ERRORS			= 0,
        ERR_PROCESSING_ERROR 		= 1,
        ERR_MISSING_DEVICE_TOKEN	= 2,
        ERR_MISSING_TOPIC		= 3,
        ERR_MISSING_PAYLOAD		= 4,
        ERR_INVALID_TOKEN_SIZE		= 5,
        ERR_INVALID_TOPIC_SIZE		= 6,
        ERR_INVALID_PAYLOAD_SIZE	= 7,
        ERR_INVALID_TOKEN		= 8,
        ERR_NONE_UNKNOWN		= 255
      };

      /***************
       ** Variables **
       ***************/
      void timeout(const time_t timeout) { _timeout = timeout; }
      const inline time_t timeout() { return _timeout; }
      void connectRetrytimeout(const time_t connectRetryTimeout) { _connectRetryTimeout = connectRetryTimeout; }
      const inline time_t connectRetrytimeout() { return _connectRetryTimeout; }

      void add(ApnsMessage *);
      const bool remove(ApnsMessage *);
      void Push(ApnsMessage *aMessage) { add(aMessage); }
      const bool run();
      void logStatsInterval(const time_t logStatsInterval) {
        _logStatsInterval = logStatsInterval;
        _logStatsTs = time(NULL) + _logStatsInterval;
      } // logStatsInterval
      const inline time_t logStatsInterval() { return _logStatsInterval; }
      const messageQueueType::size_type sendQueueSize() const { return _messageSendQueue.size(); }

    protected:

    private:
      const bool _sendPayload(ApnsMessage *);
      const bool _push(ApnsMessage *);
      void _add(ApnsMessage *);
      const bool _remove(ApnsMessage *);
      void _processMessageSendQueue();
      void _expireIdleConnection();
      const int _readResponseFromApns();
      void _processResponseFromApns(const ApnsResponse_t *);
      void _removeMessageFromQueueById(const unsigned int, const bool);
      void _removeMessageFromQueue(ApnsMessage *, const bool);
      void _removeMessageFromQueue(ApnsMessage *aMessage) { _removeMessageFromQueue(aMessage, false); }
      const unsigned int _resendStagedMessages();
      const unsigned int _removeExpiredMessagesFromQueue(messageQueueType &);
      const unsigned int _clearMessagesFromQueue(messageQueueType &);
      void _logStats();
      ApnsMessage *_findById(const unsigned int);

      messageQueueType _messageSendQueue;		// storage for messages to deliver
      messageQueueType _messageStageQueue;	// storage for messages that are in progress
      messageQueueType _messageErrorQueue;	// storage for messages with errors
      time_t _timeout;				// timeout in seconds to close connection
      time_t _logStatsInterval;			// interval to log statistics
      time_t _connectRetryTimeout;		// retry timeout for connecting
      time_t _lastActivityTs;			// last activity ts
      time_t _connectRetryTs;			// next time to try reconnecting after error
      time_t _logStatsTs;				// logstats timer
      unsigned int _lastId;			// last message id in use
      unsigned int _numStatsSent;			// number of messages sent
      unsigned int _numStatsError;		// number of messages that received errors
      unsigned int _numStatsDisconnected;		// number of times disconnected
  }; // PushController

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/

} // namespace apns
#endif
