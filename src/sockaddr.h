
#ifndef _MDG_SOCKADDR_H_
#define _MDG_SOCKADDR_H_

#include <event2/util.h>
#include <netinet/in.h>
#include <string>
#include <assert.h>

namespace eio {
class SockAddr {

  char _data[ sizeof(struct sockaddr_in6) ];

 public:

  size_t sockaddr_length() {
    if (family() == AF_INET6) return sizeof(struct sockaddr_in6);
    if (family() == AF_INET) return sizeof(struct sockaddr_in);
    return 0;
  }

  struct sockaddr *sockaddr() {
    return (struct sockaddr*)&_data[0];
  }

  void assign( struct sockaddr* addr, int len ) {
    int outlen = sizeof(_data);
    memset(&_data[0], 0, outlen);
    memcpy(&_data[0], addr, len);
    assert( len >= 0 );
    assert( sockaddr_length() == (size_t) len );
  }

  SockAddr( std::string addr ) {
    assign( addr );
  }

  bool assign( std::string addr) {
    int outlen = sizeof(_data);
    memset(&_data[0], 0, outlen);
    if (evutil_parse_sockaddr_port( addr.c_str(),
                                    (struct sockaddr*)&_data[0],
                                    &outlen) != -1) {
      return true;
    }

    std::string host, port;
    size_t pos;
    if ((pos=addr.find_first_of(':')) == std::string::npos) {
      port = "0";
      host = addr;
    } else {
      port = addr.substr( pos + 1 );
      host = addr.substr(0, pos );
    }

    struct evutil_addrinfo hints;
    struct evutil_addrinfo *answer;

    hints.ai_family = AF_UNSPEC; /* v4 or v6 is fine. */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP; /* We want a TCP socket */
    hints.ai_flags = EVUTIL_AI_ADDRCONFIG;

    if (evutil_getaddrinfo( host.c_str(), port.c_str(), &hints, &answer) != -1)
      {
        assign( answer->ai_addr, answer->ai_addrlen );
        printf("lookup succeeded %s:%s -> %s\n", host.c_str(), port.c_str(), unparse().c_str());
        return true;
      }
    else
      {
        printf("lookup failed %s:%s -> \n", host.c_str(), port.c_str());
      }

    ((struct sockaddr_in6*)&_data)->sin6_family = AF_UNSPEC;

    return false;
  }


  SockAddr( void* sockaddr, size_t len) {
    memset(&_data[0], 0, sizeof(_data));
    if (len <= sizeof(_data)) {
      memcpy(&_data[0], sockaddr, len);
    }
  }

  SockAddr( struct sockaddr_in6& addr) {
    memcpy( &_data[0], &addr, sizeof(struct sockaddr_in6));
  }

  SockAddr( struct sockaddr_in& addr) {
    memset(&_data[0], 0, sizeof(_data));
    memcpy( &_data[0], &addr, sizeof(struct sockaddr_in));
  }

  // return one of AF_INET6 or AF_INET
  int family() {
    return ((struct sockaddr_in6*)&_data)->sin6_family;
  }

  uint16_t port() {
    if (family() == AF_INET6) {
      return ntohs(((struct sockaddr_in6*)&_data[0])->sin6_port);
    } else if (family() == AF_INET) {
      return ntohs(((struct sockaddr_in*)&_data[0])->sin_port);
    } else {
      return 0;
    }
  }

  void *in_addr(int *len = NULL) {
    if (family() == AF_INET6) {
      if (len != NULL) *len = sizeof(struct in6_addr);
      return &((struct sockaddr_in6*)&_data[0])->sin6_addr;
    } else if (family() == AF_INET) {
      if (len != NULL) *len = sizeof(struct in_addr);
      return &((struct sockaddr_in*)&_data[0])->sin_addr;
    } else {
      if (len != NULL) *len = 0;
      return NULL;
    }
  }

  //
  // print as [ffff::]:80, ffff::, 1.2.3.4:80, 1.2.3.4
  //

  std::string unparse() {
    std::string result = "";

    if (family() == AF_INET6 && port() != 0) {
      result.append("[");
    }

    if (family() == AF_INET6 || family() == AF_INET) {
      char buffer[256];
      const char *res =
        evutil_inet_ntop(family(), in_addr(), &buffer[0], sizeof(buffer));
      result.append( res );
    } else {
      result.append( "*" );
    }

    if (family() == AF_INET6 && port() != 0) {
      result.append("]");
    }

    if (port() != 0) {
      char buffer[16];
      sprintf(&buffer[0], ":%i", port());
      result.append(&buffer[0]);
    }

    return result;
  }

};
};
#endif
