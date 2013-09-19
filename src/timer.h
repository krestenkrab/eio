

#ifndef __ERLIO_TIMER_H__
#define __ERLIO_TIMER_H__

#include "io.h"

namespace eio {

  class IO;

  class Timer {
    Handler *handler;
    struct event* ev;

    static void fire_timer( evutil_socket_t fd, short what, void *arg)
    {
      if (what & EV_TIMEOUT) {
        Timer *self = static_cast<Timer*>(arg);
        self->fire();
      }
    }

    static int diff_ms(timeval t1, timeval t2)
    {
      return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
              (t1.tv_usec - t2.tv_usec))/1000;
    }

    void fire() {
      if (handler != NULL) {
        handler->timeout(*this);
      }
    }

  public:

    uint32_t pending_millis()
    {
      struct timeval tv_fire;
      if (evtimer_pending(ev, &tv_fire)) {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        return diff_ms(tv_fire, tv_now);
      }

      return 0;
    }

    bool operator==(Timer& other) { return other.ev == ev; }

    Timer(IO& io, Handler *handler = NULL) : handler(handler) {
      ev = evtimer_new( io.eb, fire_timer, static_cast<void*>(this));
    }

    ~Timer() {
      event_free(ev);
    }

    void cancel() {
      event_del( ev );
    }

    void schedule( uint32_t millis ) {
      struct timeval tv = { millis / 1000, (millis % 1000) * 1000 };
      event_add( ev, &tv );
    }
  };

}

#endif
