#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

constexpr unsigned lg2(uint64_t n) { return n < 2 ? 0 : 1 + lg2(n / 2); }

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0) { return ((1ull << (begin - end)) - 1) << end; }

constexpr uint64_t splice_bits(uint64_t upper, uint64_t lower, std::size_t bits) { return (upper & ~bitmask(bits)) | (lower & bitmask(bits)); }

template <typename T>
struct is_valid {
  using argument_type = T;
  is_valid() {}
  bool operator()(const argument_type& test) { return test.valid; }
};

template <typename T>
struct is_valid<std::optional<T>> {
  bool operator()(const std::optional<T>& test) { return test.has_value(); }
};

template <typename T, typename = void>
struct eq_addr
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    const addr_type match_addr;
    const std::size_t shamt;

    explicit eq_addr(addr_type addr, std::size_t shamt = 0) : match_addr(addr), shamt(shamt) {}
    explicit eq_addr(const argument_type &elem, std::size_t shamt = 0) : eq_addr(elem.address, shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && (test.address >> shamt) == (match_addr >> shamt);
    }
};

// Specialization for types that include a member T::asid
template <typename T>
struct eq_addr<T, std::void_t<decltype(T::asid)>>
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    using asid_type = decltype(T::asid);

    const asid_type match_asid;
    const addr_type match_addr;
    const std::size_t shamt;

    eq_addr(asid_type asid, addr_type addr, std::size_t shamt = 0) : match_asid(asid), match_addr(addr), shamt(shamt) {}
    explicit eq_addr(const argument_type &elem, std::size_t shamt = 0) : eq_addr(elem.asid, elem.address, shamt) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.asid == match_asid && (test.address >> shamt) == (match_addr >> shamt);
    }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_minimal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !lhs_unary(lhs) || (rhs_unary(rhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename BIN, typename U = T, typename UN_T = is_valid<T>, typename UN_U = is_valid<U>>
struct invalid_is_maximal {
  bool operator()(const T& lhs, const U& rhs)
  {
    UN_T lhs_unary;
    UN_U rhs_unary;
    BIN cmp;

    return !rhs_unary(rhs) || (lhs_unary(lhs) && cmp(lhs, rhs));
  }
};

template <typename T, typename U = T>
struct cmp_event_cycle {
  bool operator()(const T& lhs, const U& rhs) { return lhs.event_cycle < rhs.event_cycle; }
};

template <typename T>
struct min_event_cycle : invalid_is_maximal<T, cmp_event_cycle<T>> {
};

template <typename T, typename U = T>
struct ord_event_cycle {
  using first_argument_type = T;
  using second_argument_type = U;
  bool operator()(const first_argument_type& lhs, const second_argument_type& rhs)
  {
    is_valid<first_argument_type> first_validtest;
    is_valid<second_argument_type> second_validtest;
    return !second_validtest(rhs) || (first_validtest(lhs) && lhs.event_cycle < rhs.event_cycle);
  }
};

namespace champsim
{

template <typename T>
class simple_lru_table
{
  struct block_t {
    uint64_t address;
    uint64_t last_used = 0;
    T data;
  };

  const std::size_t NUM_SET, NUM_WAY, shamt;
  uint64_t access_count = 0;
  std::vector<block_t> block{NUM_SET * NUM_WAY};

  auto get_set_span(uint64_t index)
  {
    auto set_idx = (index >> shamt) & bitmask(lg2(NUM_SET));
    auto set_begin = std::next(std::begin(block), set_idx * NUM_WAY);
    return std::pair{set_begin, std::next(set_begin, NUM_WAY)};
  }

  auto match_func(uint64_t index)
  {
    return [index, shamt = this->shamt](auto x) {
      return x.last_used > 0 && (x.address >> shamt) == (index >> shamt);
    };
  }

public:
  simple_lru_table(std::size_t sets, std::size_t ways, std::size_t shamt) : NUM_SET(sets), NUM_WAY(ways), shamt(shamt) {}

  std::optional<T> check_hit(uint64_t index)
  {
    auto [set_begin, set_end] = get_set_span(index);
    auto hit_block = std::find_if(set_begin, set_end, match_func(index));

    if (hit_block == set_end)
      return std::nullopt;

    hit_block->last_used = ++access_count;
    return hit_block->data;
  }

  void fill_cache(uint64_t index, T data)
  {
    auto [set_begin, set_end] = get_set_span(index);
    auto fill_block = std::find_if(set_begin, set_end, match_func(index));

    if (fill_block == set_end)
      fill_block = std::min_element(set_begin, set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

    *fill_block = {index, ++access_count, data};
  }

  std::optional<T> invalidate(uint64_t index)
  {
    auto [set_begin, set_end] = get_set_span(index);
    auto hit_block = std::find_if(set_begin, set_end, match_func(index));

    if (hit_block == set_end)
      return std::nullopt;

    auto oldval = std::exchange(*hit_block, {0, 0, {}});
    return oldval.data;
  }
};

} // namespace champsim

#endif
