

#ifndef __ERLIO_IO_H__
#define __ERLIO_IO_H__

#include <event2/event.h>

namespace eio {
  class IO {
    struct event_base* eb;

  public:
    IO() {
      eb = event_base_new();
    }

    ~IO() {
      event_base_free(eb);
      eb = NULL;
    }

    void dispatch() {
      event_base_dispatch(eb);
    }

    friend class Timer;
    friend class DNS;
    friend class UDPTransport;
    friend class TCPTransport;
    friend class SSLTransport;
  };

  
};

#endif
