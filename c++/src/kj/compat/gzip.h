// Copyright (c) 2017 Cloudflare, Inc. and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

#include <kj/io.h>
#include <kj/async-io.h>
#include <zlib.h>

namespace kj {

namespace _ {  // private

class GzipOutputContext final {
public:
  GzipOutputContext(kj::Maybe<int> compressionLevel);
  ~GzipOutputContext() noexcept(false);
  KJ_DISALLOW_COPY(GzipOutputContext);
  GzipOutputContext(GzipOutputContext&&) = default;

  void setInput(const void* in, size_t size);
  kj::Tuple<bool, kj::ArrayPtr<const byte>> pumpOnce(int flush);

private:
  bool compressing;
  z_stream ctx;
  byte buffer[4096];

  void fail(int result);
};

}  // namespace _ (private)

class GzipInputStream final: public InputStream {
public:
  GzipInputStream(InputStream& inner);
  ~GzipInputStream() noexcept(false);
  KJ_DISALLOW_COPY(GzipInputStream);

  size_t tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

private:
  InputStream& inner;
  z_stream ctx;
  bool atValidEndpoint = false;

  byte buffer[4096];

  size_t readImpl(byte* buffer, size_t minBytes, size_t maxBytes, size_t alreadyRead);
};

class GzipOutputStream final: public OutputStream {
public:
  GzipOutputStream(OutputStream& inner, kj::Maybe<int> compressionLevel = Z_DEFAULT_COMPRESSION);
  ~GzipOutputStream() noexcept(false);
  KJ_DISALLOW_COPY(GzipOutputStream);
  GzipOutputStream(GzipOutputStream&&) = default;

  static inline GzipOutputStream Decompress(OutputStream& inner) {
    return GzipOutputStream(inner, nullptr);
  }

  void write(const void* buffer, size_t size) override;
  using OutputStream::write;

  inline void flush() {
    pump(Z_SYNC_FLUSH);
  }

private:
  OutputStream& inner;
  _::GzipOutputContext ctx;

  void pump(int flush);
};

class GzipAsyncInputStream final: public AsyncInputStream {
public:
  GzipAsyncInputStream(AsyncInputStream& inner);
  ~GzipAsyncInputStream() noexcept(false);
  KJ_DISALLOW_COPY(GzipAsyncInputStream);

  Promise<size_t> tryRead(void* buffer, size_t minBytes, size_t maxBytes) override;

private:
  AsyncInputStream& inner;
  z_stream ctx;
  bool atValidEndpoint = false;

  byte buffer[4096];

  Promise<size_t> readImpl(byte* buffer, size_t minBytes, size_t maxBytes, size_t alreadyRead);
};

class GzipAsyncOutputStream final: public AsyncOutputStream {
public:
  GzipAsyncOutputStream(AsyncOutputStream& inner, kj::Maybe<int> compressionLevel = Z_DEFAULT_COMPRESSION);
  KJ_DISALLOW_COPY(GzipAsyncOutputStream);
  GzipAsyncOutputStream(GzipAsyncOutputStream&&) = default;

  static inline GzipAsyncOutputStream Decompress(AsyncOutputStream& inner) {
    return GzipAsyncOutputStream(inner, nullptr);
  }

  Promise<void> write(const void* buffer, size_t size) override;
  Promise<void> write(ArrayPtr<const ArrayPtr<const byte>> pieces) override;

  inline Promise<void> flush() {
    return pump(Z_SYNC_FLUSH);
  }
  // Call if you need to flush a stream at an arbitrary data point.

  Promise<void> end() {
    return pump(Z_FINISH);
  }
  // Must call to flush and finish the stream, since some data may be buffered.
  //
  // TODO(cleanup): This should be a virtual method on AsyncOutputStream.

private:
  AsyncOutputStream& inner;
  _::GzipOutputContext ctx;

  kj::Promise<void> pump(int flush);
};

}  // namespace kj
