#ifndef EXTENT_SET_H
#define EXTENT_SET_H

#include <array>
#include <tuple>

#include "address.h"
#include "extent.h"
#include "util/bit_enum.h"
#include "util/to_underlying.h"

namespace champsim
{
template <typename... Slices>
class extent_set
{
  using extent_container_type = std::tuple<Slices...>;
  extent_container_type extents;

public:
  extent_set(Slices... slices) : extents{slices...} {}

  template <typename Ext>
  [[nodiscard]] auto operator()(address_slice<Ext> addr) const
  {
    return std::apply([addr](auto... exts) { return std::tuple{address_slice{exts, addr}...}; }, extents);
  }

  [[nodiscard]] constexpr static auto size() { return std::tuple_size_v<extent_container_type>; }

  [[nodiscard]] auto bit_size() const
  {
    return std::apply([](auto... exts) { return (... + champsim::size(exts)); }, extents);
  }

  template <std::size_t I>
  [[nodiscard]] auto get() const
  {
    return std::get<I>(extents);
  }
};

namespace detail
{
template <std::size_t... i>
auto dynamic_extent_array_initializer(std::index_sequence<i...>)
{
  return std::array{((void)i, champsim::dynamic_extent{champsim::data::bits{}, champsim::data::bits{}})...};
}
} // namespace detail

template <typename... Szs>
[[nodiscard]] auto make_contiguous_extent_set(Szs... sizes)
{
  constexpr auto sz_len = sizeof...(Szs);
  if constexpr (sz_len < 2) {
    return extent_set<>{};
  }

  std::array<std::size_t, sz_len> arr{static_cast<std::size_t>(sizes)...};
  std::array<std::size_t, sz_len> lowers{};
  std::partial_sum(std::begin(arr), std::end(arr), std::begin(lowers));

  std::array<champsim::dynamic_extent, sz_len - 1> extents{detail::dynamic_extent_array_initializer(std::make_index_sequence<sz_len - 1>{})};
  std::transform(std::next(std::cbegin(lowers)), std::cend(lowers), std::cbegin(lowers), std::begin(extents), [](auto up, auto low) {
    return champsim::dynamic_extent{champsim::data::bits{up}, champsim::data::bits{low}};
  });
  return std::apply([](auto... x) { return extent_set(x...); }, extents);
}
} // namespace champsim

template <typename... Slices>
struct std::tuple_size<champsim::extent_set<Slices...>> : std::integral_constant<std::size_t, champsim::extent_set<Slices...>::size()> {
};

template <std::size_t I, typename... Slices>
struct std::tuple_element<I, champsim::extent_set<Slices...>> : std::tuple_element<I, std::tuple<Slices...>> {
};

template <std::size_t I, typename... Slices>
auto get(const champsim::extent_set<Slices...> s)
{
  return s.template get<I>();
}

#endif
