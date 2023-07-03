/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INF_STREAM_H
#define INF_STREAM_H

#include <bzlib.h>
#include <cassert>
#include <iostream>
#include <lzma.h>
#include <memory>
#include <zlib.h>

namespace champsim
{
namespace decomp_tags
{
enum class status_t { CAN_CONTINUE, END, ERROR };

namespace detail
{
template <typename State, typename R, R (*Del)(State*)>
struct end_deleter {
  void operator()(State* s)
  {
    Del(s);
    delete s;
  }
};
} // namespace detail

struct bzip2_tag_t {
  using state_type = bz_stream;
  using in_char_type = std::remove_pointer_t<decltype(state_type::next_in)>;
  using out_char_type = std::remove_pointer_t<decltype(state_type::next_out)>;
  using deflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, int, ::BZ2_bzCompressEnd>>;
  using inflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, int, ::BZ2_bzDecompressEnd>>;
  using status_type = status_t;

  static status_type deflate(deflate_state_type& x, bool flush)
  {
    auto ret = ::BZ2_bzCompress(x.get(), flush ? BZ_FLUSH : BZ_RUN);
    if (ret == BZ_RUN_OK) {
      return status_type::CAN_CONTINUE;
    }
    if (ret == BZ_FLUSH_OK) {
      return status_type::END;
    }
    return status_type::ERROR;
  }

  static status_type inflate(inflate_state_type& x)
  {
    ::BZ2_bzDecompress(x.get());
    return status_type::CAN_CONTINUE;
  }

  static deflate_state_type new_deflate_state()
  {
    deflate_state_type state{new state_type};
    *state = state_type{NULL, 0U, 0U, 0U, NULL, 0U, 0U, 0U, NULL, NULL, NULL, NULL};
    ::BZ2_bzCompressInit(state.get(), 9, 0, 0);
    return state;
  }

  static inflate_state_type new_inflate_state()
  {
    inflate_state_type state{new state_type};
    *state = state_type{NULL, 0U, 0U, 0U, NULL, 0U, 0U, 0U, NULL, NULL, NULL, NULL};
    ::BZ2_bzDecompressInit(state.get(), 0, 0);
    return state;
  }
};

template <int window = 15 + 16, int compression = Z_DEFAULT_COMPRESSION>
struct gzip_tag_t {
  using state_type = z_stream;
  using in_char_type = std::remove_pointer_t<decltype(state_type::next_in)>;
  using out_char_type = std::remove_pointer_t<decltype(state_type::next_out)>;
  using deflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, int, ::deflateEnd>>;
  using inflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, int, ::inflateEnd>>;
  using status_type = status_t;

  static status_type deflate(deflate_state_type& x, bool flush)
  {
    auto ret = ::deflate(x.get(), flush ? Z_FINISH : Z_NO_FLUSH);
    if (ret == Z_OK) {
      return status_type::CAN_CONTINUE;
    }
    if (ret == Z_STREAM_END) {
      return status_type::END;
    }
    return status_type::ERROR;
  }

  static status_type inflate(inflate_state_type& x)
  {
    ::inflate(x.get(), Z_BLOCK);
    return status_type::CAN_CONTINUE;
  }

  static deflate_state_type new_deflate_state()
  {
    deflate_state_type state{new state_type};
    *state = state_type{Z_NULL, 0, 0, Z_NULL, 0, 0, NULL, NULL, Z_NULL, Z_NULL, Z_NULL, 0, 0UL, 0UL};
    ::deflateInit(state.get(), compression);
    return state;
  }

  static inflate_state_type new_inflate_state()
  {
    inflate_state_type state{new state_type};
    *state = state_type{Z_NULL, 0, 0, Z_NULL, 0, 0, NULL, NULL, Z_NULL, Z_NULL, Z_NULL, 0, 0UL, 0UL};
    ::inflateInit2(state.get(), window);
    return state;
  }
};

template <uint32_t flags = 0>
struct lzma_tag_t {
  using state_type = lzma_stream;
  using in_char_type = std::remove_const_t<std::remove_pointer_t<decltype(state_type::next_in)>>;
  using out_char_type = std::remove_pointer_t<decltype(state_type::next_out)>;
  using deflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, void, ::lzma_end>>;
  using inflate_state_type = std::unique_ptr<state_type, detail::end_deleter<state_type, void, ::lzma_end>>;
  using status_type = status_t;

  static status_type deflate(deflate_state_type& x, bool flush)
  {
    auto ret = ::lzma_code(x.get(), flush ? LZMA_FULL_FLUSH : LZMA_RUN);
    if (ret == LZMA_OK) {
      return status_type::CAN_CONTINUE;
    } else if (ret == LZMA_STREAM_END) {
      return status_type::END;
    } else {
      return status_type::ERROR;
    }
  }

  static status_type inflate(inflate_state_type& x)
  {
    auto ret = ::lzma_code(x.get(), LZMA_RUN);
    if (ret == LZMA_OK) {
      return status_type::CAN_CONTINUE;
    } else if (ret == LZMA_STREAM_END) {
      return status_type::END;
    } else {
      return status_type::ERROR;
    }
  }

