// -*- mode:c++ ; c-basic-offset: 2 -*-

#include "transport.h"

#include <event2/buffer.h>
#include "buffer.h"
#include "io.h"

#define UDP_MAX_VECTORS 16
#define MTU 2000

#define USE_WRITEV

namespace eio {


  void
  Handler::data(StreamTransport& t, struct evbuffer_iovec* vec, int vecnt) {}

  void
  Handler::data(StreamTransport& t, Buffer& packet) {}

  void
  Handler::data(StreamTransport& t, std::string& packet) {}

  void
  Handler::udp( UDPTransport& transport, SockAddr& from, std::string& packet ) {}

  void
  Handler::timeout( Timer& timer) {}

  void
  Handler::timeout( StreamTransport& t ) {}

  void Handler::eof( StreamTransport& transport ) {}
  void Handler::error( StreamTransport& transport, int error ) {}
  void Handler::connected( StreamTransport& transport ) {}
  void Handler::disconnected( StreamTransport& transport ) {}


  bool
  Transport::send(Buffer& buf)
  {
    switch (opt_packet) {
    case 4:
      {
        size_t size = buf.length();
        uint32_t length = htonl(size);
        buf.prepend((void*)&length, sizeof(length));
        return raw_send(buf);
      }
    case 2:
      {
        size_t size = buf.length();
        if (size > EV_UINT16_MAX) return false;
        uint16_t length = htons(size);
        buf.prepend((void*)&length, sizeof(length));
        return raw_send(buf);
      }
    case 1:
      {
        size_t size = buf.length();
        if (size > EV_UINT8_MAX) return false;
        uint8_t length = size;
        buf.prepend((void*)&length, sizeof(length));
        return raw_send(buf);
      }
    case 0:
      raw_send(buf);

    default:
      ::abort();
    }
  }

  bool
  Transport::send(struct evbuffer_iovec *iovec, int iovcnt)
  {
    switch (opt_packet) {
    case 4:
      {
        size_t size = iovsize(iovec, iovcnt);
        if (size > EV_UINT32_MAX) { return false; }
        uint32_t length = htonl(size);

        struct evbuffer_iovec iov[ 1 + iovcnt ];
        iov[0].iov_base = &length;
        iov[0].iov_len = sizeof(length);
        memcpy(&iov[1], iovec, iovcnt*sizeof(struct evbuffer_iovec));
        return raw_send( iov, iovcnt+1);
      }

    case 2:
      {
        size_t size = iovsize(iovec, iovcnt);
        if (size > EV_UINT16_MAX) { return false; }
        uint16_t length = htons(size);

        struct evbuffer_iovec iov[ 1 + iovcnt ];
        iov[0].iov_base = &length;
        iov[0].iov_len = sizeof(length);
        memcpy(&iov[1], iovec, iovcnt*sizeof(struct evbuffer_iovec));
        return raw_send( iov, iovcnt+1);
      }

    case 1:
      {
        size_t size = iovsize(iovec, iovcnt);
        if (size > EV_UINT8_MAX) { return false; }
        uint8_t length = htons(size);

        struct evbuffer_iovec iov[ 1 + iovcnt ];
        iov[0].iov_base = &length;
        iov[0].iov_len = sizeof(length);
        memcpy(&iov[1], iovec, iovcnt*sizeof(struct evbuffer_iovec));
        return raw_send( iov, iovcnt+1);
      }

    case 0:
      return raw_send( iovec, iovcnt );

    default:
      exit(1);
      return false;
    }
  }

  void
  UDPTransport::did_set_active(ActiveMode mode) {
    if (mode == ACTIVE || mode == ACTIVE_ONCE) {
      event_add(ev, NULL);
    }
  }

  bool
  UDPTransport::connect(SockAddr& addr) {
    return ::connect( event_get_fd(ev), addr.sockaddr(), addr.sockaddr_length() ) == 0;
  }

  bool
  UDPTransport::connect(std::string& host, uint16_t port, DNS *dns) {
    struct evdns_base *dns_base = (dns == NULL) ? NULL : dns->base();

    char buf[ host.length() + 8 ];
    sprintf(buf, "%s:%i", host.c_str(), port);
    SockAddr addr("0.0.0.0:0");

    if (! addr.assign( std::string( buf, strlen(buf) ) )) {

      return false;

    }

    return connect( addr );
  }


