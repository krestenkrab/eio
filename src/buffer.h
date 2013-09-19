// -*- mode: c++ -*-

#ifndef __ERLIO_BUFFER_H__
#define __ERLIO_BUFFER_H__

#include <stdarg.h>
#include <event2/buffer.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/zero_copy_stream.h>

namespace eio {

  /*
   * A Buffer is like an erlang io_data; it's a list of byte arrays
   */
  class Buffer {
    struct evbuffer* evb;
    bool owned;

    friend class BufferOut;

  public:

    const char *pullup(int bytes) const { return (const char*) evbuffer_pullup(evb, bytes); }

    size_t length() const { return evbuffer_get_length(evb); }

    Buffer() {
      evb = evbuffer_new();
      owned = true;
    }

    Buffer(struct evbuffer*evb) {
      this->evb = evb;
      owned = false;
    }

    ~Buffer() {
      if (owned) { evbuffer_free(evb); }
    }

    bool write_to( evutil_socket_t fd, size_t howmuch = -1) {
      if (howmuch == -1) { howmuch = length(); }
      return evbuffer_write_atmost( evb, fd, howmuch ) != -1;
    }

    int peek(size_t len,
             size_t offset,
             struct evbuffer_iovec *vec_out, int n_vec) const
    {
      struct evbuffer_ptr offset_ptr;
      evbuffer_ptr_set(evb, &offset_ptr, offset, EVBUFFER_PTR_SET);
      return evbuffer_peek(evb, len, &offset_ptr, vec_out, n_vec);
    }

    int peek(struct evbuffer_iovec *vec_out, int n_vec) const
    {
      return evbuffer_peek(evb, length(), NULL, vec_out, n_vec);
    }

    bool add(const void* data, size_t length) { return evbuffer_add(evb, data, length) == 0; }
    bool add(Buffer& buffer) { return evbuffer_add_buffer(evb, buffer.evb) == 0; }
    bool add(struct evbuffer_iovec *iov, int iovcnt) {
      size_t size = 0;
      for (int i = 0; i < iovcnt; i++) { size += iov[i].iov_len; }

      evbuffer_expand(evb, size);

      for (int i = 0; i < iovcnt; i++) {
        if (evbuffer_add(evb, (const void*)iov[i].iov_base, iov[i].iov_len) != 0) {
          return false;
        }
      }

      return true;
    }

    bool prepend(const void* data, size_t length) {
      return evbuffer_prepend(evb, data, length) == 0;
    }

    bool prepend(Buffer& buffer) {
      return evbuffer_prepend_buffer(evb, buffer.evb);
    }

    bool add_printf( const char* fmt, ...) {
      va_list args;
      va_start(args, fmt);
      bool result = evbuffer_add_vprintf(evb, fmt, args) != -1;
      va_end(args);
      return result;
    }

    bool Parse( ::google::protobuf::MessageLite& message);
    bool Serialize( ::google::protobuf::MessageLite& message);

    std::string HMAC( const std::string& key) const;
  };

  class BufferOut : public google::protobuf::io::ZeroCopyOutputStream
  {
    struct evbuffer* evb;

    size_t reserved;
    struct evbuffer_iovec iov[2];
    int64_t bytecount;

  public:

    BufferOut(struct evbuffer* evb) : evb(evb), reserved(0), bytecount(0) {}
    BufferOut(Buffer& b) : evb(b.evb), reserved(0), bytecount(0) {}

    virtual ~BufferOut() {
      if (reserved != 0) {
        iov[0].iov_len = reserved;
        evbuffer_commit_space(evb, &iov[0], 1);
        reserved = 0;
      }
    }

    virtual bool Next(void ** data, int * size)
    {
      if (reserved != 0) {
        iov[0].iov_len = reserved;
        evbuffer_commit_space(evb, &iov[0], 1);
        reserved = 0;
      }

      if (evbuffer_reserve_space(evb, 1024, &iov[0], 2) < 0) {
        return false;
      }
      *data = iov[0].iov_base;
      reserved = *size = iov[0].iov_len;

      bytecount += reserved;
      return true;
    }

    virtual void BackUp(int size)
    {
      assert( size <= reserved );
      reserved -= size;
      bytecount -= size;
    }

    virtual int64_t ByteCount() const { return bytecount; }
  };


}

#endif