  static deflate_state_type new_deflate_state()
  {
    deflate_state_type state{new state_type};
    *state = LZMA_STREAM_INIT;
    auto ret = ::lzma_easy_encoder(state.get(), LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
    assert(ret == LZMA_OK);
    return state;
  }

  static inflate_state_type new_inflate_state()
  {
    inflate_state_type state{new state_type};
    *state = LZMA_STREAM_INIT;
    auto ret = ::lzma_stream_decoder(state.get(), std::numeric_limits<uint64_t>::max(), flags);
    assert(ret == LZMA_OK);
    return state;
  }
};
} // namespace decomp_tags

template <typename Tag, typename StreamType = std::ifstream>
struct inf_istream {
  template <typename IStrm>
  class inf_streambuf : public std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>
  {
  private:
    using base_type = std::basic_streambuf<typename IStrm::char_type, std::char_traits<typename IStrm::char_type>>;
    using int_type = typename base_type::int_type;
    using char_type = typename base_type::char_type;
    using strm_in_buf_type = typename Tag::in_char_type;
    using strm_out_buf_type = typename Tag::out_char_type;

    constexpr static std::size_t CHUNK = (1 << 16);

    std::array<strm_in_buf_type, CHUNK> in_buf;
    std::array<char_type, CHUNK> out_buf;
    typename Tag::inflate_state_type strm = Tag::new_inflate_state();
    typename std::add_pointer<IStrm>::type src;

  public:
    explicit inf_streambuf(IStrm* in) : src(in) {}
    explicit inf_streambuf(Tag /*tag*/, IStrm* in) : inf_streambuf(in) {}

    [[nodiscard]] std::size_t bytes_read() const { return strm->total_out - (this->egptr() - this->gptr()); }

  protected:
    int_type underflow() override;
  };

  std::unique_ptr<StreamType> underlying;
  std::unique_ptr<inf_streambuf<StreamType>> buffer = std::make_unique<inf_streambuf<StreamType>>(underlying.get());
  std::streamsize gcount_ = 0;
  bool eof_ = false;

  inf_istream& read(char* s, std::streamsize count)
  {
    std::istream inflated{buffer.get()};
    inflated.read(s, count);
    gcount_ = inflated.gcount();
    eof_ = inflated.eof();
    return *this;
  }

  [[nodiscard]] bool eof() const { return eof_; }
  [[nodiscard]] std::streamsize gcount() const { return gcount_; }

  explicit inf_istream(std::string s) : underlying(std::make_unique<StreamType>(s)) {}
  explicit inf_istream(StreamType&& str) : underlying(std::make_unique<StreamType>(std::move(str))) {}
};

template <typename T, typename S>
template <typename I>
auto inf_istream<T, S>::inf_streambuf<I>::underflow() -> int_type
{
  std::array<strm_out_buf_type, std::tuple_size<decltype(out_buf)>::value> uns_out_buf;

  strm->avail_out = uns_out_buf.size();
  strm->next_out = uns_out_buf.data();
  do {
    // Check to see if we have consumed all available input
    if (strm->avail_in == 0) {
      // Check to see if the input stream is sane
      if (src->fail()) {
        this->setg(this->out_buf.data(), this->out_buf.data(), this->out_buf.data());
        return base_type::underflow();
      }

      // Read data from the stream and convert to zlib-appropriate format
      std::array<char_type, std::tuple_size<decltype(in_buf)>::value> sig_in_buf;
      src->read(sig_in_buf.data(), sig_in_buf.size());
      auto bytes_read = src->gcount();
      assert(bytes_read >= 0);
      std::memcpy(in_buf.data(), sig_in_buf.data(), static_cast<std::size_t>(src->gcount()));

      // Record that bytes are available in in_buf
      strm->avail_in = static_cast<unsigned>(src->gcount());
      strm->next_in = in_buf.data();

      // If we failed to get any data
      if (strm->avail_in == 0) {
        this->setg(this->out_buf.data(), this->out_buf.data(), this->out_buf.data());
        return base_type::underflow();
      }
    }

    // Perform inflation
    auto result = T::inflate(strm);
    assert(result == T::status_type::CAN_CONTINUE || result == T::status_type::END);
  }
  // Repeat until we actually get new output
  while (strm->avail_out == uns_out_buf.size());

  // Copy into a format appropriate for the stream
  std::memcpy(this->out_buf.data(), uns_out_buf.data(), uns_out_buf.size() - strm->avail_out);

  auto bytes_remaining = std::size(uns_out_buf) - strm->avail_out;
  assert(bytes_remaining <= std::numeric_limits<std::make_signed_t<decltype(bytes_remaining)>>::max());
  this->setg(this->out_buf.data(), this->out_buf.data(),
             std::next(this->out_buf.data(), static_cast<std::make_signed_t<decltype(bytes_remaining)>>(bytes_remaining)));
  return base_type::traits_type::to_int_type(this->out_buf.front());
}
} // namespace champsim

#endif
