
#pragma once

#include <stdlib.h>

#include "gfxstream/guest/IOStream.h"

class WebrogueStream : public gfxstream::guest::IOStream {
   public:
    explicit WebrogueStream(size_t bufsize);
    ~WebrogueStream();

    virtual int connect(const char* serviceName = nullptr);
    virtual uint64_t processPipeInit();

    virtual void* allocBuffer(size_t minSize);
    virtual int commitBuffer(size_t size);
    virtual const unsigned char* readFully(void* buf, size_t len);
    virtual const unsigned char* commitBufferAndReadFully(size_t size, void* buf, size_t len);
    virtual const unsigned char* read(void* buf, size_t* inout_len);


    virtual int writeFully(const void* buf, size_t len);

   private:
    size_t m_bufsize;
    unsigned char* m_buf;
};
