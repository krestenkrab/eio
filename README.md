Asynchronous I/O in C++ for Erlang Heads
===

This library is a C++ wrapper for [libevent](http://libevent.org/), that lets you write programs with asynchronous I/O using some common Erlang idioms.  

Most importantly, it provides a straight-forward way to use the `{active, once}` and `{packet, 4}` packet options that Erlang provides, so you don't have to write that logic.

We only build it as a static library, since it's so small that it is better linked statically than depended upon in a system installation.

````c++
class ProtoHandler : eio::Handler {
   void tcp( eio::TCPTransport& sock, std::string& packet ) {
       // handle incoming packet

       // send some data
       sock.send( " some data " );

       // make the sock active again after processing
       // i.e., read next 4-byte delimited packet
       sock.set_active( eio::ACTIVE_ONCE );
   }
};

void main() {
   eio::IO io;
   eio::TCPTransport sock(io, new ProtocolHandler());

   // use 4-byte network order header for packets
   sock.set_packet( 4 );
   sock.set_active( eio::ACTIVE_ONCE );

   // connect
   eio::SockAddr addr("localhost:8080");
   sock.connect(addr);

   io.dispatch()
}
````