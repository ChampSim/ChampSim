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

#ifndef GZIP_STREAM_TAG_H
#define GZIP_STREAM_TAG_H

#include <zlib.h>

#include "decomp_tags_util.h"

namespace champsim::decomp_tags
{
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
} // namespace champsim::decomp_tags

#endif
