/*
 * Copyright 2011 Google LLC
 * SPDX-License-Identifier: MIT
 */
 
#include "WebrogueStream.h"
#include <cstdint>
#include <cstdlib>

__attribute__((import_name("commit_buffer")))
__attribute__((import_module("webrogue_gfx")))
void imported_webrogue_gfx_commit_buffer(const void* buf, uint32_t len);


__attribute__((import_name("ret_buffer_read")))
__attribute__((import_module("webrogue_gfx"))) void
imported_webrogue_gfx_ret_buffer_read(const void* buf, uint32_t len);


WebrogueStream::WebrogueStream(size_t bufsize): gfxstream::guest::IOStream(bufsize), m_buf(nullptr), m_bufsize(0) {}

static const size_t kReadSize = 512 * 1024;
static const size_t kWriteOffset = kReadSize;

WebrogueStream::~WebrogueStream() {
    
    flush();
    if (m_buf != NULL) {
        free(m_buf);
    }
}

int WebrogueStream::connect(const char* serviceName) {
    return 0;
}

uint64_t WebrogueStream::processPipeInit() {
    abort();
}

void* WebrogueStream::allocBuffer(size_t minSize) {
    // Add dedicated read buffer space at the front of the buffer.
    minSize += kReadSize;

    size_t allocSize = (m_bufsize < minSize ? minSize : m_bufsize);
    if (!m_buf) {
        m_buf = (unsigned char*)malloc(allocSize);
    } else if (m_bufsize < allocSize) {
        unsigned char* p = (unsigned char*)realloc(m_buf, allocSize);
        if (p != NULL) {
            m_buf = p;
            m_bufsize = allocSize;
        } else {
            abort();
            free(m_buf);
            m_buf = NULL;
            m_bufsize = 0;
        }
    }

    return m_buf + kWriteOffset;
};

int WebrogueStream::commitBuffer(size_t size) {
    if (size == 0) return 0;
    return writeFully(m_buf + kWriteOffset, size);
}

int WebrogueStream::writeFully(const void* buf, size_t len) {
    imported_webrogue_gfx_commit_buffer(buf, len);
    return 0;
}

const unsigned char* WebrogueStream::readFully(void* buf, size_t len) {

    imported_webrogue_gfx_ret_buffer_read(buf, len);
    return (const unsigned char*)buf;
}

const unsigned char* WebrogueStream::commitBufferAndReadFully(size_t writeSize,
                                                            void* userReadBufPtr,
                                                            size_t totalReadSize) {
    commitBuffer(writeSize);
    readFully(userReadBufPtr, totalReadSize);
    return (const unsigned char*) userReadBufPtr;
}

const unsigned char* WebrogueStream::read(void* buf, size_t* inout_len) {
    abort();
    // if (!valid()) return NULL;
    // if (!buf) {
    //     mesa_loge("WebrogueStream::read failed, buf=NULL");
    //     return NULL;  // do not allow NULL buf in that implementation
    // }

    // int n = recv(buf, *inout_len);

    // if (n > 0) {
    //     *inout_len = n;
    //     return (const unsigned char*)buf;
    // }

    // return NULL;
}

