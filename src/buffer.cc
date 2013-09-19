
#include "buffer.h"

#include <openssl/hmac.h>

namespace eio {

  class BufferIn : public google::protobuf::io::ZeroCopyInputStream {
    struct evbuffer* evb;
    size_t last_chunk_size;
    uint64_t byte_count;
  public:
    BufferIn( struct evbuffer* evb2 )
    {
      this->evb = evb2;
      last_chunk_size = 0;
      byte_count = 0;
    }

    ~BufferIn()
    {
      if (last_chunk_size != 0) {
        evbuffer_drain(evb, last_chunk_size);
      }
    }

    virtual bool Next(const void **data, int *size)
    {
      if (last_chunk_size != 0) {
        evbuffer_drain(evb, last_chunk_size);
        last_chunk_size = 0;
      }
      size_t buffer_length = evbuffer_get_length(evb);
      if (buffer_length == 0)
        return false;

      size_t chunk_size = evbuffer_get_contiguous_space(evb);
      *data = evbuffer_pullup( evb, chunk_size );
      *size = chunk_size;
      last_chunk_size = chunk_size;
      byte_count += chunk_size;

      return true;
    }

    virtual bool Skip(int count) {
      if (last_chunk_size != 0) {
        evbuffer_drain(evb, last_chunk_size);
      }
      last_chunk_size = 0;

      size_t size_left = evbuffer_get_length(evb);
      if (size_left == 0)
        return false;

      size_t skip_amount = size_left < count ? size_left : count;
      byte_count += skip_amount;
      evbuffer_drain(evb, skip_amount);
      return skip_amount == size_left;
    }

    virtual void BackUp(int count) {
      assert( count <= last_chunk_size );
      last_chunk_size -= count;
      byte_count -= count;
    }

    virtual int64_t ByteCount() const {
      return byte_count;
    }

  };

  class IOVecIn : google::protobuf::io::ZeroCopyInputStream {
    struct evbuffer_iovec *iov;
    int iovcnt;
    int next;
    int offset;
    uint64_t byte_count;

  public:
    IOVecIn( struct evbuffer_iovec *iov, int iovcnt )
      : iov(iov), iovcnt(iovcnt), next(0), byte_count(0), offset(0) {}

    virtual google::protobuf::int64 ByteCount() { return byte_count; }

    virtual bool Next( const void **data, int *size)
    {
      if (next == iovcnt) return false;
      *data = (const void*)&((const char*)iov[next].iov_base)[offset];
      *size = iov[next].iov_len - offset;
      byte_count += *size;
      next += 1;
      offset = 0;
      return true;
    }

    virtual void Backup(int count) {
      if (offset == 0) {
        next -= 1;
        offset = iov[next].iov_len - count;
        byte_count -= count;
      } else {
        offset -= count;
        byte_count -= count;
      }

      assert(offset >= 0);
    }

    virtual bool Skip(int count) {
      int here;
      int skip_left = count;
      while (next < iovcnt && (here=iov[next].iov_len) <= skip_left) {
        next += 1;
        skip_left -= here;
        byte_count += here;
      }
      if (next < iovcnt) {
        assert( iov[next].iov_len >= skip_left );
        offset = skip_left;
        byte_count += skip_left;
        return true;
      } else {
        offset = 0;
        return false;
      }
    }
  };

  bool Buffer::Parse( ::google::protobuf::MessageLite& message) {
    BufferIn in(evb);
    return message.ParseFromZeroCopyStream(&in);
  }

  bool Buffer::Serialize( ::google::protobuf::MessageLite& message) {
    BufferOut out(evb);
    return message.SerializeToZeroCopyStream(&out);
  }

  std::string Buffer::HMAC( const std::string& key ) const
  {
    unsigned char buffer[20];
    unsigned int len = 20;
    HMAC_CTX ctx;
    HMAC_CTX_init(&ctx);
    HMAC_Init(&ctx, key.data(), key.size(), EVP_sha1());

    int howmany = this->peek((struct evbuffer_iovec *) NULL, 0);
    struct evbuffer_iovec vec[howmany];
    this->peek(&vec[0], howmany);
    for (int i = 0; i < howmany; i++) {
      HMAC_Update(&ctx, (const unsigned char*) vec[i].iov_base, vec[i].iov_len);
    }
    HMAC_Final(&ctx, &buffer[0], &len);
    HMAC_CTX_cleanup(&ctx);

    return std::string((const char*)&buffer[0], (size_t) len);
  }

}
