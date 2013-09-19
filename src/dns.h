// -*- mode: c++ -*-

#ifndef __ERLIO_DNS__
#define __ERLIO_DNS__

#include <event2/dns.h>
#include "io.h"
#include <string>

namespace eio {

  struct DNSRequest {
    struct evdns_getaddrinfo_request* req;
    void cancel() { evdns_getaddrinfo_cancel(req); }
  };

  class DNS {
    struct evdns_base *dns;

    DNS(IO& io) {
      dns = evdns_base_new( io.eb , 1 );
    }

    bool lookup(std::string hostname, short port, SockAddr& into) {
      char port_buf[6];
      struct evutil_addrinfo hints;
      struct evutil_addrinfo *answer = NULL;
      int err;

      /* Convert the port to decimal. */
      evutil_snprintf(port_buf, sizeof(port_buf), "%d", (int)port);

      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC; /* v4 or v6 is fine. */
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP; /* We want a TCP socket */
      /* Only return addresses we can use. */
      hints.ai_flags = EVUTIL_AI_ADDRCONFIG;

      err = evutil_getaddrinfo( hostname.c_str(),  port_buf, &hints, &answer );
      if (err != 0)
        return false;

      into.assign( answer->ai_addr, answer->ai_addrlen );
      return true;
    }

    ~DNS() {
      evdns_base_free( dns, 1 );
    }
  };
};



#endif