  void
  UDPTransport::callback(short what)
  {
    if (what == EV_READ) {
      char buffer[MTU];
      ssize_t size;
      struct sockaddr_in6 address;
      socklen_t address_len = sizeof(address);
      evutil_socket_t sock = event_get_fd( ev );

      size = recvfrom(sock,
                      buffer,
                      MTU,
                      0,
                      (struct sockaddr*) &address,
                      &address_len);

      if (size > opt_packet) {
        const char* base = &buffer[opt_packet];
        size_t len;
        switch (opt_packet) {
        case 0:
          len = size;
          break;

        case 1:
          len = *(uint8_t*)&buffer[0];
          break;

        case 2:
          len = *(uint16_t*)&buffer[0];
          break;

        case 4:
          len = *(uint32_t*)&buffer[0];
          break;

        default:
          // error!
          abort();
        }

        std::string packet( base, len );
        SockAddr addr( (struct sockaddr*)&address, address_len );
        handler->udp( *this, addr, packet );
      } else {
        // TODO: handle error
      }
    }
  }

  void udp_callback(evutil_socket_t sock, short what, void *arg)
  {
    UDPTransport *transport = static_cast<UDPTransport*>(arg);
    transport->callback(what);
  }

  UDPTransport::UDPTransport(IO& io, Handler* handler, int family)  : Transport(io, handler) {
    evutil_socket_t sock = socket(family, SOCK_DGRAM, 0);
    evutil_make_socket_nonblocking(sock);
    evutil_make_listen_socket_reuseable(sock);
    ev = event_new(io.eb, sock, EV_READ, udp_callback, static_cast<void*>(this));
  }

  UDPTransport::~UDPTransport() {
    evutil_socket_t sock = event_get_fd(ev);
    event_free(ev);
    evutil_closesocket(sock);
  }

  bool
  UDPTransport::bind( SockAddr& addr) {
    evutil_socket_t sock = event_get_fd(ev);
    int err = ::bind(sock, addr.sockaddr(), addr.sockaddr_length());
    return err == 0;
  }

  bool UDPTransport::raw_send( struct evbuffer_iovec* iov, int iovcnt ) {
    evutil_socket_t fd = event_get_fd(ev);

    unsigned char empty[0];
    size_t data_len = iovsize(iov, iovcnt);
    size_t num_sent = -1;
    // 
    if (data_len == 0) {
      num_sent = ::send( fd, &empty[0], 0, 0 );

    } else if (iovcnt == 1) {
      num_sent = ::send( fd, iov[0].iov_base, iov[0].iov_len, 0 );

#ifdef USE_WRITEV
    } else if (iovcnt <= 16) {
      num_sent = ::writev( fd, iov, iovcnt );

#endif

    } else {
      std::string packet;
      packet.reserve(data_len);
      for (int i = 0; i < iovcnt; i++) {
        packet.append( (const char*) iov[i].iov_base, iov[i].iov_len );
      }

      // not sure if it is OK to use writev here, so we gather before the write
      num_sent = ::send( fd, packet.data(), packet.length(), 0);
    }

    return num_sent == data_len;
  }

  bool StreamTransport::raw_send( struct evbuffer_iovec* iovec, int iovcnt ) {
    if (!valid()) return false;
    Buffer out = bufferevent_get_output( bev );
    out.add( iovec, iovcnt );
    bufferevent_enable(bev, EV_WRITE);
    return true;
  }

  bool
  StreamTransport::connect(SockAddr& addr) {
    if (!valid()) return false;
    int err = bufferevent_socket_connect(bev, addr.sockaddr(), addr.sockaddr_length() );
    return (err == 0);
  }


  bool StreamTransport::connect(std::string& host, uint16_t port, DNS *dns) {
    if (!valid()) return false;
    struct evdns_base *dns_base = (dns == NULL) ? NULL : dns->base();
    int err = bufferevent_socket_connect_hostname(bev, dns_base, AF_UNSPEC, host.c_str(), port);
    return (err == 0);
  }

  void
  StreamTransport::did_set_active(ActiveMode mode) {
    if (!valid()) return;
    if (mode == ACTIVE || mode == ACTIVE_ONCE) {
      bufferevent_enable(bev, EV_READ);
    } else {
      bufferevent_disable(bev, EV_READ);
    }
  }

