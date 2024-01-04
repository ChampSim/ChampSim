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

#ifndef LZMA_STREAM_TAG_H
#define LZMA_STREAM_TAG_H

#include <lzma.h>

#include "decomp_tags_util.h"

namespace champsim::decomp_tags
{
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
} // namespace champsim::decomp_tags

#endif
