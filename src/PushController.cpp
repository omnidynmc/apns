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

#include "ApnsMessage.h"
#include "PushController.h"

namespace apns {
  using namespace openframe::loglevel;

/**************************************************************************
 ** APNS Class                                                           **
 **************************************************************************/
  const time_t PushController::CONNECT_RETRY_TIMEOUT 	= 60;
  const time_t PushController::DEFAULT_STATS_INTERVAL 	= 3600;
  const int PushController::ERROR_RESPONSE_SIZE 	= 6;
  const int PushController::ERROR_RESPONSE_COMMAND 	= 8;

  PushController::PushController(const std::string &host, const int port, const std::string &certfile, const std::string &keyfile, const std::string &capath, const time_t timeout) :
    SslController(host, port, certfile, keyfile, capath), _timeout(timeout) {

    _lastId = 0;
    _logStatsInterval = DEFAULT_STATS_INTERVAL;			// log stats every hour
    _logStatsTs = time(NULL) + _logStatsInterval;
    _lastActivityTs = time(NULL);
    _connectRetryTimeout = CONNECT_RETRY_TIMEOUT;

    _numStatsError = 0;
    _numStatsSent = 0;
    _numStatsDisconnected = 0;

    return;
  } // PushController::PushController

  PushController::~PushController() {
    _clearMessagesFromQueue(_messageSendQueue);
    _clearMessagesFromQueue(_messageStageQueue);
    _clearMessagesFromQueue(_messageErrorQueue);

    if (isConnected())
      disconnect();

    return;
  } // PushController::~PushController

  const bool PushController::run() {
    unsigned int numRows;

    if (time(NULL) < _connectRetryTs)
      return false;

    if (time(NULL) > _logStatsTs)
      _logStats();

    _processMessageSendQueue();
    _expireIdleConnection();

    if ((numRows = _removeExpiredMessagesFromQueue(_messageStageQueue)) > 0)
      LOG(LogNotice, << "Expired "
                     << numRows
                     << " message"
                     << (numRows == 1 ? "" : "s")
                     << " from stage queue."
                     << std::endl);

    if ((numRows =_removeExpiredMessagesFromQueue(_messageErrorQueue)) > 0)
      LOG(LogNotice, << "Expired "
                     << numRows
                     << " message"
                     << (numRows == 1 ? "" : "s")
                     << " from error queue."
                     << std::endl);

    return true;
  } // PushController::run

  void PushController::_processMessageSendQueue() {
    messageQueueType::iterator ptr;		// message queue iterator
    messageQueueType processQueue;		// local queue storage for processing
    unsigned int id;
    int numBytes;

    if (_messageSendQueue.empty())
      return;

    if (!isConnected() && !connect()) {
      LOG(LogWarn, << "WARNING: Messages ("
                   << _messageSendQueue.size()
                   << ") ready to send but unable connect, will retry in "
                   << _connectRetryTimeout
                   << " seconds."
                   << std::endl);
      _connectRetryTs = time(NULL) + _connectRetryTimeout;
      return;
    } // if

    LOG(LogInfo, << "INFO: Sending message queue: "
                 << _messageSendQueue.size()
                 << " message(s) left in queue."
                 << std::endl);

    // We don't want to edit the real queue so that
    // the sending queue can put failed messages back into
    // the send queue.
    processQueue = _messageSendQueue;
    while(!processQueue.empty() && isConnected()) {
      ptr = processQueue.begin();

      _messageStageQueue.insert(*ptr);
      _messageSendQueue.erase(*ptr);
      processQueue.erase(*ptr);
      id = (*ptr)->id();

      _sendPayload(*ptr);
      if ((numBytes = _readResponseFromApns()) > 0) {
        LOG(LogNotice, << "Detected a response with "
                       << numBytes
                       << " bytes to [custom identifier: "
                       << id
                       << "] deferring "
                       << _messageSendQueue.size()
                       << " queued for reconnect."
                       << std::endl);
        // On error, we will get disconnected
        disconnect();
        _numStatsDisconnected++;
        _numStatsError++;
        //break;
      } // if
    } // while

    return;
  } // _processMessageQueue

