#ifndef UTIL_SPAN_H
#define UTIL_SPAN_H

namespace champsim {
  template <typename It>
    std::pair<It, It> get_span(It begin, It end, typename std::iterator_traits<It>::difference_type sz)
    {
      assert(std::distance(begin, end) >= 0);
      assert(sz >= 0);
      auto distance = std::min(std::distance(begin, end), sz);
      return {begin, std::next(begin, distance)};
    }

  template <typename It, typename F>
    std::pair<It, It> get_span_p(It begin, It end, typename std::iterator_traits<It>::difference_type sz, F&& func)
    {
      auto [span_begin, span_end] = get_span(begin, end, sz);
      return {span_begin, std::find_if_not(span_begin, span_end, std::forward<F>(func))};
    }
}

#endif
