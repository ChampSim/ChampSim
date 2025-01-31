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

#ifndef UTIL_ALGORITHM_H
#define UTIL_ALGORITHM_H

#include <algorithm>
#include <boost/random.hpp>

#include "bandwidth.h"
#include "util/span.h"

namespace champsim
{
template <typename InputIt, typename OutputIt, typename F>
auto extract_if(InputIt begin, InputIt end, OutputIt d_begin, F func)
{
  begin = std::find_if(begin, end, func);
  for (auto i = begin; i != end; ++i) {
    if (func(*i)) {
      *d_begin++ = std::move(*i);
    } else {
      *begin++ = std::move(*i);
    }
  }
  return std::pair{begin, d_begin};
}

template <typename R, typename Output, typename F, typename G>
long int transform_while_n(R& queue, Output out, bandwidth sz, F&& test_func, G&& transform_func)
{
  auto [begin, end] = champsim::get_span_p(std::begin(queue), std::end(queue), sz, std::forward<F>(test_func));
  auto retval = std::distance(begin, end);
  std::transform(begin, end, out, std::forward<G>(transform_func));
  queue.erase(begin, end);
  return retval;
}

/**
 * This is a re-implementation of std::shuffle that uses Boost's implementation
 * of uniform_int_distribution instead of the standard library's.
 *
 * This is necessary because std::uniform_int_distrubtion is implemented
 * differently across platforms/compilers/etc. This can cause ChampSim to
 * produce different results for the same seed on different platforms.
 *
 * Since boost's implementation is platform-agnostic, this version should yield
 * identical results regardless of platform.
 *
 * \ref https://en.cppreference.com/w/cpp/algorithm/random_shuffle
 * \ref https://www.boost.org/doc/libs/1_87_0/doc/html/boost_random/reference.html
 */
template <class RandomIt, class URBG>
void shuffle(RandomIt first, RandomIt last, URBG&& g)
{
  typedef typename std::iterator_traits<RandomIt>::difference_type difference_type;
  typedef boost::random::uniform_int_distribution<difference_type> distribution_type;
  typedef typename distribution_type::param_type param_type;

  distribution_type D;
  for (difference_type i = last - first - 1; i > 0; --i) {
    std::swap(first[i], first[D(g, param_type(0, i))]);
  }
}
} // namespace champsim

#endif