  void PushController::_expireIdleConnection() {
    if (!_timeout || !isConnected())
      // Don't expire if timeout is 0
      return;

    if (time(NULL) < (_lastActivityTs+_timeout))
      // not time to expire yet
      return;

    LOG(LogNotice, << "Connection expired after "
                   << _timeout
                   << " seconds."
                   << std::endl);

    // connection is expired, disconnect for now.
    disconnect();
  } // PushController::_expireIdleConnection

  const int PushController::_readResponseFromApns() {
    ApnsResponse_t r;
    char response[ERROR_RESPONSE_SIZE];
    char *ptr = response;
    int ret;

    ret = read((void *) &response, ERROR_RESPONSE_SIZE);

    if (ret < 1)
      return -1;

    LOG(LogInfo, << "Received response from APNS that was "
                 << ret
                 << " bytes."
                 << std::endl);

    memcpy(&r.command, ptr++, sizeof(uint8_t));
    memcpy(&r.status, ptr++, sizeof(uint8_t));
    memcpy(&r.identifier, ptr, sizeof(uint32_t));

    _processResponseFromApns(&r);

    return ret;
  } // PushController::_readResponseFromApns

  void PushController::_processResponseFromApns(const ApnsResponse_t *r) {
    ApnsMessage *aMessage;
    char command;			// Command
    char status;			// Status
    unsigned int identifier;		// Identifier

    memcpy(&identifier, r->identifier, sizeof(uint32_t));

    command = r->command;
    status = r->status;
    identifier = ntohl(identifier);

    std::string safeBinaryOutput = _safeBinaryOutput((char *) &r, 6);

    LOG(LogDebug, << "INFO: RX |"
                  << safeBinaryOutput
                  << "| bytes("
                  << safeBinaryOutput.length()
                  << ")"
                  << std::endl);

    if ((int) command !=  ERROR_RESPONSE_COMMAND) {
      LOG(LogWarn, << "Reponse command unknown: "
                   << (int) command
                   << " for [custom identifier: "
                   << identifier
                   << "]"
                   << std::endl);
      return;
    } // if

    // move message to the error queue
    aMessage = _findById(identifier);
    if (aMessage != NULL) {
      _removeMessageFromQueue(aMessage, true);
      aMessage->error(status);
    } // if

    switch((int) status) {
      case ERR_NO_ERRORS:
        LOG(LogInfo, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: NO ERROR ("
                     << status
                     << ")"
                     << std::endl);
        return;
        break;
      case ERR_PROCESSING_ERROR:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: PROCESSING ERROR ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_MISSING_DEVICE_TOKEN:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: MISSING DEVICE TOKEN ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_MISSING_TOPIC:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: MISSING TOPIC ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_MISSING_PAYLOAD:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: MISSING PAYLOAD ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_INVALID_TOKEN_SIZE:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: INVALID TOKEN SIZE ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_INVALID_TOPIC_SIZE:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: INVALID TOPIC SIZE ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_INVALID_PAYLOAD_SIZE:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: INVALID PAYLOAD SIZE ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_INVALID_TOKEN:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: INVALID TOKEN ("
                     << status
                     << ")"
                     << std::endl);
        break;
      case ERR_NONE_UNKNOWN:
        LOG(LogWarn, << "Message reponse [custom identifier: "
                     << (int) identifier
                     << "]: NONE UNKNOWN ("
                     << status
                     << ")"
                     << std::endl);
        break;
    } // switch

  } // PushController::_processReponseFromApns

  // ### Queue Management ###
  void PushController::add(ApnsMessage *aMessage) {
    _add(aMessage);
  } // PushController::add

  void PushController::_add(ApnsMessage *aMessage) {
    assert(aMessage != NULL);

    // Set id for message, probably never have over 1024 messages
    // in the queue.
    aMessage->id(++_lastId % 1024);

    // update our last activity
    _lastActivityTs = time(NULL);

    _messageSendQueue.insert(aMessage);
  } // PushController::_add

  const bool PushController::remove(ApnsMessage *aMessage) {
    bool ret;

    ret = _remove(aMessage);

    return ret;
  } // PushController::remove

  const bool PushController::_remove(ApnsMessage *aMessage) {
    assert(aMessage != NULL);

    if (_messageSendQueue.find(aMessage) == _messageSendQueue.end())
      return false;

    _messageSendQueue.erase(aMessage);

    delete aMessage;

    return true;
  } // PushController::_remove

  ApnsMessage *PushController::_findById(const unsigned int id) {
    messageQueueType::iterator ptr;

    ptr = _messageStageQueue.begin();
    while(ptr != _messageStageQueue.end()) {
      if ((*ptr)->id() == id)
        return *ptr;
      ptr++;
    } // while

    return NULL;
  } // PushController::_findById

  void PushController::_removeMessageFromQueueById(const unsigned int id, const bool error) {
    ApnsMessage *aMessage;

    aMessage = _findById(id);
    if (aMessage == NULL)
      throw PushController_Exception("Unable to find ApnsMessage by id");

    _messageStageQueue.erase(aMessage);

    if (error)
      _messageErrorQueue.insert(aMessage);
    else
      delete aMessage;

  } // PushController::_removeMessageFromQueueById

  const unsigned int PushController::_clearMessagesFromQueue(messageQueueType &messageQueue) {
    const unsigned int numRows = messageQueue.size();
    messageQueueType::iterator ptr;

    while(!messageQueue.empty()) {
      ptr = messageQueue.begin();
      messageQueue.erase(ptr);
      delete (*ptr);
    } // while

    return numRows;
  } // _clearMessagesFromQueue

  const unsigned int PushController::_removeExpiredMessagesFromQueue(messageQueueType &messageQueue) {
    messageQueueType::iterator ptr;
    messageQueueType removeMe;
    std::string queueName = "";
    unsigned int numRows = 0;

    ptr = messageQueue.begin();
    while(ptr != messageQueue.end()) {
      if (time(NULL) > (*ptr)->expiry())
        removeMe.insert((*ptr));

      ptr++;
    } // while

    while(!removeMe.empty()) {
      ptr = removeMe.begin();
      messageQueue.erase((*ptr));
      removeMe.erase((*ptr));

      // we're expire, remove it
      //_logf("STATUS: Expired message [custom identifier: %d]: Removed from queue.", (*ptr)->id());

      delete (*ptr);
      numRows++;
    } // while

    return numRows;
  } // PushController::_removeExpiredMessagesFromQueue

  void PushController::_removeMessageFromQueue(ApnsMessage *aMessage, const bool error) {
    messageQueueType::iterator ptr;

    ptr = _messageStageQueue.find(aMessage);
    if (ptr == _messageStageQueue.end())
      throw PushController_Exception("Unable to find ApnsMessage");

    _messageStageQueue.erase(*ptr);

    if (error)
      _messageErrorQueue.insert(*ptr);
    else
      delete *ptr;

  } // PushController::_removeMessageFromQueue

  const bool PushController::_sendPayload(ApnsMessage *aMessage) {
    ApnsPacket_Enhanced_t p;
    std::string payloadString;
    char deviceTokenHex[aMessage->deviceToken().length()+1];
    char payload[MAXPAYLOAD_SIZE];
    size_t payloadLen;
    int ret;

    // Should never happen, we are only called by _buildPacket which
    // will set this when done.
    assert(aMessage != NULL);

    // Should we retry?
    if (!aMessage->retry()) {
      LOG(LogWarn, << "Giving up on message [custom identifier: "
                   << aMessage->id()
                   << "] after retry ("
                   << aMessage->retries()
                   << ") count expired."
                   << std::endl);
      _removeMessageFromQueue(aMessage, false);
      return false;
    } // if

    try {
      payloadString = aMessage->getPayload();
    } // try
    catch(ApnsMessage_Exception e) {
      LOG(LogWarn, << "Message removed [custom identifier: "
                   << aMessage->id()
                   << "]: "
                   << e.message());
      aMessage->error(ERR_INVALID_PAYLOAD_SIZE);
      _removeMessageFromQueue(aMessage, true);
      return false;
    } // catch

    strncpy(deviceTokenHex, aMessage->deviceToken().c_str(), sizeof(deviceTokenHex));
    strncpy(payload, payloadString.c_str(), sizeof(payload));
    payloadLen = payloadString.length();

    bzero(&p, sizeof(p));

    LOG(LogDebug, << "Sending["
                  << deviceTokenHex
                  << "] of ("
                  << payload
                  << ") "
                  << payloadLen
                  << " bytes"
                  << std::endl);

    p.command = (char) COMMAND_PUSH_ENHANCED;
    //p.command = COMMAND_PUSH_ENHANCED;

    // message format is, |COMMAND|TOKENLEN|TOKEN|PAYLOADLEN|PAYLOAD|
    //char binaryMessageBuff[sizeof(uint8_t) + sizeof(uint16_t) + DEVICE_BINARY_SIZE + sizeof(uint16_t) + MAXPAYLOAD_SIZE];

    uint16_t networkOrderTokenLength = htons(DEVICE_BINARY_SIZE);
    uint16_t networkOrderPayloadLength = htons(payloadLen);
    uint32_t networkOrderIdentifier = htonl(aMessage->id());
    uint32_t networkOrderExpiry = htonl(time(NULL)+300);
    memcpy(&p.tokenLen, &networkOrderTokenLength, sizeof(uint16_t));
    memcpy(&p.payloadLen, &networkOrderPayloadLength, sizeof(uint16_t));
    memcpy(&p.identifier, &networkOrderIdentifier, sizeof(uint32_t));
    memcpy(&p.expiry, &networkOrderExpiry, sizeof(uint32_t));

    //p.tokenLen = htons(DEVICE_BINARY_SIZE);
    //p.payloadLen = htons(_packetLen);
    //p.identifier = htonl(1);
    //p.expiry = htonl(time(NULL)+300);

    // Convert the Device Token
    unsigned int i = 0;
    int j = 0;
    int tmpi;
    char tmp[3];
    while(i < strlen(deviceTokenHex)) {
      if (deviceTokenHex[i] == ' ') {
        i++;
      } // if
      else {
        tmp[0] = deviceTokenHex[i];
        tmp[1] = deviceTokenHex[i + 1];
        tmp[2] = '\0';

        sscanf(tmp, "%x", &tmpi);
        p.deviceToken[j] = tmpi;

        i += 2;
        j++;
      } // else
    } // while

    // payload
    memcpy(p.payload, payload, payloadLen);

    //int payloadOffset = sizeof(uint8_t) + sizeof(uint16_t) + DEVICE_BINARY_SIZE + sizeof(uint16_t);
    int payloadOffset = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint16_t) + DEVICE_BINARY_SIZE + sizeof(uint16_t);
    int packetLen = payloadOffset+payloadLen;

    char packet[packetLen];
    char *ptr = packet;

    memcpy(ptr++, &p.command, sizeof(uint8_t));
    memcpy(ptr, &p.identifier, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &p.expiry, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &p.tokenLen, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(ptr, &p.deviceToken, DEVICE_BINARY_SIZE);
    ptr += DEVICE_BINARY_SIZE;
    memcpy(ptr, &p.payloadLen, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(ptr, &p.payload, payloadLen);

    ret = write((char *) &packet, packetLen);

    LOG(LogDebug, << "Write returned: "
                  << ret
                  << std::endl);

    if (ret < 1 || ret != packetLen) {
      // If we failed to write this packet, we need to put it back
      // into the queue
      _messageStageQueue.erase(aMessage);
      _messageSendQueue.insert(aMessage);
      LOG(LogWarn, << "Unable to send message [customer identifier: "
                   << aMessage->id()
                   << "].  Wrote "
                   << ret
                   << " of "
                   << packetLen
                   << " bytes, pushing back to send queue."
                   << std::endl);
    } // if

    LOG(LogDebug, << "TX |"
                  << _safeBinaryOutput(packet, packetLen)
                  << "| payloadOffset("
                  << payloadOffset
                  << ") packetLen("
                  << payloadLen
                  << ") bytes("
                  << packetLen
                  << ")"
                  << std::endl);
    LOG(LogNotice, << "Sending message [custom identifier: "
                   << aMessage->id()
                   << "]: "
                   << packetLen
                   << " bytes, try #"
                   << aMessage->retries()
                   << std::endl);

    _numStatsSent++;

    return ret ? true : false;
  } // PushController::_sendPayload

  void PushController::_logStats() {
    _logStatsTs = time(NULL) + _logStatsInterval;

    LOG(LogNotice, << "Statistics Sent("
                   << _numStatsSent
                   << ") Errors("
                   << _numStatsError
                   << ") Disconnects("
                   << _numStatsDisconnected
                   << ") next in "
                   << _logStatsInterval
                   << " seconds"
                   << std::endl);

    _numStatsSent = 0;
    _numStatsError = 0;
    _numStatsDisconnected = 0;
  } // PushController::_logStats
} // namespace apns

