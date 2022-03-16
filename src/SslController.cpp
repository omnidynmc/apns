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

#include "SslController.h"

namespace apns {
  using namespace openframe::loglevel;

/**************************************************************************
 ** APNS Class                                                           **
 **************************************************************************/
  static int s_server_session_id_context = 1;

  SslController::SslController(const std::string &host, const int port, const std::string &certfile, const std::string &keyfile, const std::string &capath)
    : _host(host), _port(port), _certfile(certfile), _keyfile(keyfile), _capath(capath) {

    _initialized = false;
    _connected = false;

    return;
  } // SslController::SslController

  SslController::~SslController() {

    return;
  } // SslController::~SslController

  const bool SslController::_connect() {
    char errmsg[120];
    int err;
    int ofcmode;

    if (_connected)
      return false;

    _initialize();

    LOG(LogNotice, << "Connecting to "
                   << _host
                   << ":"
                   << _port
                   << std::endl);

    // Create an SSL_METHOD structure (choose an SSL/TLS protocol version)
    _sslcon->meth = TLSv1_2_client_method();

    // Create an SSL_CTX structure
    _sslcon->ctx = SSL_CTX_new(_sslcon->meth);

    if (!_sslcon->ctx) {
      LOG(LogError, << "Could not get SSL context for "
                    << _host
                    << ":"
                    << _port
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    // Load the CA from the Path
    if (SSL_CTX_load_verify_locations(_sslcon->ctx, NULL, _capath.c_str()) <= 0) {
      // Handle failed load here
      ERR_error_string_n(ERR_get_error(), errmsg, sizeof(errmsg));
      LOG(LogError, << "Failed to set CA location: ("
                    << _capath
                    << ") "
                    << errmsg
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    // Load the client certificate into the SSL_CTX structure
    if (SSL_CTX_use_certificate_file(_sslcon->ctx, _certfile.c_str(), SSL_FILETYPE_PEM) <= 0) {
      ERR_error_string_n(ERR_get_error(), errmsg, sizeof(errmsg));
      LOG(LogError, << "Cannot use certificate file: ("
                    << _certfile
                    << ") "
                    << errmsg
                    << std::endl);
      //ERR_print_errors_fp(stderr);
      _deinitialize();
      return false;
    } // if

    // Load the private-key corresponding to the client certificate
    if (SSL_CTX_use_PrivateKey_file(_sslcon->ctx, _keyfile.c_str(), SSL_FILETYPE_PEM) <= 0) {
      ERR_error_string_n(ERR_get_error(), errmsg, sizeof(errmsg));
      LOG(LogError, << "Cannot use private key: ("
                    << _keyfile
                    << ") "
                    << errmsg
                    << std::endl);
      //ERR_print_errors_fp(stderr);
      _deinitialize();
      return false;
    } // if

    // Check if the client certificate and private-key matches
    if (!SSL_CTX_check_private_key(_sslcon->ctx)) {
      LOG(LogError, << "Private key does not match the certificate public key."
                    << std::endl);
      _deinitialize();
      return false;
    } // if
 
    /* Set up a TCP socket */
    _sslcon->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(_sslcon->sock == -1) {
      LOG(LogError, << "Could not get socket."
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    memset(&_sslcon->server_addr, '\0', sizeof(_sslcon->server_addr));
    _sslcon->server_addr.sin_family      = AF_INET;
    _sslcon->server_addr.sin_port        = htons(_port);       /* Server Port number */
    _sslcon->host_info = gethostbyname(_host.c_str());

    if(_sslcon->host_info) {
      /* Take the first IP */
      struct in_addr *address = (struct in_addr *)_sslcon->host_info->h_addr_list[0];
      _sslcon->server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*address)); /* Server IP */
    } // if
    else {
      LOG(LogError, << "Could not resolve hostname, "
                    << _host
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    /* Establish a TCP/IP connection to the SSL client */
    err = ::connect(_sslcon->sock, (struct sockaddr*) &_sslcon->server_addr, sizeof(_sslcon->server_addr));
    if(err == -1) {
      LOG(LogError, << "Could not connect to "
                    << _host
                    << ":"
                    << _port
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    LOG(LogNotice, << "Connected to "
                   << _host
                   << ":"
                   << _port
                   << std::endl);

    // An SSL structure is created
    _sslcon->ssl = SSL_new(_sslcon->ctx);
    if(!_sslcon->ssl) {
      LOG(LogError, << "Could not get SSL socket."
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    SSL_CTX_set_session_id_context(_sslcon->ctx, (unsigned char *) &s_server_session_id_context, sizeof(s_server_session_id_context));

    // Assign the socket into the SSL structure (SSL and socket without BIO)
    SSL_set_fd(_sslcon->ssl, _sslcon->sock);

    _sslcon->sbio = BIO_new_socket(_sslcon->sock,BIO_NOCLOSE);
    SSL_set_bio(_sslcon->ssl,_sslcon->sbio,_sslcon->sbio);

    // Perform SSL Handshake on the SSL client
    err = SSL_connect(_sslcon->ssl);
    if(err == -1) {
      LOG(LogError, << "Could not perform SSL handshake to "
                    << _host
                    << ":"
                    << _port
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    /*First we make the socket nonblocking*/
    ofcmode=fcntl(_sslcon->sock,F_GETFL,0);
    ofcmode|=O_NDELAY;
    if(fcntl(_sslcon->sock,F_SETFL,ofcmode)) {
      LOG(LogError, << "Could not set socket to non-blocking"
                    << std::endl);
      _deinitialize();
      return false;
    } // if

    //_checkCert();

    _connected = true;

    return true;
  } // SslController::_connect

  // Check that the common name matches the host name
  const bool SslController::_checkCert() {
    X509 *peer;
    char peer_CN[256];

    if(SSL_get_verify_result(_sslcon->ssl) != X509_V_OK) {
      LOG(LogWarn, << "Cannot verify certificate."
                   << std::endl);
      return false;
    } // if

    /*Check the cert chain. The chain length
      is automatically checked by OpenSSL when
      we set the verify depth in the ctx */

    /*Check the common name*/
    peer = SSL_get_peer_certificate(_sslcon->ssl);
    X509_NAME_get_text_by_NID(X509_get_subject_name(peer), NID_commonName, peer_CN, 256);

    if(strcasecmp(peer_CN,_host.c_str())) {
      LOG(LogWarn, << "Common name doesn't match host name"
                   << std::endl);
      return false;
    } // if

    return true;
  } // SslController::_checkCert


  const int SslController::write(const char *packet, const size_t len) {
    int ret = -1;

    if (!_connected)
      return ret;

    ret = SSL_write(_sslcon->ssl, packet, len);

    switch(SSL_get_error(_sslcon->ssl, ret)){
      // We wrote something
      case SSL_ERROR_NONE:
        return ret;
        break;
      case SSL_ERROR_ZERO_RETURN:
        LOG(LogDebug, << "(SSL+TX) Zero Return"
                      << std::endl);
        disconnect();
        break;
      // We would have blocked */
      case SSL_ERROR_WANT_WRITE:
        LOG(LogDebug, << "(SSL+TX) Want Write"
                      << std::endl);
        return 0;
        break;
        /* We get a WANT_READ if we're
           trying to rehandshake and we block on
           write during the current connection.

           We need to wait on the socket to be readable
           but reinitiate our write when it is */
      case SSL_ERROR_WANT_READ:
        LOG(LogDebug, << "(SSL+TX) Want Read"
                      << std::endl);
        return 0;
        break;
      case SSL_ERROR_SYSCALL:
        LOG(LogDebug, << "(SSL+TX) Syscall Failed"
                      << std::endl);
        disconnect();
        return -1;
        break;
      // Some other error */
      default:
        LOG(LogDebug, << "(SSL+TX) Unknown Error"
                      << std::endl);
        disconnect();
    } // switch

    return -1;
  } // SslController::write

  const int SslController::read(void *packet, const size_t len) {
    fd_set readfds;
    int ret = -1;

    if (!_connected)
      return ret;

    timeval timeout = {0, 100000};
    FD_ZERO(&readfds);
    FD_SET(_sslcon->sock, &readfds);

    ret = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);

    if (ret == -1) {
      //_logf("SslController::read[select]> %s", strerror(errno));

      //  If we got an error from select let's figure out why.
      switch(errno) {
        case EBADF:
        // The specified time limit is invalid.  One of its components is negative or too large.
        case EINVAL:
        // A signal was delivered before the time limit expired and before any of the selected events occurred.
        case EINTR:
        // One of the descriptor sets specified an invalid descriptor.
        default:
          return -1;
          break;
      } // switch

    // should never happen
    assert(false);
  } // if

  if (!FD_ISSET(_sslcon->sock, &readfds))
    return -1;

  ret = SSL_read(_sslcon->ssl, packet, len);
  switch(SSL_get_error(_sslcon->ssl, ret)) {
    case SSL_ERROR_NONE:
      return ret;
      break;
    case SSL_ERROR_WANT_READ:
      LOG(LogDebug, << "(SSL+RX) Want Read"
                    << std::endl);
      break;
    case SSL_ERROR_ZERO_RETURN:
      LOG(LogDebug, << "(SSL+RX) Returned Zero"
                    << std::endl);
      disconnect();
      break;
    case SSL_ERROR_SYSCALL:
      LOG(LogDebug, << "(SSL+RX) Syscall Failed"
                    << std::endl);
      disconnect();
      return -1;
    default:
      LOG(LogDebug, << "(SSL+RX) Unknown"
                    << std::endl);
      disconnect();
      return -1;
  } // switch


    return ret;
  } // SslController::read

  const bool SslController::_disconnect() {
    int err;

    if (!_connected) {
      LOG(LogNotice, << "Disconnect from "
                     << _host
                     << ":"
                     << _port
                     << " attempted but not connected."
                     << std::endl);
      return false;
    } // if

    LOG(LogNotice, << "Disconnecting from "
                   << _host
                   << ":"
                   << _port
                   << std::endl);

    // Shutdown the client side of the SSL connection
    err = SSL_shutdown(_sslcon->ssl);
    if (err == -1) {
      LOG(LogError, << "Could not shutdown SSL with "
                    << _host
                    << ":"
                    << _port
                    << std::endl);
      return false;
    } // if

    /* Terminate communication on a socket */
    err = close(_sslcon->sock);
    if(err == -1) {
      LOG(LogError, << "Could not close socket with "
                    << _host
                    << ":"
                    << _port);
      return false;
    } // if

    _deinitialize();

    return true;
  } // SslController::_disconnect()

  void SslController::_initialize() {
    // this should never happen
    assert(!_initialized);

    _connected = false;

    try {
      _sslcon = new SSL_Connection;
    } // try
    catch(std::bad_alloc xa) {
      assert(false);
    } // catch

    _sslcon->ssl = NULL;
    _sslcon->ctx = NULL;

    // initialize OpenSSL library
    //SSL_library_init();

    // Load SSL error strings
    //SSL_load_error_strings();

    _initialized = true;
  } // SslController::_initialize

  void SslController::_deinitialize() {
    /* Free the SSL structure */
    if (_sslcon->ssl != NULL)
      SSL_free(_sslcon->ssl);

    /* Free the SSL_CTX structure */
    if (_sslcon->ctx != NULL);
    SSL_CTX_free(_sslcon->ctx);

    delete _sslcon;
    _sslcon = NULL;
    _connected = false;
    _initialized = false;
  } // SslController::_deinitialize
} // namespace apns
