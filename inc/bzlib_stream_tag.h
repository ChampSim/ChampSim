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

#ifndef BZLIB_STREAM_TAG_H
#define BZLIB_STREAM_TAG_H

#include <bzlib.h>

#include "decomp_tags_util.h"

namespace champsim::decomp_tags
{
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
} // namespace champsim::decomp_tags

#endif
