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

#ifndef LIBAPNS_APNSMESSAGE_H
#define LIBAPNS_APNSMESSAGE_H

#include <set>
#include <string>

#include "PushController.h"

#include <exception>

namespace apns {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/

  class ApnsMessage_Exception : public std::exception {
    public:
      ApnsMessage_Exception(const std::string message) throw() {
        if (!message.length())
          _message = "An unknown APNS Message exception occured.";
        else
          _message = message;
      } // ApnsMessage_Exception

      virtual ~ApnsMessage_Exception() throw() { }
      virtual const char *what() const throw() { return _message.c_str(); }

      const char *message() const throw() { return _message.c_str(); }

    private:
      std::string _message;			// Message of the exception error.
  }; // class ApnsMessage_Exception

  class ApnsMessage {
    public:
      ApnsMessage(const std::string &);
      virtual ~ApnsMessage();

      friend class PushController;

      typedef std::pair<std::string, std::string> dictPairType;
      typedef std::vector<dictPairType> dictVectorType;

      // ** Type Definitions **
      static const unsigned int PAYLOAD_MAXIMUM_SIZE;
      static const unsigned int DEFAULT_MAXIMUM_RETRIES;
      static const unsigned int MAXIMUM_DICTIONARY_VALUES;
      static const unsigned int DEFAULT_EXPIRY;

      enum apnsEnvironmentEnum {
        APNS_ENVIRONMENT_DEVEL = 0,
        APNS_ENVIRONMENT_PROD = 1
      };

      // ** Members **
      void environment(const apnsEnvironmentEnum environment) { _environment = environment; }
      const apnsEnvironmentEnum environment() const { return _environment; }
      void deviceToken(const std::string &deviceToken) { _deviceToken = deviceToken; }
      const std::string deviceToken() const { return _deviceToken; }
      void customIdentifier(const std::string &customIdentifier) { _customIdentifier = customIdentifier; }
      const std::string customIdentifier() const { return _customIdentifier; }
      void text(const std::string &text) { _text = text; }
      const std::string text() const { return _text; }
      void soundName(const std::string &soundName) { _soundName = soundName; }
      const std::string soundName() const { return _soundName; }
      void actionKeyCaption(const std::string &actionKeyCaption) { _actionKeyCaption = actionKeyCaption; }
      const std::string actionKeyCaption() const { return _actionKeyCaption; }
      void maxRetries(const unsigned int maxRetries) { _maxRetries = maxRetries; }
      const unsigned int maxRetries() const { return _maxRetries; }
      void badgeNumber(const int badgeNumber) { _badgeNumber = badgeNumber; }
      const int BadgeNumber() const { return _badgeNumber; }
      void id(const unsigned int id) { _id = id; }
      const unsigned int id() const { return _id; }
      const unsigned int retries() { return _retries; }
      const bool retry() {
        if (++_retries > _maxRetries)
          return false;

        return true;
      } // retry

      void expiry(const time_t expiry) { _expiry = expiry; }
      const time_t expiry() { return _expiry; }

      const std::string getPayload();
      const int error() const { return _error; }

    protected:
      const std::string escape(const std::string &);
      void error(const int error) { _error = error; }

    private:
      apnsEnvironmentEnum _environment;		// APNS Environment
      dictVectorType _dictVector;			// Dictionary map type.
      std::string _deviceToken;			// Device token to send message to.
      std::string _text;				// Text message to send to user.
      std::string _soundName;				// Sound name.
      std::string _actionKeyCaption;			// Action key caption.
      std::string _customIdentifier;			// Custom identifier.
      int _badgeNumber;				// Badge number.
      int _error;					// Error number.
      unsigned int _id;				// Message Id.
      unsigned int _maxRetries;			// Max retires.
      unsigned int _retries;			// Number of times message was retried.
      time_t _expiry;				// Default expiration time.
  }; // ApnsMessage

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace apns
#endif