  void StreamTransport::event_cb(short what)
  {
    if ((what & BEV_EVENT_ERROR) != 0) {
      int error = EVUTIL_SOCKET_ERROR();
      if (handler != NULL) handler->error( *this, error );
    } else if ((what & BEV_EVENT_TIMEOUT) != 0) {
      if (handler != NULL) handler->timeout( *this );
    } else if ((what & BEV_EVENT_EOF) != 0) {
      if (handler != NULL) handler->eof( *this );
    } else if ((what & BEV_EVENT_CONNECTED) != 0) {
      if (handler != NULL) handler->connected( *this );
    }
  }

  static size_t max(size_t v1, size_t v2) {
    if (v1 > v2) return v1;
    return v2;
  }

  void StreamTransport::read_cb()
  {
    if (!valid()) return;
  again:
    struct evbuffer *evb = bufferevent_get_input(bev);
    size_t available = evbuffer_get_length(evb);
    size_t packet_size;

    if (opt_packet == 0) {
      packet_size = available;

    } else {
      void *head = evbuffer_pullup(evb, opt_packet);
      switch(opt_packet) {
      case 1:
        packet_size = *(uint8_t*)head;
        break;
      case 2:
        packet_size = ntohs(*(uint16_t*)head);
        break;
      case 4:
        packet_size = ntohl(*(uint32_t*)head);
        break;
      default:
        abort();//exit(1);
      }
    }

    size_t fragment_size = opt_packet + packet_size;

    if (available < fragment_size) {
      // ensure that high read watermark is big enough to contain entire fragment
      bufferevent_setwatermark(bev, EV_READ,
                               lowmark_read = packet_size,
                                max( fragment_size, highmark_read ));
    } else {

      if (opt_active == ACTIVE_ONCE) {
        set_active( PASSIVE );
      }

      // ok, we're ready to actualy read the packet

      if (opt_deliver == BUFFER) {
        struct evbuffer *into = evbuffer_new();
        if (opt_packet != 0) evbuffer_drain(evb, opt_packet);
        evbuffer_remove_buffer(evb, into, packet_size);
        Buffer intob(into);
        handler->data(*this, intob);
      } else {

        struct evbuffer_ptr packet_start;
        evbuffer_ptr_set(evb, &packet_start, opt_packet, EVBUFFER_PTR_SET);

        int iovcnt = evbuffer_peek( evb, packet_size, &packet_start, NULL, 0);
        struct evbuffer_iovec ios[iovcnt];
        evbuffer_peek( evb, packet_size, &packet_start, ios, iovcnt);

        // if we got too much, adjust the iovec to only the desired amount
        int datasize = iovsize( ios, iovcnt );
        int remove = datasize-packet_size;
        for (int i = iovcnt-1; remove > 0; i--) {
          int len = ios[i].iov_len;
          if (len >= remove) {
            ios[i].iov_len -= remove;
            remove = 0;
          } else {
            ios[i].iov_len = 0;
            remove -= len;

            if (i == (iovcnt-1)) {
              iovcnt -= 1;
            }
          }
        }

        assert( iovsize(ios, iovcnt) == packet_size );

        if (opt_deliver == IOVEC) {
          handler->data( *this, &ios[0], iovcnt );
          evbuffer_drain( evb, fragment_size );
        } else /* opt_deliver == STRING */ {

          assert( opt_deliver == STRING );

          std::string packet;
          packet.reserve(packet_size);
          for (int i = 0; i < iovcnt; i++) {
            packet.append( (const char*) ios[i].iov_base, ios[i].iov_len );
          }
          evbuffer_drain( evb, fragment_size );

          assert( packet.length() == packet_size );

          handler->data( *this, packet );
        }
      }
      if (!valid()) return;

      // take more
      if (opt_active != PASSIVE && evbuffer_get_length(evb) > opt_packet)
        goto again;

      if (opt_active != PASSIVE) {
        assert ( ( bufferevent_get_enabled(bev) & EV_READ ) != 0 );
      }

      bufferevent_setwatermark(bev, EV_READ,
                               lowmark_read=opt_packet,
                               highmark_read);
    }

    return;
  }

  void
  stream_event_cb(struct bufferevent *bev, short what, void *ctx)
  {
    StreamTransport* bet = static_cast<StreamTransport*>(ctx);
    bet->event_cb(what);
  }

  void
  stream_read_cb(struct bufferevent *bev, void *ctx)
  {
    StreamTransport* bet = static_cast<StreamTransport*>(ctx);
    bet->read_cb();
  }

}
