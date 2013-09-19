// -*- mode:c++ -*-


#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

// for struct iovec
#include <sys/uio.h>
#include <string>
#include <assert.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/buffer.h>

#include <openssl/ssl.h>

#include "sockaddr.h"
#include "buffer.h"
#include <errno.h>

#include "io.h"

namespace eio {
  typedef enum { ACTIVE, ACTIVE_ONCE, PASSIVE } ActiveMode;
  typedef enum { IOVEC, STRING, BUFFER } DeliverMode;

  class IO;
  class Timer;
  class Transport;
  class StreamTransport;
  class UDPTransport;
  class Buffer;

  class Handler {
  public:
    virtual void udp( UDPTransport& transport, SockAddr& from, std::string& packet );

    virtual void data( StreamTransport& transport, Buffer& packet );
    virtual void data( StreamTransport& transport, std::string& packet );
    virtual void data( StreamTransport& transport, struct evbuffer_iovec* iov, int ioncnt );

    virtual void eof( StreamTransport& transport );
    virtual void error( StreamTransport& transport, int error);
    virtual void timeout( StreamTransport& transport );
    virtual void connected( StreamTransport& transport );
    virtual void disconnected( StreamTransport& transport );

    virtual void timeout( Timer& timer);
  };

  class Transport {
    IO& io;

    friend class TCPTransport;

  private:

  protected:
    ActiveMode opt_active;
    DeliverMode opt_deliver;
    uint8_t opt_packet;
    Handler* handler;
    evutil_socket_t fd;

    virtual void did_set_active(ActiveMode mode) = 0;

  public:
    Transport(IO& io, Handler* handler = NULL)
      : io(io),
        opt_active(PASSIVE),
        opt_deliver(STRING),
        opt_packet(0),
        handler(handler),
        fd(-1) {}

    void set_handler(Handler *handler) { this->handler = handler; }

    bool send(struct evbuffer_iovec *iovec, int iovcnt);
    bool send(std::string& data) {
      return send(data.data(), data.size());
    }
    bool send(Buffer& ev);
    bool send(const char* data, size_t length) {
      struct evbuffer_iovec vec[] = { { (void*) data, length } };
      return send(vec, 1);
    }

    virtual bool raw_send( struct evbuffer_iovec* iovec, int iovcnt ) = 0;
    bool raw_send( Buffer& buf ) {
      return buf.write_to( fd );
    }

    virtual void set_packet(int size) {
      assert( size==0 || size==1 || size==2 || size==4 );
      opt_packet = size;
    }

    void set_deliver(DeliverMode mode) {
      this->opt_deliver = mode;
    }

    void set_active(bool on=true) { set_active(on?ACTIVE:PASSIVE); }
    void set_active(ActiveMode mode) {
      this->opt_active = mode;
      did_set_active(mode);
    }

    virtual bool connect(SockAddr& addr) = 0;

  protected:

    short read_mode() {
      switch (opt_active) {
      case ACTIVE:
        return EV_READ|EV_PERSIST;
      case ACTIVE_ONCE:
        return EV_READ;
      case PASSIVE:
        return 0;
      }
    }


  };

  class UDPTransport : public Transport {
    struct event* ev;
    virtual bool raw_send( struct evbuffer_iovec* iovec, int iovcnt );
    void callback(short what);

    virtual void did_set_active(ActiveMode mode);

  public:
    UDPTransport(IO& io, Handler* handler, int family = AF_INET);
    ~UDPTransport();

    int error() {
      int error = evutil_socket_geterror(event_get_fd(ev));
      return error;
    }

    std::string error_string() {
      int err = error();
      return evutil_socket_error_to_string(err);
    }

    bool bind(SockAddr& addr);

    friend void udp_callback(evutil_socket_t sock, short what, void *arg);

    virtual bool connect(SockAddr& addr);

  };

  void stream_read_cb(struct bufferevent *bev, void *ctx);
  void stream_event_cb(struct bufferevent *bev, short what, void *ctx);

  class StreamTransport : public Transport {

  protected:

    size_t lowmark_read, highmark_read;
    size_t lowmark_write, highmark_write;
    struct bufferevent *bev;

    virtual bool raw_send( struct evbuffer_iovec* iovec, int iovcnt );
    virtual void did_set_active(ActiveMode mode);

  public:

    StreamTransport(IO& io, struct bufferevent *bev, Handler *handler = NULL) :
      Transport( io, handler ),
      highmark_read(512 * 1024),
      highmark_write(0),
      bev(bev)
  {
    bufferevent_setcb(bev, stream_read_cb, NULL, stream_event_cb, (void*) this);
  }


    virtual void set_packet(int size) {
      Transport::set_packet(size);
      bufferevent_setwatermark(bev, EV_READ, size, highmark_read);
    }

    virtual void read_cb();
    virtual void event_cb(short what);

    virtual bool connect(SockAddr& addr);

  };

  class TCPTransport : public StreamTransport {

  public:

    TCPTransport(IO& io, Handler *handler = NULL) :
      StreamTransport( io,
                       bufferevent_socket_new( io.eb, -1,
                                               BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS),
                       handler)
    {
      //
    }

  };

  class SSLTransport : public StreamTransport {
      public:

    SSLTransport(IO& io, SSL *ssl, Handler *handler = NULL) :
      StreamTransport( io,
                       bufferevent_openssl_socket_new( io.eb, -1, ssl,
                                                       BUFFEREVENT_SSL_CONNECTING,
                                                       BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS ),
                       handler)
    {
      //
    }


  };

  static size_t iovsize(struct evbuffer_iovec *iovec, int iovcnt)
  {
    size_t size = 0;
    for (int i = 0; i < iovcnt; i++)
      size += iovec[i].iov_len;
    return size;
  }

}

#endif
